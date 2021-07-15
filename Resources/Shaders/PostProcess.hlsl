#include "ShaderUtils.hlsl"


struct VertexOutput
{
	float2 Tex	: TEXCOORD;
	float4 Pos	: SV_Position;
};

Texture2D SceneColorTexture : register(t0);
Texture2D BloomTexture : register(t1);
SamplerState LinearSampler	: register(s0);

VertexOutput VS_ScreenQuad(in uint VertID : SV_VertexID)
{
	VertexOutput Output;
	// Texture coordinates range [0, 2], but only [0, 1] appears on screen.
	float2 Tex = float2(uint2(VertID, VertID << 1) & 2);
	Output.Tex = Tex;
	Output.Pos = float4(lerp(float2(-1, 1), float2(1, -1), Tex), 0, 1);
	return Output;
}

////-------------------------------------------------------
//// Simple post processing shader 
//// only tonemap and gamma
////-------------------------------------------------------
float4 PS_Main(in VertexOutput Input) : SV_Target0
{
	float3 Color = SceneColorTexture.Sample(LinearSampler, Input.Tex).xyz;
	return float4(ToneMapping(Color), 1.0);
}

float4 PS_ToneMapAndBloom(in VertexOutput Input) : SV_Target0
{
	float3 Color = SceneColorTexture.Sample(LinearSampler, Input.Tex).xyz;
	float3 Bloom = BloomTexture.Sample(LinearSampler, Input.Tex).xyz;
	return float4(ToneMapping(Color + Bloom), 1.0);
}

cbuffer Contants : register(b0)
{
	float BloomThreshold;
};

RWTexture2D<float3> BloomResult : register(u0);

void GetSampleUV(uint2 ScreenCoord, inout float2 UV, inout float2 HalfPixelSize)
{
	float2 ScreenSize;
	BloomResult.GetDimensions(ScreenSize.x, ScreenSize.y);
	float2 InvScreenSize = rcp(ScreenSize);
	HalfPixelSize = 0.5 * InvScreenSize;
	UV = ScreenCoord * InvScreenSize + HalfPixelSize;
}

[numthreads(8, 8, 1)]
void CS_ExtractBloom(uint3 DispatchThreadID : SV_DispatchThreadID)
{
	float2 HalfPixelSize, UV;
	GetSampleUV(DispatchThreadID.xy, UV, HalfPixelSize);

	float3 Color = SceneColorTexture.SampleLevel(LinearSampler, UV, 0).xyz;
	// clamp to avoid artifacts from exceeding fp16 through framebuffer blending of multiple very bright lights
	Color.rgb = min(float3(256 * 256, 256 * 256, 256 * 256), Color.rgb);

	half TotalLuminance = Luminance(Color);
	half BloomLuminance = TotalLuminance - BloomThreshold;
	half BloomAmount = saturate(BloomLuminance * 0.5f);
	BloomResult[DispatchThreadID.xy] = BloomAmount * Color;
}


[numthreads(8, 8, 1)]
void CS_DownSample(uint3 DispatchThreadID : SV_DispatchThreadID)
{
	float2 HalfPixelSize, UV;
	GetSampleUV(DispatchThreadID.xy, UV, HalfPixelSize);

	float3 Result = SceneColorTexture.SampleLevel(LinearSampler, UV, 0).xyz * 4.0;
	Result += SceneColorTexture.SampleLevel(LinearSampler, UV - HalfPixelSize, 0).xyz;
	Result += SceneColorTexture.SampleLevel(LinearSampler, UV + HalfPixelSize, 0).xyz;
	Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(HalfPixelSize.x, -HalfPixelSize.y), 0).xyz;
	Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(-HalfPixelSize.x, HalfPixelSize.y), 0).xyz;
	BloomResult[DispatchThreadID.xy] = Result / 8.0;
}

[numthreads(8, 8, 1)]
void CS_UpSample(uint3 DispatchThreadID : SV_DispatchThreadID)
{
	float2 HalfPixelSize, UV;
	GetSampleUV(DispatchThreadID.xy, UV, HalfPixelSize);

	float3 Result = 0;
	Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(-HalfPixelSize.x * 2.0, 0.0), 0).xyz;				//left
	Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(+HalfPixelSize.x * 2.0, 0.0), 0).xyz;				//right
	Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(0.0, -HalfPixelSize.y * 2.0), 0).xyz;				//up
	Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(0.0, +HalfPixelSize.y * 2.0), 0).xyz;				//bottom
	Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(-HalfPixelSize.x, -HalfPixelSize.y), 0).xyz * 2.0;  //top-left
	Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(+HalfPixelSize.x, -HalfPixelSize.y), 0).xyz * 2.0;  //top-right
	Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(-HalfPixelSize.x, +HalfPixelSize.y), 0).xyz * 2.0;  //bottom-left
	Result += SceneColorTexture.SampleLevel(LinearSampler, UV + float2(+HalfPixelSize.x, +HalfPixelSize.y), 0).xyz * 2.0;  //bottom-right
	BloomResult[DispatchThreadID.xy] = Result / 12.0;
}