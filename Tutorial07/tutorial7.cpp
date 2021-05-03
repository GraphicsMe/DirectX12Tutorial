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

#include <d3d12.h>
#include <dxgi1_4.h>
#include <chrono>
#include <iostream>


extern FCommandListManager g_CommandListManager;

enum EShadowMode
{
	SM_Raw,
	SM_PCF,
	SM_VSM
};

const int SHADOW_BUFFER_SIZE = 1024;

class Tutorial7 : public FGame
{
public:
	Tutorial7(const GameDesc& Desc) : FGame(Desc)
	{
	}

	void OnStartup()
	{
		SetupCameraLight();
		SetupRootSignature();
		SetupMesh();
		SetupShaders();
		SetupPipelineState();

		tStart = std::chrono::high_resolution_clock::now();
	}

	void OnShutdown()
	{
	}

	void OnUpdate()
	{
		tEnd = std::chrono::high_resolution_clock::now();
		float delta = std::chrono::duration<float, std::milli>(tEnd - tStart).count();
		tStart = std::chrono::high_resolution_clock::now();

		// Update Uniforms
		//m_rotateRadians += 0.0004f * delta;
		m_rotateRadians = 0.9f;
		m_rotateRadians = fmodf(m_rotateRadians, 2.f * MATH_PI);

		m_Box->SetRotation(FMatrix::RotateY(m_rotateRadians));
		m_Pillar->SetRotation(FMatrix::RotateY(1.f));
	}
	
	void OnRender()
	{
		FCommandContext& CommandContext = FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE_DIRECT, L"3D Queue");

		ShadowPass(CommandContext);
		if (m_ShadowMode == SM_VSM)
		{
			ComputePass(CommandContext);
		}
		ColorPass(CommandContext);
		
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

	void SetupRootSignature()
	{
		
	}

	void SetupMesh()
	{
		m_Box = std::make_unique<FModel>("../Resources/Models/primitive/cube.obj");
		m_Box->SetPosition(1.f, 0.f, 0.f);

		m_Pillar = std::make_unique<FModel>("../Resources/Models/primitive/cube.obj");
		m_Pillar->SetScale(0.4f, 3.f, 0.4f);
		m_Pillar->SetPosition(0.f, 1.f, 1.f);

		m_Floor = std::make_unique<FModel>("../Resources/Models/primitive/quad.obj");
		m_Floor->SetScale(5.f);
		m_Floor->SetRotation(FMatrix::RotateX(MATH_PI * 0.5));
		m_Floor->SetPosition(0.f, -0.5f, 0.f);
	}

	void SetupShaders()
	{
		m_VertexShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/Shadow.hlsl", "vs_main", "vs_5_1");
		m_PixelShaderRaw = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/Shadow.hlsl", "ps_main", "ps_5_1");
		m_PixelShaderPCF = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/ShadowPCF.hlsl", "ps_main", "ps_5_1");
		m_PixelShaderVSM = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/ShadowVSM.hlsl", "ps_main", "ps_5_1");

		m_VSMConvertShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/VSMConvertCS.hlsl", "cs_main", "cs_5_1");
		m_BlurShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/BlurCS.hlsl", "cs_main", "cs_5_1");
		m_BlurHorizontalShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/BlurHorizontalCS.hlsl", "cs_main", "cs_5_1");
		m_BlurVerticalShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/BlurVerticalCS.hlsl", "cs_main", "cs_5_1");
	}

	ID3DBlob* GetPixelShaderByMode(EShadowMode ShadowMode)
	{
		switch (ShadowMode)
		{
		case SM_Raw:
			return m_PixelShaderRaw.Get();
		case SM_PCF:
			return m_PixelShaderPCF.Get();
		case SM_VSM:
			return m_PixelShaderVSM.Get();
		default:
			return nullptr;
		}
	}

