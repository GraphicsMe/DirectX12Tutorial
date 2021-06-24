﻿#include "ApplicationWin32.h"
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
#include "CubeBuffer.h"
#include "SkyBox.h"
#include "CubeMapCross.h"
#include "GameInput.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <chrono>
#include <iostream>


extern FCommandListManager g_CommandListManager;

const int CUBE_MAP_SIZE = 1024;
const bool CUBEMAP_DEBUG_VIEW = true;

class Tutorial9 : public FGame
{
public:
	Tutorial9(const GameDesc& Desc) : FGame(Desc)
	{
	}

	void OnStartup()
	{
		SetupCameraLight();
		SetupMesh();
		SetupShaders();
		SetupPipelineState();

		GenerateCubeMap();
	}

	void OnShutdown()
	{
	}

	void OnUpdate()
	{
		tEnd = std::chrono::high_resolution_clock::now();
		float delta = std::chrono::duration<float, std::milli>(tEnd - tStart).count();
		tStart = std::chrono::high_resolution_clock::now();
		m_Camera.Update(delta);

		if (GameInput::IsKeyDown('F'))
			SetupCameraLight();
	}

	void OnRender()
	{
		FCommandContext& CommandContext = FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE_DIRECT, L"3D Queue");

		if (CUBEMAP_DEBUG_VIEW)
			ShowCubeMapDebugView(CommandContext);
		else
			SkyPass(CommandContext);
		
		CommandContext.Finish(true);

		RenderWindow::Get().Present();
	}


