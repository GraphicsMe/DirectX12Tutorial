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
#include "Texture.h"
#include "SamplerManager.h"
#include "Model.h"
#include "ShadowBuffer.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <chrono>
#include <iostream>


class Tutorial7 : public FGame
{
public:
	Tutorial7(const GameDesc& Desc) : FGame(Desc)
	{
	}

	void OnStartup()
	{
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
		// Frame limit set to 60 fps
		tEnd = std::chrono::high_resolution_clock::now();
		float delta = std::chrono::duration<float, std::milli>(tEnd - tStart).count();
		tStart = std::chrono::high_resolution_clock::now();

		// Update Uniforms
		m_rotateRadians += 0.0004f * delta;
		m_rotateRadians = fmodf(m_rotateRadians, 2.f * MATH_PI);

		m_Box->SetRotation(FMatrix::RotateY(m_rotateRadians));

		FCamera camera(Vector3f(0.f, 1.f, -5.f), Vector3f(0.f, 0.0f, 0.f), Vector3f(0.f, 1.f, 0.f));
		m_uboVS.viewMatrix = camera.GetViewMatrix();

		const float FovVertical = MATH_PI / 4.f;
		m_uboVS.projectionMatrix = FMatrix::MatrixPerspectiveFovLH(FovVertical, (float)GetDesc().Width / GetDesc().Height, 0.1f, 100.f);
	}
	
	void OnRender()
	{
		FCommandContext& CommandContext = FCommandContext::Begin();
		FillCommandLists(CommandContext);
		
		CommandContext.Finish(true);

		RenderWindow::Get().Present();
	}

private:
	void SetupRootSignature()
	{
		FSamplerDesc DefaultSamplerDesc;

		m_RootSignature.Reset(2, 1);
		m_RootSignature[0].InitAsConstants(0, sizeof(m_uboVS) / 4, D3D12_SHADER_VISIBILITY_VERTEX);
		m_RootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_PIXEL);
		m_RootSignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_RootSignature.Finalize(L"RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	}

	void SetupMesh()
	{
		m_Box = std::make_unique<FModel>("../Resources/Models/primitive/cube.obj");

		m_Floor = std::make_unique<FModel>("../Resources/Models/primitive/quad.obj");
		m_Floor->SetScale(5.f);
		m_Floor->SetRotation(FMatrix::RotateX(MATH_PI * 0.5));
		m_Floor->SetPosition(0.f, -0.5f, 0.f);
	}

	void SetupShaders()
	{
		m_vertexShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/triangle.hlsl", "vs_main", "vs_5_1");

		m_pixelShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/triangle.hlsl", "ps_main", "ps_5_1");
	}

	void SetupPipelineState()
	{
		m_PipelineState.SetRootSignature(m_RootSignature);
		m_PipelineState.SetRasterizerState(FPipelineState::RasterizerDefault);
		m_PipelineState.SetBlendState(FPipelineState::BlendTraditional);
		m_PipelineState.SetDepthStencilState(FPipelineState::DepthStateReadWrite);
		
		std::vector<D3D12_INPUT_ELEMENT_DESC> MeshLayout;
		m_Box->GetMeshLayout(MeshLayout);
		Assert(MeshLayout.size() > 0);
		m_PipelineState.SetInputLayout((UINT)MeshLayout.size(), &MeshLayout[0]);

		m_PipelineState.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_PipelineState.SetRenderTargetFormats(1, &RenderWindow::Get().GetColorFormat(), RenderWindow::Get().GetDepthFormat());
		m_PipelineState.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_vertexShader.Get()));
		m_PipelineState.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_pixelShader.Get()));
		m_PipelineState.Finalize();

		m_ShadowPSO.SetRootSignature(m_RootSignature);
		m_ShadowPSO.SetRasterizerState(FPipelineState::RasterizerShadow);
		m_ShadowPSO.SetBlendState(FPipelineState::BlendNoColorWrite);
		m_ShadowPSO.SetDepthStencilState(FPipelineState::DepthStateReadWrite);
		m_ShadowPSO.SetInputLayout((UINT)MeshLayout.size(), &MeshLayout[0]);
		m_ShadowPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_ShadowBuffer.Create(L"Shadow Map", 2048, 2048);
		m_ShadowPSO.SetRenderTargetFormats(0, nullptr, m_ShadowBuffer.GetFormat());
		m_ShadowPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_vertexShader.Get()));
		m_ShadowPSO.Finalize();
	}

	void ShadowPass(FCommandContext& CommandContext)
	{
		CommandContext.SetRootSignature(m_RootSignature);
		CommandContext.SetPipelineState(m_ShadowPSO);
	}

	void RenderObjects(FCommandContext& CommandContext)
	{
		m_uboVS.modelMatrix = m_Box->GetModelMatrix();
		CommandContext.SetConstantArray(0, sizeof(m_uboVS) / 4, &m_uboVS);
		m_Box->Draw(CommandContext);

		m_uboVS.modelMatrix = m_Floor->GetModelMatrix();
		CommandContext.SetConstantArray(0, sizeof(m_uboVS) / 4, &m_uboVS);
		m_Floor->Draw(CommandContext);
	}

	void FillCommandLists(FCommandContext& CommandContext)
	{
		// Set necessary state.
		CommandContext.SetRootSignature(m_RootSignature);
		CommandContext.SetPipelineState(m_PipelineState);
		CommandContext.SetViewportAndScissor(0, 0, m_GameDesc.Width, m_GameDesc.Height);

		RenderWindow& renderWindow = RenderWindow::Get();
		FColorBuffer& BackBuffer = renderWindow.GetBackBuffer();
		FDepthBuffer& DepthBuffer = renderWindow.GetDepthBuffer();
		// Indicate that the back buffer will be used as a render target.
		CommandContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		CommandContext.TransitionResource(DepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
		CommandContext.SetRenderTargets(1, &BackBuffer.GetRTV(), DepthBuffer.GetDSV());

		// Record commands.
		CommandContext.ClearColor(BackBuffer);
		CommandContext.ClearDepth(DepthBuffer);
		CommandContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		RenderObjects(CommandContext);

		CommandContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_PRESENT, true);
	}

private:
	struct
	{
		FMatrix projectionMatrix;
		FMatrix modelMatrix;
		FMatrix viewMatrix;
	} m_uboVS;

	FRootSignature m_RootSignature;
	FGraphicsPipelineState m_PipelineState;
	FGraphicsPipelineState m_ShadowPSO;

	FShadowBuffer m_ShadowBuffer;

	ComPtr<ID3DBlob> m_vertexShader;
	ComPtr<ID3DBlob> m_pixelShader;
	ComPtr<ID3DBlob> m_ShadowVertexShader;

	float m_rotateRadians = 0;
	std::chrono::high_resolution_clock::time_point tStart, tEnd;

	std::unique_ptr<FModel> m_Box;
	std::unique_ptr<FModel> m_Floor;
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