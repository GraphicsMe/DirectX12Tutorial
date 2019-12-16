#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include "Common.h"

class RenderWindow
{
private:
	RenderWindow() = default;
public:
	static RenderWindow& Get();
	void Initialize();
	~RenderWindow();

	// swap chain & present
	UINT Present();
	ComPtr<ID3D12Resource> GetBackBuffer();
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRenderTargetView();

private:
	ComPtr<IDXGISwapChain3> CreateSwapChain(HWND hwnd, ComPtr<IDXGIFactory4> factory, ComPtr<ID3D12CommandQueue> commandQueue, int width, int height, int bufferCount);
	void CreateRenderTargetViews(ComPtr<IDXGISwapChain3> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap);

private:
	//ComPtr<ID3D12Device> m_d3d12Device;
	
	static const UINT BUFFER_COUNT = 2;
	//int m_bufferCount;
	UINT m_frameIndex;
	UINT m_rtvDescriptorSize;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12DescriptorHeap> m_renderTargetViewHeap;
	ComPtr<ID3D12Resource> m_renderTargets[BUFFER_COUNT];
};

