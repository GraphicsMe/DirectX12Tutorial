#include "Geometry.h"

FSkyBox::FSkyBox()
{
	m_MeshData = new MeshData("SkyBox");

	//https://docs.microsoft.com/en-us/windows/win32/direct3d9/cubic-environment-mapping
	m_MeshData->m_positions = {
		{-1.f, 1.f, 1.f},
		{1.f, 1.f, 1.f },
		{1.f, 1.f, -1.f},
		{-1.f, 1.f, -1.f},
		{-1.f, -1.f, 1.f},
		{1.f, -1.f, 1.f},
		{1.f, -1.f, -1.f},
		{-1.f, -1.f, -1.f},
	};
	m_MeshData->m_indices = {
		0, 1, 2,
		0, 2, 3,
		7, 6, 5,
		7, 5, 4,
		0, 3, 7,
		0, 7, 4,
		1, 5, 6,
		1, 6, 2,
		2, 6, 7,
		2, 7, 3,
		0, 4, 5,
		0, 5, 1,
	};
	m_MeshData->ComputeBoundingBox();
	m_MeshData->AddSubMesh(0, 0, 0);

	InitializeResource();
}

FSkyBox::~FSkyBox()
{
}

FCubeMapCross::FCubeMapCross()
{
	m_MeshData = new MeshData("CubeMapCross");

	//https://scalibq.wordpress.com/2013/06/23/cubemaps/
	m_MeshData->m_positions = {
		{ -0.25f, 0.375f, 0.0f,},
		{   0.0f, 0.375f, 0.0f,},
		{  -0.5f, 0.125f, 0.0f,},
		{ -0.25f, 0.125f, 0.0f,},
		{   0.0f, 0.125f, 0.0f,},
		{  0.25f, 0.125f, 0.0f,},
		{   0.5f, 0.125f, 0.0f,},
		{  -0.5f, -0.125f, 0.0f,},
		{ -0.25f, -0.125f, 0.0f,},
		{   0.0f, -0.125f, 0.0f,},
		{  0.25f, -0.125f, 0.0f,},
		{   0.5f, -0.125f, 0.0f,},
		{ -0.25f, -0.375f, 0.0f,},
		{   0.0f, -0.375f, 0.0f,},
	};
	m_MeshData->m_normals = {
		{-1,  1, -1 },
		{1, 1, -1 },
		{-1, 1, -1 },
		{-1,  1,  1 },
		{1,  1,  1 },
		{1,  1, -1 },
		{-1,  1, -1 },
		{-1, -1, -1 },
		{-1, -1,  1 },
		{1, -1,  1 },
		{1, -1, -1 },
		{-1, -1, -1 },
		{-1, -1, -1 },
		{1, -1, -1 }
	};
	m_MeshData->m_indices = {
		0, 1, 3, 3, 1, 4,
		2, 3, 7, 7, 3, 8,
		3, 4, 8, 8, 4, 9,
		4, 5, 9, 9, 5, 10,
		5, 6, 10, 10, 6, 11,
		8, 9, 12, 12, 9, 13
	};
	m_MeshData->ComputeBoundingBox();
	m_MeshData->AddSubMesh(0, 0, 0);

	InitializeResource();
}

FCubeMapCross::~FCubeMapCross()
{

}

FQuad::FQuad()
{
	m_MeshData = new MeshData("Quad");

	m_MeshData->m_positions = {
		{-0.5f,   0.5f	, 0, }, // 0
		{ 0.5f,   0.5f	, 0, },	// 1
		{ 0.5f, -0.5f	, 0, }, // 2
		{-0.5f,	 -0.5f	, 0, }, // 3
	};

	m_MeshData->m_texcoords = {
		{ 0.0f, 0.0f,},
		{ 1.0f, 0.0f,},
		{ 1.0f, 1.0f,},
		{ 0.0f, 1.0f,},
	};

	m_MeshData->m_normals = {
		{ 0, 0, -1 },
		{ 0, 0, -1 },
		{ 0, 0, -1 },
		{ 0, 0, -1 },
	};

	m_MeshData->m_tangents = {
		{ 0,0,0,0 },
		{ 0,0,0,0 },
		{ 0,0,0,0 },
		{ 0,0,0,0 },
	};

	m_MeshData->m_indices = {
		1, 3, 0, 2, 3, 1
	};
	m_MeshData->ComputeBoundingBox();
	m_MeshData->AddSubMesh(0, 0, 0);

	InitializeResource();
}

FQuad::~FQuad()
{

}

void FQuad::GetLightPolygonalVertexPosSet(std::vector<Vector4f>& PolygonalLightVertexPos)
{
	PolygonalLightVertexPos.resize(4);

	std::vector<Vector3f> PolygonalLightVertexOriginalPosSet
	{
		{-0.5f,   0.5f	, 0, }, // 0
		{ 0.5f,   0.5f	, 0, },	// 1
		{ 0.5f,	 -0.5f	, 0, }, // 2
		{-0.5f,	 -0.5f	, 0, }, // 3
	};

	for (int i = 0; i < PolygonalLightVertexPos.size(); ++i)
	{
		Vector3f Pos = PolygonalLightVertexOriginalPosSet[i];
		PolygonalLightVertexPos[i] = Vector4f(Pos.x, Pos.y, Pos.z, 1.0) * GetModelMatrix();
	}
}

