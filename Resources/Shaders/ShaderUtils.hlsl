#pragma pack_matrix(row_major)

const static float PI = 3.1415926535897932f;

float Pow2(float x)
{
	return x * x;
}

float2 Pow2(float2 x)
{
	return x * x;
}

float3 Pow2(float3 x)
{
	return x * x;
}

float4 Pow2(float4 x)
{
	return x * x;
}

float Pow4(float x)
{
	float xx = x * x;
	return xx * xx;
}

float2 Pow4(float2 x)
{
	float2 xx = x * x;
	return xx * xx;
}

float3 Pow4(float3 x)
{
	float3 xx = x * x;
	return xx * xx;
}

float4 Pow4(float4 x)
{
	float4 xx = x * x;
	return xx * xx;
}

float Pow5(float x)
{
	float xx = x * x;
	return xx * xx * x;
}

float Max3(float a, float b, float c)
{
	return max(a, max(b, c));
}

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ACESFilm(float3 color)
{
	float3 x = 0.8 * color;
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return saturate((x*(a*x + b)) / (x*(c*x + d) + e));
}

float LinearToSrgbChannel(float lin)
{
	if (lin < 0.00313067) return lin * 12.92;
	return pow(lin, (1.0 / 2.4)) * 1.055 - 0.055;
}

float3 LinearToSrgb(float3 lin)
{
	//return pow(lin, 1/2.2);
	return float3(LinearToSrgbChannel(lin.r), LinearToSrgbChannel(lin.g), LinearToSrgbChannel(lin.b));
}

float3 sRGBToLinear(half3 Color)
{
	Color = max(6.10352e-5, Color); // minimum positive non-denormal (fixes black problem on DX11 AMD and NV)
	return Color > 0.04045 ? pow(Color * (1.0 / 1.055) + 0.0521327, 2.4) : Color * (1.0 / 12.92);
}

float3 ToneMapping(float3 Color)
{
	return LinearToSrgb(ACESFilm(Color));
}

/** Reverses all the 32 bits. from UE4 Common.ush */
uint ReverseBits32(uint bits)
{
#if SM5_PROFILE || COMPILER_METAL
	return reversebits(bits);
#else
	bits = (bits << 16) | (bits >> 16);
	bits = ((bits & 0x00ff00ff) << 8) | ((bits & 0xff00ff00) >> 8);
	bits = ((bits & 0x0f0f0f0f) << 4) | ((bits & 0xf0f0f0f0) >> 4);
	bits = ((bits & 0x33333333) << 2) | ((bits & 0xcccccccc) >> 2);
	bits = ((bits & 0x55555555) << 1) | ((bits & 0xaaaaaaaa) >> 1);
	return bits;
#endif
}

/** Generate a pair of random sample. from UE4 MonteCarlo.ush **/
float2 Hammersley(uint Index, uint NumSamples, uint2 Random)
{
	float E1 = frac((float)Index / NumSamples + float(Random.x & 0xffff) / (1 << 16));
	float E2 = float(ReverseBits32(Index) ^ Random.y) * 2.3283064365386963e-10;
	return float2(E1, E2);
}

// [ Duff et al. 2017, "Building an Orthonormal Basis, Revisited" ]
float3x3 GetTangentBasis(float3 TangentZ)
{
	const float Sign = TangentZ.z >= 0 ? 1 : -1;
	const float a = -rcp(Sign + TangentZ.z);
	const float b = TangentZ.x * TangentZ.y * a;

	float3 TangentX = { 1 + Sign * a * Pow2(TangentZ.x), Sign * b, -Sign * TangentZ.x };
	float3 TangentY = { b,  Sign + a * Pow2(TangentZ.y), -TangentZ.y };

	return float3x3(TangentX, TangentY, TangentZ);
}

float3 TangentToWorld(float3 Vec, float3 TangentZ)
{
	return mul(Vec, GetTangentBasis(TangentZ));
}

float4 UniformSampleSphere(float2 E)
{
	float Phi = 2 * PI * E.x;
	float CosTheta = 1 - 2 * E.y;
	float SinTheta = sqrt(1 - CosTheta * CosTheta);

	float3 H;
	H.x = SinTheta * cos(Phi);
	H.y = SinTheta * sin(Phi);
	H.z = CosTheta;

	float PDF = 1.0 / (4 * PI);

	return float4(H, PDF);
}

float4 CosineSampleHemisphere(float2 E)
{
	float Phi = 2 * PI * E.x;
	float CosTheta = sqrt(E.y);
	float SinTheta = sqrt(1 - CosTheta * CosTheta);

	float3 H;
	H.x = SinTheta * cos(Phi);
	H.y = SinTheta * sin(Phi);
	H.z = CosTheta;

	float PDF = CosTheta * (1.0 / PI);

	return float4(H, PDF);
}

