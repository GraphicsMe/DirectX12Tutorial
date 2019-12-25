#pragma once

#include "MathLib.h"
#include <vector>
#include <string>

enum VertexElementType : uint8_t
{
	VET_Position	= 0,
	VET_Color		= 1,
	VET_Texcoord	= 2,
	VET_Normal		= 3,
	VET_Max			= 4
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
	std::string NormalPath;
	std::string MetallicPath;
	std::string RoughnessPath;
	std::string AoPath;
	std::string OpacityPath;
	std::string EmissivePath;
	Vector3f Albedo;
	float Metallic;
	float Roughness;
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
	MeshData(const std::string& filepath);
	~MeshData();

	bool HasVertexElement(VertexElementType type) const;

	uint32_t GetVertexCount() const;
	uint32_t GetVertexSize(VertexElementType type) const;
	uint32_t GetVertexStride(VertexElementType type) const;
	const float* GetVertexData(VertexElementType type);

	uint32_t GetIndexCount() const;
	uint32_t GetIndexSize() const;
	const uint32_t* GetIndexData();

	void AddMaterial(const MaterialData& Material);
	void AddSubMesh(uint32_t StartIndex, uint32_t IndexCount, uint32_t MaterialIndex);
	std::string GetBaseColorPath(uint32_t Index);

	friend FObjLoader;

protected:
	std::string m_filepath;
	std::vector<Vector3f> m_positions;
	std::vector<Vector3f> m_colors;
	std::vector<Vector2f> m_texcoords;
	std::vector<Vector3f> m_normals;
	std::vector<uint32_t> m_indices;

	std::vector<MaterialData> m_materials;
	std::vector<SubMeshData> m_submeshes;
};


class MeshPlane : public MeshData
{
public:
	MeshPlane();
};
