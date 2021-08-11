#pragma pack_matrix(row_major)

cbuffer VSContant : register(b0)
{
	float4x4 ModelMatrix;
	float4x4 ViewProjMatrix;
};

cbuffer PSContant : register(b0)
{
	float3	CameraPosInWorldSpace;
	float	Roughness;
	float3	DiffuseColor;
	float	bTwoSideLight;
	float3  SpecularColor;
	float	LightIntensity;
	int		DebugFlag;
	float3	pad;
	float3	PolygonalLightVertexPos[4];
};

Texture2D FilteredLightTexture	: register(t0);
Texture2D LTC_MatrixTexture		: register(t1);
Texture2D LTC_MagnitueTexture	: register(t2);

SamplerState LinearSampler : register(s0);

struct VertexInput
{
	float3 Position : POSITION;
	float2 Tex		: TEXCOORD;
	float3 Normal	: NORMAL;
	float4 Tangent	: TANGENT;
};

struct PixelInput
{
	float4 Position	: SV_Position;
	float2 Tex		: TEXCOORD0;
	float4 WorldPos : TEXCOORD1;
};

struct PixelOutput
{
	float4 Target0 : SV_Target0;
};

//------------------------------------------------------- PARAMETERS
const static float PI = 3.1415926535897932f;
const static float3x3 DiffuseMatrix = float3x3
(
	float3(1, 0, 0),
	float3(0, 1, 0),
	float3(0, 0, 1)
);

const static float LUT_SIZE = 64.0;
const static float LUT_SCALE = (LUT_SIZE - 1.0) / LUT_SIZE;
const static float LUT_BIAS = 0.5 / LUT_SIZE;

//------------------------------------------------------- Vetex Shader
PixelInput VS_Floor(VertexInput In)
{
	PixelInput Out;
	Out.Tex = In.Tex;

	Out.WorldPos = mul(float4(In.Position, 1.0), ModelMatrix);
	Out.Position = mul(Out.WorldPos, ViewProjMatrix);

	return Out;
}

//------------------------------------------------------- HELP FUNCTIONS


void ClipQuadToHorizon(inout float3 L[5], inout int n)
{
	// detect clipping config
	int config = 0;
	if (L[0].z > 0.0) config += 1;
	if (L[1].z > 0.0) config += 2;
	if (L[2].z > 0.0) config += 4;
	if (L[3].z > 0.0) config += 8;

	// clip
	n = 0;

	if (config == 0)
	{
		// clip all
	}
	else if (config == 1) // V1 clip V2 V3 V4
	{
		n = 3;
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
		L[2] = -L[3].z * L[0] + L[0].z * L[3];
	}
	else if (config == 2) // V2 clip V1 V3 V4
	{
		n = 3;
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
	}
	else if (config == 3) // V1 V2 clip V3 V4
	{
		n = 4;
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
		L[3] = -L[3].z * L[0] + L[0].z * L[3];
	}
	else if (config == 4) // V3 clip V1 V2 V4
	{
		n = 3;
		L[0] = -L[3].z * L[2] + L[2].z * L[3];
		L[1] = -L[1].z * L[2] + L[2].z * L[1];
	}
	else if (config == 5) // V1 V3 clip V2 V4) impossible
	{
		n = 0;
	}
	else if (config == 6) // V2 V3 clip V1 V4
	{
		n = 4;
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
		L[3] = -L[3].z * L[2] + L[2].z * L[3];
	}
	else if (config == 7) // V1 V2 V3 clip V4
	{
		n = 5;
		L[4] = -L[3].z * L[0] + L[0].z * L[3];
		L[3] = -L[3].z * L[2] + L[2].z * L[3];
	}
	else if (config == 8) // V4 clip V1 V2 V3
	{
		n = 3;
		L[0] = -L[0].z * L[3] + L[3].z * L[0];
		L[1] = -L[2].z * L[3] + L[3].z * L[2];
		L[2] = L[3];
	}
	else if (config == 9) // V1 V4 clip V2 V3
	{
		n = 4;
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
		L[2] = -L[2].z * L[3] + L[3].z * L[2];
	}
	else if (config == 10) // V2 V4 clip V1 V3) impossible
	{
		n = 0;
	}
	else if (config == 11) // V1 V2 V4 clip V3
	{
		n = 5;
		L[4] = L[3];
		L[3] = -L[2].z * L[3] + L[3].z * L[2];
		L[2] = -L[2].z * L[1] + L[1].z * L[2];
	}
	else if (config == 12) // V3 V4 clip V1 V2
	{
		n = 4;
		L[1] = -L[1].z * L[2] + L[2].z * L[1];
		L[0] = -L[0].z * L[3] + L[3].z * L[0];
	}
	else if (config == 13) // V1 V3 V4 clip V2
	{
		n = 5;
		L[4] = L[3];
		L[3] = L[2];
		L[2] = -L[1].z * L[2] + L[2].z * L[1];
		L[1] = -L[1].z * L[0] + L[0].z * L[1];
	}
	else if (config == 14) // V2 V3 V4 clip V1
	{
		n = 5;
		L[4] = -L[0].z * L[3] + L[3].z * L[0];
		L[0] = -L[0].z * L[1] + L[1].z * L[0];
	}
	else if (config == 15) // V1 V2 V3 V4
	{
		n = 4;
	}

	if (n == 3)
		L[3] = L[0];
	if (n == 4)
		L[4] = L[0];
}

