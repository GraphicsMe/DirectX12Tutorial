#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include "Common.h"


class CommandQueue;
class RenderWindow
{
private:
	RenderWindow() = default;
	~RenderWindow() = default;

public:
	static RenderWindow& Get();
	void Initialize();
	
	void Destroy();

	// swap chain & present
	UINT Present(uint64_t currentFenceValue, CommandQueue* commandQueue);
	ComPtr<ID3D12Resource> GetBackBuffer();
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentBackBufferView();
	D3D12_CPU_DESCRIPTOR_HANDLE GetDepthStencilHandle();

private:
	ComPtr<IDXGISwapChain3> CreateSwapChain(HWND hwnd, IDXGIFactory4* factory, ID3D12CommandQueue* commandQueue, int width, int height, int bufferCount);
	void CreateRenderTargetViews(ComPtr<IDXGISwapChain3> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap);
	void CreateDepthView(ComPtr<ID3D12DescriptorHeap> descriptorHeap);

private:
	//ComPtr<ID3D12Device> m_d3d12Device;
	
	static const UINT BUFFER_COUNT = 2;
	UINT m_frameIndex;
	uint64_t m_fenceValues[BUFFER_COUNT];

	UINT m_rtvDescriptorSize;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	ComPtr<ID3D12Resource> m_backBuffers[BUFFER_COUNT];
	ComPtr<ID3D12DescriptorHeap> m_dsvHeap;
	ComPtr<ID3D12Resource> m_depthBuffer;
};

