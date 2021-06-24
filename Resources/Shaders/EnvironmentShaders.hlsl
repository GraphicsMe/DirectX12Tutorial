#pragma pack_matrix(row_major)


cbuffer VSContant : register(b0)
{
	float4x4 ModelMatrix;
	float4x4 ViewProjMatrix;
};


Texture2D LongLatEnvironment 	: register(t0);
SamplerState LinearSampler		: register(s0);


struct VertexIN
{
	float3 Position : POSITION;
};

struct VertexOutput
{
	float4 Position	 	  : SV_Position;
	float3 LocalDirection : TEXCOORD;
};

VertexOutput VS_LongLatToCube(VertexIN In)
{
	VertexOutput Out;
	Out.LocalDirection = In.Position;
	Out.Position = mul(mul(float4(In.Position, 1.0), ModelMatrix), ViewProjMatrix);
	return Out;
}


static const float PI = 3.14159265359;
static const float2 invAtan = { 0.5 / PI, -1 / PI };
float2 SampleSphericalMap(float3 Direction)
{
	float3 v = normalize(Direction);
	float2 uv = {atan2(v.z, v.x), asin(v.y)};
	uv = saturate(uv * invAtan + 0.5);
	return uv;
}


float4 PS_LongLatToCube(VertexOutput In) : SV_Target
{
	return LongLatEnvironment.Sample(LinearSampler, SampleSphericalMap(In.LocalDirection.xyz));
}


VertexOutput VS_SkyCube(VertexIN In)
{
	VertexOutput Out;
	Out.LocalDirection = In.Position;
	Out.Position = mul(mul(float4(In.Position, 1.0), ModelMatrix), ViewProjMatrix).xyww;
	return Out;
}

float GetGray(float3 color)
{
	return dot(color, float3(0.2126, 0.7152, 0.0722));
}


TextureCube CubeEnvironment : register(t0);

float4 PS_SkyCube(VertexOutput In) : SV_Target
{
	float3 v = normalize(In.LocalDirection);
	return CubeEnvironment.Sample(LinearSampler, v);
}