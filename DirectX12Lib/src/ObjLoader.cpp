#include "ObjLoader.h"
#include "MathLib.h"
#include "Timer.h"
#include "MeshData.h"
#include "Common.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <map>
#include <algorithm>    // std::sort


#pragma warning (disable:4996)

bool const ENABLE_TANGENT = true;

MeshData* FObjLoader::LoadObj(const std::string& FilePath)
{
	double StartTime = FTimer::GetSeconds();
	std::cout << std::endl << "loading: " << FilePath << std::endl;
	std::ifstream File(FilePath);
	if (!File.is_open())
		return nullptr;
	std::stringstream Buffer;
	Buffer << File.rdbuf();

	MaterialLibType MtlLib;
	//ObjMaterial CurrentMaterial;
	std::string CurrentMaterialName;

	ObjFace face;
	bool FirstGroup = true;
	ObjGroup CurrentGroup;
	std::vector<ObjGroup> Groups;

	std::string Line;
	std::vector<float> positions;
	positions.reserve(1024 * 1024);
	std::vector<float> texcoords;
	texcoords.reserve(1024 * 1024);
	std::vector<float> normals;
	normals.reserve(1024 * 1024);
	std::vector<ObjVertexIndex> all_indices;
	all_indices.reserve(1024 * 1024);
	while (getline(Buffer, Line))
	{
		//std::cout << Line << std::endl;
		const char* line_str = Line.c_str();
		float x, y, z;
		ObjVertexIndex index;
		while(*line_str && isspace(*line_str))
			++line_str;
		if (*line_str == 0)
			continue;
		if (*line_str == '#')
		{
			continue;
		}
		else if (*line_str == 'v')
		{
			if (isspace(*(line_str + 1)))
			{
				// v, vertex
				ParseFloat3(++line_str, x, y, z);
				positions.push_back(x);
				positions.push_back(y);
				//positions.push_back(-z); //?
				positions.push_back(z);
			}
			else if (*(line_str + 1) == 't')
			{
				line_str += 2;
				// vt, texture coordinate
				ParseFloat2(line_str, x, y);
				if (x > 1)
					x -= floor(x);
				if (y > 1)
					y -= floor(y);
				texcoords.push_back(x);
				//texcoords.push_back(1 - y); //?
				texcoords.push_back(y);
			}
			else if (*(line_str + 1) == 'n')
			{
				line_str += 2;
				// vn, normal
				ParseFloat3(line_str, x, y, z);
				normals.push_back(x);
				normals.push_back(y);
				normals.push_back(-z);
			}
			else
			{
				std::cout << "Error: obj invalid line: \"" << Line << "\"" << std::endl;
			}
		}
		else if (*line_str == 'f')
		{
			face.Start = (uint32_t)all_indices.size();
			face.Num = 0;
			while (*line_str)
			{
				++line_str;
				if (ParseTripleIndex(line_str, index, (uint32_t)positions.size() / 3, (uint32_t)texcoords.size() / 2, (uint32_t)normals.size() / 3)) // line_str stop on space char or end
				{
					face.Num++;
					all_indices.push_back(index);
				}
			}
			CurrentGroup.Faces.push_back(face);
		}
		else if (*line_str == 'g' && isspace(*(line_str + 1)))
		{
			++line_str;
			SkipToNoneSpace(line_str);
			if (!FirstGroup)
			{
				// flush prior group
				//CurrentGroup.MaterialParam = CurrentMaterial;
				CurrentGroup.MaterialName = CurrentMaterialName;
				Groups.push_back(CurrentGroup);
			}
			FirstGroup = false;

			CurrentGroup.Faces.clear();
			CurrentGroup.Name = std::string(line_str);
		}
		else if (strncmp(line_str, "mtllib", 6) == 0)
		{
			line_str += 6;
			SkipToNoneSpace(line_str);
			std::string BasePath = GetBasePath(FilePath);
			BasePath += line_str;
			LoadMaterialLib(MtlLib, BasePath);
		}
		else if (strncmp(line_str, "usemtl", 6) == 0)
		{
			line_str += 6;
			SkipToNoneSpace(line_str);
			std::string mtlname = std::string(line_str);
			CurrentMaterialName = mtlname;
			//if (MtlLib.find(mtlname) != MtlLib.end())
			//{
			//	CurrentMaterial = MtlLib[mtlname];
			//}
			//else
			//{
			//	CurrentMaterial.Reset();
			//}
		}
		else
		{
			std::cout << "Warning: unhandled line: \"" << Line << "\"" << std::endl;
		}
	}

	if (!FirstGroup || Groups.empty())
	{
		// flush prior group
		//CurrentGroup.MaterialParam = CurrentMaterial;
		CurrentGroup.MaterialName = CurrentMaterialName;
		Groups.push_back(CurrentGroup);
	}
	
	bool has_texcoord = !texcoords.empty();
	bool has_normal = !normals.empty();
	std::vector<float> final_vertices;
	std::vector<Vector3f> final_positions;
	std::vector<Vector2f> final_texcoords;
	std::vector<Vector3f> final_normals;
	final_positions.reserve(positions.size());
	final_texcoords.reserve(texcoords.size());
	final_normals.reserve(normals.size());
	final_vertices.reserve(positions.size() + texcoords.size() + normals.size());
	std::vector<uint32_t> final_indices;
	final_indices.reserve(1024 * 1024);
	std::map<ObjVertexIndex, uint32_t> VertexCache;

	uint32_t VertexFloatCount = 3; // position
	if (has_texcoord)
	{
		VertexFloatCount += 2; // texcoord
	}
	if (has_normal)
	{
		VertexFloatCount += 3; // normal
	}
	if (has_texcoord && has_normal && ENABLE_TANGENT)
	{
		VertexFloatCount += 4; // tangent
	}

	uint32_t group_size = (uint32_t)Groups.size();
	for (uint32_t g = 0; g < group_size; ++g)
	{
		ObjGroup& Group = Groups[g];
		Group.HasNormal = has_normal;

		Group.StartIndex = (uint32_t)final_indices.size();
		uint32_t face_size = (uint32_t)Group.Faces.size();
		for (uint32_t f = 0; f < face_size; ++f)
		{
			//printf("group %s %d/%d, face %d/%d\n", Group.Name.c_str(), g, group_size, f, face_size);
			const ObjFace& Face = Group.Faces[f];
			ObjVertexIndex v0 = all_indices[Face.Start];
			ObjVertexIndex v1 = all_indices[Face.Start + 1];
			for (uint32_t e = 2; e < Face.Num; ++e)
			{
				ObjVertexIndex v2 = all_indices[Face.Start + e];
				uint32_t i0 = CollectVertex(v0, VertexCache, final_vertices, positions, texcoords, normals, final_positions, final_texcoords, final_normals);
				uint32_t i1 = CollectVertex(v1, VertexCache, final_vertices, positions, texcoords, normals, final_positions, final_texcoords, final_normals);
				uint32_t i2 = CollectVertex(v2, VertexCache, final_vertices, positions, texcoords, normals, final_positions, final_texcoords, final_normals);
				final_indices.push_back(i0);
				final_indices.push_back(i1);
				final_indices.push_back(i2);

				v1 = v2;
			}
		}
		Group.IndexCount = (uint32_t)final_indices.size() - Group.StartIndex;
	}

	if (has_normal && has_texcoord && ENABLE_TANGENT)
	{
		CalcTangents(final_vertices, final_indices, VertexFloatCount);
	}

	MeshData* meshdata = new MeshData(FilePath);
	meshdata->m_positions.swap(final_positions);
	meshdata->m_texcoords.swap(final_texcoords);
	meshdata->m_normals.swap(final_normals);
	meshdata->m_indices.swap(final_indices);
	//Model->SetVertices(&final_vertices[0], final_vertices.size() / VertexFloatCount, sizeof(float) * VertexFloatCount);
	//Model->SetIndices(&final_indices[0], final_indices.size());
	meshdata->ComputeBoundingBox();

	std::vector<MaterialData> UsedMaterials;

	for (uint32_t g = 0; g < Groups.size(); ++g)
	{
		//FMaterial* Material = CreateMaterial(Groups[g], Groups[g].MaterialParam);
		//Model->AddSubModel(Groups[g].StartIndex, Groups[g].IndexCount, Material);
		const ObjGroup& group = Groups[g];
		std::string MaterialName = group.MaterialName;

		std::vector<MaterialData>::iterator it = find_if(UsedMaterials.begin(), UsedMaterials.end(), 
			[&MaterialName] (const MaterialData& One) 
			{ 
				return One.Name == MaterialName;
			}
		);

		uint32_t MtlIndex = 0;
		if (it == UsedMaterials.end())
		{
			MtlIndex = (uint32_t)UsedMaterials.size();
			MaterialData Material = CreateMaterialFromName(MtlLib, group.MaterialName);
			UsedMaterials.push_back(Material);
		}
		else
		{
			MtlIndex = (uint32_t)(it - UsedMaterials.begin());
		}
		meshdata->AddSubMesh(group.StartIndex, group.IndexCount, MtlIndex);
	}
	meshdata->m_materials.swap(UsedMaterials);

	//std::vector<VertexElement> Elements;
	//Elements.push_back({"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(float) * 3});
	//if (has_texcoord)
	//{
	//	Elements.push_back({"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, sizeof(float) * 2});
	//}
	//if (has_normal)
	//{
	//	Elements.push_back({"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(float) * 3});
	//}
	//if (has_texcoord && has_normal && ENABLE_TANGENT)
	//{
	//	Elements.push_back({"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, sizeof(float) * 4});
	//}
	//Model->SetVertexElements(Elements);

	printf("LoadObj Time: %f\n", FTimer::GetSeconds() - StartTime);

	return meshdata;
}

