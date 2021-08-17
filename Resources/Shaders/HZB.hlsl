#pragma pack_matrix(row_major)

#include "ShaderUtils.hlsl"


Texture2D SceneDepthZ		: register(t0); // Depth
SamplerState PointSampler	: register(s1); // PointSamper

RWTexture2D<float2> ClosestHZB : register(u0);

cbuffer PSContant : register(b0)
{
	float2 SrcTexelSize;
	float2 InputViewportMaxBound;
	float Thickness;
};

void Gather4(float2 BufferUV, out float4 MinZ, out float4 MaxZ)
{
	float2 OffsetUV = BufferUV + float2(-0.25f, -0.25f) * SrcTexelSize;
	float2 Range = InputViewportMaxBound - SrcTexelSize;
	float2 UV = min(OffsetUV, Range);
	MinZ = SceneDepthZ.GatherRed(PointSampler, UV, 0);
	MaxZ = SceneDepthZ.GatherGreen(PointSampler, UV, 0);
}


[numthreads(8, 8, 1)]
void CS_BuildHZB(uint2 GroupId : SV_GroupID,
				 uint GroupThreadIndex : SV_GroupIndex,
				 uint2 DispatchThreadId : SV_DispatchThreadID)
{
	float2 BufferUV = (DispatchThreadId + 0.5) * SrcTexelSize * 2.0;
	float4 MinDeviceZ4, MaxDeviceZ4;
	Gather4(BufferUV, MinDeviceZ4, MaxDeviceZ4);

	if (InputViewportMaxBound.x < 1.0)
	{
		MaxDeviceZ4 = MinDeviceZ4;
	}
	float MinDeviceZ = min(min(MinDeviceZ4.x, MinDeviceZ4.y), min(MinDeviceZ4.z, MinDeviceZ4.w));
	float MaxDeviceZ = max(max(MaxDeviceZ4.x, MaxDeviceZ4.y), max(MaxDeviceZ4.z, MaxDeviceZ4.w));
	
	if (MaxDeviceZ - MinDeviceZ > Thickness)
	{
		MaxDeviceZ = min(min(MaxDeviceZ4.x, MaxDeviceZ4.y), min(MaxDeviceZ4.z, MaxDeviceZ4.w));
		
		if (MaxDeviceZ4.x - MinDeviceZ < Thickness && MaxDeviceZ < MaxDeviceZ4.x)
			MaxDeviceZ = MaxDeviceZ4.x;
		if (MaxDeviceZ4.y - MinDeviceZ < Thickness && MaxDeviceZ < MaxDeviceZ4.y)
			MaxDeviceZ = MaxDeviceZ4.y;
		if (MaxDeviceZ4.z - MinDeviceZ < Thickness && MaxDeviceZ < MaxDeviceZ4.z)
			MaxDeviceZ = MaxDeviceZ4.z;
		if (MaxDeviceZ4.w - MinDeviceZ < Thickness && MaxDeviceZ < MaxDeviceZ4.w)
			MaxDeviceZ = MaxDeviceZ4.w;
	}
	ClosestHZB[DispatchThreadId] = float2(MinDeviceZ, MaxDeviceZ);
}