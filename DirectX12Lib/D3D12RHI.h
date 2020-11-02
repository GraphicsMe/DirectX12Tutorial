#pragma once

#include <chrono>

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include "Common.h"
#include "MathLib.h"


class WindowWin32;
class CommandQueue;
class D3D12RHI
{
private:
	D3D12RHI() = default;
public:
	~D3D12RHI() = default;

	static D3D12RHI& Get();

public:
	bool Initialize();
	void Destroy();

	ComPtr<ID3D12Device> GetD3D12Device() { return m_device; }
	ComPtr<IDXGIFactory4> GetDXGIFactory() { return m_dxgiFactory; }
	CommandQueue* GetCommandQueue(D3D12_COMMAND_LIST_TYPE type);
	ComPtr<ID3DBlob> CreateShader(const std::wstring& ShaderFile, const std::string& EntryPoint, const std::string& TargetModel);
	void SetResourceBarrier(ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12Resource> resource, 
			D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);;
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flag, uint32_t numDescriptors);

private:
	void EnableDebugLayer();
	ComPtr<IDXGIFactory4> CreateDXGIFactory();
	ComPtr<IDXGIAdapter1> ChooseAdapter(ComPtr<IDXGIFactory4> factory);
	ComPtr<ID3D12Device> CreateDevice(ComPtr<IDXGIAdapter1> adapter);
	
	
private:
	ComPtr<IDXGIFactory4> m_dxgiFactory;
	ComPtr<ID3D12Device> m_device;
	
	CommandQueue* m_copyCommandQueue;
	CommandQueue* m_directCommandQueue;
};

