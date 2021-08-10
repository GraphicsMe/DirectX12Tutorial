#pragma pack_matrix(row_major)

cbuffer VSContant : register(b0)
{
	float4x4 ModelMatrix;
	float4x4 ViewProjMatrix;
};

cbuffer PSContant : register(b0)
{
	float LightIntensity;
};

Texture2D LightColorMap : register(t0);
SamplerState LinearSampler : register(s0);

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
};

struct PixelOutput
{
	float4 Target0 : SV_Target0;
};

PixelInput VS_LightPolygon(VertexInput In)
{
	PixelInput Out;
	Out.Tex = In.Tex;

	float4 WorldPos = mul(float4(In.Position, 1.0), ModelMatrix);
	float4 ClipPos = mul(WorldPos, ViewProjMatrix);
	Out.Position = ClipPos;

	return Out;
}

void PS_LightPolygon(PixelInput In, out PixelOutput Out)
{
	float3 LightColor = LightColorMap.Sample(LinearSampler, In.Tex).xyz;

	LightColor *= LightIntensity;

	Out.Target0 = float4(LightColor, 1.0);
	//Out.Target0 = float4(In.Tex, 0, 1);
}