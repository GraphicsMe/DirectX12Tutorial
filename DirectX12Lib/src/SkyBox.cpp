#include "SkyBox.h"

FSkyBox::FSkyBox()
{
	m_MeshData = new MeshData("SkyBox");
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
