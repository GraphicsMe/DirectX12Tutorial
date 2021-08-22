#pragma pack_matrix(row_major)

#include "ShaderUtils.hlsl"

Texture2D GBufferA : register(t0); // normal
Texture2D GBufferB : register(t1); // metallSpecularRoughness
Texture2D GBufferC : register(t2); // AlbedoAO
Texture2D SceneDepthZ : register(t3); // Depth

SamplerState LinearSampler : register(s0);

cbuffer PSContant : register(b0)
{
	float4x4	InvViewProj;
	float3		CameraPos;
	float		pad;
	float3		LightPos;
	float		LightScale;
	float3		LightColor;
	float		IsDirectionalLight;
};

struct BxDFContext
{
	float NoV;
	float NoL;
	float VoL;
	float NoH;
	float VoH;
};

struct FDirectLighting
{
	float3	Diffuse;
	float3	Specular;
	float3	Transmission;
};

void Init(inout BxDFContext Context, half3 N, half3 V, half3 L)
{
	Context.NoL = dot(N, L);
	Context.NoV = dot(N, V);
	Context.VoL = dot(V, L);
	float InvLenH = rsqrt(2 + 2 * Context.VoL);
	Context.NoH = saturate((Context.NoL + Context.NoV) * InvLenH);
	Context.VoH = saturate(InvLenH + InvLenH * Context.VoL);
}

// Diffuse
float3 Diffuse_Lambert(float3 DiffuseColor)
{
	return DiffuseColor * (1 / PI);
}

// [Burley 2012, "Physically-Based Shading at Disney"]
float3 Diffuse_Burley(float3 DiffuseColor, float Roughness, float NoV, float NoL, float VoH)
{
	float FD90 = 0.5 + 2 * VoH * VoH * Roughness;
	float FdV = 1 + (FD90 - 1) * Pow5(1 - NoV);
	float FdL = 1 + (FD90 - 1) * Pow5(1 - NoL);
	return DiffuseColor * ((1 / PI) * FdV * FdL);
}

// Appoximation of joint Smith term for GGX
// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
float Vis_SmithJointApprox(float a2, float NoV, float NoL)
{
	float a = sqrt(a2);
	float Vis_SmithV = NoL * (NoV * (1 - a) + a);
	float Vis_SmithL = NoV * (NoL * (1 - a) + a);
	return 0.5 * rcp(Vis_SmithV + Vis_SmithL);
}

// [Schlick 1994, "An Inexpensive BRDF Model for Physically-Based Rendering"]
float3 F_Schlick(float3 SpecularColor, float VoH)
{
	float Fc = Pow5(1 - VoH);					// 1 sub, 3 mul
	// Anything less than 2% is physically impossible and is instead considered to be shadowing
	return saturate(50.0 * SpecularColor.g) * Fc + (1 - Fc) * SpecularColor;
}

float3 SpecularGGX(float Roughness, float3 F0, BxDFContext Context, float NoL)
{
	float a2 = Pow4(Roughness);

	// need EnergyNormalization?

	float D = D_GGX(a2, Context.NoH);
	// denominator(4 * NoL * NoV) Reduce by G
	float Vis = Vis_SmithJointApprox(a2, Context.NoV, NoL);
	float3 F = F_Schlick(F0, Context.VoH);
	return (D * Vis) * F;
}

//Moving Frostbite to PBR
float GetSpecularOcclusion(float NoV, float AO, float roughness)
{
	return saturate(pow(NoV + AO, exp2(-16.0 * roughness - 1.0)) - 1.0 + AO);
}

struct PixelOutput
{
	float4	Color	 : SV_Target0;
};

PixelOutput PS_SponzaPBR(float2 Tex : TEXCOORD, float4 ScreenPos : SV_Position)
{
	PixelOutput Out;

	float3 N = GBufferA.Sample(LinearSampler, Tex).xyz;
	N = 2.0 * N - 1.0;

	float3 PBRParameters = GBufferB.Sample(LinearSampler, Tex).xyz;
	float Metallic = PBRParameters.x;
	float Specular = PBRParameters.y;
	float Roughness = PBRParameters.z;

	float4 AlbedoAo = GBufferC.Sample(LinearSampler, Tex);
	float AO = AlbedoAo.a;

	float Depth = SceneDepthZ.Sample(LinearSampler, Tex).x;

	float2 ScreenCoord = Tex;
	ScreenCoord = ViewportUVToScreenPos(ScreenCoord);
	float4 NDCPos = float4(ScreenCoord, Depth, 1.0f);
	float4 WorldPos = mul(NDCPos, InvViewProj);
	WorldPos /= WorldPos.w;

	float3 V = normalize(CameraPos - WorldPos.xyz);

	float3 L = 0;
	float falloff = 1;
	if (IsDirectionalLight)
	{
		L = -LightPos;
	}
	else
	{
		L = LightPos - WorldPos.xyz;
		float dis = length(L);
		falloff = saturate(1 - pow(dis / 3.0f, 4)) / (dis * dis + 1);
	}
	L = normalize(L);
	float3 H = normalize(L + V);

	float3 DiffuseColor = AlbedoAo.rgb - Metallic * AlbedoAo.rgb;

	// PBR Direct Lighting
	BxDFContext Context;
	Init(Context, N, V, L);
	Context.NoL = saturate(Context.NoL);
	Context.NoV = saturate(abs(Context.NoV) + 1e-5);

	float SpecularAO = GetSpecularOcclusion(Context.NoV, AO, Roughness);

	float3 Color = 0;

	FDirectLighting Lighting;
	Lighting.Diffuse = 0;
	Lighting.Specular = 0;

	// Diffuse
	Lighting.Diffuse = Context.NoL * Diffuse_Lambert(DiffuseColor) * falloff * LightScale * LightColor * AO;

	// Specular
	float3 F0 = ComputeF0(Specular, DiffuseColor, Metallic);
	Lighting.Specular = Context.NoL * SpecularGGX(Roughness, F0, Context, Context.NoL) * falloff * LightScale * LightColor * SpecularAO;

	Out.Color = float4(Lighting.Specular + Lighting.Diffuse, 1.0f);

	return Out;
}