float integrateEdge(float3 v1, float3 v2)
{
	float costheta = dot(v1, v2);
	float theta = acos(costheta);
	float3 l = cross(v1, v2);
	return 0.5f * dot(l, float3(0,0,1)) * ((theta > 0.001) ? theta / sin(theta) : 1.0);
}

float inversesqrt(float f)
{
	return 1.0f / sqrt(f);
}

float IntegrateEdgeVec(float3 v1, float3 v2)
{
	float x = dot(v1, v2);
	float y = abs(x);

	float a = 0.8543985 + (0.4965155 + 0.0145206 * y) * y;
	float b = 3.4175940 + (4.1616724 + y) * y;
	float v = a / b;

	float theta_sintheta = (x > 0.0) ? v : 0.5 * inversesqrt(max(1.0 - x * x, 1e-7)) - v;
	float3 l = cross(v1, v2);
	return dot(l,float3(0, 0, 1)) * theta_sintheta;
}

float3 integrateLTC(float3 Normal, float3 ViewDir, float3 PixelWorldPos, float3x3 LTCMatrix, float3 Points[4], bool bTwoSided)
{
	// Orthogonal basis of tangent space on shading point
	float3 Tangent = normalize(ViewDir - Normal * dot(ViewDir, Normal));
	// hlsl的cross是右手规则
	float3 Bitangent = cross(Tangent, Normal);

	float3x3 TBN = float3x3(Tangent, Bitangent, Normal);
	TBN = transpose(TBN);

	float3 L[5];
	L[0] = mul((Points[0] - PixelWorldPos), TBN);
	L[1] = mul((Points[1] - PixelWorldPos), TBN);
	L[2] = mul((Points[2] - PixelWorldPos), TBN);
	L[3] = mul((Points[3] - PixelWorldPos), TBN);
	L[0] = mul(L[0], LTCMatrix);
	L[1] = mul(L[1], LTCMatrix);
	L[2] = mul(L[2], LTCMatrix);
	L[3] = mul(L[3], LTCMatrix);
	L[4] = L[0];

	int VertexNum = 0;
	ClipQuadToHorizon(L, VertexNum);

	if (VertexNum == 0)
		return float3(0, 0, 0);

	L[0] = normalize(L[0]);
	L[1] = normalize(L[1]);
	L[2] = normalize(L[2]);
	L[3] = normalize(L[3]);
	L[4] = normalize(L[4]);

	float Sum = 0;
	Sum += IntegrateEdgeVec(L[0], L[1]);
	Sum += IntegrateEdgeVec(L[1], L[2]);
	Sum += IntegrateEdgeVec(L[2], L[3]);

	if (VertexNum >= 4)
		Sum += IntegrateEdgeVec(L[3], L[4]);
	if (VertexNum == 5)
		Sum += IntegrateEdgeVec(L[4], L[0]);
	
	Sum = bTwoSided ? abs(Sum) : max(Sum, 0.0);

	return Sum * float3(1, 1, 1);
}

float2 LTC_Coords(float Roughness, float CosTheta)
{
	float2 Coords = float2(Roughness, sqrt(1 - CosTheta));
	// scale and bias coordinates, for correct filtered lookup
	Coords = Coords * LUT_SCALE + LUT_BIAS;
	return Coords;
}

//------------------------------------------------------- Pixel Shader
void PS_Floor(PixelInput In, out PixelOutput Out)
{
	float3 PixelWorldPos = In.WorldPos;
	
	float3 GroundNormal = float3(0, 1, 0);
	float3 ViewDir = normalize(CameraPosInWorldSpace - PixelWorldPos);
	float NoV = saturate(dot(GroundNormal, ViewDir));
	float2 UV = LTC_Coords(Roughness, NoV);

	float4 t1 = LTC_MatrixTexture.SampleLevel(LinearSampler, UV, 0);

	float3x3 LTCMatrix = float3x3
	(
		float3(t1.x, 0, t1.y),
		float3(0, 1, 0),
		float3(t1.z, 0, t1.w)
	);

	bool bTwoSided = bTwoSideLight;

	float3 Diffuse = integrateLTC(GroundNormal, ViewDir, PixelWorldPos, DiffuseMatrix, PolygonalLightVertexPos, bTwoSided);
	float3 Specular = integrateLTC(GroundNormal, ViewDir, PixelWorldPos, LTCMatrix, PolygonalLightVertexPos, bTwoSided);

	float4 t2 = LTC_MagnitueTexture.SampleLevel(LinearSampler, UV, 0);

	Specular *= SpecularColor * t2.x + (1.0 - SpecularColor) * t2.y;

	float3 ResultColor = 0;

	if (DebugFlag == 1)
	{
		ResultColor += LightIntensity * (Diffuse * DiffuseColor);
	}
	else if (DebugFlag == 2)
	{
		ResultColor += LightIntensity * (Specular);
	}
	else
	{
		ResultColor += LightIntensity * (Diffuse * DiffuseColor);
		ResultColor += LightIntensity * (Specular);
	}

	Out.Target0 = float4(ResultColor, 1.0);

	// Test Cross
	//float3 vec1Red = float3(1, 0, 0);
	//float3 vec2Green = float3(0, 1, 0);
	//float3 vec3Output = cross(vec1Red, vec2Green);
	//Out.Target0 = float4(vec3Output, 1.0);
}
