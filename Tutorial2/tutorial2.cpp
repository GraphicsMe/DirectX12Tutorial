#include "ApplicationWin32.h"
#include "Game.h"
#include "Common.h"
#include "MathLib.h"
#include "Camera.h"
#include "CommandQueue.h"
#include "D3D12RHI.h"
#include "d3dx12.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <chrono>

class Tutorial2 : public Game
{
public:
	Tutorial2(const GameDesc& Desc) : Game(Desc) 
	{
		m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(Desc.Width), static_cast<float>(Desc.Height), 0.1f, 1000.f);
		m_scissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(Desc.Width), static_cast<LONG>(Desc.Height));
	}

	~Tutorial2() 
	{
		D3D12RHI::Get().GetCommandQueue()->Flush();
	}

	void LoadContent()
	{
		m_device = D3D12RHI::Get().GetD3D12Device();

		m_rootSignature = D3D12RHI::Get().CreateRootSignature();

		SetupVertexBuffer();
		SetupIndexBuffer();
		SetupUniformBuffer();
		SetupShaders();
		SetupPiplineState();
	}

	void OnUpdate()
	{

	}
	
	void OnRender()
	{
		D3D12RHI& RHI = D3D12RHI::Get();
		CommandQueue* commandQueue = RHI.GetCommandQueue();
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
		m_uboVS.projectionMatrix = FMatrix::MatrixPerspectiveFovLH(FovVertical, (float)GetDesc().Width / GetDesc().Height, 0.01f, 1000);

		ThrowIfFailed(m_uniformBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedUniformBuffer)));
		memcpy(m_mappedUniformBuffer, &m_uboVS, sizeof(m_uboVS));
		m_uniformBuffer->Unmap(0, nullptr);

		ComPtr<ID3D12GraphicsCommandList> commandList = commandQueue->GetCommandList();

		FillCommandLists(commandList);
		commandQueue->ExecuteCommandList(commandList);
		
		RHI.Present();

		//commandQueue->Flush();
	}

