#include "RenderWindow.h"
#include "D3D12RHI.h"
#include "WindowWin32.h"

#include "d3dx12.h"


RenderWindow& RenderWindow::Get()
{
	static RenderWindow renderWindow;
	return renderWindow;
}

void RenderWindow::Initialize()
{
	D3D12RHI& RHI = D3D12RHI::Get();
	WindowWin32& Window = WindowWin32::Get();
	m_swapChain = CreateSwapChain(Window.GetWindowHandle(), RHI.GetDXGIFactory(), RHI.GetD3D12CommandQueue(), Window.GetWidth(), Window.GetHeight(), BUFFER_COUNT);
	
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	m_renderTargetViewHeap = RHI.CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, BUFFER_COUNT);
	m_rtvDescriptorSize = RHI.GetD3D12Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	CreateRenderTargetViews(m_swapChain, m_renderTargetViewHeap);
}

RenderWindow::~RenderWindow()
{
}


ComPtr<IDXGISwapChain3> RenderWindow::CreateSwapChain(HWND hwnd, ComPtr<IDXGIFactory4> factory, ComPtr<ID3D12CommandQueue> commandQueue, int width, int height, int bufferCount)
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
	ThrowIfFailed(factory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &Desc, nullptr, nullptr, &swapchain1));

	ComPtr<IDXGISwapChain3> swapchain3;
	ThrowIfFailed(swapchain1.As(&swapchain3));
	return swapchain3;
}

void RenderWindow::CreateRenderTargetViews(ComPtr<IDXGISwapChain3> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap)
{
	// create frame resource
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < BUFFER_COUNT; ++i)
	{
		ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])));
		D3D12RHI::Get().GetD3D12Device()->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtvHandle);
		rtvHandle.ptr += m_rtvDescriptorSize;  // offset handle
	}
}

UINT RenderWindow::Present()
{
	m_swapChain->Present(1, 0);
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	return m_frameIndex;
}

ComPtr<ID3D12Resource> RenderWindow::GetBackBuffer()
{
	return m_renderTargets[m_frameIndex];
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderWindow::GetCurrentRenderTargetView()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_renderTargetViewHeap->GetCPUDescriptorHandleForHeapStart(),
        m_frameIndex, m_rtvDescriptorSize);
}