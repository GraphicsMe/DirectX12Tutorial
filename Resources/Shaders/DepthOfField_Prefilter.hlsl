#include "ShaderUtils.hlsl"

RWTexture2D<float4> PrefilterColor		: register(u0);
Texture2D<float3>	SceneColor			: register(t0);
Texture2D<float>	CoCBuffer			: register(t1);

SamplerState LinearSampler : register(s0);

cbuffer CB0 : register(b0)
{
    float4  Resolution;
};

//------------------------------------------------------- HELP FUNCTIONS

void GetSampleUV(uint2 ScreenCoord, inout float2 UV, inout float2 HalfPixelSize)
{
    float2 ScreenSize;
    PrefilterColor.GetDimensions(ScreenSize.x, ScreenSize.y);
    float2 InvScreenSize = rcp(ScreenSize);
    HalfPixelSize = 0.5 * InvScreenSize;
    UV = ScreenCoord * InvScreenSize + HalfPixelSize;
}

//------------------------------------------------------- ENTRY POINT
// Prefilter: downsampling and premultiplying

[numthreads(8, 8, 1)]
void cs_FragPrefilter(uint3 DTid : SV_DispatchThreadID)
{
    float2 HalfPixelSize, UV;
    GetSampleUV(DTid.xy, UV, HalfPixelSize);

    // screenPos
    float2 uv0 = UV - HalfPixelSize;
    float2 uv1 = UV + HalfPixelSize;
    float2 uv2 = UV + float2(HalfPixelSize.x, -HalfPixelSize.y);
    float2 uv3 = UV + float2(-HalfPixelSize.x, HalfPixelSize.y);

    // Sample source colors
    float3 c0 = SceneColor.SampleLevel(LinearSampler, uv0, 0).xyz;
    float3 c1 = SceneColor.SampleLevel(LinearSampler, uv1, 0).xyz;
    float3 c2 = SceneColor.SampleLevel(LinearSampler, uv2, 0).xyz;
    float3 c3 = SceneColor.SampleLevel(LinearSampler, uv3, 0).xyz;

    float3 result = (c0 + c1 + c2 + c3) * 0.25;

    // Sample CoCs
    float coc0 = CoCBuffer.SampleLevel(LinearSampler, uv0, 0).r;
    float coc1 = CoCBuffer.SampleLevel(LinearSampler, uv1, 0).r;
    float coc2 = CoCBuffer.SampleLevel(LinearSampler, uv2, 0).r;
    float coc3 = CoCBuffer.SampleLevel(LinearSampler, uv3, 0).r;

    // average coc
    // float coc = (coc0 + coc1 + coc2 + coc3) * 0.25;

    // we need most max 
    float cocMin = min(min(min(coc0, coc1), coc2), coc3);
    float cocMax = max(max(max(coc0, coc1), coc2), coc3);
    float coc = cocMax >= -cocMin ? cocMax : cocMin;

    // Apply CoC and luma weights to reduce bleeding and flickering
    float w0 = abs(coc0) / (Max3(c0.r, c0.g, c0.b) + 1.0);
    float w1 = abs(coc1) / (Max3(c1.r, c1.g, c1.b) + 1.0);
    float w2 = abs(coc2) / (Max3(c2.r, c2.g, c2.b) + 1.0);
    float w3 = abs(coc3) / (Max3(c3.r, c3.g, c3.b) + 1.0);

    // Weighted average of the color samples
    half3 avg = c0 * w0 + c1 * w1 + c2 * w2 + c3 * w3;
    avg /= max(w0 + w1 + w2 + w3, 1e-4);
    avg *= smoothstep(0, HalfPixelSize.y * 4, abs(coc));

    // alpha通道存储了coc值
    PrefilterColor[DTid.xy] = float4(avg, coc);
}