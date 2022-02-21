#pragma once

#include "MathLib.h"
#include "MeshData.h"


class MeshNode;
class MeshData;
class FCommandContext;

class SceneNode
{
public:
	SceneNode();
	const FMatrix& GetLocalToWorld() const;
	void AddChild(SceneNode* Node);
	void UpdateTransform();
	virtual bool IsMeshNode() const;
	virtual MeshData* GetFirstMeshData();
	virtual void PostLoad();
	virtual void CollectMeshList(std::vector<MeshNode*>& MeshList);

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
	MeshData* GetFirstMeshData() override { return Mesh; }

	bool IsMeshNode() const override;
	void PostLoad() override;
	void CollectMeshList(std::vector<MeshNode*>& MeshList) override;

private:
	MeshData* Mesh;
};


class Scene
{
public:
	Scene();
	void AddNode(SceneNode* Node);
	void PostLoad();
	MeshData* GetFirstMesh();
	uint32_t GetMeshCount() const { return (uint32_t)MeshList.size(); }
	MeshNode* GetMeshByIndex(uint32_t Index) { return MeshList[Index]; }

private:
	std::vector<SceneNode*> Nodes;
	std::vector<MeshNode*> MeshList;
};