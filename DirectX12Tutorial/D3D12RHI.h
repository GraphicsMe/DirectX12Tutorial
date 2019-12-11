#pragma once

#include <chrono>

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include "Common.h"
#include "MathLib.h"


class WindowWin32;
class D3D12RHI
{
public:
	D3D12RHI(WindowWin32* Window);
	virtual ~D3D12RHI();

	void Render();

private:
	ComPtr<IDXGIFactory4> CreateDXGIFactory();
	ComPtr<IDXGIAdapter1> GetAdapter(ComPtr<IDXGIFactory4> factory);
	ComPtr<ID3D12Device> CreateDevice(ComPtr<IDXGIAdapter1> adapter);
	ComPtr<ID3D12CommandQueue> CreateCommandQueue(D3D12_COMMAND_LIST_TYPE type);
	ComPtr<IDXGISwapChain3> CreateSwapChain(HWND hwnd, ComPtr<IDXGIFactory4> factory, int width, int height, int bufferCount);
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors);
	void UpdateRenderTargetViews(ComPtr<IDXGISwapChain3> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap);
	ComPtr<ID3D12GraphicsCommandList> CreateCommandList(D3D12_COMMAND_LIST_TYPE type, ComPtr<ID3D12CommandAllocator> commandAllocator);
	
	HANDLE CreateEventHandle();
	uint64_t Signal(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue);
	void WaitForFenceValue(ComPtr<ID3D12Fence> fence, uint64_t fenceValue, HANDLE fenceEvent );
	void Flush(ComPtr<ID3D12CommandQueue> commandQueue, ComPtr<ID3D12Fence> fence, uint64_t& fenceValue, HANDLE fenceEvent);

	bool Initialize(WindowWin32* Window);
	
	ComPtr<ID3D12RootSignature> CreateRootSignature();
	void InitializeViewport(int width, int height);
	void SetupVertexBuffer();
	void SetupIndexBuffer();
	void SetupUniformBuffer();
	void SetupShaders();
	ComPtr<ID3DBlob> CreateShader(const std::wstring& ShaderFile, const std::string& EntryPoint, const std::string& TargetModel);
	void SetupPiplineState();
	
	void FillCommandLists();
	void SetResourceBarrier(ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12Resource> resource, 
			D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);;

protected:
	struct
	{
		FMatrix projectionMatrix;
		FMatrix modelMatrix;
		FMatrix viewMatrix;
	} uboVS;

private:
	ComPtr<ID3D12Device> m_device;
	UINT m_frameIndex;
	UINT m_rtvDescriptorSize;
	ComPtr<IDXGISwapChain3> m_swapChain;
	ComPtr<ID3D12Resource> m_uniformBuffer;
	ComPtr<ID3D12CommandQueue> m_commandQueue;
	ComPtr<ID3DBlob> m_vertexShader;
	ComPtr<ID3DBlob> m_pixelShader;
	ComPtr<ID3D12RootSignature> m_rootSignature;
	UINT8* m_mappedUniformBuffer;
	ComPtr<ID3D12CommandAllocator> m_commandAllocator;
	ComPtr<ID3D12DescriptorHeap> m_uniformBufferHeap;
	ComPtr<ID3D12GraphicsCommandList> m_commandList;
	ComPtr<ID3D12PipelineState> m_pipelineState;

	static const UINT backBufferCount = 2;
	ComPtr<ID3D12DescriptorHeap> m_renderTargetViewHeap;
	ComPtr<ID3D12Resource> m_renderTargets[backBufferCount];

	ComPtr<ID3D12Resource> m_vertexBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

	ID3D12Resource* m_indexBuffer;
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;

	D3D12_RECT m_scissorRect;
	D3D12_VIEWPORT m_viewport;

	HANDLE m_fenceEvent;
	ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue;

	float m_elapsedTime;
	std::chrono::high_resolution_clock::time_point tStart, tEnd;
};

