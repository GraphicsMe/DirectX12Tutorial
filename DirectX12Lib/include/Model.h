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
	FModel(const std::string& FileName);
	~FModel();

	void Draw(FCommandContext& CommandContext);

	void GetMeshLayout(std::vector<D3D12_INPUT_ELEMENT_DESC>& MeshLayout);

	void SetScale(float Scale);
	void SetRotation(const FMatrix& Rotation);
	void SetPosition(const Vector3f& Position);
	void SetPosition(float x, float y, float z);
	const FMatrix GetModelMatrix() { return m_ModelMatrix;}

	const FBoundingBox& GetBoundingBox() const { return m_BoundingBox; }

private:
	void InitializeResource();
	void UpdateModelMatrix();
	void UpdateBoundingBox();

protected:
	float m_Scale;
	FMatrix m_RotationMatrix;
	Vector3f m_Position;
	FMatrix m_ModelMatrix;

	MeshData* m_MeshData;
	std::string m_FileName;

	FGpuBuffer m_VertexBuffer[VET_Max];
	FGpuBuffer m_IndexBuffer;

	std::vector<FTexture> m_Textures;

	FBoundingBox m_BoundingBox;
};