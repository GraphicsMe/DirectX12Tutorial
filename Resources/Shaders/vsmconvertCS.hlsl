Texture2D<float> Input;//		: register(t0);
RWTexture2D<float2> Output;//	: register(u0);

[numthreads(16, 16, 1)]
void cs_main(int3 dispatchThreadID: SV_DispatchThreadID)
{
	float depth = Input[dispatchThreadID.xy];
	//Output[dispatchThreadID.xy] = depth;
	Output[dispatchThreadID.xy] = float2(depth, depth * depth);
}