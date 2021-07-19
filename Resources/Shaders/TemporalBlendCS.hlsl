#include "PixelPacking_Velocity.hlsli"

RWTexture2D<float4> OutTemporal             : register(u0);
Texture2D<float3> InColor                   : register(t0);
Texture2D<float4> InTemporal                : register(t1);
//Texture2D<packed_velocity_t> VelocityBuffer : register(t2);
Texture2D<float2> VelocityBuffer            : register(t2);

Texture2D<float> DepthBuffer                : register(t3);

SamplerState MinMagLinearMipPointClamp      : register(s0);

cbuffer CB0 : register(b0)
{
    float4  Resolution;                     // width, height, 1/width, 1/height
    float2  ViewportJitter;
    int     FrameIndex;
}

//------------------------------------------------------- MACRO DEFINITION
#define SPATIAL_WEIGHT_CATMULLROM 1
#define LONGEST_VELOCITY_VECTOR_SAMPLES 0

//------------------------------------------------------- PARAMETERS
static const float Exposure = 10;
static const float BlendWeightLowerBound = 0.04f;
static const float BlendWeightUpperBound = 0.2f;
static const float MIN_VARIANCE_GAMMA = 0.75f;              // under motion
static const float MAX_VARIANCE_GAMMA = 2.f;                // no motion
static const float FRAME_VELOCITY_IN_PIXELS_DIFF = 128.0f;  // valid for 1920x1080

static const int2 SampleOffsets[9] =
{
    int2(-1, -1),
    int2(0, -1),
    int2(1, -1),
    int2(-1, 0),
    int2(0, 0),
    int2(1, 0),
    int2(-1, 1),
    int2(0, 1),
    int2(1, 1),
};

//------------------------------------------------------- HELP FUNCTIONS

float Luminance(in float3 color)
{
    return dot(color, float3(0.25f, 0.50f, 0.25f));
}

// Faster but less accurate luma computation. 
// Luma includes a scaling by 4.
float Luma4(float3 Color)
{
    return Color.r;
}

// Optimized HDR weighting function.
float HdrWeight4(float3 Color, float Exposure)
{
    return rcp(Luma4(Color) * Exposure + 4.0);
}

float3 ToneMap(float3 color)
{
    // luma weight' tonemap
    return color / (1 + Luminance(color));
}

float3 UnToneMap(float3 color)
{
    // luma weight' untonemap
    return color / (1 - Luminance(color));
}

float3 RGBToYCoCg(float3 RGB)
{
    float Y = dot(RGB, float3(1, 2, 1));
    float Co = dot(RGB, float3(2, 0, -2));
    float Cg = dot(RGB, float3(-1, 2, -1));

    float3 YCoCg = float3(Y, Co, Cg);
    return YCoCg;
}

float3 YCoCgToRGB(float3 YCoCg)
{
    float Y = YCoCg.x * 0.25;
    float Co = YCoCg.y * 0.25;
    float Cg = YCoCg.z * 0.25;

    float R = Y + Co - Cg;
    float G = Y + Cg;
    float B = Y - Co - Cg;

    float3 RGB = float3(R, G, B);
    return RGB;
}

static float CatmullRom(float x)
{
    float ax = abs(x);
    if (ax > 1.0f)
        return ((-0.5f * ax + 2.5f) * ax - 4.0f) * ax + 2.0f;
    else
        return (1.5f * ax - 2.5f) * ax * ax + 1.0f;
}

// Helper to convert ST coords to UV
float2 GetUV(float2 inST)
{
    return (inST + 0.5f.xx) * Resolution.zw;
}

float3 GetCurrentColour(float2 screenST)
{
    float2 uv = GetUV(screenST);
    float3 colour = InColor.SampleLevel(MinMagLinearMipPointClamp, uv, 0);
    //float3 colour = InColor[screenST];
    colour = ToneMap(colour);
    colour = RGBToYCoCg(colour);
    return colour;
}

float3 SampleHistory(float2 inHistoryST)
{
    float2 historyScreenUV = GetUV(inHistoryST);
    // TODO: Sample the history using Catmull-Rom to reduce blur on motion.
    // https://www.shadertoy.com/view/4tyGDD

    float3 history = InTemporal.SampleLevel(MinMagLinearMipPointClamp, historyScreenUV, 0).rgb;
    //float3 history = InTemporal[inHistoryST].rgb;
    history = ToneMap(history);
    history = RGBToYCoCg(history);
    return history;
}

