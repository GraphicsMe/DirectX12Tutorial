#include "MeshData.h"
#include "GpuBuffer.h"
#include "Texture.h"

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

private:
	void InitializeResource();

protected:
	MeshData* m_MeshData;
	std::string m_FileName;

	FGpuBuffer m_VertexBuffer[VET_Max];
	FGpuBuffer m_IndexBuffer;

	std::vector<FTexture> m_Textures;
};