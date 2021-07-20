

RWTexture2D<float4> CombineColor		: register(u0);
Texture2D<float4>	TempSceneColor		: register(t0);
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

    // simple blend
    float3 source = TempSceneColor.SampleLevel(LinearSampler, UV, 0);
    float coc = CoCBuffer.SampleLevel(LinearSampler, UV, 0);
    float4 dof = FilterColor.SampleLevel(LinearSampler, UV, 0);

    float dofStrength = smoothstep(0.1, 1, abs(coc));
    float3 color = lerp(source.rgb, dof.rgb, dofStrength);
    //float3 color = lerp(source.rgb, dof.rgb, dofStrength + dof.a - dofStrength * dof.a);

    CombineColor[DTid.xy] = float4(color, 1);
}