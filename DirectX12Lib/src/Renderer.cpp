#include "MeshData.h"
#include "Renderer.h"
#include "CommandContext.h"
#include "Scene.h"


void Renderer::Draw(Scene* pScene, FCommandContext& CommandContext, bool UseDefaultMaterial)
{
	for (uint32_t m = 0; m < pScene->GetMeshCount(); ++m)
	{
		MeshNode* Mesh = pScene->GetMeshByIndex(m);
		MeshData* Data = Mesh->GetFirstMeshData();
		for (int i = 0, slot = 0; i < VET_Max; ++i)
		{
			VertexElementType EleType = static_cast<VertexElementType>(i);
			if (Data->HasVertexElement(EleType))
			{
				CommandContext.SetVertexBuffer(slot++, Data->GetVertexBuffer(EleType)->VertexBufferView());
			}
		}
		CommandContext.SetIndexBuffer(Data->GetIndexBuffer()->IndexBufferView());

		for (uint32_t i = 0; i < Data->GetMeshCount(); ++i)
		{
			size_t MtlIndex = Data->GetSubMaterialIndex(i);

			bool HasTexture = false;
			D3D12_CPU_DESCRIPTOR_HANDLE Handles[MeshData::TEX_PER_MATERIAL];
			for (int j = 0; j < MeshData::TEX_PER_MATERIAL; ++j)
			{
				FTexture* Texture = Data->GetTextureByMeshIndex(i, j);
				if (Texture)
				{
					Handles[j] = Texture->GetSRV();
					HasTexture = true;
				}
			}
			if (HasTexture && UseDefaultMaterial)
			{
				CommandContext.SetDynamicDescriptors(2, 0, MeshData::TEX_PER_MATERIAL, Handles);
			}
			CommandContext.DrawIndexed((UINT)Data->GetSubIndexCount(i), (UINT)Data->GetSubIndexStart(i));
		}
	}
}
