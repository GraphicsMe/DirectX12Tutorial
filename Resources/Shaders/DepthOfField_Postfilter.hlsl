

RWTexture2D<float4> PostfilterColor		: register(u0);
Texture2D<float4>	BokehColor		    : register(t0);

SamplerState LinearSampler : register(s0);

cbuffer CB0 : register(b0)
{
    float  BokehRadius;
};

//------------------------------------------------------- HELP FUNCTIONS
void GetSampleUV(uint2 ScreenCoord, inout float2 UV, inout float2 HalfPixelSize)
{
    float2 ScreenSize;
    PostfilterColor.GetDimensions(ScreenSize.x, ScreenSize.y);
    float2 InvScreenSize = rcp(ScreenSize);
    HalfPixelSize = 0.5 * InvScreenSize;
    UV = ScreenCoord * InvScreenSize + HalfPixelSize;
}

//------------------------------------------------------- ENTRY POINT
// tent filter
/**
*   1 2 1
*   2 4 2
*   1 2 1
*/

[numthreads(8, 8, 1)]
void cs_FragPostfilter(uint3 DTid : SV_DispatchThreadID)
{
    // 9 tap tent filter with 4 bilinear samples
    float2 HalfPixelSize, UV;
    GetSampleUV(DTid.xy, UV, HalfPixelSize);

    float2 uv0 = UV - HalfPixelSize;
    float2 uv1 = UV + HalfPixelSize;
    float2 uv2 = UV + float2(HalfPixelSize.x, -HalfPixelSize.y);
    float2 uv3 = UV + float2(-HalfPixelSize.x, HalfPixelSize.y);

    float4 acc = 0;
    acc += BokehColor.SampleLevel(LinearSampler, uv0, 0);
    acc += BokehColor.SampleLevel(LinearSampler, uv1, 0);
    acc += BokehColor.SampleLevel(LinearSampler, uv2, 0);
    acc += BokehColor.SampleLevel(LinearSampler, uv3, 0);

    PostfilterColor[DTid.xy] = acc / 4.0f;
}