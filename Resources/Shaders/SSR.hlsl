#pragma pack_matrix(row_major)

#include "ShaderUtils.hlsl"

Texture2D GBufferA		: register(t0); // normal
Texture2D GBufferB		: register(t1); // MetallicSpecularRoughness
Texture2D GBufferC		: register(t2); // AlbedoAO
Texture2D SceneDepthZ	: register(t3); // Depth
Texture2D<float2> HiZBuffer		: register(t4); // Hi-Z Depth
Texture2D HistorySceneColor	: register(t5); // history scene buffer
Texture2D VelocityBuffer	: register(t6); // velocity buffer
TextureCube CubeMap			: register(t7); // environment map

SamplerState LinearSampler	: register(s0);
SamplerState PointSampler	: register(s1);

cbuffer PSContant : register(b0)
{
	float4x4 ViewProj;
	float4x4 InvViewProj;
	float4x4 ClipToPreClipNoAA;
	float4 RootSizeMipCount;
	float4 HZBUvFactorAndInvFactor;
	float3	CameraPos;
	float Thickness;
	float WorldThickness;
	float CompareTolerance;
	float UseHiZ;
	float UseMinMaxZ;
	int NumRays;
	int FrameIndexMod8;
};


static const int MaxStep = 256;

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

float2 GetMinMaxDepthPlanes(float2 Ray, float Level)
{
	return HiZBuffer.SampleLevel(PointSampler, float2(Ray.x, Ray.y), Level).rg;
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
	float PerPixelThickness = abs(Direction.z);
	float PerPixelCompareTolerance = 0.85 * PerPixelThickness;

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
		if (Depth + PerPixelCompareTolerance < Ray.z && Ray.z < Depth + PerPixelThickness)
		{
			OutHitUVz = Ray;
			return true;
		}
	}

	return true;
}

bool WithinThickness(float3 Ray, float MinZ, float TheThickness)
{
	return Ray.z < MinZ + TheThickness;
}

bool CastHiZRay(float3 Start, float3 Direction, out float3 OutHitUVz)
{
	float PerPixelThickness = abs(Direction.z);
	float PerPixelCompareTolerance = 0.85 * PerPixelThickness;

	Direction = normalize(Direction);

	const float2 TextureSize = RootSizeMipCount.xy;
	const float HIZ_MAX_LEVEL = RootSizeMipCount.z - 1;
	const float2 HIZ_CROSS_EPSILON = 0.5 / TextureSize;
	
	float Level = HIZ_START_LEVEL;
	float Iteration = 0.f;

	float2 CrossStep = sign(Direction.xy);
	float2 CrossOffset = CrossStep * HIZ_CROSS_EPSILON;
	CrossStep = saturate(CrossStep);

	float3 Ray = Start;
	float3 D = Direction.xyz / Direction.z;
	float3 O = IntersectDepthPlane(Start, D, -Start.z);

	bool intersected = false;
	float2 RayCell = GetCell(Ray.xy, TextureSize);
	Ray =  IntersectCellBoundary(O, D, RayCell, TextureSize, CrossStep, CrossOffset);
	while (Level >= HIZ_STOP_LEVEL && Iteration < MAX_ITERATIONS)
	{
		const float2 CellCount = GetCellCount(TextureSize, Level);
		const float2 OldCellIdx = GetCell(Ray.xy, CellCount);
		if (Ray.z > 1.0)
			return false;
		
		float2 MinMaxZ = GetMinMaxDepthPlanes(Ray.xy, Level);
		float3 TempRay = IntersectDepthPlane(O, D, max(Ray.z, MinMaxZ.x + PerPixelCompareTolerance));
		const float2 NewCellIdx = GetCell(TempRay.xy, CellCount);
		if (CrossedCellBoundary(OldCellIdx, NewCellIdx))
		{
			TempRay = IntersectCellBoundary(O, D, OldCellIdx, CellCount, CrossStep, CrossOffset);
			Level = min(HIZ_MAX_LEVEL, Level + 2);
		}
		else if (Level == HIZ_START_LEVEL && WithinThickness(TempRay, MinMaxZ.x, PerPixelThickness))
		{
			intersected = true;
		}
		Ray = TempRay;
		--Level;
		++Iteration;
	}
	OutHitUVz = Ray;
	return intersected;
}

