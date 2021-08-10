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


void clipPolygonalLightByZPlane(inout float3 vioPolygonalLightVertexPos[5], inout int voPolygonalLightVertexNumAfterClipping)
{
	int Flag4HowManyVertexAboveZPlane = 0;
	if (vioPolygonalLightVertexPos[0].y > 0.0) Flag4HowManyVertexAboveZPlane += 1;
	if (vioPolygonalLightVertexPos[1].y > 0.0) Flag4HowManyVertexAboveZPlane += 2;
	if (vioPolygonalLightVertexPos[2].y > 0.0) Flag4HowManyVertexAboveZPlane += 4;
	if (vioPolygonalLightVertexPos[3].y > 0.0) Flag4HowManyVertexAboveZPlane += 8;

	voPolygonalLightVertexNumAfterClipping = 0;
	if (Flag4HowManyVertexAboveZPlane == 0)
	{
		// none
	}
	else if (Flag4HowManyVertexAboveZPlane == 1)
	{
		voPolygonalLightVertexNumAfterClipping = 3;
		vioPolygonalLightVertexPos[1] = -vioPolygonalLightVertexPos[1].y * vioPolygonalLightVertexPos[0] + vioPolygonalLightVertexPos[0].y * vioPolygonalLightVertexPos[1];
		vioPolygonalLightVertexPos[2] = -vioPolygonalLightVertexPos[3].y * vioPolygonalLightVertexPos[0] + vioPolygonalLightVertexPos[0].y * vioPolygonalLightVertexPos[3];
	}
	else if (Flag4HowManyVertexAboveZPlane == 2)
	{
		voPolygonalLightVertexNumAfterClipping = 3;
		vioPolygonalLightVertexPos[0] = -vioPolygonalLightVertexPos[0].y * vioPolygonalLightVertexPos[1] + vioPolygonalLightVertexPos[1].y * vioPolygonalLightVertexPos[0];
		vioPolygonalLightVertexPos[2] = -vioPolygonalLightVertexPos[2].y * vioPolygonalLightVertexPos[1] + vioPolygonalLightVertexPos[1].y * vioPolygonalLightVertexPos[2];
	}
	else if (Flag4HowManyVertexAboveZPlane == 3)
	{
		voPolygonalLightVertexNumAfterClipping = 4;
		vioPolygonalLightVertexPos[2] = -vioPolygonalLightVertexPos[2].y * vioPolygonalLightVertexPos[1] + vioPolygonalLightVertexPos[1].y * vioPolygonalLightVertexPos[2];
		vioPolygonalLightVertexPos[3] = -vioPolygonalLightVertexPos[3].y * vioPolygonalLightVertexPos[0] + vioPolygonalLightVertexPos[0].y * vioPolygonalLightVertexPos[3];
	}
	else if (Flag4HowManyVertexAboveZPlane == 4)
	{
		voPolygonalLightVertexNumAfterClipping = 3;
		vioPolygonalLightVertexPos[0] = -vioPolygonalLightVertexPos[3].y * vioPolygonalLightVertexPos[2] + vioPolygonalLightVertexPos[2].y * vioPolygonalLightVertexPos[3];
		vioPolygonalLightVertexPos[1] = -vioPolygonalLightVertexPos[1].y * vioPolygonalLightVertexPos[2] + vioPolygonalLightVertexPos[2].y * vioPolygonalLightVertexPos[1];
	}
	else if (Flag4HowManyVertexAboveZPlane == 5)
	{
		voPolygonalLightVertexNumAfterClipping = 0;
	}
	else if (Flag4HowManyVertexAboveZPlane == 6)
	{
		voPolygonalLightVertexNumAfterClipping = 4;
		vioPolygonalLightVertexPos[0] = -vioPolygonalLightVertexPos[0].y * vioPolygonalLightVertexPos[1] + vioPolygonalLightVertexPos[1].y * vioPolygonalLightVertexPos[0];
		vioPolygonalLightVertexPos[3] = -vioPolygonalLightVertexPos[3].y * vioPolygonalLightVertexPos[2] + vioPolygonalLightVertexPos[2].y * vioPolygonalLightVertexPos[3];
	}
	else if (Flag4HowManyVertexAboveZPlane == 7)
	{
		voPolygonalLightVertexNumAfterClipping = 5;
		vioPolygonalLightVertexPos[4] = -vioPolygonalLightVertexPos[3].y * vioPolygonalLightVertexPos[0] + vioPolygonalLightVertexPos[0].y * vioPolygonalLightVertexPos[3];
		vioPolygonalLightVertexPos[3] = -vioPolygonalLightVertexPos[3].y * vioPolygonalLightVertexPos[2] + vioPolygonalLightVertexPos[2].y * vioPolygonalLightVertexPos[3];
	}
	else if (Flag4HowManyVertexAboveZPlane == 8)
	{
		voPolygonalLightVertexNumAfterClipping = 3;
		vioPolygonalLightVertexPos[0] = -vioPolygonalLightVertexPos[0].y * vioPolygonalLightVertexPos[3] + vioPolygonalLightVertexPos[3].y * vioPolygonalLightVertexPos[0];
		vioPolygonalLightVertexPos[1] = -vioPolygonalLightVertexPos[2].y * vioPolygonalLightVertexPos[3] + vioPolygonalLightVertexPos[3].y * vioPolygonalLightVertexPos[2];
		vioPolygonalLightVertexPos[2] = vioPolygonalLightVertexPos[3];
	}
	else if (Flag4HowManyVertexAboveZPlane == 9)
	{
		voPolygonalLightVertexNumAfterClipping = 4;
		vioPolygonalLightVertexPos[1] = -vioPolygonalLightVertexPos[1].y * vioPolygonalLightVertexPos[0] + vioPolygonalLightVertexPos[0].y * vioPolygonalLightVertexPos[1];
		vioPolygonalLightVertexPos[2] = -vioPolygonalLightVertexPos[2].y * vioPolygonalLightVertexPos[3] + vioPolygonalLightVertexPos[3].y * vioPolygonalLightVertexPos[2];
	}
	else if (Flag4HowManyVertexAboveZPlane == 10)
	{
		voPolygonalLightVertexNumAfterClipping = 0;
	}
	else if (Flag4HowManyVertexAboveZPlane == 11)
	{
		voPolygonalLightVertexNumAfterClipping = 5;
		vioPolygonalLightVertexPos[4] = vioPolygonalLightVertexPos[3];
		vioPolygonalLightVertexPos[3] = -vioPolygonalLightVertexPos[2].y * vioPolygonalLightVertexPos[3] + vioPolygonalLightVertexPos[3].y * vioPolygonalLightVertexPos[2];
		vioPolygonalLightVertexPos[2] = -vioPolygonalLightVertexPos[2].y * vioPolygonalLightVertexPos[1] + vioPolygonalLightVertexPos[1].y * vioPolygonalLightVertexPos[2];
	}
	else if (Flag4HowManyVertexAboveZPlane == 12)
	{
		voPolygonalLightVertexNumAfterClipping = 4;
		vioPolygonalLightVertexPos[1] = -vioPolygonalLightVertexPos[1].y * vioPolygonalLightVertexPos[2] + vioPolygonalLightVertexPos[2].y * vioPolygonalLightVertexPos[1];
		vioPolygonalLightVertexPos[0] = -vioPolygonalLightVertexPos[0].y * vioPolygonalLightVertexPos[3] + vioPolygonalLightVertexPos[3].y * vioPolygonalLightVertexPos[0];
	}
	else if (Flag4HowManyVertexAboveZPlane == 13)
	{
		voPolygonalLightVertexNumAfterClipping = 5;
		vioPolygonalLightVertexPos[4] = vioPolygonalLightVertexPos[3];
		vioPolygonalLightVertexPos[3] = vioPolygonalLightVertexPos[2];
		vioPolygonalLightVertexPos[2] = -vioPolygonalLightVertexPos[1].y * vioPolygonalLightVertexPos[2] + vioPolygonalLightVertexPos[2].y * vioPolygonalLightVertexPos[1];
		vioPolygonalLightVertexPos[1] = -vioPolygonalLightVertexPos[1].y * vioPolygonalLightVertexPos[0] + vioPolygonalLightVertexPos[0].y * vioPolygonalLightVertexPos[1];
	}
	else if (Flag4HowManyVertexAboveZPlane == 14)
	{
		voPolygonalLightVertexNumAfterClipping = 5;
		vioPolygonalLightVertexPos[4] = -vioPolygonalLightVertexPos[0].y * vioPolygonalLightVertexPos[3] + vioPolygonalLightVertexPos[3].y * vioPolygonalLightVertexPos[0];
		vioPolygonalLightVertexPos[0] = -vioPolygonalLightVertexPos[0].y * vioPolygonalLightVertexPos[1] + vioPolygonalLightVertexPos[1].y * vioPolygonalLightVertexPos[0];
	}
	else if (Flag4HowManyVertexAboveZPlane == 15)
	{
		voPolygonalLightVertexNumAfterClipping = 4;
	}

	if (voPolygonalLightVertexNumAfterClipping == 3)
		vioPolygonalLightVertexPos[3] = vioPolygonalLightVertexPos[0];
	if (voPolygonalLightVertexNumAfterClipping == 4)
		vioPolygonalLightVertexPos[4] = vioPolygonalLightVertexPos[0];
}

