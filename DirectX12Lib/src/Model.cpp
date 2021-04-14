#include "Model.h"
#include "ObjLoader.h"
#include "MeshData.h"
#include "CommandContext.h"

FModel::FModel(const std::string& FileName)
	: m_FileName(FileName)
	, m_Scale(1.f)
{
	m_MeshData = FObjLoader::LoadObj(FileName);
	InitializeResource();
}

FModel::~FModel()
{
	if (m_MeshData)
	{
		delete m_MeshData;
		m_MeshData = nullptr;
	}
}

void FModel::Draw(FCommandContext& CommandContext)
{
	for (int i = 0, slot = 0; i < VET_Max; ++i)
	{
		if (m_MeshData->HasVertexElement(VertexElementType(i)))
		{
			CommandContext.SetVertexBuffer(slot++, m_VertexBuffer[i].VertexBufferView());
		}
	}
	CommandContext.SetIndexBuffer(m_IndexBuffer.IndexBufferView());

	for (size_t i = 0; i < m_MeshData->GetMeshCount(); ++i)
	{
		size_t MtlIndex = m_MeshData->GetSubMaterialIndex(i);
		CommandContext.SetDynamicDescriptor(1, 0, m_Textures[MtlIndex].GetSRV());
		CommandContext.DrawIndexed((UINT)m_MeshData->GetSubIndexCount(i), (UINT)m_MeshData->GetSubIndexStart(i));
	}
}

void FModel::GetMeshLayout(std::vector<D3D12_INPUT_ELEMENT_DESC>& MeshLayout)
{
	UINT slot = 0;
	if (m_MeshData->HasVertexElement(VET_Position))
	{
		MeshLayout.push_back({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, slot++, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
	}
	if (m_MeshData->HasVertexElement(VET_Color))
	{
		MeshLayout.push_back({ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, slot++, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
	}
	if (m_MeshData->HasVertexElement(VET_Texcoord))
	{
		MeshLayout.push_back({ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, slot++, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
	}
	if (m_MeshData->HasVertexElement(VET_Normal))
	{
		MeshLayout.push_back({ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, slot++, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
	}
}

void FModel::SetScale(float Scale)
{
	m_Scale = Scale;
	UpdateModelMatrix();
}

void FModel::SetRotation(const FMatrix& Rotation)
{
	m_RotationMatrix = Rotation;
	UpdateModelMatrix();
}

void FModel::SetPosition(const Vector3f& Position)
{
	m_Position = Position;
	UpdateModelMatrix();
}

void FModel::SetPosition(float x, float y, float z)
{
	m_Position = Vector3f(x, y, z);
	UpdateModelMatrix();
}

void FModel::UpdateModelMatrix()
{
	m_ModelMatrix = FMatrix::ScaleMatrix(m_Scale) * m_RotationMatrix * FMatrix::TranslateMatrix(m_Position);
	UpdateBoundingBox();
}

void FModel::UpdateBoundingBox()
{
	Vector3f BoundMin, BoundMax;
	m_MeshData->GetBoundingBox(BoundMin, BoundMax);
	
	m_BoundingBox = m_ModelMatrix.TransformBoundingBox(BoundMin, BoundMax);
}

void FModel::InitializeResource()
{
	for (int i = 0; i < VET_Max; ++i)
	{
		VertexElementType elmType = VertexElementType(i);
		if (m_MeshData->HasVertexElement(elmType))
		{
			m_VertexBuffer[i].Create(
				L"VertexStream",
				m_MeshData->GetVertexCount(),
				m_MeshData->GetVertexStride(elmType),
				m_MeshData->GetVertexData(elmType));
		}
	}

	m_IndexBuffer.Create(L"MeshIndexBuffer", m_MeshData->GetIndexCount(), m_MeshData->GetIndexElementSize(), m_MeshData->GetIndexData());

	size_t MaterialCount = m_MeshData->GetMaterialCount();
	m_Textures.resize(MaterialCount);
	for (size_t i = 0; i < MaterialCount; ++i)
	{
		MaterialData MtlData = m_MeshData->GetMaterialData(i);
		m_Textures[i].LoadFromFile(ToWideString(MtlData.BaseColorPath));
	}
}