uint32_t FObjLoader::CollectVertex(
	const ObjVertexIndex& Index,
	std::map<ObjVertexIndex, uint32_t>& VertexCache,
	std::vector<float>& final_vertices,
	const std::vector<float>& in_positions,
	const std::vector<float>& in_texcoords,
	const std::vector<float>& in_normals,
	std::vector<Vector3f>& final_positions,
	std::vector<Vector2f>& final_texcoords,
	std::vector<Vector3f>& final_normals
)
{
	if (VertexCache.find(Index) != VertexCache.end())
	{
		return VertexCache[Index];
	}
	else
	{
		int float_count = 3;
		final_vertices.push_back(in_positions[3 * Index.vi]);
		final_vertices.push_back(in_positions[3 * Index.vi + 1]);
		final_vertices.push_back(in_positions[3 * Index.vi + 2]);
		final_positions.emplace_back(in_positions[3 * Index.vi], in_positions[3 * Index.vi + 1], in_positions[3 * Index.vi + 2]);
		if (Index.ti >= 0)
		{
			final_vertices.push_back(in_texcoords[2 * Index.ti]);
			final_vertices.push_back(in_texcoords[2 * Index.ti + 1]);
			final_texcoords.emplace_back(in_texcoords[2 * Index.ti], in_texcoords[2 * Index.ti + 1]);
			float_count += 2;
		}
		if (Index.ni >= 0)
		{
			final_vertices.push_back(in_normals[3 * Index.ni]);
			final_vertices.push_back(in_normals[3 * Index.ni + 1]);
			final_vertices.push_back(in_normals[3 * Index.ni + 2]);
			final_normals.emplace_back(in_normals[3 * Index.ni], in_normals[3 * Index.ni + 1], in_normals[3 * Index.ni + 2]);
			float_count += 3;
		}
		if (Index.ti >= 0 && Index.ni >= 0 && ENABLE_TANGENT)
		{
			final_vertices.push_back(0.f);
			final_vertices.push_back(0.f);
			final_vertices.push_back(0.f);
			final_vertices.push_back(0.f);
			float_count += 4;
		}
		uint32_t final_index = (uint32_t)final_vertices.size() / float_count - 1;
		VertexCache[Index] = final_index;
		return final_index;
	}
}

