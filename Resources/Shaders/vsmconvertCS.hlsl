Texture2D<float> Input		: register(t0);
RWTexture2D<float2> Output	: register(u0);

[numthreads(16, 16, 1)]
void cs_main(uint3 DispatchThreadID: SV_DispatchThreadID)
{
	float depth = Input[DispatchThreadID.xy];
	Output[DispatchThreadID.xy] = float2(depth, depth * depth);
}