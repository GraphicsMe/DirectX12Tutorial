#pragma pack_matrix(row_major)

struct PixelInput
{
    float3 inColor : COLOR;
};

struct PixelOutput
{
    float4 outFragColor : SV_Target0;
};


PixelOutput main(PixelInput stage_input)
{
    PixelOutput stage_output;
    stage_output.outFragColor = float4(stage_input.inColor, 1.0f);
    return stage_output;
}