#include "ApplicationWin32.h"
#include "Game.h"
#include "Common.h"
#include "MathLib.h"
#include "Camera.h"
#include "CommandQueue.h"
#include "D3D12RHI.h"
#include "d3dx12.h"
#include "RenderWindow.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <chrono>
#include <iostream>


class Tutorial3 : public Game
{
public:
	Tutorial3(const GameDesc& Desc) : Game(Desc) 
	{
		m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(Desc.Width), static_cast<float>(Desc.Height), 0.1f);
		m_scissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(Desc.Width), static_cast<LONG>(Desc.Height));
	}

	void LoadContent()
	{
		m_device = D3D12RHI::Get().GetD3D12Device();

		m_rootSignature = CreateRootSignature();

		CommandQueue* commandQueue = D3D12RHI::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
		ComPtr<ID3D12GraphicsCommandList> commandList = commandQueue->GetCommandList();
		commandList->SetName(L"Copy list");

		ComPtr<ID3D12Resource> intermediateVertexBuffer;
		ComPtr<ID3D12Resource> intermediateIndexBuffer;

		SetupVertexBuffer(commandList, intermediateVertexBuffer);
		SetupIndexBuffer(commandList, intermediateIndexBuffer);
		//SetupUniformBuffer();
		SetupShaders();
		SetupPiplineState();

		uint64_t fenceValue = commandQueue->ExecuteCommandList(commandList);
		commandQueue->WaitForFenceValue(fenceValue);
	}

	void OnUpdate()
	{

	}
	
	void OnRender()
	{
		D3D12RHI& RHI = D3D12RHI::Get();
		CommandQueue* commandQueue = RHI.GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
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

		//ThrowIfFailed(m_uniformBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedUniformBuffer)));
		//memcpy(m_mappedUniformBuffer, &m_uboVS, sizeof(m_uboVS));
		//m_uniformBuffer->Unmap(0, nullptr);

		ComPtr<ID3D12GraphicsCommandList> commandList = commandQueue->GetCommandList();

		FillCommandLists(commandList);
		
		uint64_t fenceValue = commandQueue->ExecuteCommandList(commandList);

		RenderWindow::Get().Present(fenceValue, commandQueue);	
	}

