#include "ColorBuffer.h"
#include "D3D12RHI.h"

void FColorBuffer::CreateFromSwapChain(const std::wstring& Name, ID3D12Resource* BaseResource)
{
	ID3D12Device* Device = D3D12RHI::Get().GetD3D12Device().Get();
	AssociateWithResource(Device, Name, BaseResource, D3D12_RESOURCE_STATE_PRESENT);

	m_RTVHandle = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	Device->CreateRenderTargetView(m_Resource.Get(), nullptr, m_RTVHandle);
}

void FColorBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips, DXGI_FORMAT Format)
{
	NumMips = (NumMips == 0) ? ComputeNumMips(Width, Height) : NumMips;
	
	D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags();
	D3D12_RESOURCE_DESC ResDesc = DescribeTex2D(Width, Height, 1, NumMips, Format, Flags);

	ResDesc.SampleDesc.Count = m_SampleCount;
	ResDesc.SampleDesc.Quality = 0;

	D3D12_CLEAR_VALUE ClearValue = {};
	ClearValue.Format = Format;
	ClearValue.Color[0] = m_ClearColor.x;
	ClearValue.Color[1] = m_ClearColor.y;
	ClearValue.Color[2] = m_ClearColor.z;
	ClearValue.Color[3] = m_ClearColor.x;

	ID3D12Device* Device = D3D12RHI::Get().GetD3D12Device().Get();
	CreateTextureResource(Device, Name, ResDesc, ClearValue);
	CreateDerivedViews(Device, Format, 1, NumMips);
}

void FColorBuffer::CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips /*= 1*/)
{
	D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};

	RTVDesc.Format = Format;
	SRVDesc.Format = Format;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	RTVDesc.Texture2D.MipSlice = 0;

	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = NumMips;
	SRVDesc.Texture2D.MostDetailedMip = 0;

	if (m_SRVHandle.ptr == 0)
	{
		m_RTVHandle = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		m_SRVHandle = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	Device->CreateRenderTargetView(m_Resource.Get(), &RTVDesc, m_RTVHandle);
	Device->CreateShaderResourceView(m_Resource.Get(), &SRVDesc, m_SRVHandle);
}

