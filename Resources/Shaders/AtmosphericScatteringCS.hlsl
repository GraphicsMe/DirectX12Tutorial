#pragma pack_matrix(row_major)

Texture2D<float> Input		: register(t0);
RWTexture2D<float4> Output	: register(u0);

cbuffer CSConstant : register(b0)
{
	float4 ScreenParams; //width, height, invWidth, invHeight
	float4x4 InvProjectionMatrix;
	float4x4 InvViewMatrix;
}

[numthreads(8, 8, 1)]
void cs_main(uint3 DispatchThreadID: SV_DispatchThreadID)
{
	float4 NDCPos = float4(2 * DispatchThreadID.x / ScreenParams.x - 1.0, 1.0 - 2 * DispatchThreadID.y / ScreenParams.y, 1.0, 1.0);
	float4 ViewPos = mul(NDCPos, InvProjectionMatrix);
	ViewPos /= ViewPos.w;
	float4 WorldPos = mul(ViewPos, InvViewMatrix);
	Output[DispatchThreadID.xy] = WorldPos;
}