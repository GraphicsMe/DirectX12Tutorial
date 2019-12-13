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

	CommandQueue* GetCommandQueue() { return m_commandQueue; }
	ComPtr<ID3D12Device> GetD3D12Device() { return m_device; }
	ComPtr<ID3DBlob> CreateShader(const std::wstring& ShaderFile, const std::string& EntryPoint, const std::string& TargetModel);
	ComPtr<ID3D12RootSignature> CreateRootSignature();
	void SetResourceBarrier(ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12Resource> resource, 
			D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);;
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flag, uint32_t numDescriptors);

	// swap chain & present
	UINT Present();
	ComPtr<ID3D12Resource> GetBackBuffer();
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView();

private:

	ComPtr<IDXGIFactory4> CreateDXGIFactory();
	ComPtr<IDXGIAdapter1> GetAdapter(ComPtr<IDXGIFactory4> factory);
	ComPtr<ID3D12Device> CreateDevice(ComPtr<IDXGIAdapter1> adapter);
	ComPtr<IDXGISwapChain3> CreateSwapChain(HWND hwnd, ComPtr<IDXGIFactory4> factory, int width, int height, int bufferCount);
	
	void CreateRenderTargetViews(ComPtr<IDXGISwapChain3> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap);
	
private:
	ComPtr<ID3D12Device> m_device;
	UINT m_frameIndex;
	UINT m_rtvDescriptorSize;
	ComPtr<IDXGISwapChain3> m_swapChain;
	
	CommandQueue* m_commandQueue;
	
	static const UINT backBufferCount = 2;
	ComPtr<ID3D12DescriptorHeap> m_renderTargetViewHeap;
	ComPtr<ID3D12Resource> m_renderTargets[backBufferCount];
};

