/// <summary>
/// PreIntegratedSkinLut base on GPU PRO2 - Pre-Integrated Skin Rendering
/// </summary>

const static float PI = 3.1415926535897932f;

float Gaussian(float v, float r)
{
	return 1.0 / sqrt(2.0 * PI * v) * exp(-(r * r) / (2 * v));
}

float3 Scatter(float r)
{
	// coefficients from gpu gems 3 - advance skin rendering
	return
		Gaussian(0.0064 * 1.1414, r) * float3(0.233, 0.455, 0.649) +
		Gaussian(0.0484 * 1.1414, r) * float3(0.100, 0.336, 0.334) +
		Gaussian(0.1870 * 1.1414, r) * float3(0.118, 0.198, 0.000) +
		Gaussian(0.5670 * 1.1414, r) * float3(0.113, 0.007, 0.007) +
		Gaussian(1.9900 * 1.1414, r) * float3(0.358, 0.004, 0.000) +
		Gaussian(7.4100 * 1.1414, r) * float3(0.078, 0.000, 0.000);
}

float3 intergrateDiffuseScatteringOnRing(float cosTheta, float skinRadius)
{
	// Angle from lighting direction
	float theta = acos(cosTheta);
	float3 totalWeights = 0;
	float3 totalLight = 0;
	float inc = 0.0001;

	// fix the Integration domain base base on https://zhuanlan.zhihu.com/p/384541607
	for (float a = -PI ; a <= PI ; a += inc)
	{
		float SampleAngle = theta + a;
		float diffuse = saturate(cos(SampleAngle));
		float sampleDist = abs(2.0 * skinRadius * sin(a * 0.5));
		float3 weights = Scatter(sampleDist);
		totalWeights += weights;
		totalLight += diffuse * weights;
	}
	return totalLight /= max(totalWeights, 0.0001);
}

float4 PS_PreIntegratedSkinLut(float2 Tex : TEXCOORD, float4 ScreenPos : SV_Position): SV_Target0
{
	float4 colorOut = (0, 0, 0, 1);
	
	float NoL = Tex.x * 2 - 1;
	float SkinRadius = 1.0 / (Tex.y + 0.0001);

	// output is in Linear Space
	colorOut.rgb = intergrateDiffuseScatteringOnRing(NoL, SkinRadius);
	
	return colorOut;
}


