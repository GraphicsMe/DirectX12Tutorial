#pragma pack_matrix(row_major)

static const float PI = 3.14159265f;

static const float3 SunColor = 1.f;//float3(0.9372549019607843, 0.5568627450980392, 0.2196078431372549);
static const float StepCount = 32.f;

struct VertexOutput
{
	float2 Tex	: TEXCOORD;
	float4 Pos	: SV_Position;
};

cbuffer CSConstant : register(b0)
{
	float4x4 InvProjectionMatrix;
	float4x4 InvViewMatrix;
	float4 ScreenParams; //width, height, invWidth, invHeight
	float4 LightDirAndIntensity; 
	float4 EarthCenterAndRadius;
	float2 DensityScaleHeight;
	float AtmosphereRadius;
	float MieG;
	float MieCoef;
	float3 RayleiCoef;
}

Texture2D ScatteringTexture	: register(t0);
SamplerState LinearSampler	: register(s0);

VertexOutput vs_main(in uint VertID : SV_VertexID)
{
	VertexOutput Output;
	// Texture coordinates range [0, 2], but only [0, 1] appears on screen.
	float2 Tex = float2(uint2(VertID, VertID << 1) & 2);
	Output.Tex = Tex;
	Output.Pos = float4(lerp(float2(-1, 1), float2(1, -1), Tex), 0, 1);
	return Output;
}

bool RaySphereIntersection(float3 RayOrigin, float3 RayDir, float3 SphereCenter, float SphereRadius, out float t0, out float t1)
{
	float3 OA = RayOrigin - SphereCenter;
	float A = dot(RayDir, RayDir);
	float B = 2 * dot(OA, RayDir);
	float C = dot(OA, OA) - SphereRadius * SphereRadius;
	float delta = B * B - 4 * A * C;
	if (delta < 0)
	{
		return false;
	}
	else
	{
		delta = sqrt(delta);
		t0 = (-B - delta) / (2 * A);
		t1 = (-B + delta) / (2 * A);
		return true;
	}
}

float2 PhaseFunction(float CosAngle)
{
	float PhaseR = (3.0 / (16.0 * PI)) * (1 + CosAngle * CosAngle);
	float g2 = MieG * MieG;
	float PhaseM = 3.f / (8.f * PI) * ((1.f - g2) * (1.f + CosAngle * CosAngle)) / ((2.f + g2) * pow(1.f + g2 - 2.f * MieG * CosAngle, 1.5f));
	return float2(PhaseR, PhaseM);
}

bool CalcLightOpticalDepth(float3 Position, float3 LightDir, float SampleCount, out float2 OpticalDepth)
{
	float t0, t1;
	if (!RaySphereIntersection(Position, LightDir, EarthCenterAndRadius.xyz, AtmosphereRadius, t0, t1) || t1 < 0)
		return false;
	float HitLength = t1;
	float StepSize = HitLength / SampleCount;
	float3 Step = LightDir * HitLength / SampleCount;
	float2 Intensity = 0.0;
	for (float s = 0.5; s < SampleCount; s += 1.0)
	{
		float3 SamplePos = Position + Step * s;
		float height = length(SamplePos - EarthCenterAndRadius.xyz) - EarthCenterAndRadius.w;
		if (height < 0.0)
			return false;
		Intensity += exp(-height / DensityScaleHeight);
	}
	OpticalDepth = Intensity * StepSize;
	return true;
}

float4 AtmosphericScattering(float3 RayOrigin, float3 RayDir, float DistanceScale, float SampleCount)
{
	float tmin = 0, tmax = -1.f;
	float t0, t1;
	if (RaySphereIntersection(RayOrigin, RayDir, EarthCenterAndRadius.xyz, EarthCenterAndRadius.w, t0, t1) && t0 > 0)
		tmax = t0;

	if (!RaySphereIntersection(RayOrigin, RayDir, EarthCenterAndRadius.xyz, AtmosphereRadius, t0, t1) || t1 < 0)
		return 0.f;

	if (t0 > tmin && t0 > 0.0) tmin = t0;
	if (tmax < 0.0) tmax = t1;
	
	float RayLength = tmax - tmin;
	float ds = RayLength / SampleCount;
	float3 Step = RayLength * RayDir / SampleCount;
	float3 RayleiScattering = 0.0;
	float3 MieScattering = 0.0;
	float2 OpticalDepthPA = 0.0;
	float2 Phase = PhaseFunction(dot(RayDir, -LightDirAndIntensity.xyz));
	for (int i = 0; i < SampleCount; ++i)
	{
		float3 P = RayOrigin + tmin * RayDir + (i + 0.5) * Step;

		float2 OpticalDepthCP;
		bool OverGround = CalcLightOpticalDepth(P, -LightDirAndIntensity.xyz, StepCount, OpticalDepthCP);
		float height = length(P- EarthCenterAndRadius.xyz) - EarthCenterAndRadius.w;
		if (OverGround)
		{
			float2 h = exp(-height / DensityScaleHeight) * ds;
			OpticalDepthPA += h;
			float2 OpticalDepth = OpticalDepthCP + OpticalDepthPA;
			float3 Tr = exp(-(RayleiCoef * OpticalDepth.x + MieCoef * OpticalDepth.y));
			RayleiScattering += h.x * Tr;
			MieScattering += h.y * Tr;
		}
		else
		{
			return float4(0.0, 0.0, 0.0, 1.0);
		}
	}
	RayleiScattering *= SunColor * LightDirAndIntensity.w * RayleiCoef * Phase.x;
	MieScattering *= SunColor * LightDirAndIntensity.w * MieCoef * Phase.y;
	return float4(RayleiScattering+MieScattering, 1.0);
}

float4 ps_main(in VertexOutput Input) : SV_Target0
{
	//float2 UV = float2(Input.Pos.x / ScreenParams.x, Input.Pos.y / ScreenParams.y);
	float2 UV = Input.Tex;
	float4 NDCPos = float4(2 * UV.x - 1.0, 1 - 2 * UV.y, 1.0, 1.0);
	float4 ViewPos = mul(NDCPos, InvProjectionMatrix);
	ViewPos /= ViewPos.w;
	float4 WorldPos = mul(ViewPos, InvViewMatrix);
	float4 CameraWorldPos = mul(float4(0.0, 0.0, 0.0, 1.0), InvViewMatrix);
	float3 ViewDir = normalize(WorldPos.xyz - CameraWorldPos.xyz);
	return AtmosphericScattering(CameraWorldPos.xyz, ViewDir, 1.0, StepCount);
}