private:
	void UpdateBufferResource(ComPtr<ID3D12GraphicsCommandList> commandList,
        ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource,
        size_t numElements, size_t elementSize, const void* bufferData, 
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE )
	{
		size_t bufferSize = numElements * elementSize;

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags),
			D3D12_RESOURCE_STATE_COMMON, // D3D12_RESOURCE_STATE_COPY_DEST will cause error
			nullptr,
			IID_PPV_ARGS(pDestinationResource)));

		if (bufferData)
		{
			ThrowIfFailed(m_device->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(pIntermediateResource)));

			D3D12_SUBRESOURCE_DATA subresourceData = {};
			subresourceData.pData = bufferData;
			subresourceData.RowPitch = bufferSize;
			subresourceData.SlicePitch = subresourceData.RowPitch;

			UpdateSubresources(commandList.Get(), 
				*pDestinationResource, *pIntermediateResource,
				0, 0, 1, &subresourceData);
		}
	}

	void SetupVertexBuffer(ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12Resource>& intermediateBuffer)
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

		const UINT VertexBufferSize = sizeof(vertexBufferData);

		UpdateBufferResource(commandList,
			&m_vertexBuffer, &intermediateBuffer,
			_countof(vertexBufferData), sizeof(Vertex), vertexBufferData);

		m_vertexBuffer->SetName(L"Vertex buffer");
		intermediateBuffer->SetName(L"Intermediate Vertex buffer");

		// Initialize the vertex buffer view.
		m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
		m_vertexBufferView.StrideInBytes = sizeof(Vertex);
		m_vertexBufferView.SizeInBytes = VertexBufferSize;
	}

	void SetupIndexBuffer(ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12Resource>& intermediateBuffer)
	{
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

		const UINT indexBufferSize = sizeof(indexBufferData);

		UpdateBufferResource(commandList,
			&m_indexBuffer, &intermediateBuffer,
			_countof(indexBufferData), sizeof(uint32_t), indexBufferData);

		m_indexBuffer->SetName(L"Index buffer");
		intermediateBuffer->SetName(L"Interdediate Index buffer");

		// Initialize the vertex buffer view.
		m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		m_indexBufferView.SizeInBytes = indexBufferSize;
	}

	void SetupShaders()
	{
		m_vertexShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/triangle.vert", "main", "vs_5_0");

		m_pixelShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/triangle.frag", "main", "ps_5_0");
	}

	ComPtr<ID3D12RootSignature> CreateRootSignature()
	{
		// create root signature
		//D3D12_DESCRIPTOR_RANGE1 ranges[1];
		//ranges[0].BaseShaderRegister = 0;
		//ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		//ranges[0].NumDescriptors = 1;
		//ranges[0].RegisterSpace = 0;
		//ranges[0].OffsetInDescriptorsFromTableStart = 0;
		//ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

		//D3D12_ROOT_PARAMETER1 rootParams[1];
		//rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		//rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
		//rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
		//rootParams[0].DescriptorTable.pDescriptorRanges = ranges;

		//D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc = {};
		//rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		//rootSigDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		//rootSigDesc.Desc_1_1.NumParameters = 1;
		//rootSigDesc.Desc_1_1.pParameters = rootParams;
		//rootSigDesc.Desc_1_1.NumStaticSamplers = 0;
		//rootSigDesc.Desc_1_1.pStaticSamplers = nullptr;

		CD3DX12_ROOT_PARAMETER1 rootParams[1];
		rootParams[0].InitAsConstants(sizeof(m_uboVS) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

		D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
		rootSigDesc.Init_1_1(_countof(rootParams), rootParams, 0, nullptr, rootSignatureFlags);

		D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
		if (FAILED(m_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		{
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}
		ComPtr<ID3D12RootSignature> RootSignature;
		ComPtr<ID3DBlob> signatureBlob;
		ComPtr<ID3DBlob> errorBlob;
		ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSigDesc, featureData.HighestVersion, &signatureBlob, &errorBlob));
		ThrowIfFailed(m_device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), 
			signatureBlob->GetBufferSize(), IID_PPV_ARGS(&RootSignature)));

		return RootSignature;
	}

	void SetupPiplineState()
	{
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
		  { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		  { "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};


		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_pixelShader.Get());

		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);

		psoDesc.SampleMask = UINT_MAX;
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		psoDesc.NumRenderTargets = 1;
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
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

		//ID3D12DescriptorHeap *pDescriptorHeaps[] = { m_uniformBufferHeap.Get() };
		//commandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

		//D3D12_GPU_DESCRIPTOR_HANDLE srvHandle(m_uniformBufferHeap->GetGPUDescriptorHandleForHeapStart());
		//commandList->SetGraphicsRootDescriptorTable(0, srvHandle);

		commandList->SetGraphicsRoot32BitConstants(0, sizeof(m_uboVS) / 4, &m_uboVS, 0);

		RenderWindow& renderWindow = RenderWindow::Get();
		auto BackBuffer = renderWindow.GetBackBuffer();
		// Indicate that the back buffer will be used as a render target.
		RHI.SetResourceBarrier(commandList, BackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = renderWindow.GetCurrentBackBufferView();
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = renderWindow.GetDepthStencilHandle();
		commandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

		// Record commands.
		const float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
		commandList->IASetIndexBuffer(&m_indexBufferView);

		commandList->DrawIndexedInstanced(m_indexBufferView.SizeInBytes / sizeof(uint32_t), 1, 0, 0, 0);

		RHI.SetResourceBarrier(commandList, BackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
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
	Desc.Caption = L"Tutorial 3 - Draw Cube";
	Tutorial3 tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	return 0;
}