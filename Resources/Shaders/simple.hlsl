#pragma pack_matrix(row_major)


struct MatrixParam
{
	float4x4 ModelMatrix;
	float4x4 ViewProjection;
	float4x4 ShadowMatrix;
};

#define FILTER_SIZE 8
#define GS2 ((FILTER_SIZE-1)/2)
#define PCF_SAMPLE_COUNT ((FILTER_SIZE-1)*(FILTER_SIZE-1))
#define USE_RECEIVER_PLANE_DEPTH_BIAS 1

ConstantBuffer<MatrixParam> Matrix		: register(b0);
Texture2D DiffuseTexture				: register(t0);
Texture2D ShadowMap						: register(t1);
SamplerState LinearSampler				: register(s0);
SamplerComparisonState ShadowSampler	: register(s1);
//SamplerState ShadowSampler			: register(s1);

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
	OUT.ShadowCoord = mul(WorldPos, Matrix.ShadowMatrix);
	return OUT;
}

float2 ComputeReceiverPlaneDepthBias(float3 texCoordDX, float3 texCoordDY)
{
	float2 biasUV;
	biasUV.x = texCoordDY.y * texCoordDX.z - texCoordDX.y * texCoordDY.z;
	biasUV.y = texCoordDX.x * texCoordDY.z - texCoordDY.x * texCoordDX.z;
	biasUV *= 1.0f / ((texCoordDX.x * texCoordDY.y) - (texCoordDX.y * texCoordDY.x));
	return biasUV;
}

float DoSampleCmp(float2 ReceiverPlaneDepthBias, float2 TexelSize, float2 SubTexelCoord, float CurrentDepth, int2 TexelOffset)
{
#if USE_RECEIVER_PLANE_DEPTH_BIAS
	CurrentDepth += dot(TexelOffset * TexelSize, ReceiverPlaneDepthBias);
#endif
	return ShadowMap.SampleCmpLevelZero(ShadowSampler, SubTexelCoord, CurrentDepth, TexelOffset);
}

float SampleFixedSizePCF(float3 ShadowPos)
{
	float NumSlices;
	float2 ShadowMapSize;
	ShadowMap.GetDimensions(0, ShadowMapSize.x, ShadowMapSize.y, NumSlices);
	float2 TexelSize = 1.0 / ShadowMapSize;

	float2 tc = ShadowPos.xy;
	float2 stc = ShadowMapSize * tc + float2(0.5, 0.5);
	float2 tcs = floor(stc);
	
	float2 fc = stc - tcs;
	tc = tc - fc * TexelSize; // x in "Fast Conventional Shadow Filtering"

	float2 pwAB = 2.0 - fc;
	float2 tcAB = TexelSize / pwAB;
	float2 pwM = 2.0;
	float2 tcM = 0.5 * TexelSize;
	float2 pwGH = 1.0 + fc;
	float2 tcGH = TexelSize * fc / pwGH;

	float CurrentDepth = ShadowPos.z;
	float2 ReceiverPlaneDepthBias = float2(0.0, 0.0);

#if USE_RECEIVER_PLANE_DEPTH_BIAS
	float3 ShadowPosDX = ddx_fine(ShadowPos);
	float3 ShadowPosDY = ddy_fine(ShadowPos);
	ReceiverPlaneDepthBias = ComputeReceiverPlaneDepthBias(ShadowPosDX, ShadowPosDY);
	float FractionalSamplingError = dot(TexelSize, abs(ReceiverPlaneDepthBias));
	CurrentDepth -= min(FractionalSamplingError, 0.006f);
#endif

	float s = 0.0;
	s += pwAB.x * pwAB.y * DoSampleCmp(ReceiverPlaneDepthBias, TexelSize, tc + tcAB,					CurrentDepth, int2(-GS2, -GS2));	//top left
	s += pwGH.x * pwAB.y * DoSampleCmp(ReceiverPlaneDepthBias, TexelSize, tc + float2(tcGH.x, tcAB.y),	CurrentDepth, int2(GS2, -GS2));	//top right
	s += pwAB.x * pwGH.y * DoSampleCmp(ReceiverPlaneDepthBias, TexelSize, tc + float2(tcAB.x, tcGH.y),	CurrentDepth, int2(-GS2, GS2));	//bottom left
	s += pwGH.x * pwGH.y * DoSampleCmp(ReceiverPlaneDepthBias, TexelSize, tc + tcGH,					CurrentDepth, int2(GS2, GS2));	//bottom right
	
	for (int col = 2-GS2; col <= GS2-2; col += 2)
	{
		s += pwM * pwAB.y * DoSampleCmp(ReceiverPlaneDepthBias, TexelSize, tc + float2(tcM.x, tcAB.y),	CurrentDepth, int2(col, -GS2));	//top row
		s += pwM * pwGH.y * DoSampleCmp(ReceiverPlaneDepthBias, TexelSize, tc + float2(tcM.x, tcGH.y),	CurrentDepth, int2(col, GS2));	//bottom row
		s += pwAB.x * pwM * DoSampleCmp(ReceiverPlaneDepthBias, TexelSize, tc + float2(tcAB.x, tcM.y),	CurrentDepth, int2(-GS2, col));	//left col
		s += pwGH.x * pwM * DoSampleCmp(ReceiverPlaneDepthBias, TexelSize, tc + float2(tcGH.x, tcM.y),	CurrentDepth, int2(GS2, col));	//right col
	}

	// middle
	for (int row = 2-GS2; row <= GS2-2; row += 2)
	{
		for (int col = 2-GS2; col <= GS2-2; col += 2)
		{
			s += pwM * pwM * DoSampleCmp(ReceiverPlaneDepthBias, TexelSize, tc + tcM, CurrentDepth, int2(col, row));
		}
	}

	return s / PCF_SAMPLE_COUNT;
}

float ComputeShadow(float4 ShadowCoord)
{
	//float3 ShadowPos = ShadowCoord.xyz / ShadowCoord.w;
	float3 ShadowPos = ShadowCoord.xyz;
#if FILTER_SIZE == 2
	return ShadowMap.SampleCmpLevelZero(ShadowSampler, ShadowPos.xy, ShadowPos.z);
#else
	return SampleFixedSizePCF(ShadowPos);
#endif
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