#pragma pack_matrix(row_major)
#include "AtmosphericScatteringCommon.hlsl"

struct VertexOutput
{
	float2 Tex	: TEXCOORD;
	float4 Pos	: SV_Position;
};

Texture2D ScatteringTexture	: register(t0);
SamplerState LinearSampler	: register(s0);


VertexOutput vs_main(in uint VertID : SV_VertexID)
{
	VertexOutput Output;
	// Texture coordinates range [0, 2], but only [0, 1] appears on screen.
	float2 Tex = float2(uint2(VertID, VertID << 1) & 2);
	Output.Tex = Tex;
	Output.Pos = float4(lerp(float2(-1, 1), float2(1, -1), Tex), 0, 1);
	return Output;
}


float4 ps_main(in VertexOutput Input) : SV_Target0
{
	//float2 UV = float2(Input.Pos.x / ScreenParams.x, Input.Pos.y / ScreenParams.y);
	float2 UV = Input.Tex;
	float4 NDCPos = float4(2 * UV.x - 1.0, 1 - 2 * UV.y, 1.0, 1.0);
	float4 ViewPos = mul(NDCPos, InvProjectionMatrix);
	ViewPos /= ViewPos.w;
	float4 WorldPos = mul(ViewPos, InvViewMatrix);
	float4 CameraWorldPos = mul(float4(0.0, 0.0, 0.0, 1.0), InvViewMatrix);
	float3 ViewDir = normalize(WorldPos.xyz - CameraWorldPos.xyz);
	return AtmosphericScattering(CameraWorldPos.xyz, ViewDir, 1.0, StepCount);
}