float integrateEdge(float3 v1, float3 v2,float3 n)
{
	float costheta = dot(v1, v2);
	float theta = acos(costheta);
	float3 l = cross(v1, v2);
	return dot(l, n) * ((theta > 0.001) ? theta / sin(theta) : 1.0);
}

/// <summary>
/// LTC积分
/// </summary>
/// <param name="Normal">着色点的法线</param>
/// <param name="ViewDir">View向量</param>
/// <param name="PixelWorldPos">像素的世界坐标</param>
/// <param name="LTCMatrix">LTC矩阵</param>
/// <param name="PolygonalLightVertexPos">面光源的顶点坐标</param>
/// <param name="bTwoSided">是否双面发光</param>
/// <returns>返回颜色</returns>
float3 integrateLTC(float3 Normal, float3 ViewDir, float3 PixelWorldPos, float3x3 LTCMatrix, float3 PolygonalLightVertexPos[4], bool bTwoSided)
{
	// 着色点上的切线空间正交基
	float3 Tangent = normalize(ViewDir - Normal * dot(ViewDir, Normal));
	float3 Bitangent = cross(Tangent, Normal);

	//float3x3 TBN = float3x3(Tangent, Bitangent, Normal);
	float3x3 TBN = float3x3(Bitangent, Normal, Tangent);
	TBN = transpose(TBN);

	// 将变换矩阵转换到切线空间
	LTCMatrix = LTCMatrix * TBN;

	// 多边形顶点(因为被平面z=0裁剪以后，四边形可能变成5个顶点)
	float3 PolygonalLightVertexPosInTangentSpace[5];
	float3 PolygonalLightVertexPosBeforeClipping[4];

	PolygonalLightVertexPosBeforeClipping[0] = PolygonalLightVertexPosInTangentSpace[0] = mul((PolygonalLightVertexPos[0] - PixelWorldPos), LTCMatrix);
	PolygonalLightVertexPosBeforeClipping[1] = PolygonalLightVertexPosInTangentSpace[1] = mul((PolygonalLightVertexPos[1] - PixelWorldPos), LTCMatrix);
	PolygonalLightVertexPosBeforeClipping[2] = PolygonalLightVertexPosInTangentSpace[2] = mul((PolygonalLightVertexPos[2] - PixelWorldPos), LTCMatrix);
	PolygonalLightVertexPosBeforeClipping[3] = PolygonalLightVertexPosInTangentSpace[3] = mul((PolygonalLightVertexPos[3] - PixelWorldPos), LTCMatrix);
	PolygonalLightVertexPosInTangentSpace[4] = PolygonalLightVertexPosInTangentSpace[3];

	// 裁剪后的顶点数量
	int PolygonalLightVertexNumAfterClipping = 0;
	clipPolygonalLightByZPlane(PolygonalLightVertexPosInTangentSpace, PolygonalLightVertexNumAfterClipping);

	if (PolygonalLightVertexNumAfterClipping == 0)
		return float3(0, 0, 0);

	// 把裁剪后的多边形投影到球面上（也就是对每个顶点坐标向量归一化)
	PolygonalLightVertexPosInTangentSpace[0] = normalize(PolygonalLightVertexPosInTangentSpace[0]);
	PolygonalLightVertexPosInTangentSpace[1] = normalize(PolygonalLightVertexPosInTangentSpace[1]);
	PolygonalLightVertexPosInTangentSpace[2] = normalize(PolygonalLightVertexPosInTangentSpace[2]);
	PolygonalLightVertexPosInTangentSpace[3] = normalize(PolygonalLightVertexPosInTangentSpace[3]);
	PolygonalLightVertexPosInTangentSpace[4] = normalize(PolygonalLightVertexPosInTangentSpace[4]);

	// 用累加边的公式来求解积分
	float Sum = 0;
	Sum += integrateEdge(PolygonalLightVertexPosInTangentSpace[0], PolygonalLightVertexPosInTangentSpace[1], Normal);
	Sum += integrateEdge(PolygonalLightVertexPosInTangentSpace[1], PolygonalLightVertexPosInTangentSpace[2], Normal);
	Sum += integrateEdge(PolygonalLightVertexPosInTangentSpace[2], PolygonalLightVertexPosInTangentSpace[3], Normal);
	if (PolygonalLightVertexNumAfterClipping >= 4)
		Sum += integrateEdge(PolygonalLightVertexPosInTangentSpace[3], PolygonalLightVertexPosInTangentSpace[4], Normal);
	if (PolygonalLightVertexNumAfterClipping == 5)
		Sum += integrateEdge(PolygonalLightVertexPosInTangentSpace[4], PolygonalLightVertexPosInTangentSpace[0], Normal);
	Sum *= 0.5;
	Sum = bTwoSided ? abs(Sum) : max(Sum, 0.0);

	return Sum * float3(1, 1, 1);
}