	void SetupPipelineState()
	{
		// root signature
		FSamplerDesc DefaultSamplerDesc;
		FSamplerDesc ShadowSamplerDesc;
		ShadowSamplerDesc.SetShadowMapPCFDesc(); //SetShadowMapPCFDesc

		m_RootSignature.Reset(3, 2);
		m_RootSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
		m_RootSignature[1].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
		m_RootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 2, D3D12_SHADER_VISIBILITY_PIXEL);
		m_RootSignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_RootSignature.InitStaticSampler(1, ShadowSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_RootSignature.Finalize(L"RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_PipelineState.SetRootSignature(m_RootSignature);
		m_PipelineState.SetRasterizerState(FPipelineState::RasterizerDefault);
		m_PipelineState.SetBlendState(FPipelineState::BlendDisable);
		m_PipelineState.SetDepthStencilState(FPipelineState::DepthStateReadWrite);
		
		std::vector<D3D12_INPUT_ELEMENT_DESC> MeshLayout;
		m_Box->GetMeshLayout(MeshLayout);
		Assert(MeshLayout.size() > 0);
		m_PipelineState.SetInputLayout((UINT)MeshLayout.size(), &MeshLayout[0]);

		m_PipelineState.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_PipelineState.SetRenderTargetFormats(1, &RenderWindow::Get().GetColorFormat(), RenderWindow::Get().GetDepthFormat());
		m_PipelineState.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_VertexShader.Get()));
		m_PipelineState.SetPixelShader(CD3DX12_SHADER_BYTECODE(GetPixelShaderByMode(m_ShadowMode)));
		m_PipelineState.Finalize();

