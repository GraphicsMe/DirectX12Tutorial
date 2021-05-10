
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

float3 ACESFilm(float3 x)
{
	//return x;
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float4 ps_main(in VertexOutput Input) : SV_Target0
{
	float3 Color = ScatteringTexture.Sample(LinearSampler, Input.Tex).xyz;
	return float4(ACESFilm(Color), 1.0);
}