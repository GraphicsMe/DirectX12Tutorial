#include <chrono>
#include <iostream>
#include <D3Dcompiler.h>
#include <dxgidebug.h>

#include "D3D12RHI.h"
#include "Common.h"
#include "WindowWin32.h"
#include "Camera.h"
#include "CommandQueue.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace std;
//using namespace std::chrono;

D3D12RHI::D3D12RHI(WindowWin32* Window)
	: m_rootSignature(nullptr)
	, m_vertexShader(nullptr)
	, m_pixelShader(nullptr)
{
	Initialize(Window);
}


D3D12RHI::~D3D12RHI()
{
	if (m_commandQueue)
	{
		delete m_commandQueue;
		m_commandQueue = nullptr;
	}

//#ifdef _DEBUG
//	ComPtr<IDXGIDebug1> dxgiDebug;
//	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
//	{
//		dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
//	}
//#endif
}

ComPtr<IDXGIFactory4> D3D12RHI::CreateDXGIFactory()
{
	UINT dxgiFactoryFlags = 0;
#ifdef _DEBUG
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	//ThrowIfFailed(debugInterface->QueryInterface(IID_PPV_ARGS(&debugController1)));
	ComPtr<ID3D12Debug1> debugController1;
	ThrowIfFailed(debugInterface.As(&debugController1));
	debugController1->EnableDebugLayer();
	debugController1->SetEnableGPUBasedValidation(true);

	dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	ComPtr<IDXGIFactory4> dxgiFactory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));
	return dxgiFactory;
}

ComPtr<IDXGIAdapter1> D3D12RHI::GetAdapter(ComPtr<IDXGIFactory4> factory)
{
	ComPtr<IDXGIAdapter1> adapter;
	int BestAdapterIndex = -1;
	SIZE_T MaxGPUMemory = 0;
	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, adapter.GetAddressOf()); ++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)))
		{
			float sizeGB = 1 << 30;
			std::wcout << desc.Description << ": " << desc.DedicatedVideoMemory / sizeGB << "G" << std::endl;
			if (desc.DedicatedVideoMemory > MaxGPUMemory)
			{
				BestAdapterIndex = adapterIndex;
				MaxGPUMemory = desc.DedicatedVideoMemory;
			}
			adapter->Release();
		}
	}

	if(BestAdapterIndex < 0)
	{
		return nullptr;
	}

	HRESULT hr = factory->EnumAdapters1(BestAdapterIndex, &adapter);
	Assert(hr == S_OK);
	return adapter;
}

ComPtr<ID3D12Device> D3D12RHI::CreateDevice(ComPtr<IDXGIAdapter1> adapter)
{
	ComPtr<ID3D12Device> device;
	ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)));
	device->SetName(L"D3D12 RHI Device");
#ifdef _DEBUG
	ComPtr<ID3D12InfoQueue> InfoQueue;
	if (SUCCEEDED(device.As(&InfoQueue)))
	{
		InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);
	}
#endif
	return device;
}

ComPtr<IDXGISwapChain3> D3D12RHI::CreateSwapChain(HWND hwnd, ComPtr<IDXGIFactory4> factory, int width, int height, int bufferCount)
{
	DXGI_SWAP_CHAIN_DESC1 Desc = {};
	Desc.Width = width;
	Desc.Height = height;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.Stereo = FALSE;
	Desc.SampleDesc = {1, 0}; // count, quality
	Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Desc.BufferCount = bufferCount;
	Desc.Scaling = DXGI_SCALING_STRETCH;
	Desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	Desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	ComPtr<IDXGISwapChain1> swapchain1;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(m_commandQueue->GetD3D12CommandQueue().Get(), hwnd, &Desc, nullptr, nullptr, &swapchain1));

	ComPtr<IDXGISwapChain3> swapchain3;
	ThrowIfFailed(swapchain1.As(&swapchain3));
	return swapchain3;
}

