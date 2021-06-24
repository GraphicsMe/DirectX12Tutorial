#include "CubeMapCross.h"

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
