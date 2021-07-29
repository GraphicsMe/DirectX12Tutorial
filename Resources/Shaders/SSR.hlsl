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

static const int MaxStep = 128;
static const float MaxWorldDistance = 1.0;
static const float Thickness = 0.007;

bool CastScreenSpaceRay(float3 RayStartScreen, float3 RayStepScreen, out float3 OutHitUVz)
{
	float3 RayStartUVz = float3(RayStartScreen.xy * float2(0.5, -0.5) + 0.5, RayStartScreen.z); //[0,1]
	float3 RayStepUVz =  float3(RayStepScreen.xy  * float2(0.5, -0.5),	   RayStepScreen.z); //[-1,1]
	float Step = 1.0 / MaxStep;
	RayStepUVz *= Step;
	OutHitUVz = RayStartUVz;
	
	float Depth;
	float3 Ray = RayStartUVz;
	for (int i = 0; i < MaxStep; ++i)
	{
		Ray += RayStepUVz;
		if (Ray.z < 0 || Ray.z > 1)
			return false;
		Depth = SceneDepthZ.SampleLevel(SSRSampler, Ray.xy, 0).x;
		if (Ray.z > Depth + Thickness)
		{
			OutHitUVz = Ray;
			return true;
		}
	}
	return false;
}

float4 PS_SSR(float2 Tex : TEXCOORD, float4 ScreenPos : SV_Position) : SV_Target
{
	float3 N = GBufferA.Sample(LinearSampler, Tex).xyz;
	N = 2.0 * N - 1.0;

	float Depth = SceneDepthZ.Sample(LinearSampler, Tex).x;
	float2 ScreenCoord = float2(2.0, -2.0) * Tex + float2(-1.0, 1.0);
	float3 Screen0 = float3(ScreenCoord, Depth); //[-1,1]
	float4 World0 = mul(float4(Screen0,1.0), InvViewProj);
	World0 /= World0.w;

	float3 V = normalize(CameraPos - World0.xyz);
	float3 R = reflect(-V, N); //incident ray, surface normal
	float3 World1 = World0.xyz + R * MaxWorldDistance;
	float4 Clip1 = mul(float4(World1, 1.0), ViewProj);
	float3 Screen1 = Clip1.xyz / Clip1.w;

	float3 StartScreen = Screen0;			//[-1, 1]
	float3 StepScreen = Screen1 - Screen0;	//[-2, 2]
	
	float3 HitUVz;
	bool bHit = CastScreenSpaceRay(StartScreen, StepScreen, HitUVz);
	
	if (bHit)
	{
		return SceneColor.Sample(LinearSampler, HitUVz.xy);
	}
	return float4(0.0, 0.0, 0.0, 0.0);
}