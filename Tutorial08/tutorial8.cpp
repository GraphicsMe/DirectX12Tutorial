#include "ApplicationWin32.h"
#include "Game.h"
#include "Common.h"
#include "MathLib.h"
#include "Camera.h"
#include "CommandQueue.h"
#include "D3D12RHI.h"
#include "d3dx12.h"
#include "RenderWindow.h"
#include "CommandListManager.h"
#include "CommandContext.h"
#include "RootSignature.h"
#include "GpuBuffer.h"
#include "PipelineState.h"
#include "DirectXTex.h"
#include "SamplerManager.h"
#include "Model.h"
#include "ShadowBuffer.h"
#include "Light.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <chrono>
#include <iostream>


extern FCommandListManager g_CommandListManager;


const int SHADOW_BUFFER_SIZE = 1024;

class Tutorial8 : public FGame
{
public:
	Tutorial8(const GameDesc& Desc) : FGame(Desc)
	{
	}

	void OnStartup()
	{
		SetupCameraLight();
		SetupMesh();
		SetupShaders();
		SetupPipelineState();
	}

	void OnShutdown()
	{
	}

	void OnUpdate()
	{
	}
	
	void OnRender()
	{
		FCommandContext& CommandContext = FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE_DIRECT, L"3D Queue");

		//ComputePass(CommandContext);

		PostProcess(CommandContext);
		
		CommandContext.Finish(true);

		RenderWindow::Get().Present();
	}

private:
	void SetupCameraLight()
	{
		m_Camera = FCamera(Vector3f(0.f, 1.f, -5.f), Vector3f(0.f, 0.0f, 0.f), Vector3f(0.f, 1.f, 0.f));
		
		const float FovVertical = MATH_PI / 4.f;
		m_Camera.SetPerspectiveParams(FovVertical, (float)GetDesc().Width / GetDesc().Height, 0.1f, 100.f);

		m_DirectionLight.SetDirection(Vector3f(-1.f, -1.f, 0.f));
	}

	void SetupMesh()
	{
	}

	void SetupShaders()
	{
		m_ScatteringCS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/AtmosphericScatteringCS.hlsl", "cs_main", "cs_5_1");
		m_PostVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PostProcess.hlsl", "vs_main", "vs_5_1");
		m_PostPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PostProcess.hlsl", "ps_main", "ps_5_1");
	}

	void SetupPipelineState()
	{
		m_ScatteringSignature.Reset(2, 0);
		m_ScatteringSignature[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		m_ScatteringSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
		m_ScatteringSignature.Finalize(L"Atmospheric Scattering RootSignature");

		m_ScatteringPSO.SetRootSignature(m_ScatteringSignature);
		m_ScatteringPSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(m_ScatteringCS.Get()));
		m_ScatteringPSO.Finalize();
		m_ScatteringBuffer.Create(L"Scattering Buffer", m_GameDesc.Width, m_GameDesc.Height, 1, DXGI_FORMAT_R8G8B8A8_UNORM);

		FSamplerDesc DefaultSamplerDesc;
		m_PostSignature.Reset(2, 1);
		m_PostSignature[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		m_PostSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
		m_PostSignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_PostSignature.Finalize(L"Post Process RootSignature");

		m_PostPSO.SetRootSignature(m_PostSignature);
		m_PostPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_PostPSO.SetBlendState(FPipelineState::BlendDisable);
		m_PostPSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
		// no need to set input layout
		m_PostPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_PostPSO.SetRenderTargetFormats(1, &RenderWindow::Get().GetColorFormat(), DXGI_FORMAT_UNKNOWN);
		m_PostPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_PostVS.Get()));
		m_PostPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_PostPS.Get()));
		m_PostPSO.Finalize();
	}

	
	void ComputePass(FCommandContext& GfxContext)
	{
		FComputeContext& CommandContext = FComputeContext::Begin(L"Compute Queue");

		CommandContext.SetRootSignature(m_ScatteringSignature);
		CommandContext.SetPipelineState(m_ScatteringPSO);
		
		CommandContext.TransitionResource(m_ScatteringBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		CommandContext.SetDynamicDescriptor(0, 0, m_ScatteringBuffer.GetUAV());

		uint32_t GroupCountX = (m_GameDesc.Width  + 7) / 8;
		uint32_t GroupCountY = (m_GameDesc.Height + 7) / 8;

		CommandContext.Dispatch(GroupCountX, GroupCountY, 1);
		
		CommandContext.Finish(false);
	}

	void PostProcess(FCommandContext& GfxContext)
	{
		// Set necessary state.
		GfxContext.SetRootSignature(m_PostSignature);
		GfxContext.SetPipelineState(m_PostPSO);
		GfxContext.SetViewportAndScissor(0, 0, m_GameDesc.Width, m_GameDesc.Height);

		RenderWindow& renderWindow = RenderWindow::Get();
		FColorBuffer& BackBuffer = renderWindow.GetBackBuffer();
		
		// Indicate that the back buffer will be used as a render target.
		//CommandContext.TransitionResource(m_ScatteringBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		
		GfxContext.SetRenderTargets(1, &BackBuffer.GetRTV());

		GfxContext.ClearColor(BackBuffer);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		//m_PSConstant.LightDirection = m_DirectionLight.GetDirection();
		//GfxContext.SetDynamicConstantBufferView(1, sizeof(m_PSConstant), &m_PSConstant);
	
		//GfxContext.SetDynamicDescriptor(0, 0, m_ScatteringBuffer.GetSRV());
		//GfxContext.SetDynamicDescriptor(1, 0, BackBuffer.Get());

		// no need to set vertex buffer and index buffer
		GfxContext.Draw(3);
		
		GfxContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_PRESENT);
	}

private:
	struct
	{
		FMatrix ModelMatrix;
		FMatrix ViewProjection;
		FMatrix ShadowMatrix;
	} m_VSConstant;

	__declspec(align(16)) struct {
		Vector3f LightDirection;
	} m_PSConstant;

	FRootSignature m_ScatteringSignature;
	FRootSignature m_PostSignature;

	FComputePipelineState m_ScatteringPSO;
	FGraphicsPipelineState m_PostPSO;

	FColorBuffer m_ScatteringBuffer;

	ComPtr<ID3DBlob> m_ScatteringCS;
	ComPtr<ID3DBlob> m_PostVS;
	ComPtr<ID3DBlob> m_PostPS;

	FCamera m_Camera;
	FMatrix m_ProjectionMatrix;
	FDirectionalLight m_DirectionLight;
};

int main()
{
	ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	GameDesc Desc;
	Desc.Caption = L"Tutorial 8 - Atmospheric Scattering";
	Tutorial8 tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	CoUninitialize();
	return 0;
}