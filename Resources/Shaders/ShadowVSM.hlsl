#pragma pack_matrix(row_major)

cbuffer PSConstant : register(b0)
{
	float3 LightDirection;
	float MinVariance;
}

Texture2D DiffuseTexture				: register(t0);
Texture2D ShadowMap						: register(t1);
SamplerState LinearSampler				: register(s0);
SamplerState ShadowSampler				: register(s1);


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

float ChebyshevUpperBound(float2 Moments, float t) 
{
	float Variance = Moments.y - Moments.x * Moments.x;
	Variance = max(Variance, MinVariance);

	// Compute probabilistic upper bound.
	float d = t - Moments.x;
	float pMax = Variance / (Variance + d * d);
	return (t <= Moments.x ? 1.0 : pMax);
}

float ComputeShadow(float4 ShadowCoord, float3 Normal)
{
	float2 Moments = ShadowMap.Sample(ShadowSampler, ShadowCoord.xy).xy;
	return ChebyshevUpperBound(Moments, saturate(ShadowCoord.z));
}

PixelOutput ps_main(VertexOutput Input)
{
	PixelOutput output;
	float4 texColor = DiffuseTexture.Sample(LinearSampler, Input.tex);
	float Shadow = ComputeShadow(Input.ShadowCoord, Input.normal);
	output.outFragColor = texColor * saturate(0.2 + Shadow);
	return output;
}