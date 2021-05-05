
struct VertexOutput
{
	float2 Tex	: TEXCOORD;
	float4 Pos	: SV_Position;
};

VertexOutput vs_main(in uint VertID : SV_VertexID)
{
	VertexOutput Output;
	// Texture coordinates range [0, 2], but only [0, 1] appears on screen.
	float2 Tex = float2(uint2(VertID, VertID << 1) & 2);
	Output.Tex = Tex;
	Output.Pos = float4(lerp(float2(-1, 1), float2(1, -1), Tex), 0, 1);
	return Output;
}

float4 ps_main(in VertexOutput Input) : SV_Target0
{
	return float4(Input.Tex, 0.f, 1.f);
}