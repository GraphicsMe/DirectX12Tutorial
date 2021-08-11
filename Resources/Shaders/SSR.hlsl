#pragma pack_matrix(row_major)

#include "ShaderUtils.hlsl"

Texture2D GBufferA		: register(t0); // normal
Texture2D GBufferB		: register(t1); // metallSpecularRoughness
Texture2D GBufferC		: register(t2); // AlbedoAO
Texture2D SceneDepthZ	: register(t3); // Depth
Texture2D HiZBuffer		: register(t4); // Hi-Z Depth
Texture2D SceneColor	: register(t5); // history scene buffer
TextureCube CubeMap		: register(t6); // environment map

SamplerState LinearSampler	: register(s0);
SamplerState PointSampler	: register(s1);

cbuffer PSContant : register(b0)
{
	float4x4 ViewProj;
	float4x4 InvViewProj;
	float4 RootSizeMipCount;
	float4 HZBUvFactorAndInvFactor;
	float3	CameraPos;
	float Thickness;
	float CompareTolerance;
	float UseHiZ;
};


static const int MaxStep = 256;
static const float MaxWorldDistance = 1.0;

static const float HIZ_START_LEVEL = 0.0f;
static const float HIZ_STOP_LEVEL = 0.0f;
static const float MAX_ITERATIONS = 256.0;

float3 ProjectWorldPos(float3 WorldPos)
{
	float4 ClipPos = mul(float4(WorldPos, 1.0), ViewProj);
	float3 Projected = ClipPos.xyz / ClipPos.w;
	Projected.xy = Projected.xy * float2(0.5, -0.5) + 0.5; //[-1,1] -> [0,1]
	return Projected;
}

// unproject a screen point to world space
float3 UnprojectScreen(float3 ScreenPoint)
{
	ScreenPoint.xy = float2(2.0, -2.0) * ScreenPoint.xy + float2(-1.0, 1.0); //[0,1] -> [-1,1]
	float4 WorldPos = mul(float4(ScreenPoint, 1.0), InvViewProj);
	return WorldPos.xyz / WorldPos.w;
}

float3 ApplyHZBUvFactor(float3 ScreenPos)
{
	return float3(ScreenPos.xy * HZBUvFactorAndInvFactor.xy * 0.5, ScreenPos.z);
}

float3 IntersectDepthPlane(float3 RayOrigin, float3 RayDir, float t)
{
	return RayOrigin + RayDir * t;
}

float2 GetCellCount(float2 Size, float Level)
{
	return floor(Size / (Level > 0.0 ? exp2(Level) : 1.0));
}

float2 GetCell(float2 Ray, float2 CellCount)
{
	return floor(Ray * CellCount);
}

bool CrossedCellBoundary(float2 CellIdxA, float2 CellIdxB)
{
	return CellIdxA.x != CellIdxB.x || CellIdxA.y != CellIdxB.y;
}

float GetMinimumDepthPlane(float2 Ray, float Level)
{
	return HiZBuffer.SampleLevel(PointSampler, float2(Ray.x, Ray.y), Level).r;
}

float3 IntersectCellBoundary(
	float3 RayOrigin, float3 RayDirection, 
	float2 CellIndex, float2 CellCount, 
	float2 CrossStep, float2 CrossOffset)
{
	float2 Cell = CellIndex + CrossStep;
	Cell /= CellCount;
	Cell += CrossOffset;

	float2 delta = Cell - RayOrigin.xy;
	delta /= RayDirection.xy;
	float t = min(delta.x, delta.y);
	return IntersectDepthPlane(RayOrigin, RayDirection, t);
}

bool CastSimpleRay(float3 Start, float3 Direction, out float3 OutHitUVz)
{
	float2 TextureSize;
	SceneDepthZ.GetDimensions(TextureSize.x, TextureSize.y);
	int MaxLinearStep = max(TextureSize.x, TextureSize.y);

	Direction = normalize(Direction);
	float3 Step = Direction;
	float StepScale = abs(Direction.x) > abs(Direction.y) ? TextureSize.x : TextureSize.y;
	Step /= StepScale;

	float Depth;
	float3 Ray = Start;
	for (int i = 0; i < MaxLinearStep; ++i)
	{
		Ray += Step;
		if (Ray.z < 0 || Ray.z > 1)
			return false;
		Depth = SceneDepthZ.SampleLevel(PointSampler, Ray.xy, 0).x;
		if (Depth + CompareTolerance < Ray.z && Ray.z < Depth + Thickness)
		{
			OutHitUVz = Ray;
			return true;
		}
	}

	return true;
}

