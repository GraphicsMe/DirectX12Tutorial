#include "PixelPacking_Velocity.hlsli"

RWTexture2D<float4> OutTemporal : register(u0);
// SceneColor
Texture2D<float3> InColor : register(t0);
// TemporalBuffer
Texture2D<float4> InTemporal : register(t1);
// VelocityBuffer
Texture2D<packed_velocity_t> VelocityBuffer : register(t2);
// Depth
Texture2D<float> DepthBuffer : register(t3);

SamplerState LinearSampler : register(s0);

static const float BlendWeightLowerBound = 0.03f;
static const float BlendWeightUpperBound = 0.12f;
static const float BlendWeightVelocityScale = 100.0f * 60.0f;

cbuffer CB0 : register(b0)
{
    float2 RcpBufferDim;            // 1 / width, 1 / height
    float TemporalBlendFactor;
    float RcpSpeedLimiter;
    float2 ViewportJitter;
}

float Luminance(in float3 color)
{
    return dot(color, float3(0.25f, 0.50f, 0.25f));
}

float3 ToneMap(float3 color)
{
    // luma weight的色调映射
    return color / (1 + Luminance(color));
}

float3 UnToneMap(float3 color)
{
    // luma weight的反色调映射
    return color / (1 - Luminance(color));
}

float3 RGB2YCoCgR(float3 rgbColor)
{
    float3 YCoCgRColor;

    YCoCgRColor.y = rgbColor.r - rgbColor.b;
    float temp = rgbColor.b + YCoCgRColor.y / 2;
    YCoCgRColor.z = rgbColor.g - temp;
    YCoCgRColor.x = temp + YCoCgRColor.z / 2;

    return YCoCgRColor;
}

float3 YCoCgR2RGB(float3 YCoCgRColor)
{
    float3 rgbColor;

    float temp = YCoCgRColor.x - YCoCgRColor.z / 2;
    rgbColor.g = YCoCgRColor.z + temp;
    rgbColor.b = temp - YCoCgRColor.y / 2;
    rgbColor.r = rgbColor.b + YCoCgRColor.y;

    return rgbColor;
}

float HistoryClip(float3 History, float3 Filtered, float3 NeighborMin, float3 NeighborMax)
{
    float3 BoxMin = NeighborMin;
    float3 BoxMax = NeighborMax;

    float3 RayOrigin = History;
    float3 RayDir = Filtered - History;
    RayDir = abs(RayDir) < (1.0 / 65536.0) ? (1.0 / 65536.0) : RayDir;
    float3 InvRayDir = rcp(RayDir);

    float3 MinIntersect = (BoxMin - RayOrigin) * InvRayDir;
    float3 MaxIntersect = (BoxMax - RayOrigin) * InvRayDir;
    float3 EnterIntersect = min(MinIntersect, MaxIntersect);
    return max(EnterIntersect.x, max(EnterIntersect.y, EnterIntersect.z));
}

float3 ClampHistory(float3 NeighborMin, float3 NeighborMax, float3 HistoryColor, float3 Filtered)
{

    float3 TargetColor = Filtered;

    float ClipBlend = HistoryClip(HistoryColor, TargetColor, NeighborMin, NeighborMax);

    ClipBlend = saturate(ClipBlend);

    HistoryColor = lerp(HistoryColor, TargetColor, ClipBlend);

    return HistoryColor;
}

#define DynamicCameraTAA 1




[numthreads(8, 8, 1)]
void cs_main(
    uint3 DTid : SV_DispatchThreadID, 
    uint GI : SV_GroupIndex, 
    uint3 GTid : SV_GroupThreadID, 
    uint3 Gid : SV_GroupID)
{
    // first frame use currentColor
    float alpha = TemporalBlendFactor;
    if (alpha >= 1.0f)
    {
        OutTemporal[DTid.xy] = float4(InColor[DTid.xy], 1);
        return;
    }

#if DynamicCameraTAA
    // 当前像素的uv
    float2 uv = DTid.xy;

    // 带抖动的uv，采样DepthBuffer和InColor需要使用带抖动的uv，才能消除抖动的影响
    float2 jitteredUV = uv + ViewportJitter;

    // 采样速度缓冲
    float2 closestOffset = float2(0.0f, 0.0f);
    float closestDepth = 1.0f;
    // 求出3x3范围内的最近深度对应的偏移量，深度越小，越可能为前景
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float2 sampleOffset = float2(x, y);
            float2 sampleUV = jitteredUV + sampleOffset;

            float NeighborhoodDepthSamp = DepthBuffer[sampleUV];

            if (NeighborhoodDepthSamp < closestDepth)
            {
                closestDepth = NeighborhoodDepthSamp;
                closestOffset = sampleOffset;
            }
        }
    }

    float2 velocity = UnpackVelocity(VelocityBuffer[uv + closestOffset]).xy;
    // convert to 0-1
    float lenVelocity = length(velocity * RcpBufferDim); 

    float2 historyUV = uv + velocity;

    // current frame color
    float3 currColor = InColor[jitteredUV];
    currColor = ToneMap(currColor);
    currColor = RGB2YCoCgR(currColor);

    // sample history color
    float3 prevColor = InTemporal[historyUV].rgb;
    prevColor = ToneMap(prevColor);
    prevColor = RGB2YCoCgR(prevColor);

    // sample neighborhoods
    float2 InputSampleUV = jitteredUV;

    float3 neighborMin = float3(9999999.0f, 9999999.0f, 9999999.0f);
    float3 neighborMax = float3(-99999999.0f, -99999999.0f, -99999999.0f);

    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            // offset
            float2 sampleOffset = float2(x, y);
            float2 sampleUV = InputSampleUV + sampleOffset;

            // sample
            float3 NeighborhoodSamp = InColor[sampleUV];
            NeighborhoodSamp = max(NeighborhoodSamp, 0.0f);

            NeighborhoodSamp = ToneMap(NeighborhoodSamp);
            NeighborhoodSamp = RGB2YCoCgR(NeighborhoodSamp);

            // AAABB
            neighborMin = min(neighborMin, NeighborhoodSamp);
            neighborMax = max(neighborMax, NeighborhoodSamp);
        }
    }

    // simplest clip
    //prevColor = clamp(prevColor, neighborMin, neighborMax);
    
    // Neighborhood Clamping
    prevColor = ClampHistory(neighborMin, neighborMax, prevColor, (neighborMin + neighborMax) / 2.0f);

    // 当前像素的权重
    float weightCurr = lerp(BlendWeightLowerBound, BlendWeightUpperBound, saturate(lenVelocity * BlendWeightVelocityScale));
    float weightPrev = 1.0f - weightCurr;

    // rcp 求倒数
    float RcpWeight = rcp(weightCurr + weightPrev);
    float3 color = (currColor * weightCurr + prevColor * weightPrev) * RcpWeight;

    color = YCoCgR2RGB(color);
    color = UnToneMap(color);

    OutTemporal[uv] = float4(color, 1);



#else
    
    float2 uv = DTid.xy;
    float3 currColor = InColor[uv].rgb;
    float3 prevColor = InTemporal[uv].rgb;
    OutTemporal[DTid.xy] = float4(currColor * TemporalBlendFactor + (1 - TemporalBlendFactor) * prevColor, 1);

#endif

}
