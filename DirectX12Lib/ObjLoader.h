#pragma once

#include <map>
#include <vector>
#include <string>
#include "MathLib.h"


class MeshData;
struct MaterialData;


struct ObjVertexIndex
{
	int vi, ti, ni;

	bool operator < (const ObjVertexIndex& rhs) const
	{
		if (vi != rhs.vi)
			return vi < rhs.vi;
		else if (ti != rhs.ti)
			return ti < rhs.ti;
		else if (ni != rhs.ni)
			return ni < rhs.ni;
		return false;
	}
};

struct ObjFace
{
	uint32_t Start;
	uint32_t Num;
};

struct ObjMaterial
{
	std::string Name;
	std::string AlbedoPath;
	std::string NormalPath;
	std::string MetallicPath;
	std::string RoughnessPath;
	std::string AoPath;
	std::string OpacityPath;
	std::string EmissivePath;
	Vector3f Albedo;
	float Metallic;
	float Roughness;
	ObjMaterial()
	{
		Reset();
	}

	void Reset()
	{
		Name.clear();
		AlbedoPath.clear();
		NormalPath.clear();
		MetallicPath.clear();
		RoughnessPath.clear();
		AoPath.clear();
		OpacityPath.clear();
		EmissivePath.clear();
		Albedo = Vector3f(1.f);
		Metallic = 0.f;
		Roughness = 0.f;
	}
};

struct ObjGroup
{
	std::string Name;
	uint32_t StartIndex;
	uint32_t IndexCount;
	std::vector<ObjFace> Faces;

	bool HasNormal;

	//ObjMaterial MaterialParam;
	std::string MaterialName;
};

typedef std::map<std::string, ObjMaterial> MaterialLibType;

class FObjLoader
{
public:
	static MeshData* LoadObj(const std::string& FilePath);
	
private:
	static bool LoadMaterialLib(MaterialLibType& MtlLib, const std::string& MtlFilePath);

	static inline uint32_t FixIndex(int idx, uint32_t n);
	static bool ParseMap(const char* &str, const std::string& base_path, const std::string& map_key, std::string& OutPath);
	static bool ParseTripleIndex(const char* &str, ObjVertexIndex& index, uint32_t vn, uint32_t tn, uint32_t nn);

	static uint32_t CollectVertex(
		const ObjVertexIndex& index,
		std::map<ObjVertexIndex, uint32_t>& VertexCache,
		std::vector<float>& final_vertices,
		const std::vector<float>& in_positions,
		const std::vector<float>& in_texcoords,
		const std::vector<float>& in_normals,
		std::vector<Vector3f>& final_positions,
		std::vector<Vector2f>& final_texcoords,
		std::vector<Vector3f>& final_normals
	);

	void static CalcTangents(std::vector<float>& final_vertices, const std::vector<uint32_t>& final_indices, uint32_t VertexFloatCount);

	static inline float ParseFloat(const char*& token);
	static inline void ParseFloat2(const char*& token, float& x, float& y);
	static inline void ParseFloat3(const char*& token, float& x, float& y, float& z);
	static void SkipToNoneSpace(const char* &str);
	static std::string GetBasePath(const std::string& FilePath);
	static MaterialData* CreateMaterial(const ObjGroup& Group, const ObjMaterial& MtlParam);
	static MaterialData CreateMaterialFromName(MaterialLibType& MtlLib, const std::string& MaterialName);
};