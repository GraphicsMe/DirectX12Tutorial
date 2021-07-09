

RWTexture2D<float4> OutTemporal : register(u0);
// SceneColor
Texture2D<float3> InColor : register(t0);
// TemporalBuffer
Texture2D<float4> InTemporal : register(t1);

SamplerState LinearSampler : register(s0);

cbuffer CB0 : register(b0)
{
    float2 RcpBufferDim;            // 1 / width, 1 / height
    float TemporalBlendFactor;
    float RcpSpeedLimiter;
    float2 ViewportJitter;
}

[numthreads(8, 8, 1)]
void cs_main(uint3 DTid : SV_DispatchThreadID, uint GI : SV_GroupIndex, uint3 GTid : SV_GroupThreadID, uint3 Gid : SV_GroupID)
{
    float alpha = TemporalBlendFactor;

    float3 Input = InColor[DTid.xy];
    float4 temp = InTemporal[DTid.xy];
    temp.a = 1;

    OutTemporal[DTid.xy] = float4(alpha * Input, 1) + (1 - alpha) * temp;
}
