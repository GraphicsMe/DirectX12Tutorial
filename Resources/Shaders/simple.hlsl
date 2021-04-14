#pragma pack_matrix(row_major)


struct MatrixParam
{
	float4x4 ModelMatrix;
	float4x4 ViewProjection;
	float4x4 LightMatrix;
};

ConstantBuffer<MatrixParam> Matrix		: register(b0);
Texture2D DiffuseTexture				: register(t0);
Texture2D ShadowMap						: register(t1);
SamplerState LinearSampler				: register(s0);
SamplerComparisonState ShadowSampler	: register(s1);
//SamplerState ShadowSampler	: register(s1);

struct VertexIN
{
    float3 inPos : POSITION;
	float2 tex   : TEXCOORD;
    float3 normal : NORMAL;
};

struct VertexOutput
{
	float2 tex			: TEXCOORD;
    float4 gl_Position	: SV_Position;
    float3 normal		: NORMAL;
	float4 ShadowCoord	: TEXCOORD1;
};

struct PixelOutput
{
    float4 outFragColor : SV_Target0;
};


VertexOutput vs_main(VertexIN IN)
{
	VertexOutput OUT;
	float4 WorldPos = mul(float4(IN.inPos, 1.0f), Matrix.ModelMatrix);
	OUT.gl_Position = mul(WorldPos, Matrix.ViewProjection);
	//OUT.color = IN.color;
	OUT.tex = IN.tex;
	OUT.normal = IN.normal;
	OUT.ShadowCoord = mul(WorldPos, Matrix.LightMatrix);
	return OUT;
}

float ComputeShadow(float4 ShadowCoord)
{
	float3 NormalizedShadowCoord = ShadowCoord.xyz / ShadowCoord.w;
	
	NormalizedShadowCoord.x = 0.5 + 0.5 * NormalizedShadowCoord.x;
	NormalizedShadowCoord.y = 0.5 - 0.5 * NormalizedShadowCoord.y;
	
	return ShadowMap.SampleCmpLevelZero(ShadowSampler, NormalizedShadowCoord.xy, NormalizedShadowCoord.z);
}

PixelOutput ps_main(VertexOutput IN)
{
	PixelOutput output;
	//output.outFragColor = float4(IN.color, 1.0f);
	float4 texColor = DiffuseTexture.Sample( LinearSampler, IN.tex );
	float Shadow = ComputeShadow(IN.ShadowCoord);
	//output.outFragColor = float4(Shadow, Shadow, Shadow, 1.0);	
	output.outFragColor = texColor * saturate(0.2 + Shadow);
	return output;
}