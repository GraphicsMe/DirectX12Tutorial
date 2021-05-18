#pragma pack_matrix(row_major)
#include "AtmosphericScatteringCommon.hlsl"

Texture2D<float> Input		: register(t0);
RWTexture2D<float4> Output	: register(u0);


[numthreads(8, 8, 1)]
void cs_main(uint3 DispatchThreadID: SV_DispatchThreadID)
{
	float4 NDCPos = float4(2 * DispatchThreadID.x / ScreenParams.x - 1.0, 1.0 - 2 * DispatchThreadID.y / ScreenParams.y, 1.0, 1.0);
	float4 ViewPos = mul(NDCPos, InvProjectionMatrix);
	ViewPos /= ViewPos.w;
	float4 WorldPos = mul(ViewPos, InvViewMatrix);
	float4 CameraWorldPos = mul(float4(0.0, 0.0, 0.0, 1.0), InvViewMatrix);
	float3 ViewDir = normalize(WorldPos.xyz - CameraWorldPos.xyz);
	Output[DispatchThreadID.xy] = AtmosphericScattering(CameraWorldPos.xyz, ViewDir, 1.0, StepCount);;
}