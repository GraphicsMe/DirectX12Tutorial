#include "GLTFLoader.h"
#include "MeshData.h"
#include "Common.h"
#include "Scene.h"

#include <iostream>

#define TINYGLTF_IMPLEMENTATION
//#define STB_IMAGE_IMPLEMENTATION
//#define STB_IMAGE_WRITE_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#pragma warning (disable:4996)
#include "tinygltf/tiny_gltf.h"


Scene* FGLTFLoader::LoadFromFile(const std::string& FilePath)
{
	tinygltf::Model TinyModel;
	tinygltf::TinyGLTF TinyLoader;
	std::string Error, Warning;

	bool BinaryFile = false;
	auto ExtPos = FilePath.rfind('.');
	if (ExtPos != std::string::npos)
	{
		BinaryFile = FilePath.substr(ExtPos + 1) == "glb";
	}
	bool Success = false;
	if (BinaryFile)
		Success = TinyLoader.LoadBinaryFromFile(&TinyModel, &Error, &Warning, FilePath); //glb
	else
		Success = TinyLoader.LoadASCIIFromFile(&TinyModel, &Error, &Warning, FilePath);  //gltf

	if (!Error.empty())
		std::cerr << "Error: " << Error << std::endl;
	if (!Warning.empty())
		std::cout << "Warning: " << Warning << std::endl;

	Assert(Success && "Load GLTF Failed!");

	auto LastSeparator = FilePath.rfind('/');
	if (LastSeparator == std::string::npos)
		LastSeparator = FilePath.rfind('\\');
	Assert(LastSeparator != std::string::npos);
	std::string BasePath = FilePath.substr(0, LastSeparator + 1);

	auto GetTexture = [&TinyModel, &BasePath](const tinygltf::ParameterMap& Parameters, const std::string& Key, const std::string& DefaultPath) //
	{
		auto it = Parameters.find(Key);
		if (it != Parameters.end())
		{
			int TextureIndex = it->second.TextureIndex();
			Assert(TextureIndex >= 0 && TextureIndex < TinyModel.textures.size());
			int ImageIndex = TinyModel.textures[TextureIndex].source;
			Assert(ImageIndex >= 0 && ImageIndex < TinyModel.images.size());
			return BasePath + TinyModel.images[ImageIndex].uri;
		}
		return DefaultPath;
	};

	// load materials
	std::vector<MaterialData> Materials;
	Materials.resize(TinyModel.materials.size());
	for (size_t i = 0; i < TinyModel.materials.size(); ++i)
	{
		MaterialData& Material = Materials[i];
		const tinygltf::Material& TinyMat = TinyModel.materials[i];
		Material.Name = TinyMat.name;
		Material.BaseColorPath = GetTexture(TinyMat.values, "baseColorTexture", "../Resources/Textures/white.png");
		Material.MetallicRoughnessPath = GetTexture(TinyMat.values, "metallicRoughnessTexture", "../Resources/Textures/white.png");
		Material.NormalPath = GetTexture(TinyMat.additionalValues, "normalTexture", "../Resources/Textures/default_normal.png");
		Material.EmissivePath = GetTexture(TinyMat.additionalValues, "emissiveTexture", "../Resources/Textures/black.png");
		Material.AoPath = GetTexture(TinyMat.additionalValues, "occlusionTexture", "../Resources/Textures/white.png");
	}

	Scene* MyScene = new Scene();

	Assert(TinyModel.scenes.size() > 0);
	const tinygltf::Scene& TinyScene = TinyModel.scenes[TinyModel.defaultScene > -1 ? TinyModel.defaultScene : 0];
	for (size_t i = 0; i < TinyScene.nodes.size(); ++i)
	{
		const tinygltf::Node& TinyNode = TinyModel.nodes[TinyScene.nodes[i]];
		SceneNode* Node = LoadNode(nullptr, TinyNode, TinyModel, Materials);
		MyScene->AddNode(Node);
	}

	MyScene->PostLoad();

	return MyScene;
}

SceneNode* FGLTFLoader::LoadNode(SceneNode* Parent, const tinygltf::Node& TinyNode, const tinygltf::Model& TinyModel, const std::vector<MaterialData>& Materials)
{
	SceneNode* Node = nullptr;
	if (TinyNode.mesh >= 0)
	{
		// mesh node
		Node = LoadMesh(TinyModel.meshes[TinyNode.mesh], TinyModel, Materials);
	}
	else if (TinyNode.camera >= 0)
	{
		// camera node
		Assert(0); //@todo
	}
	else
	{
		Node = new SceneNode();
	}

	Node->Parent = Parent;
	Node->Name = TinyNode.name;
	if (TinyNode.scale.size() == 3)
	{
		Node->Scale = Vector3f::MakeVector(TinyNode.scale.data());
	}
	if (TinyNode.translation.size() == 3)
	{
		Node->Translation = Vector3f::MakeVector(TinyNode.translation.data());
	}
	if (TinyNode.rotation.size() == 4)
	{
		Node->Rotation = FQuaternion::MakeQuaternion(TinyNode.rotation.data());
	}

	for (size_t i = 0; i < TinyNode.children.size(); ++i)
	{
		LoadNode(Node, TinyModel.nodes[TinyNode.children[i]], TinyModel, Materials);
	}
	return Node;
}


