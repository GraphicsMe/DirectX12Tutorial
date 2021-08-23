#pragma pack_matrix(row_major)


cbuffer VSContant : register(b0)
{
	float4x4 ModelMatrix;
	float4x4 ViewProjection;
	float4x4 ShadowMatrix;
};

cbuffer PSConstant : register(b0)
{
	float3 LightDirection;
}

Texture2D DiffuseTexture				: register(t0);
Texture2D ShadowMap						: register(t1);
SamplerState LinearSampler				: register(s0);
SamplerState ShadowSampler				: register(s1);

struct VertexInput
{
    float3 inPos : POSITION;
	float2 tex   : TEXCOORD;
    float3 normal : NORMAL;
};

struct VertexOutput
{
	float2 tex			: TEXCOORD;
	float4 gl_Position	: SV_Position;
	float3 normal		: NORMAL;
	float4 ShadowCoord	: TEXCOORD1;
};

struct PixelOutput
{
	float4 outFragColor : SV_Target0;
};

VertexOutput vs_main(VertexInput Input)
{
	VertexOutput Output;
	float4 WorldPos = mul(float4(Input.inPos, 1.0f), ModelMatrix);
	Output.gl_Position = mul(WorldPos, ViewProjection);
	Output.tex = Input.tex;
	Output.normal = normalize(mul(Input.normal, (float3x3)ModelMatrix));
	Output.ShadowCoord = mul(WorldPos, ShadowMatrix);
	return Output;
}

float ComputeShadow(float4 ShadowCoord, float3 Normal)
{
	float SampleDepth = ShadowMap.Sample(ShadowSampler, ShadowCoord.xy).x;
	float CurentDepth = saturate(ShadowCoord.z);
	float Bias = 0.0001 + (1.0 - dot(Normal, -LightDirection)) * 0.0001;
	return CurentDepth-Bias < SampleDepth;
}

PixelOutput ps_main(VertexOutput Input)
{
	PixelOutput output;
	float4 texColor = DiffuseTexture.Sample( LinearSampler, Input.tex );
	float Shadow = ComputeShadow(Input.ShadowCoord, Input.normal);
	output.outFragColor = texColor * saturate(0.2 + Shadow);
	return output;
}