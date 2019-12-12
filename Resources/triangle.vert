#pragma pack_matrix(row_major)

cbuffer ubo : register(b0)
{
	float4x4 ubo_projectionMatrix : packoffset(c0);
	float4x4 ubo_modelMatrix : packoffset(c4);
	float4x4 ubo_viewMatrix : packoffset(c8);
};

struct VertexInput
{
    float3 inPos : POSITION;
    float3 inColor : COLOR;
};

struct VertexOutput
{
    float3 outColor : COLOR;
    float4 gl_Position : SV_Position;
};

VertexOutput main(VertexInput stage_input)
{
	VertexOutput stage_output;
	stage_output.gl_Position = mul(float4(stage_input.inPos, 1.0f), mul(ubo_modelMatrix, mul(ubo_viewMatrix, ubo_projectionMatrix)));
	stage_output.outColor = stage_input.inColor;
	return stage_output;
}