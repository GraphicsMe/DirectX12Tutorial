
Texture2D			SceneColor		: register(t0); // color
Texture2D			SSSBlurColor	: register(t1); // color

SamplerState LinearSampler : register(s0);
SamplerState PointSampler  : register(s1);

cbuffer PSContant : register(b0)
{
	float3	SubsurfaceColor;
	float   EffectStr;
};

struct PixelOutput
{
	float4	Target0 : SV_Target0;
};

PixelOutput PS_SSSCombine(float2 Tex : TEXCOORD, float4 ScreenPos : SV_Position)
{
	PixelOutput Out;
	Out.Target0 = float4(0, 0, 0, 0);

	float3 SceneColor = SSSBlurColor.SampleLevel(LinearSampler, Tex, 0).rgb;
	float3 SSSColor = SSSBlurColor.SampleLevel(LinearSampler, Tex, 0).rgb;
	float3 Color = SceneColor + EffectStr * SSSColor * SubsurfaceColor;
	Out.Target0 = float4(Color, 1);
	return Out;
}


