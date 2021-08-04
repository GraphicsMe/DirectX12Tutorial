#pragma pack_matrix(row_major)

#include "ShaderUtils.hlsl"

Texture2D GBufferA		: register(t0); // normal
Texture2D GBufferB		: register(t1); // metallSpecularRoughness
Texture2D GBufferC		: register(t2); // AlbedoAO
Texture2D SceneDepthZ	: register(t3); // Depth
Texture2D SceneColor	: register(t4); // history scene buffer
TextureCube CubeMap		: register(t5); // history scene buffer

SamplerState LinearSampler	: register(s0);
SamplerState SSRSampler		: register(s1);

cbuffer PSContant : register(b0)
{
	float4x4 ViewProj;
	float4x4 InvViewProj;
	float3	CameraPos;
};

static const int MaxStep = 256;
static const float MaxWorldDistance = 1.0;
static const float Thickness = 0.007;

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
		Depth = SceneDepthZ.SampleLevel(SSRSampler, Ray.xy, 0).x;
		if (Ray.z > Depth + Thickness)
		{
			OutHitUVz = Ray;
			return true;
		}
	}

	return true;
}

float4 PS_SSR(float2 Tex : TEXCOORD, float4 ScreenPos : SV_Position) : SV_Target
{
	float3 N = GBufferA.Sample(LinearSampler, Tex).xyz;
	N = 2.0 * N - 1.0;

	float Depth = SceneDepthZ.Sample(LinearSampler, Tex).x;
	float3 Screen0 = float3(Tex, Depth);
	float3 World0 = UnprojectScreen(Screen0);

	float3 V = normalize(CameraPos - World0);
	float3 R = reflect(-V, N); //incident ray, surface normal
	float3 World1 = World0 + R * MaxWorldDistance;
	float3 Screen1 = ProjectWorldPos(World1);

	float3 StartScreen = Screen0;			//[0, 1]
	float3 StepScreen = Screen1 - Screen0;	//[-1, 1]

	float3 HitUVz;
	bool bHit = CastSimpleRay(StartScreen, StepScreen, HitUVz);

	if (bHit)
	{
		return SceneColor.Sample(LinearSampler, HitUVz.xy);
	}
	return float4(0.0, 0.0, 0.0, 0.0);
}