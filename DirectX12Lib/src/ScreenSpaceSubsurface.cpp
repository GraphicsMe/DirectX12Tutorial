#include "ScreenSpaceSubsurface.h"
#include "CommandContext.h"
#include "BufferManager.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "SamplerManager.h"
#include "D3D12RHI.h"

using namespace BufferManager;

namespace ScreenSpaceSubsurface
{
	// Paramters
	bool					g_SSSSkinEnable = true;
	float					g_sssStrength = 0.15f;
	float					g_sssWidth = 3.0f;
	float					g_sssClampScale = 50;
	float					g_EffectStr = 0.5f;

	// Buffers
	FColorBuffer			g_SceneColorCopy;
	FColorBuffer			g_SubsurfaceColor[2];

	// Shaders and PSOs
	FRootSignature			m_SubsurfaceSignature;
	FGraphicsPipelineState	m_SubsurfacePSO;
	ComPtr<ID3DBlob>		m_ScreenQuadVS;
	ComPtr<ID3DBlob>		m_SubsurfacePS;

	FGraphicsPipelineState	m_SubsurfaceCombinePSO;
	ComPtr<ID3DBlob>		m_SubsurfaceCombinePS;

	FGraphicsPipelineState	m_TempBufferPSO;
	ComPtr<ID3DBlob>		m_TempBufferPS;
}

void ScreenSpaceSubsurface::Initialize()
{
	uint32_t bufferWidth = g_SceneColorBuffer.GetWidth();
	uint32_t bufferHeight = g_SceneColorBuffer.GetHeight();

	// Buffers
	g_SubsurfaceColor[0].Create(L"Subsurface Color 0", bufferWidth, bufferHeight, 1, g_SceneColorBuffer.GetFormat());
	g_SubsurfaceColor[1].Create(L"Subsurface Color 1", bufferWidth, bufferHeight, 1, g_SceneColorBuffer.GetFormat());
	g_SceneColorCopy.Create(L"SceneColor Copy ", bufferWidth, bufferHeight, 1, g_SceneColorBuffer.GetFormat());

	// Shader and PSO
	m_ScreenQuadVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PostProcess.hlsl", "VS_ScreenQuad", "vs_5_1");
	m_SubsurfacePS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/ScreenSpaceSubsurfaceBlur.hlsl", "PS_SSSBlur", "ps_5_1");
	m_SubsurfaceCombinePS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/ScreenSpaceSubsurfaceCombine.hlsl", "PS_SSSCombine", "ps_5_1");
	m_TempBufferPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/TempBufferCopy.hlsl", "PS_CopyBuffer", "ps_5_1");

	FSamplerDesc LinearSampler(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
	FSamplerDesc PointSampler(D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	m_SubsurfaceSignature.Reset(2, 2);
	m_SubsurfaceSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
	m_SubsurfaceSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
	m_SubsurfaceSignature.InitStaticSampler(0, LinearSampler, D3D12_SHADER_VISIBILITY_PIXEL);
	m_SubsurfaceSignature.InitStaticSampler(1, PointSampler, D3D12_SHADER_VISIBILITY_PIXEL);
	m_SubsurfaceSignature.Finalize(L"Subsurface");

	m_SubsurfacePSO.SetRootSignature(m_SubsurfaceSignature);
	m_SubsurfacePSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);

	m_SubsurfacePSO.SetBlendState(FPipelineState::BlendDisable);

	D3D12_DEPTH_STENCIL_DESC DSS = FPipelineState::DepthStateDisabled;
	DSS.DepthEnable = FALSE;
	DSS.StencilEnable = TRUE;
	DSS.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	DSS.StencilWriteMask = 0;
	DSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_EQUAL;
	DSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	DSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	DSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	DSS.BackFace = DSS.FrontFace;

	m_SubsurfacePSO.SetDepthStencilState(DSS);
	m_SubsurfacePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

	m_SubsurfacePSO.SetRenderTargetFormats(1, &g_SceneColorCopy.GetFormat(), g_SceneDepthZ.GetFormat());
	m_SubsurfacePSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_ScreenQuadVS.Get()));
	m_SubsurfacePSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_SubsurfacePS.Get()));
	m_SubsurfacePSO.Finalize();

	// Combine
	m_SubsurfaceCombinePSO.SetRootSignature(m_SubsurfaceSignature);
	m_SubsurfaceCombinePSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
	m_SubsurfaceCombinePSO.SetBlendState(FPipelineState::BlendDisable);
	m_SubsurfaceCombinePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_SubsurfaceCombinePSO.SetRenderTargetFormats(1, &g_SceneColorCopy.GetFormat(), g_SceneDepthZ.GetFormat());
	m_SubsurfaceCombinePSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_ScreenQuadVS.Get()));
	m_SubsurfaceCombinePSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_SubsurfaceCombinePS.Get()));
	m_SubsurfaceCombinePSO.Finalize();

	// TempBufferPSO
	m_TempBufferPSO.SetRootSignature(m_SubsurfaceSignature);
	m_TempBufferPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
	m_TempBufferPSO.SetBlendState(FPipelineState::BlendDisable);
	m_TempBufferPSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
	m_TempBufferPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_TempBufferPSO.SetRenderTargetFormats(1, &g_SceneColorCopy.GetFormat(), g_SceneDepthZ.GetFormat());
	m_TempBufferPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_ScreenQuadVS.Get()));
	m_TempBufferPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_TempBufferPS.Get()));
	m_TempBufferPSO.Finalize();
}

