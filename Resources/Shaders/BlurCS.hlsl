Texture2D<float2> Input		: register(t0);
RWTexture2D<float2> Output	: register(u0);


static const float Weights[9] = { 1.f / 256.f, 8.f / 256.f, 28.f / 256.f, 56.f / 256.f, 
					70.f / 256.f, 56.f / 256.f, 28.f / 256.f, 8.f / 256.f, 1.f / 256.f };



groupshared float2 CacheRG[256];

void BlurHorizontally(uint Index, uint LeftMostIndex)
{
	float2 Result = 0;
	[unroll]
	for (uint i = 0; i < 9; ++i)
	{
		Result += Weights[i] * CacheRG[LeftMostIndex + i];
	}
	CacheRG[Index] = Result;
}

void BlurVertically(uint Index, uint2 TopMost)
{
	float2 Result = 0;
	uint CurrentIndex = TopMost.x + TopMost.y << 4;
	[unroll]
	for (uint i = 0; i < 9; ++i)
	{
		Result += Weights[i] * CacheRG[TopMost.x + TopMost.y << 4];
		TopMost.y += 1;
	}
	CacheRG[Index] = Result;
}

[numthreads(8, 8, 1)]
void cs_main(uint3 GroupID: SV_GroupID, uint3 GroupThreadID: SV_GroupThreadID, uint3 DispatchThreadID: SV_DispatchThreadID)
{
/*
 _______16x16_______
|--4--|             |
|	  ___8x8___		|
|	 |		   |	|
|	 |		   |	|
|	 |		   |	|
|	 |_________|	|
|					|
|___________________|
*/
	// 1. load 16x16 shared data, each thread load 4 pixels
	uint2 GroupTL = (GroupID.xy << 3) - 4;
	uint2 SharedID = GroupThreadID.xy << 1;
	uint2 ThreadTL = GroupTL + SharedID;
	uint Index = SharedID.x + SharedID.y << 4;
	CacheRG[Index +  0] = Input[ThreadTL + uint2(0, 0)];
	CacheRG[Index +  1] = Input[ThreadTL + uint2(1, 0)];
	CacheRG[Index + 16] = Input[ThreadTL + uint2(0, 1)];
	CacheRG[Index + 17] = Input[ThreadTL + uint2(0, 1)];

	// make sure shared data is filled for later access
	GroupMemoryBarrierWithGroupSync();

	// 2. blur horizontally, each thread blur one pixel
	BlurHorizontally(Index, Index - 4);

	// 3. blur vertically, each thread blur one pixel

}