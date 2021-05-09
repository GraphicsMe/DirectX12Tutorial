#pragma pack_matrix(row_major)

static const float PI = 3.14159265f;
static const float PlanetRadius = 6371000.0f;
static const float AtmosphereHeight = 80000.0f;
static const float DensityScaleHeight = 7994.0f;
static const float3 LightIntensity = float3(4.f, 4.f, 4.f);
static const float3 SunColor = 1.f;//float3(0.9372549019607843, 0.5568627450980392, 0.2196078431372549);
static const float3 RayleiCoef = float3(5.8f, 13.5f, 33.1f) * 0.000001f;
static const float3 PlanetCenter = float3(0.f, -PlanetRadius, 0.f);


struct VertexOutput
{
	float2 Tex	: TEXCOORD;
	float4 Pos	: SV_Position;
};

cbuffer CSConstant : register(b0)
{
	float4 ScreenParams; //width, height, invWidth, invHeight
	float4 LightDirection;
	float4x4 InvProjectionMatrix;
	float4x4 InvViewMatrix;
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

float2 RaySphereIntersection(float3 RayOrigin, float3 RayDir, float3 SphereCenter, float SphereRadius)
{
	float3 OA = RayOrigin - SphereCenter;
	float A = dot(RayDir, RayDir);
	float B = 2 * dot(OA, RayDir);
	float C = dot(OA, OA) - SphereRadius * SphereRadius;
	float delta = B * B - 4 * A * C;
	if (delta < 0)
	{
		return float2(-1, -1);
	}
	else
	{
		delta = sqrt(delta);
		return float2(-B - delta, -B + delta) / (2 * A);
	}
}

float PhaseFunction(float CosAngle)
{
	return (3.0 / (16.0 * PI)) * (1 + CosAngle * CosAngle);
}

bool CalcLightOpticalDepth(float3 Position, float3 LightDir, out float OpticalDepth)
{
	float2 Intersection = RaySphereIntersection(Position, LightDir, PlanetCenter, PlanetRadius+AtmosphereHeight);
	if (Intersection.y < 0)
		return false;
	float StepCount = 20.0;
	float StepSize = Intersection.y / StepCount;
	float3 Step = LightDir * Intersection.y / StepCount;
	float Intensity = 0.0;
	for (float s = 0.5; s < StepCount; s += 1.0)
	{
		float3 SamplePos = Position + Step * s;
		float height = length(SamplePos - PlanetCenter) - PlanetRadius;
		if (height < 0.0)
			return false;
		Intensity += exp(-height / DensityScaleHeight);
	}
	OpticalDepth = Intensity * StepSize;
	return true;
}

float4 AtmosphericScattering(float3 RayOrigin, float3 RayDir, float DistanceScale, float SampleCount)
{
	float2 Intersection = RaySphereIntersection(RayOrigin, RayDir, PlanetCenter, PlanetRadius+AtmosphereHeight);
	
	float RayLength = Intersection.y;
	float ds = RayLength / SampleCount;
	float3 Step = RayLength * RayDir / SampleCount;
	float3 Scattering = 0.0;
	float OpticalDepthPA = 0.0;
	float Th = PhaseFunction(dot(RayDir, -LightDirection.xyz));
	for (int i = 0; i < SampleCount; ++i)
	{
		float3 P = RayOrigin + (i + 0.5) * Step;

		float OpticalDepthCP;
		bool OverGround = CalcLightOpticalDepth(P, -LightDirection.xyz, OpticalDepthCP);
		float height = length(P-PlanetCenter) - PlanetRadius;
		if (OverGround)
		{
			OpticalDepthPA += exp(-height / DensityScaleHeight) * ds;
			float Tr = exp(-RayleiCoef * (OpticalDepthCP + OpticalDepthPA));
			float h = exp(-abs(height) / DensityScaleHeight);
			Scattering += Th * h * Tr * ds;
		}
		else
		{
			return float4(0.0, 0.0, 0.0, 1.0);
		}
	}
	Scattering *= SunColor * LightIntensity * RayleiCoef;
	return float4(Scattering, 1.0);
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
	return AtmosphericScattering(CameraWorldPos.xyz, ViewDir, 1.0, 16);
}