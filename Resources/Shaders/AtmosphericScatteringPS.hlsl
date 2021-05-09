#pragma pack_matrix(row_major)

struct VertexOutput
{
	float2 Tex	: TEXCOORD;
	float4 Pos	: SV_Position;
};

cbuffer CSConstant : register(b0)
{
	float4 ScreenParams; //width, height, invWidth, invHeight
	float4x4 InvProjectionMatrix;
	float4x4 InvViewMatrix;
}

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

float2 RaySphereIntersection(float3 RayOrigin, float3 RayDir, float3 SphereCenter, float SphereRadius)
{
	float3 OA = RayOrigin - SphereCenter;
	float A = dot(RayDir, RayDir);
	float B = 2 * dot(OA, RayDir);
	float C = dot(OA, OA) - SphereRadius * SphereRadius;
	float delta = B * B - 4 * A * C;
	if (delta < 0)
	{
		return float2(-1, -1);
	}
	else
	{
		delta = sqrt(delta);
		return float2(-B - delta, -B + delta) / (2 * A);
	}
}

float4 AtmosphericScattering(float3 RayOrigin, float3 RayDir, float RayLength, float3 PlanetCenter, float DistanceScale, float3 LightDir, float SampleCount, out float4 Extinction)
{

}

float4 ps_main(in VertexOutput Input) : SV_Target0
{
	//float2 UV = float2(Input.Pos.x / ScreenParams.x, Input.Pos.y / ScreenParams.y);
	float2 UV = Input.Tex;
	float4 NDCPos = float4(2 * UV.x - 1.0, 1 - 2 * UV.y, 1.0, 1.0);
	float4 ViewPos = mul(NDCPos, InvProjectionMatrix);
	ViewPos /= ViewPos.w;
	float4 WorldPos = mul(ViewPos, InvViewMatrix);
	return WorldPos;
}