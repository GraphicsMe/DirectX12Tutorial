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


//-------------------------------------------------------
// Convert Longtitude-Latitude Mapping to Cube Mapping
//-------------------------------------------------------
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


//-------------------------------------------------------
// SkyBox
//-------------------------------------------------------
TextureCube CubeEnvironment : register(t0);

float4 PS_SkyCube(VertexOutput In) : SV_Target
{
	// Local Direction don't need to normalized
	return CubeEnvironment.Sample(LinearSampler, In.LocalDirection);
}

struct VertexIN_CubeMapCross
{
	float3 Position : POSITION;
	float3 Normal : Normal;
};


//-------------------------------------------------------
// CubeMap Cross View
//-------------------------------------------------------
cbuffer PSContant : register(b0)
{
	float Exposure;
};

VertexOutput VS_CubeMapCross(VertexIN_CubeMapCross In)
{
	VertexOutput Out;
	Out.LocalDirection = In.Normal;
	Out.Position = mul(mul(float4(In.Position, 1.0), ModelMatrix), ViewProjMatrix);
	return Out;
}

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ACESFilm( float3 color )
{
	float3 x = 0.8 * color;
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}

float4 PS_CubeMapCross(VertexOutput In) : SV_Target
{
	float3 Color = CubeEnvironment.Sample(LinearSampler, In.LocalDirection).xyz;

	Color = ACESFilm(Color * Exposure);

	return float4(pow(Color, 1/2.2), 1.0);
}