void ScreenSpaceSubsurface::Destroy()
{
	g_SubsurfaceColor[0].Destroy();
	g_SubsurfaceColor[1].Destroy();
	g_SceneColorCopy.Destroy();
}

void ScreenSpaceSubsurface::Render(FCommandContext& CommandContext)
{
	if (g_SSSSkinEnable == false)
		return;

	// blur
	{
		CommandContext.SetRootSignature(m_SubsurfaceSignature);
		CommandContext.SetPipelineState(m_SubsurfacePSO);
		CommandContext.TransitionResource(g_SceneDepthZ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ);

		for (int i = 0; i < 6; ++i)
		{
			// Horizontal Blur
			if (i == 0)
			{
				CommandContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}
			else
			{
				CommandContext.TransitionResource(g_SubsurfaceColor[1], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}

			CommandContext.TransitionResource(g_SubsurfaceColor[0], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
			CommandContext.SetRenderTargets(1, &g_SubsurfaceColor[0].GetRTV(), g_SceneDepthZ.GetDSV());
			CommandContext.ClearColor(g_SubsurfaceColor[0]);

			__declspec(align(16)) struct
			{
				Vector2f	dir;
				float		sssStrength;
				float		sssWidth;
				float		pixelSize;
				float		clampScale;
			} Subsurface_Constants;

			Subsurface_Constants.dir = Vector2f(0, 1);
			Subsurface_Constants.pixelSize = 1.0f / g_SceneColorBuffer.GetWidth();
			Subsurface_Constants.sssStrength = g_sssStrength;
			Subsurface_Constants.sssWidth = g_sssWidth;
			Subsurface_Constants.clampScale = g_sssClampScale;

			CommandContext.SetDynamicConstantBufferView(0, sizeof(Subsurface_Constants), &Subsurface_Constants);

			if (i == 0)
			{
				CommandContext.SetDynamicDescriptor(1, 0, g_SceneColorBuffer.GetSRV());
			}
			else
			{
				CommandContext.SetDynamicDescriptor(1, 0, g_SubsurfaceColor[1].GetSRV());
			}
			CommandContext.SetDynamicDescriptor(1, 1, g_SceneDepthZ.GetSRV());

			CommandContext.SetStencilRef(0x1);
			// no need to set vertex buffer and index buffer
			CommandContext.Draw(3);

			// Vertical Blur
			CommandContext.TransitionResource(g_SubsurfaceColor[0], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			CommandContext.TransitionResource(g_SubsurfaceColor[1], D3D12_RESOURCE_STATE_RENDER_TARGET, true);

			CommandContext.SetRenderTargets(1, &g_SubsurfaceColor[1].GetRTV(), g_SceneDepthZ.GetDSV());
			CommandContext.ClearColor(g_SubsurfaceColor[1]);

			Subsurface_Constants.dir = Vector2f(1, 0);
			Subsurface_Constants.pixelSize = 1.0f / g_SceneColorBuffer.GetWidth();
			Subsurface_Constants.sssStrength = g_sssStrength;
			Subsurface_Constants.sssWidth = g_sssWidth;
			Subsurface_Constants.clampScale = g_sssClampScale;

			CommandContext.SetDynamicConstantBufferView(0, sizeof(Subsurface_Constants), &Subsurface_Constants);

			CommandContext.SetDynamicDescriptor(1, 0, g_SubsurfaceColor[0].GetSRV());
			CommandContext.SetDynamicDescriptor(1, 1, g_SceneDepthZ.GetSRV());

			CommandContext.SetStencilRef(0x1);
			// no need to set vertex buffer and index buffer
			CommandContext.Draw(3);
		}
	}

	// copy to tempbuffer
	{
		CommandContext.SetRootSignature(m_SubsurfaceSignature);
		CommandContext.SetPipelineState(m_TempBufferPSO);

		CommandContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		CommandContext.TransitionResource(g_SceneColorCopy, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		CommandContext.SetRenderTargets(1, &g_SceneColorCopy.GetRTV());

		CommandContext.SetDynamicDescriptor(1, 0, g_SceneColorBuffer.GetSRV());
		CommandContext.Draw(3);
	}

	// Combine
	{
		CommandContext.SetRootSignature(m_SubsurfaceSignature);
		CommandContext.SetPipelineState(m_SubsurfaceCombinePSO);
		
		CommandContext.TransitionResource(g_SceneColorCopy, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		CommandContext.TransitionResource(g_SubsurfaceColor[1], D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		CommandContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		CommandContext.SetRenderTargets(1, &g_SceneColorBuffer.GetRTV(), g_SceneDepthZ.GetDSV());

		__declspec(align(16)) struct
		{
			Vector3f	SubsurfaceColor;
			float		EffectStr;
		} Combine_Constants;

		Combine_Constants.SubsurfaceColor = Vector3f(0.655000, 0.559480f, 0.382083f);
		Combine_Constants.EffectStr = g_EffectStr;

		CommandContext.SetDynamicConstantBufferView(0, sizeof(Combine_Constants), &Combine_Constants);
		CommandContext.SetDynamicDescriptor(1, 0, g_SceneColorCopy.GetSRV());
		CommandContext.SetDynamicDescriptor(1, 1, g_SubsurfaceColor[1].GetSRV());

		CommandContext.Draw(3);
	}
}
