#include "Scene.h"
#include "MathLib.h"

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

void SceneNode::CollectMeshBatch(std::vector<MeshDrawCommand>& MeshDrawCommands)
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

void MeshNode::CollectMeshBatch(std::vector<MeshDrawCommand>& MeshDrawCommands)
{
	Mesh->CollectMeshBatch(MeshDrawCommands);
	for (size_t i = 0; i < Children.size(); ++i)
	{
		Children[i]->CollectMeshBatch(MeshDrawCommands);
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
		Nodes[i]->UpdateTransform();
	}

	MeshDrawCommands.clear();
	for (size_t i = 0; i < Nodes.size(); ++i)
	{
		Nodes[i]->CollectMeshBatch(MeshDrawCommands);
	}
}