void FObjLoader::CalcTangents(std::vector<float>& final_vertices, const std::vector<uint32_t>& final_indices, uint32_t VertexFloatCount)
{
	Assert(ENABLE_TANGENT);
	Assert(VertexFloatCount == 12);
	Assert(final_indices.size() % 3 == 0);
	Assert(final_vertices.size() % VertexFloatCount == 0);
	uint32_t TriangleCount = (uint32_t)final_indices.size() / 3;
	uint32_t VertexCount = (uint32_t)final_vertices.size() / VertexFloatCount;

	std::vector<Vector3f> tan1;
	tan1.resize(VertexCount);
	std::vector<Vector3f> tan2;
	tan2.resize(VertexCount);
	for (uint32_t a = 0; a < TriangleCount; ++a)
	{
		uint32_t i1 = final_indices[3 * a];
		uint32_t i2 = final_indices[3 * a + 1];
		uint32_t i3 = final_indices[3 * a + 2];

		Vector3f* v1 = (Vector3f*)&final_vertices[VertexFloatCount * i1];
		Vector3f* v2 = (Vector3f*)&final_vertices[VertexFloatCount * i2];
		Vector3f* v3 = (Vector3f*)&final_vertices[VertexFloatCount * i3];
		Vector2f* w1 = (Vector2f*)&final_vertices[VertexFloatCount * i1 + 3];
		Vector2f* w2 = (Vector2f*)&final_vertices[VertexFloatCount * i2 + 3];
		Vector2f* w3 = (Vector2f*)&final_vertices[VertexFloatCount * i3 + 3];

		float x1 = v2->x - v1->x;
		float x2 = v3->x - v1->x;
		float y1 = v2->y - v1->y;
		float y2 = v3->y - v1->y;
		float z1 = v2->z - v1->z;
		float z2 = v3->z - v1->z;
		float s1 = w2->x - w1->x;
		float s2 = w3->x - w1->x;
		float t1 = w2->y - w1->y;
		float t2 = w3->y - w1->y;

		float r = 1.0F / (s1 * t2 - s2 * t1);
		Vector3f sdir((t2 * x1 - t1 * x2) * r, (t2 * y1 - t1 * y2) * r, (t2 * z1 - t1 * z2) * r);
		Vector3f tdir((s1 * x2 - s2 * x1) * r, (s1 * y2 - s2 * y1) * r, (s1 * z2 - s2 * z1) * r);

		tan1[i1] += sdir;
		tan1[i2] += sdir;
		tan1[i3] += sdir;
		tan2[i1] += tdir;
		tan2[i2] += tdir;
		tan2[i3] += tdir;
	}
	for (uint32_t a = 0; a < VertexCount; a++)
	{
		// position(3), tex(2), normal(3), tangent(3)
		const Vector3f& n = *(Vector3f*)&final_vertices[VertexFloatCount * a + 5];
		const Vector3f& t = tan1[a];
		// Gram-Schmidt orthogonalize.
		Vector4f& tangent = *(Vector4f*)&final_vertices[VertexFloatCount * a + 8];
		tangent = (t - n * n.Dot(t)).Normalize();
		// Calculate handedness.
		tangent.w = (Cross(n, t).Dot(tan2[a]) < 0.0f) ? -1.0f : 1.0f;
	}
}

