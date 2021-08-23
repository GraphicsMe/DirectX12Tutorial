﻿#pragma once
#include "MeshData.h"
#include "GpuBuffer.h"
#include "Texture.h"
#include "MathLib.h"

#include <string>


class MeshData;
class FCommandContext;


class FModel
{
public:
	FModel();
	FModel(const std::string& FileName, bool FlipV = false, bool NegateZ = false, bool FlipNormalZ = false);
	virtual ~FModel();

	virtual bool IsSkyBox() const {return false;}
	virtual void Draw(FCommandContext& CommandContext, bool UseDefualtMaterial = true);

	void GetMeshLayout(std::vector<D3D12_INPUT_ELEMENT_DESC>& MeshLayout);

	D3D12_CPU_DESCRIPTOR_HANDLE GetTextureView(uint32_t SubMeshIndex, uint32_t TexIndex);

	void SetScale(float Scale);
	void SetScale(float x, float y, float z);
	void SetRotation(const FMatrix& Rotation);
	void SetPosition(const Vector3f& Position);
	void SetPosition(float x, float y, float z);
	const FMatrix GetModelMatrix() { return m_ModelMatrix;}
	const FMatrix GetPreviousModelMatrix() { return m_PreviousModelMatrix; }
	void Update();

	const FBoundingBox& GetBoundingBox() const { return m_BoundingBox; }

protected:
	void InitializeResource();
	void UpdateModelMatrix();
	void UpdateBoundingBox();

protected:
	Vector3f m_Scale;
	FMatrix m_RotationMatrix;
	Vector3f m_Position;
	FMatrix m_ModelMatrix;
	FMatrix m_PreviousModelMatrix;

	MeshData* m_MeshData;
	std::string m_FileName;

	FGpuBuffer m_VertexBuffer[VET_Max];
	FGpuBuffer m_IndexBuffer;

	std::vector<FTexture> m_Textures;

	FBoundingBox m_BoundingBox;
};