float HistoryClip(float3 History, float3 Filtered, float3 NeighborMin, float3 NeighborMax)
{
    float3 BoxMin = NeighborMin;
    float3 BoxMax = NeighborMax;

    float3 RayOrigin = History;
    float3 RayDir = Filtered - History;
    RayDir.x = abs(RayDir.x) < (1.0 / 65536.0) ? (1.0 / 65536.0) : RayDir.x;
    RayDir.y = abs(RayDir.y) < (1.0 / 65536.0) ? (1.0 / 65536.0) : RayDir.y;
    RayDir.z = abs(RayDir.z) < (1.0 / 65536.0) ? (1.0 / 65536.0) : RayDir.z;
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

float2 WeightedLerpFactors(float WeightA, float WeightB, float Blend)
{
    float BlendA = (1.0 - Blend) * WeightA;
    float BlendB = Blend * WeightB;
    float RcpBlend = rcp(BlendA + BlendB);
    BlendA *= RcpBlend;
    BlendB *= RcpBlend;
    return float2(BlendA, BlendB);
}

float2 GetVelocity(float2 uv)
{
    float2 velocity = 0;
#if LONGEST_VELOCITY_VECTOR_SAMPLES
    const float2 offsets[8] = { float2(-1, -1), float2(-1, 0), float2(-1, 1), float2(0, 1), float2(1, 1), float2(1, 0), float2(1, -1), float2(0, -1) };
    const uint numberOfSamples = 8;

    float currentLengthSq = dot(velocity.xy, velocity.xy);
    [unroll]
    for (uint i = 0; i < numberOfSamples; ++i)
    {
        const float2 neighbor_velocity = VelocityBuffer[uv + offsets[i]].xy;
        const float sampleLengthSq = dot(neighbor_velocity.xy, neighbor_velocity.xy);
        if (sampleLengthSq > currentLengthSq)
        {
            velocity = neighbor_velocity;
            currentLengthSq = sampleLengthSq;
        }
    }
#else
    // 3x3 closest depth
    float2 closestOffset = float2(0.0f, 0.0f);
    float closestDepth = 1.0f;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float2 sampleOffset = float2(x, y);
            float2 sampleUV = uv + sampleOffset;

            float NeighborhoodDepthSamp = DepthBuffer[sampleUV];

            if (NeighborhoodDepthSamp < closestDepth)
            {
                closestDepth = NeighborhoodDepthSamp;
                closestOffset = sampleOffset;
            }
        }
    }
    velocity = VelocityBuffer[uv + closestOffset].xy;

#endif 
    return velocity;
}