bool FObjLoader::LoadMaterialLib(MaterialLibType& MtlLib, const std::string& MtlFilePath)
{
	MtlLib.clear();

	std::ifstream File(MtlFilePath);
	if (!File.is_open())
		return false;
	std::stringstream Buffer;
	Buffer << File.rdbuf();

	float color_r, color_g, color_b;
	ObjMaterial CurrentMaterial;

	bool FirstMat = true;
	std::string Line;
	while (getline(Buffer, Line))
	{
		//std::cout << Line << std::endl;
		const char* line_str = Line.c_str();
		while(*line_str && isspace(*line_str))
			++line_str;

		if (strncmp(line_str, "newmtl", 6) == 0)
		{
			if (!FirstMat)
			{
				MtlLib[CurrentMaterial.Name] = CurrentMaterial;
				CurrentMaterial.Reset();
			}
			FirstMat = false;

			line_str += 6;
			SkipToNoneSpace(line_str);
			
			CurrentMaterial.Name = std::string(line_str);
		}
		else if (strncmp(line_str, "map_", 4) == 0)
		{
			std::string BasePath = GetBasePath(MtlFilePath);
			if (ParseMap(line_str, BasePath, "map_Kd", CurrentMaterial.AlbedoPath))
			{
				continue;
			}
			if (ParseMap(line_str, BasePath, "map_bump", CurrentMaterial.NormalPath))
			{
				continue;
			}
			if (ParseMap(line_str, BasePath, "map_Pm", CurrentMaterial.MetallicPath))
			{
				continue;
			}
			if (ParseMap(line_str, BasePath, "map_Pr", CurrentMaterial.RoughnessPath))
			{
				continue;
			}
			if (ParseMap(line_str, BasePath, "map_Ao", CurrentMaterial.AoPath))
			{
				continue;
			}
			if (ParseMap(line_str, BasePath, "map_Op", CurrentMaterial.OpacityPath))
			{
				continue;
			}
			if (ParseMap(line_str, BasePath, "map_Ke", CurrentMaterial.EmissivePath))
			{
				continue;
			}
		}
		else if (strncmp(line_str, "Kd", 2) == 0)
		{
			line_str += 2;
			ParseFloat3(line_str, color_r, color_g, color_b);
			CurrentMaterial.Albedo = Vector3f(color_r, color_g, color_b);
		}
		else if (strncmp(line_str, "Pm", 2) == 0)
		{
			line_str += 2;
			CurrentMaterial.Metallic = ParseFloat(line_str);
		}
		else if (strncmp(line_str, "Pr", 2) == 0)
		{
			line_str += 2;
			CurrentMaterial.Roughness = ParseFloat(line_str);
		}
	}
	if (!FirstMat)
	{
		MtlLib[CurrentMaterial.Name] = CurrentMaterial;
	}
	return true;
}

