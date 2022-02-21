#pragma once

#include "Common.h"
#include "MathLib.h"
#include "GpuBuffer.h"
#include "Texture.h"
#include <vector>
#include <string>

enum VertexElementType : uint8_t
{
	VET_Position	= 0,
	VET_Color		= 1,
	VET_Texcoord	= 2,
	VET_Normal		= 3,
	VET_Tangent		= 4,
	VET_Max			= 5
};


struct VertexElement
{
	std::string SemanticName;
	uint32_t SemanticIndex;
	uint32_t Format;
	uint32_t Slot;
	uint32_t Size;
};


struct MaterialData
{
	std::string Name;
	std::string BaseColorPath;
	std::string MetallicRoughnessPath;
	std::string NormalPath;
	std::string MetallicPath;
	std::string RoughnessPath;
	std::string AoPath;
	std::string OpacityPath;
	std::string EmissivePath;
	Vector3f Albedo;
	float Metallic = 0.f;
	float Roughness = 0.f;
};

class MeshData;
struct MeshDrawCommand
{
	MeshData* Mesh;
	uint32_t StartIndex;
	uint32_t IndexCount;
};

struct SubMeshData
{
	SubMeshData(uint32_t InStartIndex, uint32_t InIndexCount, uint32_t InMaterialIndex)
		: StartIndex(InStartIndex)
		, IndexCount(InIndexCount)
		, MaterialIndex(InMaterialIndex)
	{}

	uint32_t MaterialIndex;
	uint32_t StartIndex, IndexCount;
};

class FObjLoader;

class MeshData
{
public:
	const static int TEX_PER_MATERIAL = 7;

	MeshData(const std::string& filepath);
	~MeshData();

	void PostLoad();

	bool HasVertexElement(VertexElementType type) const;

	uint32_t GetVertexCount() const;
	uint32_t GetVertexSize(VertexElementType type) const;
	uint32_t GetVertexStride(VertexElementType type) const;
	const float* GetVertexData(VertexElementType type);

	uint32_t GetIndexCount() const;
	uint32_t GetIndexSize() const;
	uint32_t GetIndexElementSize() const;
	const uint32_t* GetIndexData();

	size_t GetMaterialCount() const { return m_materials.size(); }
	size_t GetMeshCount() const { return m_submeshes.size(); }
	uint32_t GetSubIndexStart(size_t Index) const;
	size_t GetSubIndexCount(size_t Index) const;
	size_t GetSubMaterialIndex(size_t Index) const;

	void AddMaterial(const MaterialData& Material);
	void AddSubMesh(uint32_t StartIndex, uint32_t IndexCount, uint32_t MaterialIndex);
	std::string GetBaseColorPath(uint32_t MtlIndex);
	std::string GetOpacityPath(uint32_t MtlIndex);
	std::string GetEmissivePath(uint32_t MtlIndex);
	std::string GetMetallicPath(uint32_t MtlIndex);
	std::string GetRoughnessPath(uint32_t MtlIndex);
	std::string GetAOPath(uint32_t MtlIndex);
	std::string GetNormalPath(uint32_t MtlIndex);
	const MaterialData& GetMaterialData(size_t Index);

	FGpuBuffer* GetVertexBuffer(VertexElementType EleIndex) { return &m_VertexBuffer[static_cast<uint32_t>(EleIndex)]; }
	FGpuBuffer* GetIndexBuffer() { return &m_IndexBuffer; }
	FTexture* GetTexture(uint32_t MtlIndex, int TexIndex);
	FTexture* GetTextureByMeshIndex(uint32_t SubMeshIndex, int TexIndex);


	void CollectMeshBatch(std::vector<MeshDrawCommand>& MeshDrawCommands);

	void ComputeBoundingBox();
	void GetBoundingBox(Vector3f& BoundMin, Vector3f& BoundMax);
	void GetMeshLayout(std::vector<D3D12_INPUT_ELEMENT_DESC>& MeshLayout);

	friend FObjLoader;

private:
	void InitRenderingResource();

public:
	std::string m_filepath;
	std::vector<Vector3f> m_positions;
	std::vector<Vector3f> m_colors;
	std::vector<Vector2f> m_texcoords;
	std::vector<Vector3f> m_normals;
	std::vector<Vector4f> m_tangents;
	std::vector<uint32_t> m_indices;

	std::vector<uint8_t*> m_buffers;

	std::vector<MaterialData> m_materials;
	std::vector<SubMeshData> m_submeshes;

	Vector3f m_BoundMin, m_BoundMax;

private:
	FGpuBuffer m_VertexBuffer[VET_Max];
	FGpuBuffer m_IndexBuffer;

	std::vector<FTexture> m_Textures;
};


class MeshPlane : public MeshData
{
public:
	MeshPlane();
};