//------------------------------------------------------- ENTRY POINT
[numthreads(8, 8, 1)]
void cs_main(
    uint3 DTid : SV_DispatchThreadID,
    uint GI : SV_GroupIndex,
    uint3 GTid : SV_GroupThreadID,
    uint3 Gid : SV_GroupID)
{
    float2 uv = GetUV(DTid.xy);
    if (uv.x >= 1.0f || uv.y >= 1.0f)
    {
        return;
    }

    // first frame use currentColor
    if (FrameIndex <= 1.0f)
    {
        OutTemporal[DTid.xy] = float4(InColor[DTid.xy], 1);
        return;
    }

    // screenPos
    const uint2 screenST = DTid.xy;

    float2 velocity = GetVelocity(screenST);
    // calculate confidence factor based on the velocity of current pixel, everything moving faster than FRAME_VELOCITY_IN_PIXELS_DIFF frame-to-frame will be marked as no-history
    const float velocityConfidenceFactor = saturate(1.f - length(velocity) / FRAME_VELOCITY_IN_PIXELS_DIFF);

    const float2 historyScreenST = screenST + velocity;
    const float2 historyScreenUV = GetUV(historyScreenST);
    const float uvWeight = (historyScreenUV >= float2(0.f, 0.f) && historyScreenUV <= float2(1.f, 1.f)) ? 1.0f : 0.f;
    const bool hasValidHistory = (velocityConfidenceFactor * uvWeight) > 0.f;

    if (hasValidHistory == false)
    {
        OutTemporal[DTid.xy] = float4(InColor[DTid.xy], 1);
        return;
    }

    // current frame color
    float3 currColor = GetCurrentColour(screenST);

    // sample history color
    float3 prevColor = SampleHistory(historyScreenST);
    
    // SetupSampleWeight
    float SampleWeights[9];
    float TotalWeight = 0.0f;
    for (int i = 0; i < 9; i++)
    {
        float PixelOffsetX = SampleOffsets[i].x;
        float PixelOffsetY = SampleOffsets[i].y;

#if SPATIAL_WEIGHT_CATMULLROM
        SampleWeights[i] = CatmullRom(PixelOffsetX) * CatmullRom(PixelOffsetY);
        TotalWeight += SampleWeights[i];
#else
        // Normal distribution, Sigma = 0.47
        SampleWeights[i] = exp(-2.29f * (PixelOffsetX * PixelOffsetX + PixelOffsetY * PixelOffsetY));
        TotalWeight += SampleWeights[i];
#endif
    }
    for (int i = 0; i < 9; i++)
    {
        SampleWeights[i] /= TotalWeight;
    }

    // sample neighborhoods
    uint N = 9;
    float3 m1 = 0.0f;
    float3 m2 = 0.0f;
    float3 neighborMin = float3(9999999.0f, 9999999.0f, 9999999.0f);
    float3 neighborMax = float3(-99999999.0f, -99999999.0f, -99999999.0f);

    // used for FilterColor
    float3 neighborhood[9];
    float NeighborsFinalWeight = 0;
    float3 NeighborsColor = 0;
    float3 FilteredColor = 0;
    
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            // convert to [0,8]
            int i = (y + 1) * 3 + x + 1;

            // offset
            float2 sampleOffset = float2(x, y);
            float2 sampleST = screenST + sampleOffset;

            // sample
            float3 NeighborhoodSamp = GetCurrentColour(sampleST);

            // cache
            neighborhood[i] = NeighborhoodSamp;

            // AAABB
            neighborMin = min(neighborMin, NeighborhoodSamp);
            neighborMax = max(neighborMax, NeighborhoodSamp);

            m1 += NeighborhoodSamp;
            m2 += NeighborhoodSamp * NeighborhoodSamp;

            float SampleSpatialWeight = SampleWeights[i];
            float SampleHdrWeight = HdrWeight4(NeighborhoodSamp, Exposure);

            // combine two weight
            float SampleFinalWeight = SampleSpatialWeight * SampleHdrWeight;
            
            NeighborsColor += SampleFinalWeight * NeighborhoodSamp;
            NeighborsFinalWeight += SampleFinalWeight;
        }
    }

    // compute filteredColor
    FilteredColor = NeighborsColor * rcp(max(NeighborsFinalWeight, 0.0001));

    // shappen 
    float3 highFreq = neighborhood[1] + neighborhood[3] + neighborhood[5] + neighborhood[7] - 4 * neighborhood[4];
    FilteredColor += highFreq * 0.1f;

   float LumaHistory = Luma4(prevColor);
    // simplest clip
    //prevColor = clamp(prevColor, neighborMin, neighborMax);

    // neighborhood clamping
    //prevColor = ClampHistory(neighborMin, neighborMax, prevColor, (neighborMin + neighborMax) / 2.0f);

    // variance clip
    float3 mu = m1 / N;
    float3 sigma = sqrt(abs(m2 / N - mu * mu));
    float VarianceClipGamma = lerp(MIN_VARIANCE_GAMMA, MAX_VARIANCE_GAMMA, velocityConfidenceFactor);
    neighborMin = mu - VarianceClipGamma * sigma;
    neighborMax = mu + VarianceClipGamma * sigma;
    prevColor = ClampHistory(neighborMin, neighborMax, prevColor, mu);

    // compute blend amount 
    float BlendFinal;
    {
        float LumaFiltered = Luma4(FilteredColor);
        BlendFinal = lerp(BlendWeightLowerBound, BlendWeightUpperBound, saturate(1 - velocityConfidenceFactor));
        //BlendFinal = max(BlendFinal, saturate(0.01 * LumaHistory / abs(LumaFiltered - LumaHistory)));
    }

    float FilterWeight = HdrWeight4(FilteredColor, Exposure);
    float HistoryWeight = HdrWeight4(prevColor, Exposure);

    float2 Weights = WeightedLerpFactors(HistoryWeight, FilterWeight, BlendFinal);
    float3 color = Weights.x * prevColor + Weights.y * FilteredColor;

    color = YCoCgToRGB(color);
    color = UnToneMap(color);

    OutTemporal[DTid.xy] = float4(color, 1);
}
