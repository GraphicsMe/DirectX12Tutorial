#pragma pack_matrix(row_major)

#include "ShaderUtils.hlsl"


Texture2D SceneDepthZ		: register(t0); // Depth
SamplerState PointSampler	: register(s1); // PointSamper

RWTexture2D<float> ClosestHZB : register(u0);

cbuffer PSContant : register(b0)
{
	float2 SrcTexelSize;
	float2 InputViewportMaxBound;
};

float4 Gather4(float2 BufferUV)
{
	float2 OffsetUV = BufferUV + float2(-0.25f, -0.25f) * SrcTexelSize;
	float2 Range = InputViewportMaxBound - SrcTexelSize;
	float2 UV = min(OffsetUV, Range);
	return SceneDepthZ.GatherRed(PointSampler, UV, 0);
}


[numthreads(8, 8, 1)]
void CS_BuildHZB(uint2 GroupId : SV_GroupID,
				 uint GroupThreadIndex : SV_GroupIndex,
				 uint2 DispatchThreadId : SV_DispatchThreadID)
{
	float2 BufferUV = (DispatchThreadId + 0.5) * SrcTexelSize * 2.0;
	float4 DeviceZ = Gather4(BufferUV);

	float ClosestDeviceZ = min(min(DeviceZ.x, DeviceZ.y), min(DeviceZ.z, DeviceZ.w));
	ClosestHZB[DispatchThreadId] = ClosestDeviceZ;
}