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

#include <d3d12.h>
#include <dxgi1_4.h>
#include <chrono>
#include <iostream>

extern FCommandListManager g_CommandListManager;

class Tutorial3 : public FGame
{
public:
	Tutorial3(const GameDesc& Desc) : FGame(Desc) 
	{
	}

	void OnStartup()
	{
		SetupRootSignature();

		SetupShaders();

		SetupMesh();
		
		SetupPipelineState();

	}

	void OnUpdate()
	{

	}
	
	void OnRender()
	{
		// Frame limit set to 60 fps
		tEnd = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::milli>(tEnd - tStart).count();
		if (time < (1000.0f / 60.0f))
		{
			return;
		}
		tStart = std::chrono::high_resolution_clock::now();

		// Update Uniforms
		m_elapsedTime += 0.001f * time;
		m_elapsedTime = fmodf(m_elapsedTime, 6.283185307179586f);
		m_uboVS.modelMatrix = FMatrix::RotateY(m_elapsedTime);

		FCamera camera(Vector3f(0.f, 0.f, -5.f), Vector3f(0.f, 0.0f, 0.f), Vector3f(0.f, 1.f, 0.f));
		m_uboVS.viewMatrix = camera.GetViewMatrix();

		const float FovVertical = MATH_PI / 4.f;
		m_uboVS.projectionMatrix = FMatrix::MatrixPerspectiveFovLH(FovVertical, (float)GetDesc().Width / GetDesc().Height, 0.1f, 100.f);

		FCommandContext& CommandContext = FCommandContext::Begin();
		FillCommandLists(CommandContext);
		
		CommandContext.Finish(true);

		RenderWindow::Get().Present();	
	}

private:
	void SetupRootSignature()
	{
		m_RootSignature.Reset(1, 0);
		m_RootSignature[0].InitAsConstants(0, sizeof(m_uboVS) / 4, D3D12_SHADER_VISIBILITY_VERTEX);
		m_RootSignature.Finalize(L"Tutorial3", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	}

	void SetupMesh()
	{
		struct Vertex
		{
			float position[3];
			float color[3];
		};
		/*
						  4--------5
						 /| 	  /|
						/ |	     / |
						0-------1  |
						| 7-----|--6
						|/		| /
						3-------2
		*/

		Vertex vertexBufferData[] =
		{
			{ { -1.f,  1.f, -1.f }, { 1.f, 0.f, 0.f } }, // 0
			{ {  1.f,  1.f, -1.f }, { 0.f, 1.f, 0.f } }, // 1
			{ {  1.f, -1.f, -1.f }, { 0.f, 0.f, 1.f } }, // 2
			{ { -1.f, -1.f, -1.f }, { 1.f, 1.f, 0.f } }, // 3
			{ { -1.f,  1.f,  1.f }, { 1.f, 0.f, 1.f } }, // 4
			{ {  1.f,  1.f,  1.f }, { 0.f, 1.f, 1.f } }, // 5
			{ {  1.f, -1.f,  1.f }, { 1.f, 1.f, 1.f } }, // 6
			{ { -1.f, -1.f,  1.f }, { 0.f, 0.f, 0.f } }  // 7
		};

		m_VertexBuffer.Create(L"VertexBuffer", _countof(vertexBufferData), sizeof(Vertex), vertexBufferData);
		uint32_t indexBufferData[] = { 
			0, 1, 2,
			0, 2, 3,
			2, 1, 5,
			2, 5, 6,
			4, 0, 3,
			4, 3, 7,
			3, 2, 6,
			3, 6, 7,
			0, 4, 5,
			0, 5, 1,
			5, 4, 7,
			5, 7, 6
		};

		m_IndexBuffer.Create(L"IndexBuffer", _countof(indexBufferData), sizeof(uint32_t), indexBufferData);
	}

	void SetupShaders()
	{
		m_vertexShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/triangle.vert", "main", "vs_5_0");

		m_pixelShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/triangle.frag", "main", "ps_5_0");
	}

	void SetupPipelineState()
	{
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
		  { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		  { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		m_PipelineState.SetRootSignature(m_RootSignature);
		m_PipelineState.SetRasterizerState(FPipelineState::RasterizerDefault);
		m_PipelineState.SetBlendState(FPipelineState::BlendDisable);
		m_PipelineState.SetDepthStencilState(FPipelineState::DepthStateReadWrite);
		m_PipelineState.SetInputLayout(_countof(inputElementDescs), inputElementDescs);
		m_PipelineState.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_PipelineState.SetRenderTargetFormats(1, &RenderWindow::Get().GetColorFormat(), RenderWindow::Get().GetDepthFormat());
		m_PipelineState.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_vertexShader.Get()));
		m_PipelineState.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_pixelShader.Get()));
		m_PipelineState.Finalize();
	}

	void FillCommandLists(FCommandContext& CommandContext)
	{
		// Set necessary state.
		CommandContext.SetRootSignature(m_RootSignature);
		CommandContext.SetPipelineState(m_PipelineState);
		CommandContext.SetViewportAndScissor(0, 0, m_GameDesc.Width, m_GameDesc.Height);

		CommandContext.SetConstantArray(0, sizeof(m_uboVS) / 4, &m_uboVS);

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
		CommandContext.SetVertexBuffer(0, m_VertexBuffer.VertexBufferView());
		CommandContext.SetIndexBuffer(m_IndexBuffer.IndexBufferView());

		CommandContext.DrawIndexed(m_IndexBuffer.GetElementCount());

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

	ComPtr<ID3DBlob> m_vertexShader;
	ComPtr<ID3DBlob> m_pixelShader;

	FGpuBuffer m_VertexBuffer;
	FGpuBuffer m_IndexBuffer;

	float m_elapsedTime = 0;
	std::chrono::high_resolution_clock::time_point tStart, tEnd;
};

int main()
{
	GameDesc Desc;
	Desc.Caption = L"Tutorial 3 - Draw Cube";
	Tutorial3 tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	return 0;
}