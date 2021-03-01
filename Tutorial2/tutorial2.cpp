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

#include <d3d12.h>
#include <dxgi1_4.h>
#include <chrono>
#include <iostream>

extern FCommandListManager g_CommandListManager;


class Tutorial2 : public FGame
{
public:
	Tutorial2(const GameDesc& Desc) : FGame(Desc) 
	{
	}

	void OnStartup()
	{
		m_device = D3D12RHI::Get().GetD3D12Device();

		m_rootSignature.Reset(1, 0);
		m_rootSignature[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 0, 1, D3D12_SHADER_VISIBILITY_VERTEX);
		m_rootSignature.Finalize(L"Tutorial2", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		FCommandContext& CommandContext = FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE_COPY);

		SetupShaders();

		SetupMesh(CommandContext);

		SetupUniformBuffer();
		SetupPiplineState();

		CommandContext.FinishFrame(true);
	}

	void OnUpdate()
	{

	}
	
	void OnRender()
	{
		static int count = 0;
		++count;
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
		m_elapsedTime += 0.001f * time;
		m_elapsedTime = fmodf(m_elapsedTime, 6.283185307179586f);
		m_uboVS.modelMatrix = FMatrix::RotateY(m_elapsedTime);

		FCamera camera(Vector3f(0.f, 0.f, -3.5f), Vector3f(0.f, 0.0f, 0.f), Vector3f(0.f, 1.f, 0.f));
		m_uboVS.viewMatrix = camera.GetViewMatrix();

		const float FovVertical = MATH_PI / 4.f;
		m_uboVS.projectionMatrix = FMatrix::MatrixPerspectiveFovLH(FovVertical, (float)GetDesc().Width / GetDesc().Height, 0.1f, 100.f);

		//ThrowIfFailed(m_uniformBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedUniformBuffer)));
		memcpy(m_mappedUniformBuffer, &m_uboVS, sizeof(m_uboVS));
		//m_uniformBuffer->Unmap(0, nullptr);

		FillCommandLists(CommandContext, CommandContext.GetCommandList());
		
		CommandContext.FinishFrame(true);

		RenderWindow::Get().Present();	
	}

private:
	void SetupMesh(FCommandContext& CommandContext)
	{
		struct Vertex
		{
			float position[3];
			float color[3];
		};
		/*                 2
						  / \
						 /	 \
						1-----0
		*/

		Vertex vertexBufferData[3] =
		{
		  { { 1.0f,  -1.0f, 0.0f },{ 1.0f, 0.0f, 0.0f } },
		  { { -1.0f, -1.0f, 0.0f },{ 0.0f, 1.0f, 0.0f } },
		  { { 0.0f,  1.0f,  0.0f },{ 0.0f, 0.0f, 1.0f } }
		};

		const UINT VertexBufferSize = sizeof(vertexBufferData);
		m_VertexBuffer.Create(L"VertexBuffer", _countof(vertexBufferData), sizeof(Vertex), vertexBufferData);

		uint32_t indexBufferData[3] = { 0, 1, 2 };
		const UINT indexBufferSize = sizeof(indexBufferData);
		m_IndexBuffer.Create(L"IndexBuffer", _countof(indexBufferData), sizeof(uint32_t), indexBufferData);
	}

	void SetupShaders()
	{
		m_vertexShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/triangle.vert", "main", "vs_5_0");

		m_pixelShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/triangle.frag", "main", "ps_5_0");
	}

	void SetupPiplineState()
	{
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
		  { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		  { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_rootSignature.GetSignature();
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_pixelShader.Get());

		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		//two sided
		psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

		// disable depth stencil
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;

		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
	}

	void FillCommandLists(FCommandContext& CommandContext, ComPtr<ID3D12GraphicsCommandList> commandList)
	{
		D3D12RHI& RHI = D3D12RHI::Get();
		commandList->SetPipelineState(m_pipelineState.Get());

		// Set necessary state.
		CommandContext.SetRootSignature(m_rootSignature);
		CommandContext.SetViewportAndScissor(0, 0, m_GameDesc.Width, m_GameDesc.Height);

		ID3D12DescriptorHeap *pDescriptorHeaps[] = { m_uniformBufferHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

		D3D12_GPU_DESCRIPTOR_HANDLE srvHandle(m_uniformBufferHeap->GetGPUDescriptorHandleForHeapStart());
		commandList->SetGraphicsRootDescriptorTable(0, srvHandle);

		RenderWindow& renderWindow = RenderWindow::Get();
		auto BackBuffer = renderWindow.GetBackBuffer2();
		FDepthBuffer& DepthBuffer = renderWindow.GetDepthBuffer();
		// Indicate that the back buffer will be used as a render target.
		CommandContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		CommandContext.TransitionResource(DepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
		CommandContext.SetRenderTargets(1, &BackBuffer.GetRTV());

		// Record commands.
		CommandContext.ClearColor(BackBuffer);
		CommandContext.ClearDepth(DepthBuffer);
		CommandContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		CommandContext.SetVertexBuffer(0, m_VertexBuffer.VertexBufferView());
		CommandContext.SetIndexBuffer(m_IndexBuffer.IndexBufferView());

		CommandContext.DrawIndexed(m_IndexBuffer.GetElementCount());

		CommandContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_PRESENT, true);
	}

	void SetupUniformBuffer()
	{
		int AlignedSize = (sizeof(m_uboVS) + 255) & ~255;    // CB size is required to be 256-byte aligned.
		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(AlignedSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_uniformBuffer)));

		m_uniformBufferHeap = D3D12RHI::Get().CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 1);
		m_uniformBufferHeap->SetName(L"Constant Buffer Upload Resource Heap");

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_uniformBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = AlignedSize;

		D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle(m_uniformBufferHeap->GetCPUDescriptorHandleForHeapStart());
		m_device->CreateConstantBufferView(&cbvDesc, cbvHandle);

		// We do not intend to read from this resource on the CPU. (End is less than or equal to begin)
		D3D12_RANGE readRange;
		readRange.Begin = 0;
		readRange.End = 0;
		ThrowIfFailed(m_uniformBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedUniformBuffer)));
		memcpy(m_mappedUniformBuffer, &m_uboVS, sizeof(m_uboVS));
		//m_uniformBuffer->Unmap(0, &readRange);
	}

private:
	struct
	{
		FMatrix projectionMatrix;
		FMatrix modelMatrix;
		FMatrix viewMatrix;
	} m_uboVS;

	ComPtr<ID3D12Device> m_device;

	FRootSignature m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;

	ComPtr<ID3DBlob> m_vertexShader;
	ComPtr<ID3DBlob> m_pixelShader;

	FGpuBuffer m_VertexBuffer;
	FGpuBuffer m_IndexBuffer;

	ComPtr<ID3D12Resource> m_uniformBuffer;
	ComPtr<ID3D12DescriptorHeap> m_uniformBufferHeap;
	UINT8* m_mappedUniformBuffer;

	float m_elapsedTime;
	std::chrono::high_resolution_clock::time_point tStart, tEnd;
};

int main()
{
	GameDesc Desc;
	Desc.Caption = L"Tutorial 2 - Draw Triangle";
	Tutorial2 tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	return 0;
}