float4 CosineSampleHemisphere(float2 E, float3 N)
{
	float3 H = UniformSampleSphere(E).xyz;
	H = normalize(N + H);

	float PDF = dot(H, N) * (1.0 / PI);

	return float4(H, PDF);
}

// 3D random number generator inspired by PCGs (permuted congruential generator). from UE4 Random.ush
uint3 Rand3DPCG16(int3 p)
{
	uint3 v = uint3(p);
	v = v * 1664525u + 1013904223u;

	// That gives a simple mad per round.
	v.x += v.y*v.z;
	v.y += v.z*v.x;
	v.z += v.x*v.y;
	v.x += v.y*v.z;
	v.y += v.z*v.x;
	v.z += v.x*v.y;

	// only top 16 bits are well shuffled
	return v >> 16u;
}

float4 ImportanceSampleGGX(float2 E, float a2)
{
	float Phi = 2 * PI * E.x;
	float CosTheta = sqrt((1 - E.y) / (1 + (a2 - 1) * E.y));
	float SinTheta = sqrt(1 - CosTheta * CosTheta);

	float3 H;
	H.x = SinTheta * cos(Phi);
	H.y = SinTheta * sin(Phi);
	H.z = CosTheta;

	float d = (CosTheta * a2 - CosTheta) * CosTheta + 1;
	float D = a2 / (PI*d*d);
	float PDF = D * CosTheta;

	return float4(H, PDF);
}

// GGX / Trowbridge-Reitz
// [Walter et al. 2007, "Microfacet models for refraction through rough surfaces"]
float D_GGX(float a2, float NoH)
{
	float d = (NoH * a2 - NoH) * NoH + 1;	// 2 mad
	return a2 / (PI*d*d);					// 4 mul, 1 rcp
}

half ComputeReflectionCaptureMipFromRoughness(float Roughness, half CubemapMaxMip)
{
	half LevelFrom1x1 = 1.0 - 1.2 * log2(max(Roughness, 0.001));
	return CubemapMaxMip - 1 - LevelFrom1x1;
}

float ComputeReflectionCaptureRoughnessFromMip(float Mip, half CubemapMaxMip)
{
	float LevelFrom1x1 = CubemapMaxMip - 1 - Mip;
	return exp2((1.0 - LevelFrom1x1) / 1.2);
}

float3 GetSHIrradiance(float3 Normal,int Degree,float3 Coeffs[16])
{
	float3 Color = 0;

	float x = Normal.x;
	float y = Normal.y;
	float z = Normal.z;
	float x2 = x * x;
	float y2 = y * y;
	float z2 = z * z;

	float basis[16];

	basis[0] = 1.f / 2.f * sqrt(1.f / PI);
	basis[1] = sqrt(3.f / (4.f * PI)) * z;
	basis[2] = sqrt(3.f / (4.f * PI)) * y;
	basis[3] = sqrt(3.f / (4.f * PI)) * x;
	basis[4] = 1.f / 2.f * sqrt(15.f / PI) * x * z;
	basis[5] = 1.f / 2.f * sqrt(15.f / PI) * z * y;
	basis[6] = 1.f / 4.f * sqrt(5.f / PI) * (-x * x - z * z + 2 * y * y);
	basis[7] = 1.f / 2.f * sqrt(15.f / PI) * y * x;
	basis[8] = 1.f / 4.f * sqrt(15.f / PI) * (x * x - z * z);
	basis[9] = 1.f / 4.f * sqrt(35.f / (2.f * PI)) * (3 * x2 - z2) * z;
	basis[10] = 1.f / 2.f * sqrt(105.f / PI) * x * z * y;
	basis[11] = 1.f / 4.f * sqrt(21.f / (2.f * PI)) * z * (4 * y2 - x2 - z2);
	basis[12] = 1.f / 4.f * sqrt(7.f / PI) * y * (2 * y2 - 3 * x2 - 3 * z2);
	basis[13] = 1.f / 4.f * sqrt(21.f / (2.f * PI)) * x * (4 * y2 - x2 - z2);
	basis[14] = 1.f / 4.f * sqrt(105.f / PI) * (x2 - z2) * y;
	basis[15] = 1.f / 4.f * sqrt(35.f / (2 * PI)) * (x2 - 3 * z2) * x;

	for (int i = 0; i < Degree * Degree; i++)
		Color += Coeffs[i] * basis[i];
	return Color;
}

float Luminance(float3 Linear)
{
	return dot(Linear, float3(0.3, 0.59, 0.11));
}

// convert view space linear depth
float LinearEyeDepth(float depth,float near,float far)
{
	float z = depth;
	return (near * far) / (far - z * (far - near));
}

float2 ViewportUVToScreenPos(float2 ViewportUV)
{
	return float2(2 * ViewportUV.x - 1, 1 - 2 * ViewportUV.y);
}

float2 ScreenPosToViewportUV(float2 ScreenPos)
{
	return float2(0.5f * ScreenPos.x + 0.5f, 0.5f - 0.5f * ScreenPos.y);
}