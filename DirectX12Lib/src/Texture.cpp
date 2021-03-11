#include "Texture.h"
#include "D3D12RHI.h"
#include "DirectXTex.h"
#include "CommandContext.h"


void FTexture::Create(uint32_t Width, uint32_t Height, DXGI_FORMAT Format, const void* InitialData)
{
	m_CurrentState = D3D12_RESOURCE_STATE_COPY_DEST;

	D3D12_RESOURCE_DESC TexDesc = {};
	TexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	TexDesc.Width = Width;
	TexDesc.Height = Height;
	TexDesc.DepthOrArraySize = 1;
	TexDesc.MipLevels = 1;
	TexDesc.Format = Format;
	TexDesc.SampleDesc.Count = 1;
	TexDesc.SampleDesc.Quality = 0;
	TexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	TexDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES HeapProps;
	HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProps.VisibleNodeMask = 1;
	HeapProps.CreationNodeMask = 1;

	ThrowIfFailed(D3D12RHI::Get().GetD3D12Device()->CreateCommittedResource(&HeapProps,
		D3D12_HEAP_FLAG_NONE, &TexDesc, m_CurrentState, nullptr, IID_PPV_ARGS(&m_Resource)));

	m_Resource->SetName(L"Texture2D");

	size_t RowPitch, SlicePitch;
	DirectX::ComputePitch(Format, Width, Height, RowPitch, SlicePitch);

	D3D12_SUBRESOURCE_DATA TexData;
	TexData.pData = InitialData;
	TexData.RowPitch = RowPitch;
	TexData.SlicePitch = SlicePitch;

	FCommandContext::InitializeTexture(*this, 1, &TexData);

	if (m_CpuDescriptorHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_CpuDescriptorHandle = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12RHI::Get().GetD3D12Device()->CreateShaderResourceView(m_Resource.Get(), nullptr, m_CpuDescriptorHandle);
}

void FTexture::LoadFromFile(const std::wstring& FileName)
{
	m_CurrentState = D3D12_RESOURCE_STATE_COPY_DEST;

	DirectX::ScratchImage image;
	HRESULT hr;
	if (FileName.rfind(L".dds") != std::string::npos)
	{
		hr = DirectX::LoadFromDDSFile(FileName.c_str(), DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image);
	}
	else if (FileName.rfind(L".tga") != std::string::npos)
	{
		hr = DirectX::LoadFromTGAFile(FileName.c_str(), nullptr, image);
	}
	else if (FileName.rfind(L".hdr") != std::string::npos)
	{
		hr = DirectX::LoadFromHDRFile(FileName.c_str(), nullptr, image);
	}
	else
	{
		hr = DirectX::LoadFromWICFile(FileName.c_str(), DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image);
	}
	ThrowIfFailed(hr);
	ID3D12Device* Device = D3D12RHI::Get().GetD3D12Device().Get();
	ThrowIfFailed(DirectX::CreateTextureEx(Device, image.GetMetadata(), D3D12_RESOURCE_FLAG_NONE, true, m_Resource.ReleaseAndGetAddressOf()));

	m_Resource->SetName(FileName.c_str());

	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	ThrowIfFailed(PrepareUpload(Device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), subresources));
	
	Assert(subresources.size() > 0);
	FCommandContext::InitializeTexture(*this, (UINT)subresources.size(), &subresources[0]);

	if (m_CpuDescriptorHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_CpuDescriptorHandle = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12RHI::Get().GetD3D12Device()->CreateShaderResourceView(m_Resource.Get(), nullptr, m_CpuDescriptorHandle);

}

