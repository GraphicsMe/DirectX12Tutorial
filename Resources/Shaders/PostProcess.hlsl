#include "ShaderUtils.hlsl"


struct VertexOutput
{
	float2 Tex	: TEXCOORD;
	float4 Pos	: SV_Position;
};

Texture2D SceneColorTexture : register(t0);
SamplerState LinearSampler	: register(s0);

VertexOutput VS_ScreenQuad(in uint VertID : SV_VertexID)
{
	VertexOutput Output;
	// Texture coordinates range [0, 2], but only [0, 1] appears on screen.
	float2 Tex = float2(uint2(VertID, VertID << 1) & 2);
	Output.Tex = Tex;
	Output.Pos = float4(lerp(float2(-1, 1), float2(1, -1), Tex), 0, 1);
	return Output;
}

////-------------------------------------------------------
//// Simple post processing shader 
//// only tonemap and gamma
////-------------------------------------------------------
float4 PS_Main(in VertexOutput Input) : SV_Target0
{
	float3 Color = SceneColorTexture.Sample(LinearSampler, Input.Tex).xyz;
	return float4(ToneMapping(Color), 1.0);
}