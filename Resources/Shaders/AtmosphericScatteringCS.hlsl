Texture2D<float> Input		: register(t0);
RWTexture2D<float2> Output	: register(u0);

[numthreads(16, 16, 1)]
void cs_main(uint3 DispatchThreadID: SV_DispatchThreadID)
{
	Output[DispatchThreadID.xy] = float4(1.f, 0.f, 0.f, 1.f);
}