uint32_t FObjLoader::FixIndex(int idx, uint32_t n)
{
	if (idx > 0)
	{
		return idx - 1;
	}
	else if (idx == 0)
	{
		return 0;
	}
	else
	{
		return idx + n;
	}
}

bool FObjLoader::ParseMap(const char* &str, const std::string& base_path, const std::string& map_key, std::string& OutPath)
{
	if (strncmp(str, map_key.c_str(), map_key.length()) == 0)
	{
		str += map_key.length();
		SkipToNoneSpace(str);
		OutPath = base_path + str;
		return true;
	}
	return false;
}

bool FObjLoader::ParseTripleIndex(const char* &str, ObjVertexIndex& index, uint32_t vn, uint32_t tn, uint32_t nn)
{
	// i/j, i//k, i/j/k
	index.vi = index.ti = index.ni = -1;
	str += strspn(str, " \t");
	if (*str == 0)
		return false;
	index.vi = FixIndex(atoi(str), vn);
	str += strcspn(str, "/ \t\r");
	if (str[0] != '/')
	{
		// i
		return true;
	}
	++str;

	if (str[0] == '/')
	{
		// i//k
		++str;
		index.ni = FixIndex(atoi(str), nn);
		str += strcspn(str, "/ \t\r");
		return true;
	}
	// i/j/k or i/j
	index.ti = FixIndex(atoi(str), tn);
	str += strcspn(str, "/ \t\r");
	
	if (str[0] != '/')
	{
		// i/j
		return true;
	}
	++str;
	
	index.ni = FixIndex(atoi(str), nn);
	str += strcspn(str, "/ \t\r");
	return true;
}