float2 LTC_Coords(float Roughness, float CosTheta)
{
	float Theta = acos(CosTheta);
	float2 Coords = float2(Roughness, Theta / (0.5 * PI));
	const float LUT_SIZE = 32.0;
	Coords.y = 1 - Coords.y;
	// scale and bias coordinates, for correct filtered lookup
	Coords = Coords * (LUT_SIZE - 1.0) / LUT_SIZE + 0.5 / LUT_SIZE;
	return Coords;
}

//------------------------------------------------------- Pixel Shader
void PS_Floor(PixelInput In, out PixelOutput Out)
{
	float3 PixelWorldPos = In.WorldPos.xyz;
	
	float3 GroundNormal = float3(0, 1, 0);
	float3 ViewDir = normalize(CameraPosInWorldSpace - PixelWorldPos);
	float NoV = dot(GroundNormal, ViewDir);
	float2 UV = LTC_Coords(Roughness, NoV);

	float4 LTCMatrixComponents = LTC_MatrixTexture.SampleLevel(LinearSampler, UV, 0);

	float3x3 LTCMatrix = float3x3
	(
		float3(1, 0, LTCMatrixComponents.y),
		float3(0, LTCMatrixComponents.z, 0),
		float3(LTCMatrixComponents.w, 0, LTCMatrixComponents.x)
	);

	bool bTwoSided = bTwoSideLight;

	float3 Diffuse = integrateLTC(GroundNormal, ViewDir, PixelWorldPos, DiffuseMatrix, PolygonalLightVertexPos, bTwoSided);
	float3 Specular = integrateLTC(GroundNormal, ViewDir, PixelWorldPos, LTCMatrix, PolygonalLightVertexPos, bTwoSided);

	Specular *= LTC_MagnitueTexture.SampleLevel(LinearSampler, UV, 0).w;
	float2 Schlick = LTC_MagnitueTexture.SampleLevel(LinearSampler, UV, 0).xy;
	Specular *= SpecularColor * Schlick.x + (1.0 - SpecularColor) * Schlick.y;

	float3 ResultColor = 0;

	if (DebugFlag == 1)
	{
		ResultColor += LightIntensity * (Diffuse * DiffuseColor);
	}
	else if (DebugFlag == 2)
	{
		ResultColor += LightIntensity * (Specular) / 10;
	}
	else
	{
		ResultColor += LightIntensity * (Diffuse * DiffuseColor);
		ResultColor += LightIntensity * (Specular) / 10;
	}
	
	ResultColor /= PI;

	Out.Target0 = float4(ResultColor, 1.0);
}
