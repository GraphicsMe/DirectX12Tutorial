#include "ShaderUtils.hlsl"


struct VertexOutput
{
	float2 Tex	: TEXCOORD;
	float4 Pos	: SV_Position;
};

cbuffer PSContant : register(b0)
{
	float2 TexelSize;
	int SrvLevel;
};

Texture2D SrcMipTexture	: register(t0);
SamplerState LinearSampler	: register(s0);

float4 PS_Main(in VertexOutput Input) : SV_Target0
{
	float4 c= SrcMipTexture.SampleLevel(LinearSampler, Input.Tex, SrvLevel);
	return c*1.0;
}