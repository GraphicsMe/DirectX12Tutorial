Texture2D<float4> SceneColor : register(t0);

SamplerState LinearSampler	: register(s0);
SamplerState PointSampler	: register(s1);

float4 PS_CopyBuffer(float2 Tex : TEXCOORD, float4 ScreenPos : SV_Position) : SV_Target
{
	return SceneColor.SampleLevel(LinearSampler, Tex, 0);
}