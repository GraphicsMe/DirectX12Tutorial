#include "ShaderUtils.hlsl"

RWTexture2D<float3> CombineColor		: register(u0);
Texture2D<float3>	TempSceneColor		: register(t0);
Texture2D<float>	CoCBuffer		    : register(t1);
Texture2D<float4>	FilterColor		    : register(t2);

SamplerState LinearSampler : register(s0);

cbuffer CB0 : register(b0)
{
    float  BokehRadius;
};

//------------------------------------------------------- HELP FUNCTIONS
void GetSampleUV(uint2 ScreenCoord, inout float2 UV, inout float2 PixelSize)
{
    float2 ScreenSize;
    CombineColor.GetDimensions(ScreenSize.x, ScreenSize.y);
    float2 InvScreenSize = rcp(ScreenSize);
    PixelSize = InvScreenSize;
    UV = ScreenCoord * InvScreenSize + 0.5 * PixelSize;
}

//------------------------------------------------------- ENTRY POINT

[numthreads(8, 8, 1)]
void cs_FragCombine(uint3 DTid : SV_DispatchThreadID)
{
    // screenPos
    float2 PixelSize, UV;
    GetSampleUV(DTid.xy, UV, PixelSize);

    float3 source = TempSceneColor.SampleLevel(LinearSampler, UV, 0);
    // 采样当前片段的CoC值
    float coc = CoCBuffer.SampleLevel(LinearSampler, UV, 0);
    coc = (coc - 0.5) * 2.0 * BokehRadius;

    // 采样散景
    float4 dof = FilterColor.SampleLevel(LinearSampler, UV, 0);

    // Convert CoC to far field alpha value.
    // 对CoC正值进行插值，用于获取后散景
    float ffa = smoothstep(PixelSize.y * 2.0, PixelSize.y * 4.0, coc);

    // 非线性插值
    float3 color = lerp(source, dof.rgb, ffa + dof.a - ffa * dof.a);

    CombineColor[DTid.xy] = color;
}