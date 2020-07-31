#include "RenderWindow.h"
#include "D3D12RHI.h"
#include "WindowWin32.h"
#include "d3dx12.h"
#include "CommandQueue.h"

const int MSAA_SAMPLE = 1;

RenderWindow& RenderWindow::Get()
{
	static RenderWindow renderWindow;
	return renderWindow;
}

void RenderWindow::Initialize(ComPtr<ID3D12CommandQueue> commandQueue)
{
	D3D12RHI& RHI = D3D12RHI::Get();
	WindowWin32& Window = WindowWin32::Get();
	m_swapChain.Reset();
	m_swapChain = CreateSwapChain(Window.GetWindowHandle(), RHI.GetDXGIFactory(), commandQueue, Window.GetWidth(), Window.GetHeight(), BUFFER_COUNT);
	
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	m_rtvHeap = RHI.CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, BUFFER_COUNT);
	m_rtvDescriptorSize = RHI.GetD3D12Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CreateRenderTargetViews(m_swapChain, m_rtvHeap);

	m_dsvHeap = RHI.CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 1);
	CreateDepthView(m_dsvHeap);
}

void RenderWindow::Destroy()
{

}

ComPtr<IDXGISwapChain3> RenderWindow::CreateSwapChain(HWND hwnd, ComPtr<IDXGIFactory4> factory, ComPtr<ID3D12CommandQueue> commandQueue, int width, int height, int bufferCount)
{
	DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS QualityLevels;
	QualityLevels.Format = BackBufferFormat;
	QualityLevels.SampleCount = MSAA_SAMPLE;
	QualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	QualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(D3D12RHI::Get().GetD3D12Device()->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
		&QualityLevels,
		sizeof(QualityLevels)));

	DXGI_SWAP_CHAIN_DESC1 Desc = {};
	Desc.Width = width;
	Desc.Height = height;
	Desc.Format = BackBufferFormat;
	Desc.Stereo = FALSE;
	Desc.SampleDesc.Count = 1;
	Desc.SampleDesc.Quality = 0;
	Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Desc.BufferCount = bufferCount;
	Desc.Scaling = DXGI_SCALING_STRETCH;
	Desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	Desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	Desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	ComPtr<IDXGISwapChain1> swapchain1;
	ThrowIfFailed(factory->CreateSwapChainForHwnd(commandQueue.Get(), hwnd, &Desc, nullptr, nullptr, &swapchain1));

	ComPtr<IDXGISwapChain3> swapchain3;
	ThrowIfFailed(swapchain1.As(&swapchain3));
	return swapchain3;
}

void RenderWindow::CreateRenderTargetViews(ComPtr<IDXGISwapChain3> swapChain, ComPtr<ID3D12DescriptorHeap> descriptorHeap)
{
	// create frame resource
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart());

	for (int i = 0; i < BUFFER_COUNT; ++i)
	{
		ThrowIfFailed(swapChain->GetBuffer(i, IID_PPV_ARGS(&m_backBuffers[i])));
		D3D12RHI::Get().GetD3D12Device()->CreateRenderTargetView(m_backBuffers[i].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(m_rtvDescriptorSize);  // offset handle
	}
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

UINT RenderWindow::Present(uint64_t currentFenceValue, CommandQueue* commandQueue)
{
	m_fenceValues[m_frameIndex] = currentFenceValue;
	m_swapChain->Present(1, 0);
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	commandQueue->WaitForFenceValue(m_fenceValues[m_frameIndex]);
	return m_frameIndex;
}

ComPtr<ID3D12Resource> RenderWindow::GetBackBuffer()
{
	return m_backBuffers[m_frameIndex];
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderWindow::GetCurrentBackBufferView()
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE RenderWindow::GetDepthStencilHandle()
{
	return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}