private:
	void SetupCameraLight()
	{
		m_Camera = FCamera(Vector3f(0.f, 0.f, -1.f), Vector3f(0.f, 0.f, 0.f), Vector3f(0.f, 1.f, 0.f));
		m_Camera.SetMouseMoveSpeed(1e-4f);
		m_Camera.SetMouseRotateSpeed(1e-4f);

		const float FovVertical = MATH_PI / 4.f;
		m_Camera.SetPerspectiveParams(FovVertical, (float)GetDesc().Width / GetDesc().Height, 0.1f, 100.f);
	}

	void SetupMesh()
	{
		m_TextureLongLat.LoadFromFile(L"../Resources/HDR/spruit_sunrise_2k.hdr");
		m_SkyBox = new FSkyBox();
		m_CubeMapCross = new FCubeMapCross();
	}

	void SetupShaders()
	{
		m_LongLatToCubeVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "VS_LongLatToCube", "vs_5_1");
		m_LongLatToCubePS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_LongLatToCube", "ps_5_1");
		m_SkyVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "VS_SkyCube", "vs_5_1");
		m_SkyPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_SkyCube", "ps_5_1");
		m_CubeMapCrossVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "VS_CubeMapCross", "vs_5_1");
		m_CubeMapCrossPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_CubeMapCross", "ps_5_1");
	}

	void SetupPipelineState()
	{
		FSamplerDesc DefaultSamplerDesc;
		m_GenCubeSignature.Reset(2, 1);
		m_GenCubeSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
		m_GenCubeSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		m_GenCubeSignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_GenCubeSignature.Finalize(L"Generate Cubemap RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_GenCubePSO.SetRootSignature(m_GenCubeSignature);
		m_GenCubePSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_GenCubePSO.SetBlendState(FPipelineState::BlendDisable);
		m_GenCubePSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
		
		std::vector<D3D12_INPUT_ELEMENT_DESC> MeshLayout;
		m_SkyBox->GetMeshLayout(MeshLayout);
		Assert(MeshLayout.size() > 0);
		m_GenCubePSO.SetInputLayout((UINT)MeshLayout.size(), &MeshLayout[0]);

		m_GenCubePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		DXGI_FORMAT Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		m_GenCubePSO.SetRenderTargetFormats(1, &Format, DXGI_FORMAT_UNKNOWN);
		m_GenCubePSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_LongLatToCubeVS.Get()));
		m_GenCubePSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_LongLatToCubePS.Get()));
		m_GenCubePSO.Finalize();

		m_CubeBuffer.Create(L"CubeMap", CUBE_MAP_SIZE, CUBE_MAP_SIZE, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);

		m_SkySignature.Reset(2, 1);
		m_SkySignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
		m_SkySignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		m_SkySignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_SkySignature.Finalize(L"Sky RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_SkyPSO.SetRootSignature(m_SkySignature);
		m_SkyPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_SkyPSO.SetBlendState(FPipelineState::BlendDisable);
		m_SkyPSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
		m_SkyPSO.SetInputLayout((UINT)MeshLayout.size(), &MeshLayout[0]);
		m_SkyPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_SkyPSO.SetRenderTargetFormats(1, &RenderWindow::Get().GetColorFormat(), RenderWindow::Get().GetDepthFormat());
		m_SkyPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_SkyVS.Get()));
		m_SkyPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_SkyPS.Get()));
		m_SkyPSO.Finalize();

		m_CubeMapCrossViewSignature.Reset(3, 1);
		m_CubeMapCrossViewSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
		m_CubeMapCrossViewSignature[1].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
		m_CubeMapCrossViewSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		m_CubeMapCrossViewSignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_CubeMapCrossViewSignature.Finalize(L"Cube Map Cross View RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_CubeMapCrossViewPSO.SetRootSignature(m_CubeMapCrossViewSignature);
		m_CubeMapCrossViewPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_CubeMapCrossViewPSO.SetBlendState(FPipelineState::BlendDisable);
		m_CubeMapCrossViewPSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);

		std::vector<D3D12_INPUT_ELEMENT_DESC> DebugMeshLayout;
		m_CubeMapCross->GetMeshLayout(DebugMeshLayout);
		Assert(DebugMeshLayout.size() > 0);
		m_CubeMapCrossViewPSO.SetInputLayout((UINT)DebugMeshLayout.size(), &DebugMeshLayout[0]);

		m_CubeMapCrossViewPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_CubeMapCrossViewPSO.SetRenderTargetFormats(1, &RenderWindow::Get().GetColorFormat(), RenderWindow::Get().GetDepthFormat());
		m_CubeMapCrossViewPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_CubeMapCrossVS.Get()));
		m_CubeMapCrossViewPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_CubeMapCrossPS.Get()));
		m_CubeMapCrossViewPSO.Finalize();
	}

	void GenerateCubeMap()
	{
		FCommandContext& GfxContext = FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE_DIRECT, L"3D Queue");

		GfxContext.SetRootSignature(m_GenCubeSignature);
		GfxContext.SetPipelineState(m_GenCubePSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GfxContext.SetViewportAndScissor(0, 0, CUBE_MAP_SIZE, CUBE_MAP_SIZE); // very important, cubemap size

		GfxContext.TransitionResource(m_CubeBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		
		GfxContext.SetDynamicDescriptor(1, 0, m_TextureLongLat.GetSRV());

		m_VSConstants.ModelMatrix = FMatrix(); // identity
		for (int i = 0; i < 6; ++i)
		{
			GfxContext.SetRenderTargets(1, &m_CubeBuffer.GetRTV(i));
			GfxContext.ClearColor(m_CubeBuffer, i, 0);

			m_VSConstants.ViewProjMatrix = m_CubeBuffer.GetViewProjMatrix(i);
			GfxContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstants), &m_VSConstants);
			
			m_SkyBox->Draw(GfxContext);
		}
		GfxContext.Finish(true);
	}

	void SkyPass(FCommandContext& GfxContext)
	{
		// Set necessary state.
		GfxContext.SetRootSignature(m_SkySignature);
		GfxContext.SetPipelineState(m_SkyPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GfxContext.SetViewportAndScissor(0, 0, m_GameDesc.Width, m_GameDesc.Height);

		RenderWindow& renderWindow = RenderWindow::Get();
		FColorBuffer& BackBuffer = renderWindow.GetBackBuffer();
		FDepthBuffer& DepthBuffer = renderWindow.GetDepthBuffer();

		// Indicate that the back buffer will be used as a render target.
		GfxContext.TransitionResource(m_CubeBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(DepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

		GfxContext.SetRenderTargets(1, &BackBuffer.GetRTV());
		GfxContext.SetRenderTargets(1, &BackBuffer.GetRTV(), DepthBuffer.GetDSV());

		GfxContext.ClearColor(BackBuffer);
		GfxContext.ClearDepth(DepthBuffer);

		m_VSConstants.ModelMatrix = FMatrix::TranslateMatrix(m_Camera.GetPosition()); // move with camera
		m_VSConstants.ViewProjMatrix = m_Camera.GetViewMatrix() * m_Camera.GetProjectionMatrix();
		GfxContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstants), &m_VSConstants);

		GfxContext.SetDynamicDescriptor(1, 0, m_CubeBuffer.GetSRV());

		m_SkyBox->Draw(GfxContext);

		GfxContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_PRESENT);
	}

	void ShowCubeMapDebugView(FCommandContext& GfxContext)
	{
		// Set necessary state.
		GfxContext.SetRootSignature(m_CubeMapCrossViewSignature);
		GfxContext.SetPipelineState(m_CubeMapCrossViewPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		int Size = std::min(m_GameDesc.Width, m_GameDesc.Height);
		GfxContext.SetViewportAndScissor((m_GameDesc.Width - Size)/2, (m_GameDesc.Height - Size) / 2, Size, Size);

		RenderWindow& renderWindow = RenderWindow::Get();
		FColorBuffer& BackBuffer = renderWindow.GetBackBuffer();

		// Indicate that the back buffer will be used as a render target.
		GfxContext.TransitionResource(m_CubeBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		GfxContext.SetRenderTargets(1, &BackBuffer.GetRTV());

		GfxContext.ClearColor(BackBuffer);

		m_VSConstants.ModelMatrix = FMatrix(); // identity
		m_VSConstants.ViewProjMatrix = FMatrix::MatrixOrthoLH(1.f, 1.f, -1.f, 1.f);
		GfxContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstants), &m_VSConstants);

		m_PSConstants.Exposure = 1.f;
		GfxContext.SetDynamicConstantBufferView(1, sizeof(m_PSConstants), &m_PSConstants);

		GfxContext.SetDynamicDescriptor(2, 0, m_CubeBuffer.GetSRV());

		m_CubeMapCross->Draw(GfxContext);

		GfxContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_PRESENT);
	}