ComPtr<ID3D12DescriptorHeap> D3D12RHI::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flag, uint32_t numDescriptors)
{
	// create render target view
	D3D12_DESCRIPTOR_HEAP_DESC Desc = {};
	Desc.NumDescriptors = numDescriptors;
	Desc.Type = type;
	Desc.Flags = flag;

	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&descriptorHeap)));

	return descriptorHeap;
}

void D3D12RHI::UpdateRenderTargetViews(ComPtr<IDXGISwapChain3> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap)
{
	// create frame resource
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

	UINT rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	for (int i = 0; i < backBufferCount; ++i)
	{
		ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
		m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
		rtvHandle.ptr += rtvDescriptorSize;  // offset handle
	}
}

bool D3D12RHI::Initialize(WindowWin32* Window)
{
	int width = Window->GetWidth();
	int height = Window->GetHeight();

	ComPtr<IDXGIFactory4> dxgiFactory = CreateDXGIFactory();
	ComPtr<IDXGIAdapter1> dxgiAdapter = GetAdapter(dxgiFactory);
	
	// 1. create device
	m_device = CreateDevice(dxgiAdapter);

	// 2. create command queue
	//m_commandQueue = CreateCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	m_commandQueue = new CommandQueue(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	
	// 3. create swap chain
	m_swapChain = CreateSwapChain(Window->GetWindowHandle(), dxgiFactory, width, height, backBufferCount);
	
	InitializeViewport(width, height);

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	m_renderTargetViewHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, backBufferCount);
	m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	UpdateRenderTargetViews(m_swapChain, m_renderTargetViewHeap);

	// Create Command Allocator
	ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

	m_rootSignature = CreateRootSignature();

	SetupVertexBuffer();
	SetupIndexBuffer();
	SetupUniformBuffer();
	SetupShaders();
	SetupPiplineState();

	return true;
}

ComPtr<ID3D12RootSignature> D3D12RHI::CreateRootSignature()
{
	// create root signature
	D3D12_DESCRIPTOR_RANGE1 ranges[1];
	ranges[0].BaseShaderRegister = 0;
	ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	ranges[0].NumDescriptors = 1;
	ranges[0].RegisterSpace = 0;
	ranges[0].OffsetInDescriptorsFromTableStart = 0;
	ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

	D3D12_ROOT_PARAMETER1 rootParams[1];
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParams[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParams[0].DescriptorTable.pDescriptorRanges = ranges;

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	rootSigDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSigDesc.Desc_1_1.NumStaticSamplers = 0;
	rootSigDesc.Desc_1_1.pParameters = rootParams;
	rootSigDesc.Desc_1_1.NumParameters = 1;
	rootSigDesc.Desc_1_1.pStaticSamplers = nullptr;

	ComPtr<ID3D12RootSignature> RootSignature;
	ID3DBlob* signature;
	ID3DBlob* error;
	try
	{
		ThrowIfFailed(D3D12SerializeVersionedRootSignature(&rootSigDesc, &signature, &error));
		ThrowIfFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&RootSignature)));
		RootSignature->SetName(L"My Root Signature");
	}
	catch (std::exception e)
	{
		const char* errstr = (const char*)error->GetBufferPointer();
		std::cout << errstr << std::endl;
		error->Release();
		error = nullptr;
	}
	if (signature)
	{
		signature->Release();
		signature = nullptr;
	}
	return RootSignature;
}

void D3D12RHI::InitializeViewport(int width, int height)
{
	m_scissorRect = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};

	m_viewport = 
	{
		0.f,
		0.f,
		static_cast<float>(width),
		static_cast<float>(height),
		0.1f,
		1000.f
	};
}

void D3D12RHI::SetupVertexBuffer()
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

void D3D12RHI::SetupIndexBuffer()
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

void D3D12RHI::SetupUniformBuffer()
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

	m_uniformBufferHeap = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 1);
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

void D3D12RHI::SetupShaders()
{
	m_vertexShader = CreateShader(L"../Resources/triangle.vert", "main", "vs_5_0");

	m_pixelShader = CreateShader(L"../Resources/triangle.frag", "main", "ps_5_0");
}

