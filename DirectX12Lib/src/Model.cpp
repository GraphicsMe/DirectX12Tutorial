#include "Model.h"
#include "ObjLoader.h"
#include "MeshData.h"
#include "CommandContext.h"


FModel::FModel()
	: m_Scale(1.f)
	, m_MeshData(nullptr)
{

}

FModel::FModel(const std::string& FileName, bool FlipV, bool NegateZ, bool FlipNormalZ)
	: m_FileName(FileName)
	, m_Scale(1.f)
{
	m_MeshData = FObjLoader::LoadObj(FileName, FlipV, NegateZ, FlipNormalZ);
	m_MeshData->GetBoundingBox(m_BoundingBox.BoundMin, m_BoundingBox.BoundMax);
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

void FModel::Draw(FCommandContext& CommandContext, bool UseDefualtMaterial)
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
		if (MtlIndex < m_MeshData->GetMaterialCount())
		{
			D3D12_CPU_DESCRIPTOR_HANDLE Handles[MeshData::TEX_PER_MATERIAL];
			for (int j = 0; j < MeshData::TEX_PER_MATERIAL ; ++j)
			{
				Handles[j] = m_Textures[MeshData::TEX_PER_MATERIAL * MtlIndex + j].GetSRV();
			}
			if(UseDefualtMaterial)
				CommandContext.SetDynamicDescriptors(2, 0, MeshData::TEX_PER_MATERIAL, Handles);
		}
		CommandContext.DrawIndexed((UINT)m_MeshData->GetSubIndexCount(i), (UINT)m_MeshData->GetSubIndexStart(i));
	}
}

D3D12_CPU_DESCRIPTOR_HANDLE FModel::GetTextureView(uint32_t SubMeshIndex, uint32_t TexIndex)
{
	D3D12_CPU_DESCRIPTOR_HANDLE Result;
	Result.ptr = 0;
	if (SubMeshIndex < m_MeshData->GetMeshCount())
	{
		size_t MtlIndex = m_MeshData->GetSubMaterialIndex(SubMeshIndex);
		if (MtlIndex < m_MeshData->GetMaterialCount() && TexIndex < MeshData::TEX_PER_MATERIAL)
		{
			Result = m_Textures[MeshData::TEX_PER_MATERIAL * MtlIndex + TexIndex].GetSRV();
		}
	}
	return Result;
}

void FModel::GetMeshLayout(std::vector<D3D12_INPUT_ELEMENT_DESC>& MeshLayout)
{
	m_MeshData->GetMeshLayout(MeshLayout);
}

void FModel::SetScale(float Scale)
{
	m_Scale = Vector3f(Scale);
	UpdateModelMatrix();
}

void FModel::SetScale(float x, float y, float z)
{
	m_Scale = Vector3f(x, y, z);
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

void FModel::Update()
{
	m_PreviousModelMatrix = m_ModelMatrix;
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

	uint32_t MaterialCount = (uint32_t)m_MeshData->GetMaterialCount();
	m_Textures.resize(MaterialCount * MeshData::TEX_PER_MATERIAL);
	for (uint32_t i = 0; i < MaterialCount; ++i)
	{
		MaterialData MtlData = m_MeshData->GetMaterialData(i);
		//basecolor, opacity, emissive, metallic, roughness, ao, normal
		m_Textures[MeshData::TEX_PER_MATERIAL * i + 0].LoadFromFile(ToWideString(m_MeshData->GetBaseColorPath(i)), true);
		m_Textures[MeshData::TEX_PER_MATERIAL * i + 1].LoadFromFile(ToWideString(m_MeshData->GetOpacityPath(i)), false);
		m_Textures[MeshData::TEX_PER_MATERIAL * i + 2].LoadFromFile(ToWideString(m_MeshData->GetEmissivePath(i)), true);
		m_Textures[MeshData::TEX_PER_MATERIAL * i + 3].LoadFromFile(ToWideString(m_MeshData->GetMetallicPath(i)), false);
		m_Textures[MeshData::TEX_PER_MATERIAL * i + 4].LoadFromFile(ToWideString(m_MeshData->GetRoughnessPath(i)), false);
		m_Textures[MeshData::TEX_PER_MATERIAL * i + 5].LoadFromFile(ToWideString(m_MeshData->GetAOPath(i)), false);
		m_Textures[MeshData::TEX_PER_MATERIAL * i + 6].LoadFromFile(ToWideString(m_MeshData->GetNormalPath(i)), false);
	}
}