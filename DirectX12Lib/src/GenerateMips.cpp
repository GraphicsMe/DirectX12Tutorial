#include "GenerateMips.h"
#include "SamplerManager.h"
#include "D3D12RHI.h"
#include "CubeBuffer.h"
#include "CommandContext.h"

FRootSignature FGenerateMips::m_GenMipSignature;
FGraphicsPipelineState FGenerateMips::m_GenMipPSO;
ComPtr<ID3DBlob> FGenerateMips::m_ScreenQuadVS;
ComPtr<ID3DBlob> FGenerateMips::m_GenerateMipPS;

struct
{
	Vector2f TexelSize;
	int SrcLevel;
} PSConstants;

void FGenerateMips::Initialize()
{
	FSamplerDesc DefaultSamplerDesc;
	m_GenMipSignature.Reset(2, 1);
	m_GenMipSignature[0].InitAsConstants(0, sizeof(PSConstants) / 4, D3D12_SHADER_VISIBILITY_PIXEL);
	m_GenMipSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	m_GenMipSignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
	m_GenMipSignature.Finalize(L"Generate Mipmap RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	m_GenMipPSO.SetRootSignature(m_GenMipSignature);
	m_GenMipPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
	m_GenMipPSO.SetBlendState(FPipelineState::BlendDisable);
	m_GenMipPSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
	// no need to set input layout
	m_GenMipPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	DXGI_FORMAT CubeFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	m_GenMipPSO.SetRenderTargetFormats(1, &CubeFormat, DXGI_FORMAT_UNKNOWN);

	m_ScreenQuadVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PostProcess.hlsl", "VS_ScreenQuad", "vs_5_1");
	m_GenerateMipPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/GenerateMips.hlsl", "PS_Main", "ps_5_1");

	m_GenMipPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_ScreenQuadVS.Get()));
	m_GenMipPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_GenerateMipPS.Get()));
	m_GenMipPSO.Finalize();
}

void FGenerateMips::Destroy()
{

}

void FGenerateMips::Generate(FCubeBuffer& CubeBuffer, FCommandContext& CommandContext)
{
	Assert(CubeBuffer.GetWidth() == CubeBuffer.GetHeight());



	uint32_t SrcSize = CubeBuffer.GetWidth();

	static bool first = true;
	for (uint32_t MipLevel = 1; MipLevel < CubeBuffer.GetNumMips(); ++MipLevel)
	{
		uint32_t DstSize = SrcSize >> MipLevel;
		PSConstants.SrcLevel = MipLevel - 1;
		PSConstants.TexelSize = Vector2f(1.f / DstSize);

		for (int Face = 0; Face < 6; ++Face)
		{
			CommandContext.SetRootSignature(m_GenMipSignature);
			CommandContext.SetPipelineState(m_GenMipPSO);
			CommandContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			CommandContext.SetViewportAndScissor(0, 0, DstSize, DstSize);

			uint32_t SrcSubIndex = CubeBuffer.GetSubresourceIndex(Face, MipLevel - 1);
			uint32_t DstSubIndex = CubeBuffer.GetSubresourceIndex(Face, MipLevel);
			CommandContext.TransitionSubResource(CubeBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, SrcSubIndex);

			CommandContext.SetRenderTargets(1, &CubeBuffer.GetRTV(Face, MipLevel));
			CommandContext.ClearColor(CubeBuffer, Face, MipLevel);

			CommandContext.SetConstantArray(0, sizeof(PSConstants) / 4, &PSConstants);
			CommandContext.SetDynamicDescriptor(1, 0, CubeBuffer.GetFaceMipSRV(Face, MipLevel-1));

			CommandContext.Draw(3);

			if (MipLevel == CubeBuffer.GetNumMips() - 1)
			{
				CommandContext.TransitionSubResource(CubeBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, DstSubIndex);
			}

			CommandContext.Flush(true);
		}
	}
	first = false;
}