bool CastHiZRay(float3 Start, float3 Direction, out float3 OutHitUVz)
{
	Direction = normalize(Direction);

	const float2 TextureSize = RootSizeMipCount.xy;
	const float HIZ_MAX_LEVEL = RootSizeMipCount.z - 1;
	//const float2 HIZ_CROSS_EPSILON = min(0.0000001, 0.05 / TextureSize); // maybe need to be smaller or larger? this is mip level 0 texel size
	const float2 HIZ_CROSS_EPSILON = 0.5 / TextureSize;
	
	float Level = HIZ_START_LEVEL;
	float Iteration = 0.f;

	float2 CrossStep = sign(Direction.xy);
	float2 CrossOffset = CrossStep * HIZ_CROSS_EPSILON;
	CrossStep = saturate(CrossStep);

	float3 Ray = Start;
	float3 D = Direction.xyz / Direction.z;
	float3 O = IntersectDepthPlane(Start, D, -Start.z);

	float2 RayCell = GetCell(Ray.xy, TextureSize);
	Ray =  IntersectCellBoundary(O, D, RayCell, TextureSize, CrossStep, CrossOffset);
	while (Level >= HIZ_STOP_LEVEL && Iteration < MAX_ITERATIONS)
	{
		const float2 CellCount = GetCellCount(TextureSize, Level);
		const float2 OldCellIdx = GetCell(Ray.xy, CellCount);
	
		float MinZ = GetMinimumDepthPlane(Ray.xy, Level);
		float3 TempRay = IntersectDepthPlane(O, D, max(Ray.z, MinZ + CompareTolerance));
		const float2 NewCellIdx = GetCell(TempRay.xy, CellCount);
		if (CrossedCellBoundary(OldCellIdx, NewCellIdx))
		{
			TempRay = IntersectCellBoundary(O, D, OldCellIdx, CellCount, CrossStep, CrossOffset);
			Level = min(HIZ_MAX_LEVEL, Level + 2);
		}
		Ray = TempRay;
		--Level;
		++Iteration;
	}
	OutHitUVz = Ray;
	return true;
}

float4 PS_SSR(float2 Tex : TEXCOORD, float4 ScreenPos : SV_Position) : SV_Target
{
	float3 N = GBufferA.Sample(LinearSampler, Tex).xyz;
	N = 2.0 * N - 1.0;

	float Depth;
	if (UseHiZ > 0.f)
	{
		float2 uv = Tex * HZBUvFactorAndInvFactor.xy * 0.5;
		Depth = GetMinimumDepthPlane(uv, 0);
	}
	else
	{
		Depth = SceneDepthZ.SampleLevel(LinearSampler, Tex, 0).x;
	}
	
	float3 Screen0 = float3(Tex, Depth);
	float3 World0 = UnprojectScreen(Screen0);

	float3 V = normalize(CameraPos - World0);
	float3 R = reflect(-V, N); //incident ray, surface normal
	float3 World1 = World0 + R * MaxWorldDistance;
	float3 Screen1 = ProjectWorldPos(World1);

	if (UseHiZ > 0.f)
	{
		Screen0 = ApplyHZBUvFactor(Screen0);
		Screen1 = ApplyHZBUvFactor(Screen1);
	}

	float3 StartScreen = Screen0;			//[0, 1]
	float3 StepScreen = Screen1 - Screen0;	//[-1, 1]

	bool bHit;
	float3 HitUVz;
	if (UseHiZ > 0.f)
	{
		bHit = CastHiZRay(StartScreen, StepScreen, HitUVz);
	}
	else
	{
		bHit = CastSimpleRay(StartScreen, StepScreen, HitUVz);
	}

	if (bHit)
	{
		if (UseHiZ > 0.f)
		{
			float2 UV = HitUVz.xy * HZBUvFactorAndInvFactor.zw * 2;
			float4 Sample = SceneColor.SampleLevel(LinearSampler, UV, 0);
			return float4(Sample.xyz, 1.0);
		}
		else
		{
			return SceneColor.SampleLevel(LinearSampler, HitUVz.xy, 0);
		}
		
	}
	return float4(0.0, 0.0, 0.0, 0.0);
}