ComPtr<ID3DBlob> D3D12RHI::CreateShader(const std::wstring& ShaderFile, const std::string& EntryPoint, const std::string& TargetModel)
{
	// 👋 Declare handles
	ID3DBlob* errors = nullptr;

#ifdef _DEBUG
	// Enable better shader debugging with the graphics debugging tools.
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif

	ComPtr<ID3DBlob> ShaderBlob;
	if(!SUCCEEDED(D3DCompileFromFile(ShaderFile.c_str(), nullptr, nullptr, EntryPoint.c_str(), TargetModel.c_str(), compileFlags, 0, ShaderBlob.GetAddressOf(), &errors)))
	{
		const char* errStr = (const char*)errors->GetBufferPointer();
		std::cout << errStr << std::endl;
		errors->Release();
		errors = nullptr;
		Assert(0 && errStr);
	}
	return ShaderBlob;
}

void D3D12RHI::SetupPiplineState()
{
	// 👋 Declare handles

	// Define the vertex input layout.
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
	try
	{
		ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
	}
	catch (std::exception e)
	{
		std::cout << "Failed to create Graphics Pipeline!";
	}
}

void D3D12RHI::FillCommandLists(ComPtr<ID3D12GraphicsCommandList> commandList)
{
	//ThrowIfFailed(m_commandAllocator->Reset());

	//ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

	commandList->SetPipelineState(m_pipelineState.Get());

	// Set necessary state.
	commandList->SetGraphicsRootSignature(m_rootSignature.Get());
	commandList->RSSetViewports(1, &m_viewport);
	commandList->RSSetScissorRects(1, &m_scissorRect);

	ID3D12DescriptorHeap *pDescriptorHeaps[] = { m_uniformBufferHeap.Get() };
	commandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

	D3D12_GPU_DESCRIPTOR_HANDLE srvHandle(m_uniformBufferHeap->GetGPUDescriptorHandleForHeapStart());
	commandList->SetGraphicsRootDescriptorTable(0, srvHandle);

	// Indicate that the back buffer will be used as a render target.
	SetResourceBarrier(commandList, m_renderTargets[m_frameIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart());
	rtvHandle.ptr = rtvHandle.ptr + (m_frameIndex * m_rtvDescriptorSize);
	commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	// Record commands.
	const float clearColor[] = { 0.2f, 0.2f, 0.2f, 1.0f };
	commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
	commandList->IASetIndexBuffer(&m_indexBufferView);

	commandList->DrawInstanced(3, 1, 0, 0);

	SetResourceBarrier(commandList, m_renderTargets[m_frameIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

	//ThrowIfFailed(commandList->Close());
}

void D3D12RHI::SetResourceBarrier(ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12Resource> resource, 
	D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
	D3D12_RESOURCE_BARRIER BarrierDesc;
	BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	BarrierDesc.Transition.pResource = resource.Get();
	BarrierDesc.Transition.StateBefore = stateBefore;
	BarrierDesc.Transition.StateAfter = stateAfter;
	BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &BarrierDesc);
}

void D3D12RHI::Render()
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

	FCamera camera(Vector3f(0.f, 0.f, -3.5f), Vector3f(0.f, 0.0f, 0.f), Vector3f(0.f, 1.f, 0.f));
	m_uboVS.viewMatrix = camera.GetViewMatrix();

	const float FovVertical = MATH_PI / 4.f;
	m_uboVS.projectionMatrix = FMatrix::MatrixPerspectiveFovLH(FovVertical, m_viewport.Width / m_viewport.Height, 0.01f, 1000);

	ThrowIfFailed(m_uniformBuffer->Map(0, nullptr, reinterpret_cast<void**>(&m_mappedUniformBuffer)));
	memcpy(m_mappedUniformBuffer, &m_uboVS, sizeof(m_uboVS));
	m_uniformBuffer->Unmap(0, nullptr);

	ComPtr<ID3D12GraphicsCommandList> commandList = m_commandQueue->GetCommandList();

	FillCommandLists(commandList);
	m_commandQueue->ExecuteCommandList(commandList);
	m_swapChain->Present(1, 0);

	//m_myCommandQueue->Flush();

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}