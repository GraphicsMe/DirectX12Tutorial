#include "ShaderUtils.hlsl"


struct VertexOutput
{
	float2 Tex	: TEXCOORD;
	float4 Pos	: SV_Position;
};

cbuffer PSContant : register(b0)
{
	int MipIndex;
	int NumMips;
	int CubeFace;
};

Texture2D SrcMipTexture	: register(t0);
SamplerState LinearSampler	: register(s0);

float4 PS_Main_2D(in VertexOutput Input) : SV_Target0
{
	return SrcMipTexture.SampleLevel(LinearSampler, Input.Tex, MipIndex-1);
}


float3 GetCubemapVector(float2 ScaledUVs, int InCubeFace)
{
	float3 CubeCoordinates;

	//@todo - this could be a 3x3 matrix multiply
	if (InCubeFace == 0)
	{
		CubeCoordinates = float3(1, -ScaledUVs.y, -ScaledUVs.x);
	}
	else if (InCubeFace == 1)
	{
		CubeCoordinates = float3(-1, -ScaledUVs.y, ScaledUVs.x);
	}
	else if (InCubeFace == 2)
	{
		CubeCoordinates = float3(ScaledUVs.x, 1, ScaledUVs.y);
	}
	else if (InCubeFace == 3)
	{
		CubeCoordinates = float3(ScaledUVs.x, -1, -ScaledUVs.y);
	}
	else if (InCubeFace == 4)
	{
		CubeCoordinates = float3(ScaledUVs.x, -ScaledUVs.y, 1);
	}
	else
	{
		CubeCoordinates = float3(-ScaledUVs.x, -ScaledUVs.y, -1);
	}

	return CubeCoordinates;
}

VertexOutput VS_Main_Cube(in uint VertID : SV_VertexID)
{
	float2 positions[6] = {
		float2(-1.0, 1.0),   //0
		float2(1.0, 1.0),    //1
		float2(-1.0, -1.0),  //3
		float2(1.0, 1.0),	 //1
		float2(1.0, -1.0),   //2
		float2(-1.0, -1.0),  //3
	};
	float2 texs[6] = {
		float2(0.0, 0.0),   //0
		float2(1.0, 0.0),   //1
		float2(0.0, 1.0),   //3
		float2(1.0, 0.0),   //1
		float2(1.0, 1.0),   //2
		float2(0.0, 1.0),   //3
	};
	VertexOutput Output;
	Output.Tex = texs[VertID];
	Output.Pos = float4(positions[VertID], 0.0, 1.0);
	return Output;
}

TextureCube CubeMap : register(t0);

// from UE4 ReflectionEnvironmentShaders.usf DownsamplePS
float4 PS_Main_Cube(in VertexOutput Input) : SV_Target0
{
	float2 ScaledUVs = Input.Tex * 2 - 1;
	float3 CubeCoordinates = GetCubemapVector(ScaledUVs, CubeFace);
	uint MipSize = 1 << (NumMips - MipIndex - 1);

	float3 TangentZ = normalize(CubeCoordinates);
	float3 TangentX = normalize(cross(GetCubemapVector(ScaledUVs + float2(0, 1), CubeFace), TangentZ));
	float3 TangentY = cross(TangentZ, TangentX);

	const float SampleOffset = 2.0 * 2 / MipSize;

	float2 Offsets[] =
	{
		float2(-1, -1) * 0.7,
		float2(1, -1) * 0.7,
		float2(-1,  1) * 0.7,
		float2(1,  1) * 0.7,

		float2(0, -1),
		float2(-1,  0),
		float2(1,  0),
		float2(0,  1),
	};

	float4 OutColor = CubeMap.SampleLevel(LinearSampler, CubeCoordinates, 0);

	for (uint i = 0; i < 8; i++)
	{
		float Weight = 0.375; // Weight * 8 = 3

		float3 SampleDir = CubeCoordinates;
		SampleDir += TangentX * (Offsets[i].x * SampleOffset);
		SampleDir += TangentY * (Offsets[i].y * SampleOffset);
		OutColor += CubeMap.SampleLevel(LinearSampler, SampleDir, 0) * Weight;
	}

	OutColor /= 4.0;

	return OutColor;
}