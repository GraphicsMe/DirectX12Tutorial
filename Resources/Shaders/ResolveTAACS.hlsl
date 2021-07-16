


Texture2D<float4> TemporalColor : register(t0);
RWTexture2D<float3> OutColor : register(u0);

[numthreads( 8, 8, 1 )]
void cs_main( uint3 DTid : SV_DispatchThreadID )
{
    float4 Color = TemporalColor[DTid.xy];
    OutColor[DTid.xy] = float4(Color.xyz, 1);
}
