#pragma once

#include <vector>
#include <string>
#include "MeshData.h"

namespace tinygltf
{

	class Node;
	class Model;
	struct Mesh;
} // namespace tinygltf

class Scene;
class MeshNode;
class MeshData;
class SceneNode;
class FGLTFLoader
{
public:
	static Scene* LoadFromFile(const std::string& FilePath);

private:
	static SceneNode* LoadNode(SceneNode* Parent, const tinygltf::Node& TinyNode, const tinygltf::Model& TinyModel, const std::vector<MaterialData>& Materials);
	static MeshNode* LoadMesh(const tinygltf::Mesh& TinyNode, const tinygltf::Model& TinyModel, const std::vector<MaterialData>& Materials);
};