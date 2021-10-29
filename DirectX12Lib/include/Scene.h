#pragma once

#include "MathLib.h"
#include "MeshData.h"


class MeshData;
class SceneNode
{
public:
	SceneNode();
	const FMatrix& GetLocalToWorld() const;
	void AddChild(SceneNode* Node);
	void UpdateTransform();
	virtual bool IsMeshNode() const;
	virtual void CollectMeshBatch(std::vector<MeshDrawCommand>& MeshDrawCommands);

public:
	std::string Name;
	SceneNode* Parent;
	std::vector<SceneNode*> Children;

	Vector3f Scale;
	Vector3f Translation;
	FQuaternion Rotation;
	FMatrix LocalToWorld;

private:
	bool LocalToWorldUpdated;
};


class MeshNode : public SceneNode
{
public:
	MeshNode(MeshData* MData = nullptr);
	void SetMesh(MeshData* MData) { Mesh = MData; }

	bool IsMeshNode() const override;
	void CollectMeshBatch(std::vector<MeshDrawCommand>& MeshDrawCommands) override;

private:
	MeshData* Mesh;
};


class Scene
{
public:
	Scene();
	void AddNode(SceneNode* Node);
	void PostLoad();

private:
	std::vector<SceneNode*> Nodes;
	std::vector<MeshDrawCommand> MeshDrawCommands;
};