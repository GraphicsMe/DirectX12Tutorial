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
#include "Texture.h"
#include "SamplerManager.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <chrono>
#include <iostream>


class Tutorial5 : public FGame
{
public:
	Tutorial5(const GameDesc& Desc) : FGame(Desc) 
	{
	}

	void OnStartup()
	{
		SetupRootSignature();

		m_Texture.LoadFromFile(L"../Resources/Textures/Foliage.dds");
		SetupShaders();
		SetupMesh();
		SetupPipelineState();
	}

	void OnUpdate()
	{

	}
	
	void OnRender()
	{
		FCommandContext& CommandContext = FCommandContext::Begin();

		// Frame limit set to 60 fps
		tEnd = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::milli>(tEnd - tStart).count();
		if (time < (1000.0f / 60.0f))
		{
			return;
		}
		tStart = std::chrono::high_resolution_clock::now();

		// Update Uniforms
		//m_elapsedTime += 0.001f * time;
		m_elapsedTime = fmodf(m_elapsedTime, 6.283185307179586f);
		m_uboVS.modelMatrix = FMatrix::RotateY(m_elapsedTime);

		FCamera camera(Vector3f(0.f, 0.f, -5.f), Vector3f(0.f, 0.0f, 0.f), Vector3f(0.f, 1.f, 0.f));
		m_uboVS.viewMatrix = camera.GetViewMatrix();

		const float FovVertical = MATH_PI / 4.f;
		m_uboVS.projectionMatrix = FMatrix::MatrixPerspectiveFovLH(FovVertical, (float)GetDesc().Width / GetDesc().Height, 0.1f, 100.f);

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
		m_RootSignature.Finalize(L"Tutorial4", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	}

	void SetupMesh()
	{
		struct VertexPos
		{
			float position[3];
		};
		struct VertexCol
		{
			float color[3];
		};
		struct VertexTex
		{
			float tex[2];
		};
		/*
			0-------1
			|		|
			|		|
			3-------2
		*/

		VertexPos vertexPosData[] =
		{
			{ -1.f,  1.f, -1.f },
			{  1.f,  1.f, -1.f },
			{  1.f, -1.f, -1.f },
			{ -1.f, -1.f, -1.f },
		};
		VertexCol vertexColData[] = 
		{
			{ 1.f, 0.f, 0.f },
			{ 0.f, 1.f, 0.f },
			{ 0.f, 0.f, 1.f },
			{ 1.f, 1.f, 0.f },
		};
		VertexTex vertexTexData[] = 
		{
			{0.f, 0.f},
			{1.f, 0.f},
			{1.f, 1.f},
			{0.f, 1.f},
		};

		m_VertexPositionBuffer.Create(L"VertexPositionBuffer", _countof(vertexPosData), sizeof(VertexPos), vertexPosData);
		m_VertexColorBuffer.Create(L"VertexColorBuffer", _countof(vertexColData), sizeof(VertexCol), vertexColData);
		m_VertexUVBuffer.Create(L"VertexUvBuffer", _countof(vertexTexData), sizeof(VertexTex), vertexTexData);

		uint32_t indexBufferData[] = { 
			0, 1, 2,
			0, 2, 3,
		};
		m_IndexBuffer.Create(L"IndexBuffer", _countof(indexBufferData), sizeof(uint32_t), indexBufferData);
	}

	void SetupShaders()
	{
		m_vertexShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/triangle.hlsl", "vs_main", "vs_5_1");

		m_pixelShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/triangle.hlsl", "ps_main", "ps_5_1");
	}

	void SetupPipelineState()
	{
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
		  { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		  { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		  { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		m_PipelineState.SetRootSignature(m_RootSignature);
		m_PipelineState.SetRasterizerState(FPipelineState::RasterizerDefault);
		m_PipelineState.SetBlendState(FPipelineState::BlendTraditional);
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

		CommandContext.SetDynamicDescriptor(1, 0, m_Texture.GetSRV());

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
		D3D12_VERTEX_BUFFER_VIEW VertexViews[] = { m_VertexPositionBuffer.VertexBufferView(), m_VertexColorBuffer.VertexBufferView(), m_VertexUVBuffer.VertexBufferView() };
		CommandContext.SetVertexBuffers(0, 3, VertexViews);
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

	FGpuBuffer m_VertexPositionBuffer;
	FGpuBuffer m_VertexColorBuffer;
	FGpuBuffer m_VertexUVBuffer;
	FGpuBuffer m_IndexBuffer;

	FTexture m_Texture;

	float m_elapsedTime = 0;
	std::chrono::high_resolution_clock::time_point tStart, tEnd;
};

int main()
{
	GameDesc Desc;
	Desc.Caption = L"Tutorial 5 - Multiple vertex stream";
	Tutorial5 tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	return 0;
}