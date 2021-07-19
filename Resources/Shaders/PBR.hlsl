#pragma pack_matrix(row_major)

#include "ShaderUtils.hlsl"
#include "PixelPacking_Velocity.hlsli"

cbuffer VSContant : register(b0)
{
	float4x4 ModelMatrix;
	float4x4 ViewProjMatrix;
	float4x4 PreviousModelMatrix;
	float4x4 PreviousViewProjMatrix;
	float2	 ViewportSize;
};

cbuffer PSContant : register(b0)
{
	float	Exposure;
	int		MipLevel;
	int		MaxMipLevel;
	int		NumSamplesPerDir;
	int		Degree;
	float3	CameraPos;
	int		bSHDiffuse;
	float3	pad;
	float3	Coeffs[16];
};


Texture2D BaseMap 			: register(t0);
Texture2D OpacityMap 		: register(t1);
Texture2D EmissiveMap 		: register(t2);
Texture2D MetallicMap 		: register(t3);
Texture2D RoughnessMap 		: register(t4);
Texture2D AOMap 			: register(t5);
Texture2D NormalMap 		: register(t6);
TextureCube IrradianceCubeMap : register(t7);
TextureCube PrefilteredCubeMap: register(t8);
Texture2D PreintegratedGF 	: register(t9);

SamplerState LinearSampler	: register(s0);

//RWTexture2D<packed_velocity_t> VelocityBuffer : register(u0);
RWTexture2D<float2> VelocityBuffer            : register(u0);

struct VertexInput
{
	float3 Position : POSITION;
	float2 Tex		: TEXCOORD;
	float3 Normal	: NORMAL;
	float4 Tangent	: TANGENT;
};

struct PixelInput
{
	float4 Position	: SV_Position;
	float2 Tex		: TEXCOORD0;
	float3 T		: TEXCOORD1;
	float3 B		: TEXCOORD2;
	float3 N		: TEXCOORD3;
	float3 WorldPos	: TEXCOORD4;
	float3 PreviousScreenPos : TEXCOORD5;
	float3 CurrentScreenPos  : TEXCOORD6;
};

PixelInput VS_PBR(VertexInput In)
{
	PixelInput Out;
	Out.Tex = In.Tex;

	float4 PreviousWorldPos = mul(float4(In.Position, 1.0), PreviousModelMatrix); 
	float4 ClipPos = mul(PreviousWorldPos, PreviousViewProjMatrix);
	ClipPos /= ClipPos.w;
	Out.PreviousScreenPos.xy = ClipPos.xy * 0.5 + 0.5;
	Out.PreviousScreenPos.y = 1 - Out.PreviousScreenPos.y;
	Out.PreviousScreenPos.xy *= ViewportSize;
	Out.PreviousScreenPos.z = ClipPos.z;

	float4 WorldPos = mul(float4(In.Position, 1.0), ModelMatrix);
	ClipPos = mul(WorldPos, ViewProjMatrix);

	Out.WorldPos = WorldPos.xyz;
	Out.Position = ClipPos;

	ClipPos /= ClipPos.w;
	Out.CurrentScreenPos.xy = ClipPos.xy * 0.5 + 0.5;
	Out.CurrentScreenPos.y = 1 - Out.CurrentScreenPos.y;
	Out.CurrentScreenPos.xy *= ViewportSize;
	Out.CurrentScreenPos.z = ClipPos.z;

	//Out.WorldPos = WorldPos.xyz;
	//Out.Position = mul(float4(Out.WorldPos, 1), ViewProjMatrix);

	Out.N = mul(In.Normal, (float3x3)ModelMatrix);
	Out.T = mul(In.Tangent.xyz, (float3x3)ModelMatrix);
	Out.B = cross(In.Normal, In.Tangent.xyz) * In.Tangent.w;
	Out.B = mul(Out.B, (float3x3)ModelMatrix);
	
	return Out;
}

float4 visualizeVec(float3 v)
{
	float3 vv = (v + 1) / 2;
	return float4(vv, 1.0);
}


float3 F_schlickR(float cosTheta, float3 F0, float roughness)
{
	return F0 + (max(1.0 - roughness, F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

float4 PS_PBR(PixelInput In) : SV_Target
{
	// write velocity
	VelocityBuffer[In.Position.xy] = In.PreviousScreenPos.xy - In.CurrentScreenPos.xy;

	float Opacity = OpacityMap.Sample(LinearSampler, In.Tex).r;
	clip(Opacity < 0.1f ? -1 : 1);

	float3 Albedo = BaseMap.Sample(LinearSampler, In.Tex).xyz;
	float Metallic = MetallicMap.Sample(LinearSampler, In.Tex).x;
	float Roughness = RoughnessMap.Sample(LinearSampler, In.Tex).x;
	float AO = AOMap.Sample(LinearSampler, In.Tex).x;

	float3x3 TBN = float3x3(normalize(In.T), normalize(In.B), normalize(In.N));
	float3 tNormal = NormalMap.Sample(LinearSampler, In.Tex).xyz;
	tNormal = 2 * tNormal - 1.0; // [0,1] -> [-1, 1]
	float3 N = mul(tNormal, TBN);

	float3 V = normalize(CameraPos - In.WorldPos);
	float3 R = reflect(-V, N); //incident ray, surface normal

	float NoV = saturate(dot(N, V));
	float3 F0 = lerp(0.04, Albedo.rgb, Metallic);
	float3 F = F_schlickR(NoV, F0, Roughness);

	float3 kD = (1.0 - F) * (1.0 - Metallic);

	float3 Irradiance = 0;
	if (bSHDiffuse)
	{
		// SH Irradiance
		Irradiance = GetSHIrradiance(N, 4, Coeffs);
	}
	else
	{
		Irradiance = IrradianceCubeMap.SampleLevel(LinearSampler, N, 0).xyz;
	}

	float3 Diffuse = Albedo * kD * Irradiance;

	float Mip = ComputeReflectionCaptureMipFromRoughness(Roughness, MaxMipLevel-1);
	float2 BRDF = PreintegratedGF.Sample(LinearSampler, float2(NoV, Roughness)).rg;

	
	float3 PrefilteredColor = PrefilteredCubeMap.SampleLevel(LinearSampler, R, Mip).rgb;
	float3 Specular = PrefilteredColor * (F * BRDF.x + BRDF.y);

	float3 Emissive = EmissiveMap.Sample(LinearSampler, In.Tex).xyz;
	float3 FinalColor = Emissive + (Diffuse + Specular) * AO;

	return float4(FinalColor, 1);
}