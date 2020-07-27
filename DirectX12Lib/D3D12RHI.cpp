#include <chrono>
#include <iostream>
#include <D3Dcompiler.h>
#include <dxgidebug.h>
#include "d3dx12.h"

#include "D3D12RHI.h"
#include "Common.h"
#include "WindowWin32.h"
#include "Camera.h"
#include "CommandQueue.h"
#include "RenderWindow.h"

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace std;
//using namespace std::chrono;


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

ComPtr<IDXGIAdapter1> D3D12RHI::ChooseAdapter(ComPtr<IDXGIFactory4> factory)
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

		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr)))
		{
			float sizeGB = 1 << 30;
			std::wcout << "***Adapter: " << desc.Description << ": " << desc.DedicatedVideoMemory / sizeGB << "G" << std::endl;
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



D3D12RHI& D3D12RHI::Get()
{
	static D3D12RHI Singleton;
	return Singleton;
}

bool D3D12RHI::Initialize()
{
	// 0. create dxgi factory
	m_dxgiFactory = CreateDXGIFactory();
	ComPtr<IDXGIAdapter1> dxgiAdapter = ChooseAdapter(m_dxgiFactory);
	
	// 1. create device
	m_device = CreateDevice(dxgiAdapter);

	// 2. create command queue
	m_copyCommandQueue = new CommandQueue(m_device, D3D12_COMMAND_LIST_TYPE_COPY);
	m_directCommandQueue = new CommandQueue(m_device, D3D12_COMMAND_LIST_TYPE_DIRECT);

	// 3. create render window(swapchain)
	RenderWindow::Get().Initialize(m_directCommandQueue->GetD3D12CommandQueue());

	return true;
}

void D3D12RHI::Destroy()
{
	if (m_copyCommandQueue)
	{
		m_copyCommandQueue->Flush();
		delete m_copyCommandQueue;
		m_copyCommandQueue = nullptr;
	}
	if (m_directCommandQueue)
	{
		m_directCommandQueue->Flush();
		delete m_directCommandQueue;
		m_directCommandQueue = nullptr;
	}
}

CommandQueue* D3D12RHI::GetCommandQueue(D3D12_COMMAND_LIST_TYPE type)
{
	switch(type)
	{
	case D3D12_COMMAND_LIST_TYPE_COPY:
		return m_copyCommandQueue;
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		return m_directCommandQueue;
	default:
		return nullptr;
	}
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