		m_ShadowPSO.SetRootSignature(m_RootSignature);
		m_ShadowPSO.SetRasterizerState(FPipelineState::RasterizerShadow);
		m_ShadowPSO.SetBlendState(FPipelineState::BlendNoColorWrite);
		m_ShadowPSO.SetDepthStencilState(FPipelineState::DepthStateReadWrite);
		m_ShadowPSO.SetInputLayout((UINT)MeshLayout.size(), &MeshLayout[0]);
		m_ShadowPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_ShadowBuffer.Create(L"Shadow Map", SHADOW_BUFFER_SIZE, SHADOW_BUFFER_SIZE);
		m_ShadowPSO.SetRenderTargetFormats(0, nullptr, m_ShadowBuffer.GetFormat());
		m_ShadowPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_VertexShader.Get()));
		m_ShadowPSO.Finalize();

		m_CSSignature.Reset(2, 0);
		m_CSSignature[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		m_CSSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
		m_CSSignature.Finalize(L"Compute Shader RootSignature");

		m_VSMConvertPSO.SetRootSignature(m_CSSignature);
		m_VSMConvertPSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(m_VSMConvertShader.Get()));
		m_VSMConvertPSO.Finalize();
		m_VSMBuffer.Create(L"VSM Buffer", SHADOW_BUFFER_SIZE, SHADOW_BUFFER_SIZE, 1, DXGI_FORMAT_R16G16_UNORM);

		m_BlurPSO.SetRootSignature(m_CSSignature);
		m_BlurPSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(m_BlurShader.Get()));
		m_BlurPSO.Finalize();

		m_BlurHorizontalPSO.SetRootSignature(m_CSSignature);
		m_BlurHorizontalPSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(m_BlurHorizontalShader.Get()));
		m_BlurHorizontalPSO.Finalize();

		m_BlurVerticalPSO.SetRootSignature(m_CSSignature);
		m_BlurVerticalPSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(m_BlurVerticalShader.Get()));
		m_BlurVerticalPSO.Finalize();

		m_BlurredVSMBuffer.Create(L"Blurred VSM Buffer", SHADOW_BUFFER_SIZE, SHADOW_BUFFER_SIZE, 1, DXGI_FORMAT_R16G16_UNORM);
	}

	void ShadowPass(FCommandContext& CommandContext)
	{
		CommandContext.SetRootSignature(m_RootSignature);
		CommandContext.SetPipelineState(m_ShadowPSO);
		CommandContext.SetViewportAndScissor(0, 0, m_ShadowBuffer.GetWidth(), m_ShadowBuffer.GetHeight());
		CommandContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		m_ShadowBuffer.BeginRendering(CommandContext);

		FBoundingBox WorldBound = m_Box->GetBoundingBox();
		WorldBound.Include(m_Pillar->GetBoundingBox());
		m_DirectionLight.UpdateShadowBound(WorldBound);

		m_VSConstant.ShadowMatrix = FMatrix();
		RenderObjects(CommandContext, m_DirectionLight.GetViewMatrix(), m_DirectionLight.GetProjectMatrix());

		m_VSConstant.ShadowMatrix = m_DirectionLight.GetLightToShadowMatrix();
		CommandContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstant), &m_VSConstant);
		m_ShadowBuffer.EndRendering(CommandContext);
	}

	void RenderObjects(FCommandContext& CommandContext, const FMatrix& ViewMatrix, const FMatrix& ProjectMatrix)
	{
		m_VSConstant.ViewProjection = ViewMatrix * ProjectMatrix;

		m_VSConstant.ModelMatrix = m_Box->GetModelMatrix();
		CommandContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstant), &m_VSConstant);
		m_Box->Draw(CommandContext);

		m_VSConstant.ModelMatrix = m_Pillar->GetModelMatrix();
		CommandContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstant), &m_VSConstant);
		m_Pillar->Draw(CommandContext);

		m_VSConstant.ModelMatrix = m_Floor->GetModelMatrix();
		CommandContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstant), &m_VSConstant);
		m_Floor->Draw(CommandContext);
	}
	
	void ComputePass(FCommandContext& GfxContext)
	{
		// 1. convert shadow map to vsm map
		GfxContext.TransitionResource(m_ShadowBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		g_CommandListManager.GetComputeQueue().WaitForFenceValue(GfxContext.Flush());

		FComputeContext& CommandContext = FComputeContext::Begin(L"Compute Queue");

		CommandContext.SetRootSignature(m_CSSignature);
		CommandContext.SetPipelineState(m_VSMConvertPSO);
		
		CommandContext.TransitionResource(m_VSMBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		CommandContext.SetDynamicDescriptor(0, 0, m_ShadowBuffer.GetDepthSRV());
		CommandContext.SetDynamicDescriptor(1, 0, m_VSMBuffer.GetUAV());

		uint32_t GroupCount = (SHADOW_BUFFER_SIZE + 15) / 16;
		CommandContext.Dispatch(GroupCount, GroupCount, 1);

		// 2. blur vsm map
		const static bool USE_TWO_PASS = false;
		if (USE_TWO_PASS)
		{
			CommandContext.SetPipelineState(m_BlurHorizontalPSO);
			CommandContext.TransitionResource(m_VSMBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			CommandContext.TransitionResource(m_BlurredVSMBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			CommandContext.SetDynamicDescriptor(0, 0, m_VSMBuffer.GetSRV());
			CommandContext.SetDynamicDescriptor(1, 0, m_BlurredVSMBuffer.GetUAV());
			GroupCount = (SHADOW_BUFFER_SIZE + 7) / 8;
			CommandContext.Dispatch(GroupCount, GroupCount, 1);

			CommandContext.SetPipelineState(m_BlurVerticalPSO);
			CommandContext.TransitionResource(m_VSMBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			CommandContext.TransitionResource(m_BlurredVSMBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			CommandContext.SetDynamicDescriptor(0, 0, m_VSMBuffer.GetSRV());
			CommandContext.SetDynamicDescriptor(1, 0, m_BlurredVSMBuffer.GetUAV());
			GroupCount = (SHADOW_BUFFER_SIZE + 7) / 8;
			CommandContext.Dispatch(GroupCount, GroupCount, 1);
		}
		else
		{
			CommandContext.SetPipelineState(m_BlurPSO);
			CommandContext.TransitionResource(m_VSMBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			CommandContext.TransitionResource(m_BlurredVSMBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

			CommandContext.SetDynamicDescriptor(0, 0, m_VSMBuffer.GetSRV());
			CommandContext.SetDynamicDescriptor(1, 0, m_BlurredVSMBuffer.GetUAV());
			GroupCount = (SHADOW_BUFFER_SIZE + 7) / 8;
			CommandContext.Dispatch(GroupCount, GroupCount, 1);
		}
		
		CommandContext.Finish(false);
	}

	void ColorPass(FCommandContext& CommandContext)
	{
		if (m_ShadowMode == SM_VSM)
		{
			g_CommandListManager.GetGraphicsQueue().StallForProducer(g_CommandListManager.GetComputeQueue());
		}
		// Set necessary state.
		CommandContext.SetRootSignature(m_RootSignature);
		CommandContext.SetPipelineState(m_PipelineState);
		CommandContext.SetViewportAndScissor(0, 0, m_GameDesc.Width, m_GameDesc.Height);
		CommandContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		RenderWindow& renderWindow = RenderWindow::Get();
		FColorBuffer& BackBuffer = renderWindow.GetBackBuffer();
		FDepthBuffer& DepthBuffer = renderWindow.GetDepthBuffer();
		// Indicate that the back buffer will be used as a render target.
		CommandContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		CommandContext.TransitionResource(DepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
		if (m_ShadowMode == SM_VSM)
		{
			CommandContext.TransitionResource(m_BlurredVSMBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
		else
		{
			CommandContext.TransitionResource(m_ShadowBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		}
		CommandContext.SetRenderTargets(1, &BackBuffer.GetRTV(), DepthBuffer.GetDSV());

		// Record commands.
		CommandContext.ClearColor(BackBuffer);
		CommandContext.ClearDepth(DepthBuffer);

		m_PSConstant.LightDirection = m_DirectionLight.GetDirection();
		CommandContext.SetDynamicConstantBufferView(1, sizeof(m_PSConstant), &m_PSConstant);
	
		if (m_ShadowMode == SM_VSM)
		{
			CommandContext.SetDynamicDescriptor(2, 1, m_BlurredVSMBuffer.GetSRV());
		}
		else
		{
			CommandContext.SetDynamicDescriptor(2, 1, m_ShadowBuffer.GetDepthSRV());
		}

		RenderObjects(CommandContext, m_Camera.GetViewMatrix(), m_Camera.GetProjectionMatrix());

		CommandContext.TransitionResource(m_BlurredVSMBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		CommandContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_PRESENT);
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

	FRootSignature m_RootSignature;
	FRootSignature m_CSSignature;

	FGraphicsPipelineState m_PipelineState;
	FGraphicsPipelineState m_ShadowPSO;
	FComputePipelineState m_VSMConvertPSO;
	FComputePipelineState m_BlurPSO;
	FComputePipelineState m_BlurHorizontalPSO;
	FComputePipelineState m_BlurVerticalPSO;

	FShadowBuffer m_ShadowBuffer;
	FColorBuffer m_VSMBuffer;
	FColorBuffer m_BlurredVSMBuffer;

	ComPtr<ID3DBlob> m_VertexShader;
	ComPtr<ID3DBlob> m_PixelShaderRaw;
	ComPtr<ID3DBlob> m_PixelShaderPCF;
	ComPtr<ID3DBlob> m_PixelShaderVSM;
	ComPtr<ID3DBlob> m_VSMConvertShader;
	ComPtr<ID3DBlob> m_BlurShader;
	ComPtr<ID3DBlob> m_BlurHorizontalShader;
	ComPtr<ID3DBlob> m_BlurVerticalShader;

	float m_rotateRadians = 0;
	std::chrono::high_resolution_clock::time_point tStart, tEnd;

	std::unique_ptr<FModel> m_Box;
	std::unique_ptr<FModel> m_Pillar;
	std::unique_ptr<FModel> m_Floor;

	FCamera m_Camera;
	FMatrix m_ProjectionMatrix;
	FDirectionalLight m_DirectionLight;

	EShadowMode m_ShadowMode = SM_VSM;
};

int main()
{
	ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	GameDesc Desc;
	Desc.Caption = L"Tutorial 7 - Shadow Mapping";
	Tutorial7 tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	CoUninitialize();
	return 0;
}