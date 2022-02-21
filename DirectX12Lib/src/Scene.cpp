#include "Scene.h"
#include "MathLib.h"
#include "CommandContext.h"

SceneNode::SceneNode()
	: Parent(nullptr)
	, LocalToWorldUpdated(false)
{

}

const FMatrix& SceneNode::GetLocalToWorld() const
{
	return LocalToWorld;
}

void SceneNode::AddChild(SceneNode* Node)
{
	Children.push_back(Node);
}

void SceneNode::UpdateTransform()
{
	if (!LocalToWorldUpdated)
	{
		FMatrix ParentToWorld;
		if (Parent)
		{
			ParentToWorld = Parent->GetLocalToWorld();
		}
		LocalToWorld = FMatrix::ScaleMatrix(Scale) * Rotation.ToMatrix() * FMatrix::TranslateMatrix(Translation) * ParentToWorld;
		LocalToWorldUpdated = true;

		for (size_t i = 0; i < Children.size(); ++i)
		{
			Children[i]->UpdateTransform();
		}
	}
}

bool SceneNode::IsMeshNode() const
{
	return false;
}

MeshData* SceneNode::GetFirstMeshData()
{
	for (size_t i = 0; i < Children.size(); ++i)
	{
		MeshData* Mesh = Children[i]->GetFirstMeshData();
		if (Mesh)
		{
			return Mesh;
		}
	}
	return nullptr;
}

void SceneNode::PostLoad()
{
	UpdateTransform();
}

void SceneNode::CollectMeshList(std::vector<MeshNode*>& MeshList)
{
	
}

MeshNode::MeshNode(MeshData* MData)
	: Mesh(MData)
{

}


bool MeshNode::IsMeshNode() const
{
	return true;
}

void MeshNode::PostLoad()
{
	SceneNode::PostLoad();

	if (Mesh)
	{
		Mesh->PostLoad();
	}
}

void MeshNode::CollectMeshList(std::vector<MeshNode*>& MeshList)
{
	MeshList.push_back(this);
	for (size_t i = 0; i < Children.size(); ++i)
	{
		Children[i]->CollectMeshList(MeshList);
	}
}

Scene::Scene()
{

}

void Scene::AddNode(SceneNode* Node)
{
	Nodes.push_back(Node);
}

void Scene::PostLoad()
{
	for (size_t i = 0; i < Nodes.size(); ++i)
	{
		Nodes[i]->PostLoad();
	}

	MeshList.clear();
	for (size_t i = 0; i < Nodes.size(); ++i)
	{
		Nodes[i]->CollectMeshList(MeshList);
	}
}

MeshData* Scene::GetFirstMesh()
{
	for (size_t i = 0; i < Nodes.size(); ++i)
	{
		MeshData* Mesh = Nodes[i]->GetFirstMeshData();
		if (Mesh)
		{
			return Mesh;
		}
	}

	return nullptr;
}