private:
	void SetupVertexBuffer()
	{
		struct Vertex
		{
			float position[3];
			float color[3];
		};

		Vertex vertexBufferData[3] =
		{
		  { { 1.0f,  -1.0f, 0.0f },{ 1.0f, 0.0f, 0.0f } },
		  { { -1.0f,  -1.0f, 0.0f },{ 0.0f, 1.0f, 0.0f } },
		  { { 0.0f, 1.0f, 0.0f },{ 0.0f, 0.0f, 1.0f } }
		};

		const UINT VertexBufferSize = sizeof(vertexBufferData);

		D3D12_HEAP_PROPERTIES heapProps;
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC vertexBufferResourceDesc;
		vertexBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		vertexBufferResourceDesc.Alignment = 0;
		vertexBufferResourceDesc.Width = VertexBufferSize;
		vertexBufferResourceDesc.Height = 1;
		vertexBufferResourceDesc.DepthOrArraySize = 1;
		vertexBufferResourceDesc.MipLevels = 1;
		vertexBufferResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		vertexBufferResourceDesc.SampleDesc.Count = 1;
		vertexBufferResourceDesc.SampleDesc.Quality = 0;
		vertexBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		vertexBufferResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&vertexBufferResourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_vertexBuffer)));

		// Copy the triangle data to the vertex buffer.
		UINT8* pVertexDataBegin;
		ThrowIfFailed(m_vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, vertexBufferData, sizeof(vertexBufferData));
		m_vertexBuffer->Unmap(0, nullptr);

		// Initialize the vertex buffer view.
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(Vertex);
		m_vertexBufferView.SizeInBytes = VertexBufferSize;
	}

	void SetupIndexBuffer()
	{
		uint32_t indexBufferData[3] = { 0, 1, 2 };

		const UINT indexBufferSize = sizeof(indexBufferData);

		D3D12_HEAP_PROPERTIES heapProps;
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC vertexBufferResourceDesc;
		vertexBufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		vertexBufferResourceDesc.Alignment = 0;
		vertexBufferResourceDesc.Width = indexBufferSize;
		vertexBufferResourceDesc.Height = 1;
		vertexBufferResourceDesc.DepthOrArraySize = 1;
		vertexBufferResourceDesc.MipLevels = 1;
		vertexBufferResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		vertexBufferResourceDesc.SampleDesc.Count = 1;
		vertexBufferResourceDesc.SampleDesc.Quality = 0;
		vertexBufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		vertexBufferResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&vertexBufferResourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_indexBuffer)));

		// Copy data to DirectX 12 driver memory:
		UINT8* pVertexDataBegin;
		ThrowIfFailed(m_indexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pVertexDataBegin)));
		memcpy(pVertexDataBegin, indexBufferData, sizeof(indexBufferData));
		m_indexBuffer->Unmap(0, nullptr);

		// Initialize the vertex buffer view.
		m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		m_indexBufferView.SizeInBytes = indexBufferSize;
	}

	void SetupShaders()
	{
		m_vertexShader = D3D12RHI::Get().CreateShader(L"../Resources/triangle.vert", "main", "vs_5_0");

		m_pixelShader = D3D12RHI::Get().CreateShader(L"../Resources/triangle.frag", "main", "ps_5_0");
	}

	void SetupPiplineState()
	{
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
		  { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		  { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};


		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.VS = { m_vertexShader->GetBufferPointer(), m_vertexShader->GetBufferSize() };
		psoDesc.PS = { m_pixelShader->GetBufferPointer(), m_pixelShader->GetBufferSize() };

		D3D12_RASTERIZER_DESC rasterDesc;
		rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
		rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
		rasterDesc.FrontCounterClockwise = FALSE;
		rasterDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
		rasterDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
		rasterDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
		rasterDesc.DepthClipEnable = TRUE;
		rasterDesc.MultisampleEnable = FALSE;
		rasterDesc.AntialiasedLineEnable = FALSE;
		rasterDesc.ForcedSampleCount = 0;
		rasterDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

		psoDesc.RasterizerState = rasterDesc;

		D3D12_BLEND_DESC blendDesc;
		blendDesc.AlphaToCoverageEnable = FALSE;
		blendDesc.IndependentBlendEnable = FALSE;
		const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
		{
			FALSE,FALSE,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
			D3D12_LOGIC_OP_NOOP,
			D3D12_COLOR_WRITE_ENABLE_ALL,
		};
		for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
			blendDesc.RenderTarget[i] = defaultRenderTargetBlendDesc;

		psoDesc.BlendState = blendDesc;
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
	}

	void FillCommandLists(ComPtr<ID3D12GraphicsCommandList> commandList)
	{
		D3D12RHI& RHI = D3D12RHI::Get();
		commandList->SetPipelineState(m_pipelineState.Get());

		// Set necessary state.
		commandList->SetGraphicsRootSignature(m_rootSignature.Get());
		commandList->RSSetViewports(1, &m_viewport);
		commandList->RSSetScissorRects(1, &m_scissorRect);

		ID3D12DescriptorHeap *pDescriptorHeaps[] = { m_uniformBufferHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

		D3D12_GPU_DESCRIPTOR_HANDLE srvHandle(m_uniformBufferHeap->GetGPUDescriptorHandleForHeapStart());
		commandList->SetGraphicsRootDescriptorTable(0, srvHandle);

		auto BackBuffer = RHI.GetBackBuffer();
		// Indicate that the back buffer will be used as a render target.
		RHI.SetResourceBarrier(commandList, BackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = RHI.GetCurrentRenderTargetView();

		commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

		// Record commands.
		const float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
		commandList->IASetIndexBuffer(&m_indexBufferView);

		commandList->DrawInstanced(3, 1, 0, 0);

		RHI.SetResourceBarrier(commandList, BackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		//ThrowIfFailed(commandList->Close());
	}

	void SetupUniformBuffer()
	{
		D3D12_HEAP_PROPERTIES heapProps;
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProps.CreationNodeMask = 1;
		heapProps.VisibleNodeMask = 1;
	
		D3D12_RESOURCE_DESC uboResourceDesc;
		uboResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		uboResourceDesc.Alignment = 0;
		uboResourceDesc.Width = (sizeof(m_uboVS) + 255) & ~255;
		uboResourceDesc.Height = 1;
		uboResourceDesc.DepthOrArraySize = 1;
		uboResourceDesc.MipLevels = 1;
		uboResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
		uboResourceDesc.SampleDesc.Count = 1;
		uboResourceDesc.SampleDesc.Quality = 0;
		uboResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		uboResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&uboResourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_uniformBuffer)));

		m_uniformBufferHeap = D3D12RHI::Get().CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 1);
		m_uniformBufferHeap->SetName(L"Constant Buffer Upload Resource Heap");

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = m_uniformBuffer->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(m_uboVS) + 255) & ~255;    // CB size is required to be 256-byte aligned.

		D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle(m_uniformBufferHeap->GetCPUDescriptorHandleForHeapStart());
		cbvHandle.ptr = cbvHandle.ptr + m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 0;

		m_device->CreateConstantBufferView(&cbvDesc, cbvHandle);

		// We do not intend to read from this resource on the CPU. (End is less than or equal to begin)
		D3D12_RANGE readRange;
		readRange.Begin = 0;
		readRange.End = 0;

		ThrowIfFailed(m_uniformBuffer->Map(0, &readRange, reinterpret_cast<void**>(&m_mappedUniformBuffer)));
		memcpy(m_mappedUniformBuffer, &m_uboVS, sizeof(m_uboVS));
		m_uniformBuffer->Unmap(0, &readRange);
	}

private:
	struct
	{
		FMatrix projectionMatrix;
		FMatrix modelMatrix;
		FMatrix viewMatrix;
	} m_uboVS;

	D3D12_VIEWPORT m_viewport;
	D3D12_RECT m_scissorRect;

	ComPtr<ID3D12Device> m_device;

	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;

	ComPtr<ID3DBlob> m_vertexShader;
	ComPtr<ID3DBlob> m_pixelShader;

	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	ComPtr<ID3D12Resource> m_indexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

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