float FObjLoader::ParseFloat(const char*& token)
{
	token += strspn(token, " \t");
	float f = (float)atof(token);
	token += strcspn(token, " \t\r");
	return f;
}

void FObjLoader::ParseFloat2(const char*& token, float& x, float& y)
{
	x = ParseFloat(token);
	y = ParseFloat(token);
}

void FObjLoader::ParseFloat3(const char*& token, float& x, float& y, float& z)
{
	x = ParseFloat(token);
	y = ParseFloat(token);
	z = ParseFloat(token);
}

void FObjLoader::SkipToNoneSpace(const char* &str)
{
	while(*str && isspace(*str))
		++str;
}

std::string FObjLoader::GetBasePath(const std::string& FilePath)
{
	std::string::size_type Pos = FilePath.rfind('/');
	if (Pos == std::string::npos)
		Pos = FilePath.rfind('\\');
	Assert(Pos != std::string::npos);
	return FilePath.substr(0, Pos + 1);
}

MaterialData* FObjLoader::CreateMaterial(const ObjGroup& Group, const ObjMaterial& MtlParam)
{
	std::string ShaderPath;
	bool PureColor = false;
	bool PureColorWithNormalMap = false;
	if (Group.HasNormal)
	{
		if (MtlParam.MetallicPath.empty() && MtlParam.RoughnessPath.empty() && MtlParam.AoPath.empty())
		{
			if (MtlParam.NormalPath.empty())
			{
				PureColor = true;
				ShaderPath = "../Resources/Shaders/PBR_color.shader";
			}
			else
			{
				PureColorWithNormalMap = true;
				ShaderPath = "../Resources/Shaders/PBR_color_normalmap.shader";
			}
		}
		else
		{
			ShaderPath = "../Resources/Shaders/PBR.shader";
		}
	}
	else
	{
		ShaderPath = "../Resources/Shaders/SimpleShader.shader";
	}
	std::map<std::string, std::string> Defines;
	if (!MtlParam.OpacityPath.empty())
	{
		Defines["ALPHA_TEST"] = "";
	}
	/*
	FMaterial* Material = new FMaterial(MtlParam.Name, ShaderPath, Defines);
	if (!PureColor)
	{
		Material->SetAlbedoTexture(MtlParam.AlbedoPath);
		Material->SetMetallicTexture(MtlParam.MetallicPath);
		Material->SetRoughnessTexture(MtlParam.RoughnessPath);
		Material->SetAoTexture(MtlParam.AoPath);
		if (!MtlParam.OpacityPath.empty())
			Material->SetOpacityTexture(MtlParam.OpacityPath);
	}
	if (!PureColor || PureColorWithNormalMap)
	{
		Material->SetNormalTexture(MtlParam.NormalPath);
	}
	
	//Material->SetEmissiveTexture(MtlParam.EmissivePath);
	Material->SetAlbedo(MtlParam.Albedo);
	Material->SetMetallic(MtlParam.Metallic);
	Material->SetRoughness(MtlParam.Roughness);
	*/
	return nullptr;
}

MaterialData FObjLoader::CreateMaterialFromName(MaterialLibType& MtlLib, const std::string& MaterialName)
{
	MaterialData Material;
	if (MtlLib.find(MaterialName) != MtlLib.end())
	{
		ObjMaterial& objMat = MtlLib[MaterialName];
		Material.Name = MaterialName;
		Material.BaseColorPath = objMat.AlbedoPath;
	}
	else
	{
		Material.Name = "missing";
		Material.BaseColorPath = "../Resources/Textures/white.png";
	}
	return Material;
}
