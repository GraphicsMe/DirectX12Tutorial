#include "RenderWindow.h"
#include "D3D12RHI.h"
#include "WindowWin32.h"
#include "d3dx12.h"
#include "CommandQueue.h"
#include "CommandListManager.h"

const int MSAA_SAMPLE = 1;

extern FCommandListManager g_CommandListManager;


RenderWindow& RenderWindow::Get()
{
	static RenderWindow renderWindow;
	return renderWindow;
}

void RenderWindow::Initialize()
{
	D3D12RHI& RHI = D3D12RHI::Get();
	WindowWin32& Window = WindowWin32::Get();
	m_swapChain.Reset();
	m_swapChain = CreateSwapChain(Window.GetWindowHandle(), RHI.GetDXGIFactory(), g_CommandListManager.GetGraphicsQueue().GetD3D12CommandQueue(), Window.GetWidth(), Window.GetHeight(), BUFFER_COUNT);
	
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	for (int i = 0; i < BUFFER_COUNT; ++i)
	{
		ComPtr<ID3D12Resource> BackBuffer;
		ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&BackBuffer)));
		m_BackBuffers[i].CreateFromSwapChain(L"BackBuffer", BackBuffer.Detach());
	}

	m_dsvHeap = RHI.CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 1);
	CreateDepthView(m_dsvHeap);
}

void RenderWindow::Destroy()
{

}

ComPtr<IDXGISwapChain3> RenderWindow::CreateSwapChain(HWND hwnd, IDXGIFactory4* factory, ID3D12CommandQueue* commandQueue, int width, int height, int bufferCount)
{
	DXGI_SWAP_CHAIN_DESC1 Desc = {};
	Desc.Width = width;
	Desc.Height = height;
	Desc.BufferCount = bufferCount;
	Desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	Desc.SampleDesc.Count = 1;
	Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	ComPtr<IDXGISwapChain1> swapchain1;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(commandQueue, hwnd, &Desc, nullptr, nullptr, &swapchain1));

	ComPtr<IDXGISwapChain3> swapchain3;
	ThrowIfFailed(swapchain1.As(&swapchain3));
	return swapchain3;
}

void RenderWindow::CreateDepthView(ComPtr<ID3D12DescriptorHeap> descriptorHeap)
{
	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	optimizedClearValue.DepthStencil = { 1.0f, 0 };
 
	ThrowIfFailed(D3D12RHI::Get().GetD3D12Device()->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, WindowWin32::Get().GetWidth(), WindowWin32::Get().GetHeight(),
			1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL),
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&optimizedClearValue,
		IID_PPV_ARGS(&m_depthBuffer)
	));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
	dsv.Format = DXGI_FORMAT_D32_FLOAT;
	dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv.Texture2D.MipSlice = 0;
	dsv.Flags = D3D12_DSV_FLAG_NONE;
 
	D3D12RHI::Get().GetD3D12Device()->CreateDepthStencilView(m_depthBuffer.Get(), &dsv, descriptorHeap->GetCPUDescriptorHandleForHeapStart());
}

UINT RenderWindow::Present()
{
	m_swapChain->Present(1, 0);
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	return m_frameIndex;
}

ComPtr<ID3D12Resource> RenderWindow::GetBackBuffer()
{
	return m_BackBuffers[m_frameIndex].GetResource();
}

FColorBuffer& RenderWindow::GetBackBuffer2()
{
	return m_BackBuffers[m_frameIndex];
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderWindow::GetCurrentBackBufferView()
{
	return m_BackBuffers[m_frameIndex].GetRTV();
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderWindow::GetDepthStencilHandle()
{
	return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}