MeshNode* FGLTFLoader::LoadMesh(const tinygltf::Mesh& TinyMesh, const tinygltf::Model& TinyModel, const std::vector<MaterialData>& Materials)
{
	size_t TotalIndexCount = 0;
	size_t TotalVertexCount = 0;

	MeshData* meshdata = new MeshData("");
	for (size_t i = 0; i < Materials.size(); ++i)
	{
		meshdata->AddMaterial(Materials[i]);
	}
	
	bool HasNormal = false;
	bool HasTexcoord0 = false;
	bool HasTexcoord1 = false;
	bool HasTangent = false;
	bool HasIndices = false;
	int IndexType = 0;

	for (size_t i = 0; i < TinyMesh.primitives.size(); ++i)
	{
		const tinygltf::Primitive& Primitive = TinyMesh.primitives[i];
		if (i == 0)
		{
			HasNormal = Primitive.attributes.find("NORMAL") != Primitive.attributes.end();
			HasTexcoord0 = Primitive.attributes.find("TEXCOORD_0") != Primitive.attributes.end();
			HasTexcoord1 = Primitive.attributes.find("TEXCOORD_1") != Primitive.attributes.end();
			HasTangent = Primitive.attributes.find("TANGENT") != Primitive.attributes.end();
			HasIndices = Primitive.indices > -1;

		}
		else
		{
			Assert(HasNormal == (Primitive.attributes.find("NORMAL") != Primitive.attributes.end()));
			Assert(HasTexcoord0 == (Primitive.attributes.find("TEXCOORD_0") != Primitive.attributes.end()));
			Assert(HasTexcoord1 == (Primitive.attributes.find("TEXCOORD_1") != Primitive.attributes.end()));
			Assert(HasTangent == (Primitive.attributes.find("TANGENT") != Primitive.attributes.end()));
			Assert(HasIndices == Primitive.indices > 0);
		}
		

		auto PostionIt = Primitive.attributes.find("POSITION");
		Assert(PostionIt != Primitive.attributes.end());
		const tinygltf::Accessor& PosAccessor = TinyModel.accessors[PostionIt->second];
		Assert(PosAccessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT && "Pos Component should be float");
		Assert(PosAccessor.type == TINYGLTF_TYPE_VEC3 && "Position type should be vec3");
		TotalVertexCount += PosAccessor.count;
		if (HasIndices)
		{
			const tinygltf::Accessor& IndexAccesor = TinyModel.accessors[Primitive.indices];
			TotalIndexCount += IndexAccesor.count;
			if (i == 0)
				IndexType = IndexAccesor.componentType;
			else
				Assert(IndexType == IndexAccesor.componentType);
			Assert(IndexType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT || IndexType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT);
		}
	}

	Assert(HasIndices);

	std::vector<Vector3f> final_positions;
	std::vector<Vector2f> final_texcoords;
	std::vector<Vector3f> final_normals;
	std::vector<Vector4f> final_tangents;
	//std::vector<unsigned int> final_indices;
	final_positions.resize(TotalVertexCount);
	if (HasNormal) final_normals.resize(TotalVertexCount);
	/*if (HasTexcoord0)*/ final_texcoords.resize(TotalVertexCount);
	if (HasTangent) final_tangents.resize(TotalVertexCount);
	meshdata->m_indices.resize(TotalIndexCount);
	
	int FinalVertexCount = 0;
	int FinalIndexCount = 0;
	for (size_t j = 0; j < TinyMesh.primitives.size(); ++j)
	{
		// Position
		const tinygltf::Primitive& Primitive = TinyMesh.primitives[j];
		auto PostionIt = Primitive.attributes.find("POSITION");
		Assert(PostionIt != Primitive.attributes.end());
		const tinygltf::Accessor& PosAccessor = TinyModel.accessors[PostionIt->second];
		const tinygltf::BufferView& PosView = TinyModel.bufferViews[PosAccessor.bufferView];
		int PosStride = PosAccessor.ByteStride(PosView) / tinygltf::GetComponentSizeInBytes(PosAccessor.componentType);
		float* PosBuffer = (float*)&TinyModel.buffers[PosView.buffer].data[PosAccessor.byteOffset + PosView.byteOffset];

		// Tex0
		int Tex0Stride = 0;
		float* Tex0Buffer = nullptr;
		auto Tex0It = Primitive.attributes.find("TEXCOORD_0");
		if (HasTexcoord0)
		{
			Assert(Tex0It != Primitive.attributes.end());
			const tinygltf::Accessor& Tex0Accessor = TinyModel.accessors[Tex0It->second];
			const tinygltf::BufferView& Tex0View = TinyModel.bufferViews[Tex0Accessor.bufferView];
			Tex0Stride = Tex0Accessor.ByteStride(Tex0View) / tinygltf::GetComponentSizeInBytes(Tex0Accessor.componentType);
			Tex0Buffer = (float*)&TinyModel.buffers[Tex0View.buffer].data[Tex0Accessor.byteOffset + Tex0View.byteOffset];
		}

		// Normal
		int NormalStride = 0;
		float* NormalBuffer = nullptr;
		auto NormalIt = Primitive.attributes.find("NORMAL");
		if (HasNormal)
		{
			Assert(NormalIt != Primitive.attributes.end());
			const tinygltf::Accessor& NormalAccessor = TinyModel.accessors[NormalIt->second];
			const tinygltf::BufferView& NormalView = TinyModel.bufferViews[NormalAccessor.bufferView];
			NormalStride = NormalAccessor.ByteStride(NormalView) / tinygltf::GetComponentSizeInBytes(NormalAccessor.componentType);
			NormalBuffer = (float*)&TinyModel.buffers[NormalView.buffer].data[NormalAccessor.byteOffset + NormalView.byteOffset];
		}

		// Tangent
		int TangentStride = 0;
		float* TangentBuffer = nullptr;
		auto TangentIt = Primitive.attributes.find("TANGENT");
		if (HasTangent)
		{
			Assert(TangentIt != Primitive.attributes.end());
			const tinygltf::Accessor& TangentAccessor = TinyModel.accessors[TangentIt->second];
			const tinygltf::BufferView& TangentView = TinyModel.bufferViews[TangentAccessor.bufferView];
			TangentStride = TangentAccessor.ByteStride(TangentView) / tinygltf::GetComponentSizeInBytes(TangentAccessor.componentType);
			TangentBuffer = (float*)&TinyModel.buffers[TangentView.buffer].data[TangentAccessor.byteOffset + TangentView.byteOffset];
		}

		int VertexStart = FinalVertexCount;
		size_t VertexCount = PosAccessor.count;
		for (size_t k = 0; k < VertexCount; ++k)
		{
			final_positions[FinalVertexCount] = Vector3f::MakeVector(PosBuffer + k * PosStride);
			final_texcoords[FinalVertexCount] = HasTexcoord0 ? Vector2f::MakeVector(Tex0Buffer + k * Tex0Stride) : Vector2f(0.f);
			if (HasNormal)
			{
				final_normals[FinalVertexCount] = Vector3f::MakeVector(NormalBuffer + k * NormalStride);
			}
			if (HasTangent)
			{
				final_tangents[FinalVertexCount] = Vector4f::MakeVector(TangentBuffer + k * TangentStride);
			}
			++FinalVertexCount;
		}

		// Index
		const tinygltf::Accessor& IndexAccessor = TinyModel.accessors[Primitive.indices];
		const tinygltf::BufferView& IndexView = TinyModel.bufferViews[IndexAccessor.bufferView];
		size_t IndexCount = IndexAccessor.count;
		const void* DataPtr = &TinyModel.buffers[IndexView.buffer].data[IndexAccessor.byteOffset + IndexView.byteOffset];
		int IndexStart = FinalIndexCount;
		switch (IndexAccessor.componentType)
		{
		case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT:
		{
			const uint32_t* buf = static_cast<const uint32_t*>(DataPtr);
			for (size_t index = 0; index < IndexCount; index++)
			{
				meshdata->m_indices[FinalIndexCount++] = buf[index] + VertexStart;
			}
			break;
		}
		case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT:
		{
			const uint16_t* buf = static_cast<const uint16_t*>(DataPtr);
			for (size_t index = 0; index < IndexCount; index++)
			{
				meshdata->m_indices[FinalIndexCount++] = buf[index] + VertexStart;
			}
			break;
		}
		case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE:
		{
			const uint8_t* buf = static_cast<const uint8_t*>(DataPtr);
			for (size_t index = 0; index < IndexCount; index++)
			{
				meshdata->m_indices[FinalIndexCount++] = buf[index] + VertexStart;
			}
			break;
		}
		default:
			std::cerr << "Index component type " << IndexAccessor.componentType << " not supported!" << std::endl;
			Assert(0);
		}

		meshdata->AddSubMesh(IndexStart, (uint32_t)IndexCount, Primitive.material);
	}

	meshdata->m_positions.swap(final_positions);
	meshdata->m_texcoords.swap(final_texcoords);
	meshdata->m_normals.swap(final_normals);
	meshdata->m_tangents.swap(final_tangents);
	//meshdata->m_indices.swap(final_indices);
	meshdata->ComputeBoundingBox();

	return new MeshNode(meshdata);
}
