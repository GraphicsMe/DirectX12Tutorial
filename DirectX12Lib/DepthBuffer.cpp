#include "DepthBuffer.h"
#include "D3D12RHI.h"

void FDepthBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, DXGI_FORMAT Format)
{
	D3D12_RESOURCE_DESC ResDesc = DescribeTex2D(Width, Height, 1, 1, Format, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

	ID3D12Device* Device = D3D12RHI::Get().GetD3D12Device().Get();

	D3D12_CLEAR_VALUE ClearValue = {};
	ClearValue.Format = Format;
	ClearValue.DepthStencil = { m_ClearDepth, m_ClearStencil };
	CreateTextureResource(Device, Name, ResDesc, ClearValue);
	CreateDerivedViews(Device, Format);
}

void FDepthBuffer::CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format)
{
	ID3D12Resource* Resource = m_Resource.Get();

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Format = Format;
	if (Resource->GetDesc().SampleDesc.Count == 1)
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;
	}
	else
	{
		dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
	}

	if (m_hDSV.ptr == 0)
	{
		m_hDSV = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	}

	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	Device->CreateDepthStencilView(Resource, &dsvDesc, m_hDSV);

	if (m_hDepthSRV.ptr == 0)
		m_hDepthSRV = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// Create the shader resource view
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = GetDepthSRVFormat(Format);
	if (dsvDesc.ViewDimension == D3D12_DSV_DIMENSION_TEXTURE2D)
	{
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MipLevels = 1;
	}
	else
	{
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
	}
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	Device->CreateShaderResourceView(Resource, &SRVDesc, m_hDepthSRV);
}

DXGI_FORMAT FDepthBuffer::GetDepthSRVFormat(DXGI_FORMAT DSVFormat)
{
	switch (DSVFormat)
	{
		// 32-bit Z w/ Stencil
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
		return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

		// No Stencil
	case DXGI_FORMAT_R32_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;

		// 24-bit Z
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

		// 16-bit Z w/o Stencil
	case DXGI_FORMAT_R16_TYPELESS:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
		return DXGI_FORMAT_R16_UNORM;

	default:
		return DXGI_FORMAT_UNKNOWN;
	}
}