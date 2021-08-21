#include "ShaderUtils.hlsl"

Texture2D			SceneColor		: register(t0); // color
Texture2D<float>	SceneDepthZ		: register(t1); // depth

SamplerState LinearSampler	: register(s0);
SamplerState PointSampler	: register(s1);

cbuffer PSContant : register(b0)
{
	float2	dir;
	float2	NearFar;
	float	sssWidth;
};

struct PixelOutput
{
	float4	Target0 : SV_Target0;
};

const static float4 kernel[] =
{
	float4(0.530605, 0.613514, 0.739601, 0),
	float4(0.000973794, 1.11862e-005, 9.43437e-007, -3),
	float4(0.00333804, 7.85443e-005, 1.2945e-005, -2.52083),
	float4(0.00500364, 0.00020094, 5.28848e-005, -2.08333),
	float4(0.00700976, 0.00049366, 0.000151938, -1.6875),
	float4(0.0094389, 0.00139119, 0.000416598, -1.33333),
	float4(0.0128496, 0.00356329, 0.00132016, -1.02083),
	float4(0.017924, 0.00711691, 0.00347194, -0.75),
	float4(0.0263642, 0.0119715, 0.00684598, -0.520833),
	float4(0.0410172, 0.0199899, 0.0118481, -0.333333),
	float4(0.0493588, 0.0367726, 0.0219485, -0.1875),
	float4(0.0402784, 0.0657244, 0.04631, -0.0833333),
	float4(0.0211412, 0.0459286, 0.0378196, -0.0208333),
	float4(0.0211412, 0.0459286, 0.0378196, 0.0208333),
	float4(0.0402784, 0.0657244, 0.04631, 0.0833333),
	float4(0.0493588, 0.0367726, 0.0219485, 0.1875),
	float4(0.0410172, 0.0199899, 0.0118481, 0.333333),
	float4(0.0263642, 0.0119715, 0.00684598, 0.520833),
	float4(0.017924, 0.00711691, 0.00347194, 0.75),
	float4(0.0128496, 0.00356329, 0.00132016, 1.02083),
	float4(0.0094389, 0.00139119, 0.000416598, 1.33333),
	float4(0.00700976, 0.00049366, 0.000151938, 1.6875),
	float4(0.00500364, 0.00020094, 5.28848e-005, 2.08333),
	float4(0.00333804, 7.85443e-005, 1.2945e-005, 2.52083),
	float4(0.000973794, 1.11862e-005, 9.43437e-007, 3),
};

PixelOutput PS_SSSBlur(float2 Tex : TEXCOORD, float4 ScreenPos : SV_Position)
{
	PixelOutput Out;
	Out.Target0 = float4(0, 0, 0, 0);

	float   near = NearFar.x;
	float   far = NearFar.y;

	float2 ScreenSize;
	SceneColor.GetDimensions(ScreenSize.x, ScreenSize.y);

	float2 texelSize = float2(1.f / ScreenSize.x, 1.f / ScreenSize.y);

	float3 colorM = SceneColor.Sample(PointSampler, Tex).rgb;

	float depthM = Linear01Depth(SceneDepthZ.Sample(PointSampler, Tex).r, near, far);

	float rayRadiusUV = 0.5 * sssWidth / depthM;

	// calculate the final step to fetch the surrounding pixels:
	float2 finalStep = rayRadiusUV * dir;
	finalStep *= 1.0 / 3.0; // divide by 3 as the kernels range from -3 to 3

	// accumulate the center sample:
	float3 colorBlurred = colorM;
	colorBlurred.rgb *= kernel[0].rgb;

	// accumulate the other samples:
	for (int i = 1; i < 25; ++i)
	{
		// fetch color and depth for current sample:
		float2 offset = Tex + kernel[i].a * finalStep;
		float3 color = SceneColor.Sample(LinearSampler, offset).rgb;
		float depth = Linear01Depth(SceneDepthZ.Sample(LinearSampler, offset).r, near, far);

		// lerp back to center sample if depth difference too big
		float maxDepthDiff = 0.01;
		float alpha = min(distance(depth, depthM) / maxDepthDiff, maxDepthDiff);

		// reject sample if it isnt tagged as SSS
		//alpha *= 1.0 - color.a;

		color.rgb = lerp(color.rgb, colorM.rgb, alpha);

		// accumulate:
		colorBlurred.rgb += kernel[i].rgb * color.rgb;
	}

	Out.Target0 = float4(colorBlurred, 1);
	return Out;
}


