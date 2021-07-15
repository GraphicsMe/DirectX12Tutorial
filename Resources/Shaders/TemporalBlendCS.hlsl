#include "PixelPacking_Velocity.hlsli"

RWTexture2D<float4> OutTemporal             : register(u0);
Texture2D<float3> InColor                   : register(t0);
Texture2D<float4> InTemporal                : register(t1);
//Texture2D<packed_velocity_t> VelocityBuffer : register(t2);
Texture2D<float2> VelocityBuffer            : register(t2);

Texture2D<float> DepthBuffer                : register(t3);

SamplerState LinearSampler                  : register(s0);

cbuffer CB0 : register(b0)
{
    float2 RcpBufferDim;                    // 1 / width, 1 / height
    float TemporalBlendFactor;
    float RcpSpeedLimiter;
    float2 ViewportJitter;
}

//------------------------------------------------------- MACRO DEFINITION
#define SPATIAL_WEIGHT_CATMULLROM 1

//------------------------------------------------------- PARAMETERS
static const float VarianceClipGamma = 1.0f;
static const float Exposure = 10;
static const float BlendWeightLowerBound = 0.04f;
static const float BlendWeightUpperBound = 0.2f;
static const float BlendWeightVelocityScale = 100.0f * 80.0f;

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

float3 SampleHistory(float2 historyUV)
{
    float3 history;
    history = InTemporal[historyUV].rgb;

    // TODO: Sample the history using Catmull-Rom to reduce blur on motion.
    // https://www.shadertoy.com/view/4tyGDD

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

float2 WeightedLerpFactors(float WeightA, float WeightB, float Blend)
{
    float BlendA = (1.0 - Blend) * WeightA;
    float BlendB = Blend * WeightB;
    float RcpBlend = rcp(BlendA + BlendB);
    BlendA *= RcpBlend;
    BlendB *= RcpBlend;
    return float2(BlendA, BlendB);
}

//------------------------------------------------------- ENTRY POINT
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

    float2 uv = DTid.xy;

    // sample velocity
    float2 closestOffset = float2(0.0f, 0.0f);
    float closestDepth = 1.0f;
    // 3x3 closest depth
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

    //float2 velocity = UnpackVelocity(VelocityBuffer[uv + closestOffset]).xy;
    float2 velocity = VelocityBuffer[uv + closestOffset].xy;
    // convert to 0-1
    float lenVelocity = length(velocity * RcpBufferDim); 

    float2 historyUV = uv + velocity;

    // current frame color
    float3 currColor = InColor[uv];
    currColor = ToneMap(currColor);
    currColor = RGBToYCoCg(currColor);

    // sample history color
    float3 prevColor = SampleHistory(historyUV);
    
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
    float2 InputSampleUV = uv;
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
            float2 sampleUV = InputSampleUV + sampleOffset;

            // sample
            float3 NeighborhoodSamp = InColor[sampleUV];
            NeighborhoodSamp = max(NeighborhoodSamp, 0.0f);

            NeighborhoodSamp = ToneMap(NeighborhoodSamp);
            NeighborhoodSamp = RGBToYCoCg(NeighborhoodSamp);

            // cache
            neighborhood[i] = NeighborhoodSamp;

            // AAABB
            neighborMin = min(neighborMin, NeighborhoodSamp);
            neighborMax = max(neighborMax, NeighborhoodSamp);

            m1 += NeighborhoodSamp;
            m2 += NeighborhoodSamp * NeighborhoodSamp;

            // 
            float SampleSpatialWeight = SampleWeights[i];
            float SampleHdrWeight = HdrWeight4(NeighborhoodSamp, Exposure);
            // combine two weight
            float SampleFinalWeight = SampleSpatialWeight * SampleHdrWeight;
            
            NeighborsColor += SampleFinalWeight * NeighborhoodSamp;
            NeighborsFinalWeight += SampleFinalWeight;
        }
    }

    // compute filteredColor
    FilteredColor = NeighborsColor * rcp(NeighborsFinalWeight);

    // shappen 
    float3 highFreq = neighborhood[1] + neighborhood[3] + neighborhood[5] + neighborhood[7] - 4 * neighborhood[4];
    FilteredColor += highFreq * 0.1f;

    // variance AABB
    float3 mu = m1 / N;
    float3 sigma = sqrt(abs(m2 / N - mu * mu));
    neighborMin = mu - VarianceClipGamma * sigma;
    neighborMax = mu + VarianceClipGamma * sigma;

    float LumaHistory = Luma4(prevColor);

    // simplest clip
    //prevColor = clamp(prevColor, neighborMin, neighborMax);
    
    // neighborhood clamping
    //prevColor = ClampHistory(neighborMin, neighborMax, prevColor, (neighborMin + neighborMax) / 2.0f);

    // variance clip
    prevColor = ClampHistory(neighborMin, neighborMax, prevColor, mu);

    // compute blend amount
    float BlendFinal;
    {
        float LumaFiltered = Luma4(FilteredColor);
        
        BlendFinal = lerp(BlendWeightLowerBound, BlendWeightUpperBound, saturate(lenVelocity * BlendWeightVelocityScale));
        //BlendFinal = max(BlendFinal, saturate(0.001 * LumaHistory / abs(LumaFiltered - LumaHistory)));
    }

    float FilterWeight = HdrWeight4(FilteredColor, Exposure);
    float HistoryWeight = HdrWeight4(prevColor, Exposure);

    float2 Weights = WeightedLerpFactors(HistoryWeight, FilterWeight, BlendFinal);
    float3 color = Weights.x * prevColor + Weights.y * FilteredColor;

    color = YCoCgToRGB(color);
    color = UnToneMap(color);
    OutTemporal[uv] = float4(color, 1);
}
