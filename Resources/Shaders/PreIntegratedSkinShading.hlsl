#pragma pack_matrix(row_major)

struct VertexInput
{
	float3 Position : POSITION;
	float2 Tex		: TEXCOORD;
	float3 Normal	: NORMAL;
	float4 Tangent	: TANGENT;
};

struct PixelInput
{
	float4 Position : SV_Position;
	float2 Tex		: TEXCOORD0;
	float3 T		: TEXCOORD1;
	float3 B		: TEXCOORD2;
	float3 N		: TEXCOORD3;
	float3 WorldPos : TEXCOORD4;
};

struct PixelOutput
{
	float4 Target0 : SV_Target0;
};

cbuffer VSContant : register(b0)
{
	float4x4 ModelMatrix;
	float4x4 ViewProjMatrix;
};

cbuffer PSContant : register(b0)
{
	float3	CameraPos;
	float   CurveFactor;
	float3	LightDir;
	int		DebugFlag;
	float3	SubsurfaceColor;
	float	UseBlurNoaml;
	float3	TuneNormalBlur;
	float	Smooth;
	float	SpecularScale;
};

Texture2D BaseMap				: register(t0);
Texture2D OpacityMap			: register(t1);
Texture2D EmissiveMap			: register(t2);
Texture2D MetallicMap			: register(t3);
Texture2D RoughnessMap			: register(t4);
Texture2D AOMap					: register(t5);
Texture2D NormalMap				: register(t6);
Texture2D PreintegratedSkinLut	: register(t7);
Texture2D BlurNormalMap			: register(t8);
Texture2D SpecularBRDF			: register(t9);

SamplerState LinearSampler		: register(s0);


PixelInput VS_PreIntegratedSkin(VertexInput In)
{
	PixelInput Out;
	Out.Tex = In.Tex;

	float4 WorldPos = mul(float4(In.Position, 1.0), ModelMatrix);
	float4 ClipPos = mul(WorldPos, ViewProjMatrix);
	Out.Position = ClipPos;

	Out.WorldPos = WorldPos;

	Out.N = mul(In.Normal, (float3x3)ModelMatrix);
	Out.T = mul(In.Tangent.xyz, (float3x3)ModelMatrix);
	Out.B = cross(In.Normal, In.Tangent.xyz) * In.Tangent.w;
	Out.B = mul(Out.B, (float3x3)ModelMatrix);

	return Out;
}

float3 SkinDiffuse(float curv, float3 NdotL)
{
	float3 lookup = saturate(NdotL * 0.5 + 0.5);
	float3 diffuse;
	diffuse.r = PreintegratedSkinLut.Sample(LinearSampler, float2(lookup.r, curv)).r;
	diffuse.g = PreintegratedSkinLut.Sample(LinearSampler, float2(lookup.g, curv)).g;
	diffuse.b = PreintegratedSkinLut.Sample(LinearSampler, float2(lookup.b, curv)).b;

	return diffuse;
}

void PS_PreIntegratedSkin(PixelInput In, out PixelOutput Out)
{
	float3 Albedo = BaseMap.Sample(LinearSampler, In.Tex).xyz;
	float Metallic = MetallicMap.Sample(LinearSampler, In.Tex).x;
	float Roughness = RoughnessMap.Sample(LinearSampler, In.Tex).x;
	float AO = AOMap.Sample(LinearSampler, In.Tex).x;
	float3 Emissive = EmissiveMap.Sample(LinearSampler, In.Tex).xyz;

	float3 BaseColor = Albedo * (1 - Metallic);

	float3x3 TBN = float3x3(normalize(In.T), normalize(In.B), normalize(In.N));
	
	float3 normMapHigh = NormalMap.Sample(LinearSampler, In.Tex).xyz * 2.0 - 1.0;
	float3 normMapLow = BlurNormalMap.Sample(LinearSampler, In.Tex).xyz * 2.0 - 1.0;

	float3 N_high = mul(normMapHigh, TBN);
	float3 N_low = mul(normMapLow, TBN);

	// lerp NormalMaps
	float3 rN = lerp(N_high, N_low, TuneNormalBlur.r);
	float3 gN = lerp(N_high, N_low, TuneNormalBlur.g);
	float3 bN = lerp(N_high, N_low, TuneNormalBlur.b);

	//float3 L = float3(-1, -1, -1);
	float3 L = -LightDir;
	float3 V = normalize(CameraPos - In.WorldPos);
	float3 H = normalize(L + V);

	// curvatura
	float deltaWorldNormal = length(fwidth(In.N));
	float deltaWorldPos = length(fwidth(In.WorldPos));
	float curvatura = saturate((deltaWorldNormal / deltaWorldPos) * CurveFactor);

	// Diffuse Lighting
	bool bUseBlurNormal = UseBlurNoaml > 0;
	float3 NdotL = 0;
	if (bUseBlurNormal)
	{
		NdotL = float3(dot(rN, L), dot(gN, L), dot(bN, L));
	}
	else
	{
		NdotL = dot(In.N, L);
	}

	float3 diffuse = SkinDiffuse(curvatura, NdotL) * BaseColor * SubsurfaceColor;

	float NoH = dot(N_high, H);
	float PH = pow(2.0 * SpecularBRDF.Sample(LinearSampler, float2(NoH, Smooth)).g, 10.0);
	float F = 0.028;
	float3 specular = max(PH * F / dot(H, H), 0) * SpecularScale;

	float3 FinalColor;
	if (DebugFlag == 1)
	{
		FinalColor = diffuse;
	}
	else if (DebugFlag == 2)
	{
		FinalColor = specular;
	}
	else
	{
		FinalColor = diffuse + specular;
	}

	Out.Target0 = float4(FinalColor, 1);
}