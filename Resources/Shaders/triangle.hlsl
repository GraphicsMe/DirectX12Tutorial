#pragma pack_matrix(row_major)


struct ModelViewProjection
{
	float4x4 projectionMatrix;
	float4x4 modelMatrix;
	float4x4 viewMatrix;
};

ConstantBuffer<ModelViewProjection> MVP	: register(b0);
Texture2D DiffuseTexture				: register(t0);
SamplerState LinearSampler				: register(s0);


struct VertexIN
{
    float3 inPos : POSITION;
    float3 color : COLOR;
	float2 tex   : TEXCOORD;
};

struct VertexOutput
{
    float3 color		: COLOR;
	float2 tex			: TEXCOORD;
    float4 gl_Position	: SV_Position;
};

struct PixelOutput
{
    float4 outFragColor : SV_Target0;
};


VertexOutput vs_main(VertexIN IN)
{
	VertexOutput OUT;
	OUT.gl_Position = mul(float4(IN.inPos, 1.0f), mul(MVP.modelMatrix, mul(MVP.viewMatrix, MVP.projectionMatrix)));
	OUT.color = IN.color;
	OUT.tex = IN.tex;
	return OUT;
}


PixelOutput ps_main(VertexOutput IN)
{
	PixelOutput output;
	float4 texColor = DiffuseTexture.Sample( LinearSampler, IN.tex );
	//output.outFragColor = float4(IN.color, 1.0f);
	output.outFragColor = texColor;
	return output;
}