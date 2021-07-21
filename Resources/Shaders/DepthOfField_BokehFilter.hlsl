
//#define KERNEL_SMALL
//#define KERNEL_MEDIUM
#define KERNEL_LARGE
//#define KERNEL_VERYLARGE
#include "DiskKernels.hlsl"

RWTexture2D<float4> BokehColor			: register(u0);
Texture2D<float4>	PrefilterColor		: register(t0);

SamplerState LinearSampler : register(s0);

cbuffer CB0 : register(b0)
{
    float  BokehRadius;
};

//------------------------------------------------------- HELP FUNCTIONS

void GetSampleUV(uint2 ScreenCoord, inout float2 UV, inout float2 PixelSize)
{
    float2 ScreenSize;
    BokehColor.GetDimensions(ScreenSize.x, ScreenSize.y);
    float2 InvScreenSize = rcp(ScreenSize);
    PixelSize = InvScreenSize;
    UV = ScreenCoord * InvScreenSize + 0.5 * InvScreenSize;
}

//------------------------------------------------------- ENTRY POINT
// Bokeh filter with disk-shaped kernels

[numthreads(8, 8, 1)]
void cs_FragBokehFilter(uint3 DTid : SV_DispatchThreadID)
{
    float2 PixelSize, UV;
    GetSampleUV(DTid.xy, UV, PixelSize);

    float4 center = PrefilterColor.SampleLevel(LinearSampler, UV, 0);

    float4 bgAcc = 0.0; // Background: far field bokeh
    float4 fgAcc = 0.0; // Foreground: near field bokeh
    for (int k = 0; k < kSampleCount; ++k)
    {
        float2 offset = kDiskKernel[k] * BokehRadius;
        float dist = length(offset);
        offset *= PixelSize;
        float4 samp = PrefilterColor.SampleLevel(LinearSampler, UV + offset, 0);

        // BG: Compare CoC of the current sample and the center sample and select smaller one.
        float bgCoC = max(min(center.a, samp.a), 0.0);

        // Compare the CoC to the sample distance. Add a small margin to smooth out.
        const float margin = PixelSize.y * 2;
        float bgWeight = saturate((bgCoC - dist + margin) / margin);
        // Foregound's coc is negative
        float fgWeight = saturate((-samp.a - dist + margin) / margin);

        // Cut influence from focused areas because they're darkened by CoC premultiplying. This is only needed for near field.
        // 减少聚焦区域的影响，它们因CoC预乘而变暗。这仅适用于近场。
        fgWeight *= step(PixelSize.y, -samp.a);

        // Accumulation
        bgAcc += half4(samp.rgb, 1.0) * bgWeight;
        fgAcc += half4(samp.rgb, 1.0) * fgWeight;
    }

    // Get the weighted average.
    bgAcc.rgb /= bgAcc.a + (bgAcc.a == 0.0); // zero-div guard
    fgAcc.rgb /= fgAcc.a + (fgAcc.a == 0.0);

    // FG: Normalize the total of the weights.
    // 归一化前散景权重
    fgAcc.a *= 3.14159265359 / kSampleCount;

    // Alpha premultiplying
    // alpha存储的是前散景的权重
    float alpha = saturate(fgAcc.a);
    // 前散景和后散景融合
    float3 rgb = lerp(bgAcc.rgb, fgAcc.rgb, alpha);

    BokehColor[DTid.xy] = float4(rgb, alpha);
}