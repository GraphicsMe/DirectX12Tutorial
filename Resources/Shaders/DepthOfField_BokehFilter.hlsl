
//#define KERNEL_SMALL
#define KERNEL_MEDIUM
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

// 权重值
float Weigh(float coc, float radius)
{
    //return coc >= radius;
    return saturate((coc - radius + 2) / 2);
}

//------------------------------------------------------- ENTRY POINT
// Bokeh filter with disk-shaped kernels

[numthreads(8, 8, 1)]
void cs_FragBokehFilter(uint3 DTid : SV_DispatchThreadID)
{
    float2 PixelSize, UV;
    GetSampleUV(DTid.xy, UV, PixelSize);

#if 0
    float3 color = 0;
    float weight = 0;
    for (int k = 0; k < kSampleCount; ++k)
    {
        float2 offset = kDiskKernel[k] * BokehRadius;
        // 到采样点的距离
        float radius = length(offset);
        offset *= PixelSize;
        float4 s = PrefilterColor.SampleLevel(LinearSampler, UV + offset, 0);
        // 比较采样点的Coc是否覆盖了着色点，给予一个权重
        float sw = Weigh(abs(s.a), radius);
        color += s.rgb * sw;
        weight += sw;
    }
    color *= 1.0 / weight;
    BokehColor[DTid.xy] = float4(color, 1);
#endif

    // processing pixel
    float4 current = PrefilterColor.SampleLevel(LinearSampler, UV, 0);

    float3 bgColor = 0, fgColor = 0;
    float bgWeight = 0, fgWeight = 0;
    for (int k = 0; k < kSampleCount; ++k)
    {
        float2 offset = kDiskKernel[k] * BokehRadius;
        // 到采样点的距离
        float radius = length(offset);
        offset *= PixelSize;
        float4 sample = PrefilterColor.SampleLevel(LinearSampler, UV + offset, 0);

        float bgw = Weigh(max(0, sample.a), radius);
        bgColor += sample.rgb * bgw;
        bgWeight += bgw;

        float fgw = Weigh(-sample.a, radius);
        fgColor += sample.rgb * fgw;
        fgWeight += fgw;
    }

    // 防止为0
    bgColor *= 1 / (bgWeight + (bgWeight == 0));
    fgColor *= 1 / (fgWeight + (fgWeight == 0));

    // 重组前景 & 后景
    //float bgfg = min(1, fgWeight);
    float bgfg = min(1, fgWeight * 3.14159265359 / kSampleCount);

    half3 color = lerp(bgColor, fgColor, bgfg);
    BokehColor[DTid.xy] = float4(color, bgfg);
}