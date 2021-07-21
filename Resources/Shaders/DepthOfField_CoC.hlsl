#include "ShaderUtils.hlsl"

RWTexture2D<float>  CoCBuffer               : register(u0);
RWTexture2D<float3> TempSceneColor          : register(u1);
Texture2D<float3>   SceneColor              : register(t0);
Texture2D<float>    DepthBuffer             : register(t1);


SamplerState LinearSampler                  : register(s0);


cbuffer CB0 : register(b0)
{
    float   FoucusDistance;
    float   FoucusRange;
    float2  pad;
    float2  ClipSpaceNearFar;
    float   BokehRadius;
    float   RcpBokehRadius;
};

//------------------------------------------------------- MACRO DEFINITION


//------------------------------------------------------- HELP FUNCTIONS
// @return 0..1 0=in focus, 1:max blurry
float ComputeCircleOfConfusion(const float FocalLength, const float FoucusDistance, const float Aperture, const float SceneDepth)
{
    // depth of the pixel in m unit
    float D = SceneDepth;
    // Focal length in mm (Camera property e.g. 75mm)
    float F = FocalLength;
    // Plane in Focus in m unit
    float P = FoucusDistance;

    // Camera property e.g. 0.5f, like aperture
    float A = Aperture;

    P *= 1000.0f;
    D *= 1000.0f;

    float MaxBgdCoC = A * F * rcp(P - F);
    float CoCRadius = (1 - P / max(D, 1e-4)) * MaxBgdCoC;
    return saturate(abs(CoCRadius));
}

//------------------------------------------------------- ENTRY POINT

[numthreads(8, 8, 1)]
void cs_FragCoC
(
    uint3 DTid  : SV_DispatchThreadID
)
{
    // screenPos
    const uint2 ScreenST = DTid.xy;
    // non-linear depth
    float Depth = DepthBuffer[ScreenST];
    // convert to LinearEyeDepth
    float SceneDepth = LinearEyeDepth(Depth, ClipSpaceNearFar.x, ClipSpaceNearFar.y);
    
    //float CoCRadius = ComputeCircleOfConfusion(FocalLength, FoucusDistance, Aperture, SceneDepth);

    // compute simple coc 
    float coc = (SceneDepth - FoucusDistance) / FoucusRange;

    // clamp to (-1,1) and multiple BokehRadius
    coc = clamp(coc, -1, 1) * BokehRadius;
    CoCBuffer[ScreenST] = coc;

    // copy to temp scene color
    TempSceneColor[ScreenST] = SceneColor[ScreenST];
}