private:
	__declspec(align(16)) struct
	{
		FMatrix ModelMatrix;
		FMatrix ViewProjMatrix;
	} m_VSConstants;

	__declspec(align(16)) struct
	{
		float Exposure;
	} m_PSConstants;

	FRootSignature m_GenCubeSignature;
	FRootSignature m_SkySignature;
	FRootSignature m_CubeMapCrossViewSignature;

	FGraphicsPipelineState m_GenCubePSO;
	FGraphicsPipelineState m_SkyPSO;
	FGraphicsPipelineState m_CubeMapCrossViewPSO;

	FCubeBuffer m_CubeBuffer;
	FTexture m_TextureLongLat;

	ComPtr<ID3DBlob> m_LongLatToCubeVS;
	ComPtr<ID3DBlob> m_ScreenQuadVS;
	ComPtr<ID3DBlob> m_SkyPS, m_SkyVS;
	ComPtr<ID3DBlob> m_CubeMapCrossPS, m_CubeMapCrossVS;
	ComPtr<ID3DBlob> m_LongLatToCubePS;

	FCamera m_Camera;
	FDirectionalLight m_DirectionLight;
	FModel* m_SkyBox, *m_CubeMapCross;

	std::chrono::high_resolution_clock::time_point tStart, tEnd;
};

int main()
{
	ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	GameDesc Desc;
	Desc.Caption = L"Tutorial 9 - IBL Maps Generator";
	Tutorial9 tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	CoUninitialize();
	return 0;
}