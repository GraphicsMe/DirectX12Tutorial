
Texture2D			SceneColor		: register(t0); // color
Texture2D<float>	SceneDepthZ		: register(t1); // depth

SamplerState LinearSampler : register(s0);
SamplerState PointSampler : register(s1);

cbuffer PSContant : register(b0)
{
	float2	dir;
	float	sssStrength;
	float	sssWidth;
	float	pixelSize;
	float	clampScale;
};

struct PixelOutput
{
	float4	Target0 : SV_Target0;
};

PixelOutput PS_SSSBlur(float2 Tex : TEXCOORD, float4 ScreenPos : SV_Position)
{
	PixelOutput Out;
	Out.Target0 = float4(0, 0, 0, 0);

	// Gaussian weights for the six samples around the current pixel:
	//   -3 -2 -1 +1 +2 +3
	float w[6] = { 0.006,   0.061,   0.242,  0.242,  0.061, 0.006 };
	float o[6] = {  -1.0, -0.6667, -0.3333, 0.3333, 0.6667,   1.0 };

	// Fetch color and linear depth for current pixel:
	float3 colorM = SceneColor.Sample(PointSampler, Tex).rgb;
	float depthM = SceneDepthZ.Sample(PointSampler, Tex).r;

	// Accumulate center sample, multiplying it with its gaussian weight:
	float3 colorBlurred = colorM;
	colorBlurred.rgb *= 0.382;

	// Calculate the step that we will use to fetch the surrounding pixels,
	// where "step" is:
	//     step = sssStrength * gaussianWidth * pixelSize * dir
	// The closer the pixel, the stronger the effect needs to be, hence the factor 1.0 / depthM.
	// 越近的像素，Blur强度应该越大
	float2 step = sssStrength * sssWidth * pixelSize * dir;
	//float2 finalStep = 1 * step / depthM;
	float2 finalStep = 1 * step;

	// Accumulate the other samples:
	[unroll]
	for (int i = 0; i < 6; i++)
	{
		// Fetch color and depth for current sample:
		float2 offset = Tex + o[i] * finalStep;
		float3 color = SceneColor.SampleLevel(LinearSampler, offset, 0).rgb;
		float depth = SceneDepthZ.SampleLevel(LinearSampler, offset, 0).r;

		// If the difference in depth is huge, we lerp color back to "colorM":
		float s = min(0.0125 * clampScale * abs(depthM - depth), 1.0);
		color = lerp(color, colorM.rgb, s);

		// Accumulate:
		colorBlurred.rgb += w[i] * color;
	}

	// The result will be alpha blended with current buffer by using specific
	// RGB weights. For more details, I refer you to the GPU Pro chapter :)
	Out.Target0 = float4(colorBlurred, 1);
	return Out;
}


