#include "PixelBuffer.h"
#include "d3dx12.h"


D3D12_RESOURCE_DESC FPixelBuffer::DescribeTex2D(uint32_t Width, uint32_t Height, uint32_t DepthOrArraySize, uint32_t NumMips, DXGI_FORMAT Format, UINT Flags)
{
	m_Width = Width;
	m_Height = Height;
	m_ArraySize = DepthOrArraySize;
	m_Format = Format;

	D3D12_RESOURCE_DESC Desc = {};
	Desc.Alignment = 0;
	Desc.DepthOrArraySize = (UINT16)DepthOrArraySize;
	Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	Desc.Flags = (D3D12_RESOURCE_FLAGS)Flags;
	Desc.Format = Format;
	Desc.Width = (UINT)Width;
	Desc.Height = (UINT)Height;
	Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	Desc.MipLevels = (UINT16)NumMips;
	Desc.SampleDesc.Count = 1;
	Desc.SampleDesc.Quality = 0;

	return Desc;
}

void FPixelBuffer::AssociateWithResource(ID3D12Device* Device, const std::wstring& Name, ID3D12Resource* Resource, D3D12_RESOURCE_STATES State)
{
	Assert(Resource != nullptr);

	D3D12_RESOURCE_DESC Desc = Resource->GetDesc();
	m_Resource.Attach(Resource);
	m_CurrentState = State;

	m_Width = (uint32_t)Desc.Width;
	m_Height = Desc.Height;
	m_ArraySize = Desc.DepthOrArraySize;
	m_Format = Desc.Format;

#if _DEBUG
	m_Resource->SetName(Name.c_str());
#else
	(Name);
#endif
}

void FPixelBuffer::CreateTextureResource(ID3D12Device* Device, const std::wstring& Name, const D3D12_RESOURCE_DESC& ResourceDesc, D3D12_CLEAR_VALUE ClearValue, D3D12_GPU_VIRTUAL_ADDRESS GpuAddress)
{
	Destroy();

	CD3DX12_HEAP_PROPERTIES HeapProps(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(Device->CreateCommittedResource(&HeapProps, D3D12_HEAP_FLAG_NONE,
		&ResourceDesc, D3D12_RESOURCE_STATE_COMMON, &ClearValue, IID_PPV_ARGS(&m_Resource)));

	m_CurrentState = D3D12_RESOURCE_STATE_COMMON;
	m_GpuAddress = 0;

#if _DEBUG
	m_Resource->SetName(Name.c_str());
#else
	(Name);
#endif
}