float ComputeHitVignetteFromScreenPos(float2 ScreenPos)
{
	float2 Vignette = saturate(abs(ScreenPos) * 5 - 4);

	return saturate(1.0 - dot(Vignette, Vignette));
}

void ReprojectHit(float3 HitUVz, out float2 OutPrevUV, out float OutVignette)
{
	float2 ThisScreen = 2.0 * HitUVz.xy - 1.0; //[-1,1]
	float4 ThisClip = float4(ThisScreen, HitUVz.z, 1);
	float4 PrevClip = mul(ThisClip, ClipToPreClipNoAA);
	float2 PrevScreen = PrevClip.xy / PrevClip.w;

	float2 Velocity = VelocityBuffer.SampleLevel(PointSampler, HitUVz.xy, 0).xy;
	PrevScreen = ThisClip.xy - Velocity;

	float2 PrevUV = 0.5 * PrevScreen.xy + 0.5;

	OutVignette = min(ComputeHitVignetteFromScreenPos(ThisScreen), ComputeHitVignetteFromScreenPos(PrevScreen));
	OutPrevUV = PrevUV;
}

float4 PS_SSR(float2 Tex : TEXCOORD, float4 SVPosition : SV_Position) : SV_Target
{
	float3 N = GBufferA.Sample(LinearSampler, Tex).xyz;
	N = 2.0 * N - 1.0;

	float2 uv = Tex * HZBUvFactorAndInvFactor.xy * 0.5;
	float Depth = UseHiZ > 0.f ? GetMinimumDepthPlane(uv, 0) : SceneDepthZ.SampleLevel(LinearSampler, Tex, 0).x;
	
	float3 Screen0 = float3(Tex, Depth);
	float3 World0 = UnprojectScreen(Screen0);
	Screen0 = UseHiZ > 0.f ? ApplyHZBUvFactor(Screen0) : Screen0;

	float3 V = normalize(CameraPos - World0);
	//float3 L = reflect(-V, N); //incident ray, surface normal

	float3 MetallicSpecularRoughness = GBufferB.SampleLevel(LinearSampler, Tex, 0).xyz;
	float Roughness = MetallicSpecularRoughness.z;
	float a = Roughness * Roughness;
	float a2 = a * a;

	uint2 PixelPos = (uint2)SVPosition.xy;
	uint2 Random = Rand3DPCG16(int3(PixelPos, FrameIndexMod8)).xy;

	float3x3 TangentBasis = GetTangentBasis(N);
	float3 TangentV = mul(TangentBasis, V);

	float4 OutColor = 0;
	for (int i = 0; i < NumRays; i++)
	{
		float2 E = Hammersley16(i, NumRays, Random);
		float3 H = mul(ImportanceSampleVisibleGGX(UniformSampleDisk(E), a2, TangentV).xyz, TangentBasis);
		float3 L = 2 * dot(V, H) * H - V;
		//float3 L = reflect(-V, H);
		//float3 L = reflect(-V, N);

		float3 World1 = World0 + L * WorldThickness;
		float3 Screen1 = ProjectWorldPos(World1);
		Screen1 = UseHiZ > 0.f ? ApplyHZBUvFactor(Screen1) : Screen1;

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
			HitUVz.xy = UseHiZ > 0.f ? HitUVz.xy * HZBUvFactorAndInvFactor.zw * 2 : HitUVz.xy;

			float Vignette;
			float2 PrevUV;
			ReprojectHit(HitUVz, PrevUV, Vignette);
			OutColor += HistorySceneColor.SampleLevel(LinearSampler, PrevUV, 0) * Vignette;
		}
	}
	OutColor /= NumRays;
	
	return OutColor;
}