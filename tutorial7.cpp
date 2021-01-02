#include "ApplicationWin32.h"
#include "Game.h"
#include "Common.h"
#include "MathLib.h"
#include "Camera.h"
#include "CommandQueue.h"
#include "D3D12RHI.h"
#include "d3dx12.h"
#include "RenderWindow.h"
#include "MeshData.h"
#include "ObjLoader.h"

#include "DirectXTex.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <chrono>
#include <iostream>


class Tutorial2 : public Game
{
public:
	Tutorial2(const GameDesc& Desc) : Game(Desc) 
	{
		m_viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(Desc.Width), static_cast<float>(Desc.Height), 0.1f);
		m_scissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(Desc.Width), static_cast<LONG>(Desc.Height));
	}

	~Tutorial2() 
	{
		D3D12RHI::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT)->Flush();
	}

	void LoadContent()
	{
		m_device = D3D12RHI::Get().GetD3D12Device();

		m_rootSignature = CreateRootSignature();

		m_cube = FObjLoader::LoadObj("../Resources/Models/primitive/cube.obj");
		m_sphere = FObjLoader::LoadObj("../Resources/Models/primitive/sphere.obj");

		SetupSRVHeap();
		
		std::string basecolor_path = m_cube->GetBaseColorPath(0);
		SetupTexture(ToWideString(basecolor_path));

		CommandQueue* commandQueue = D3D12RHI::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
		ComPtr<ID3D12GraphicsCommandList> commandList = commandQueue->GetCommandList();
		commandList->SetName(L"Copy list");
		
		SetupVertexBuffer(commandList);
		SetupIndexBuffer(commandList);
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

	void SetupVertexBuffer(ComPtr<ID3D12GraphicsCommandList> commandList)
	{
		for (int i = 0; i < VET_Max; ++i)
		{
			VertexElementType elmType = VertexElementType(i);
			if (m_cube->HasVertexElement(elmType))
			{
				UpdateBufferResource(commandList, &m_vertexBuffer[i], &m_intermediateVertexBuffer[i], 
					m_cube->GetVertexCount(), m_cube->GetVertexStride(elmType), m_cube->GetVertexData(elmType));
				m_vertexBufferView[i].BufferLocation = m_vertexBuffer[i]->GetGPUVirtualAddress();
				m_vertexBufferView[i].StrideInBytes = m_cube->GetVertexStride(elmType);
				m_vertexBufferView[i].SizeInBytes = m_cube->GetVertexSize(elmType);
			}
		}
	}

	void SetupIndexBuffer(ComPtr<ID3D12GraphicsCommandList> commandList)
	{
		UpdateBufferResource(commandList,
			&m_indexBuffer, &m_intermediateIndexBuffer,
			m_cube->GetIndexCount(), sizeof(uint32_t), (const void*)m_cube->GetIndexData());

		m_indexBuffer->SetName(L"Index buffer");
		m_intermediateIndexBuffer->SetName(L"Interdediate Index buffer");

		// Initialize the vertex buffer view.
		m_indexBufferView.BufferLocation = m_indexBuffer->GetGPUVirtualAddress();
		m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
		m_indexBufferView.SizeInBytes = m_cube->GetIndexSize();
	}

	void SetupShaders()
	{
		m_vertexShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/triangle.hlsl", "vs_main", "vs_5_1");

		m_pixelShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/triangle.hlsl", "ps_main", "ps_5_1");
	}

	ComPtr<ID3D12RootSignature> CreateRootSignature()
	{
		// create root signature
		CD3DX12_DESCRIPTOR_RANGE1 ranges[1];
		ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

		CD3DX12_ROOT_PARAMETER1 rootParams[2];
		rootParams[0].InitAsConstants(sizeof(m_uboVS) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);
		rootParams[1].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);

		D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.MipLODBias = 0;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
		samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
		samplerDesc.ShaderRegister = 0;
		samplerDesc.RegisterSpace = 0;
		samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
		rootSigDesc.Init_1_1(_countof(rootParams), rootParams, 1, &samplerDesc, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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
		UINT slot = 0;
		std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
		if (m_cube->HasVertexElement(VET_Position))
		{
			inputElements.push_back({ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, slot++, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		}
		if (m_cube->HasVertexElement(VET_Color))
		{
			inputElements.push_back({ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, slot++, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		}
		if (m_cube->HasVertexElement(VET_Texcoord))
		{
			inputElements.push_back({ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, slot++, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		}
		if (m_cube->HasVertexElement(VET_Normal))
		{
			inputElements.push_back({ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, slot++, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 });
		}

		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_rootSignature.Get();
		psoDesc.InputLayout = { &inputElements[0], (UINT)inputElements.size() };
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(m_vertexShader.Get());
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(m_pixelShader.Get());

		// enable alpha blending
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		{
			psoDesc.BlendState.RenderTarget[i].BlendEnable = TRUE;
			psoDesc.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_SRC_ALPHA;
			psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
			psoDesc.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
		}

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

		commandList->SetGraphicsRoot32BitConstants(0, sizeof(m_uboVS) / 4, &m_uboVS, 0);

		ID3D12DescriptorHeap *pDescriptorHeaps[] = { m_srvHeap.Get() };
		commandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

		D3D12_GPU_DESCRIPTOR_HANDLE srvHandle(m_srvHeap->GetGPUDescriptorHandleForHeapStart());
		commandList->SetGraphicsRootDescriptorTable(1, srvHandle);
		
		//commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_textureRes.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		RenderWindow& renderWindow = RenderWindow::Get();
		auto BackBuffer = renderWindow.GetBackBuffer();
		// Indicate that the back buffer will be used as a render target.
		RHI.SetResourceBarrier(commandList, BackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = renderWindow.GetCurrentRenderTargetView();
		D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = renderWindow.GetDepthStencilHandle();
		commandList->OMSetRenderTargets(1, &rtvHandle, true, &dsvHandle);

		// Record commands.
		const float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
		commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
		commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		for (int i = 0, slot = 0; i < VET_Max; ++i)
		{
			if (m_cube->HasVertexElement(VertexElementType(i)))
			{
				commandList->IASetVertexBuffers(slot++, 1, &m_vertexBufferView[i]);
			}
		}
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

	void SetupSRVHeap()
	{
		m_srvHeap = D3D12RHI::Get().CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 1);
		m_srvHeap->SetName(L"Shader Resource View Descriptor Heap");
	}
	
	void SetupTexture(const std::wstring& FileName)
	{
		DirectX::ScratchImage image;
		HRESULT hr;
		if (FileName.rfind(L".dds") != std::string::npos)
		{
			hr = DirectX::LoadFromDDSFile(FileName.c_str(), DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image);
		}
		else if (FileName.rfind(L".tga") != std::string::npos)
		{
			hr = DirectX::LoadFromTGAFile(FileName.c_str(), nullptr, image);
		}
		else if (FileName.rfind(L".hdr") != std::string::npos)
		{
			hr = DirectX::LoadFromHDRFile(FileName.c_str(), nullptr, image);
		}
		else
		{
			hr = DirectX::LoadFromWICFile(FileName.c_str(), DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image);
		}
		ThrowIfFailed(hr);

		ComPtr<ID3D12Resource> intermediateBuffer;
		CommandQueue* commandQueue = D3D12RHI::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
		ComPtr<ID3D12GraphicsCommandList> commandList = commandQueue->GetCommandList();
		
		ThrowIfFailed(DirectX::CreateTextureEx(m_device.Get(), image.GetMetadata(), D3D12_RESOURCE_FLAG_NONE, true, &m_textureRes));

		std::vector<D3D12_SUBRESOURCE_DATA> subresources;
		ThrowIfFailed(PrepareUpload(m_device.Get(), image.GetImages(), image.GetImageCount(), image.GetMetadata(), subresources ));
		const UINT64 uploadBufferSize = GetRequiredIntermediateSize(m_textureRes.Get(), 0, (UINT)subresources.size());

		ThrowIfFailed(m_device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(intermediateBuffer.GetAddressOf())));

		UpdateSubresources(commandList.Get(), m_textureRes.Get(), intermediateBuffer.Get(),
				0, 0, static_cast<unsigned int>(subresources.size()),
				subresources.data());

		commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_textureRes.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

		uint64_t fenceValue = commandQueue->ExecuteCommandList(commandList);
		commandQueue->WaitForFenceValue(fenceValue);

		// Describe and create a SRV for the texture.
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = image.GetMetadata().format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		m_device->CreateShaderResourceView(m_textureRes.Get(), &srvDesc, m_srvHeap->GetCPUDescriptorHandleForHeapStart());
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

	ComPtr<ID3D12Resource> m_intermediateIndexBuffer;

	ComPtr<ID3D12Resource> m_vertexBuffer[VET_Max];
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView[VET_Max];
	ComPtr<ID3D12Resource> m_intermediateVertexBuffer[VET_Max];

	ComPtr<ID3D12Resource> m_indexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

	ComPtr<ID3D12Resource> m_uniformBuffer;
	ComPtr<ID3D12DescriptorHeap> m_uniformBufferHeap;
	UINT8* m_mappedUniformBuffer;

	ComPtr<ID3D12Resource> m_textureRes;
	ComPtr<ID3D12DescriptorHeap> m_srvHeap;

	float m_elapsedTime;
	std::chrono::high_resolution_clock::time_point tStart, tEnd;

	MeshData* m_cube;
	MeshData* m_sphere;
};

int main()
{
	ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	GameDesc Desc;
	Desc.Caption = L"Tutorial 6 - Mesh Loader";
	Tutorial2 tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	CoUninitialize();
	return 0;
}