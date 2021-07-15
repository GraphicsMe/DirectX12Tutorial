#pragma pack_matrix(row_major)

#include "ShaderUtils.hlsl"

#include "PixelPacking_Velocity.hlsli"

Texture2D<float> DepthBuffer : register(t0);
RWTexture2D<packed_velocity_t> VelocityBuffer : register(u0);
//RWTexture2D<float2> VelocityBuffer : register(u0);

cbuffer CBuffer : register(b0)
{
    matrix CurToPrevXForm;
}

[numthreads( 8, 8, 1 )]
void cs_main( uint3 DTid : SV_DispatchThreadID )
{
	uint2 st = DTid.xy;
    // ScreenPos
    float2 CurPixel = st + 0.5;
	// Depth
	float Depth = DepthBuffer[st];

	// NDC
	float4 HPos = float4(CurPixel,Depth,1.0f);

	float4 PrevHPos = mul(HPos, CurToPrevXForm);

	// PerspectiveDivde
	PrevHPos.xyz /= PrevHPos.w;

    // PrevHPos.xy 前一帧的屏幕空间像素坐标
    // PrevHPos.z 前一帧的深度
    VelocityBuffer[st] = PackVelocity(PrevHPos.xyz - float3(CurPixel, Depth));
	//VelocityBuffer[st] = float2(PrevHPos.xy - CurPixel);
}

