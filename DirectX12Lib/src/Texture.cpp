#include "Texture.h"
#include "D3D12RHI.h"
#include "DirectXTex.h"
#include "CommandContext.h"
#include "CommandListManager.h"
#include "CommandContext.h"

using namespace DirectX;

extern FCommandListManager g_CommandListManager;

void FTexture::Create(uint32_t Width, uint32_t Height, DXGI_FORMAT Format, const void* InitialData)
{
	m_Width = Width;
	m_Height = Height;

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
		D3D12_HEAP_FLAG_NONE, &TexDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_Resource)));
	InitializeState(D3D12_RESOURCE_STATE_COPY_DEST);

	m_Resource->SetName(L"Texture2D");

	size_t RowPitch, SlicePitch;
	DirectX::ComputePitch(Format, Width, Height, RowPitch, SlicePitch);

	D3D12_SUBRESOURCE_DATA TexData;
	TexData.pData = InitialData;
	TexData.RowPitch = RowPitch;
	TexData.SlicePitch = SlicePitch;

	FCommandContext::InitializeTexture(*this, 1, &TexData);

	if (m_CpuDescriptorHandle.ptr == D3D12_CPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_CpuDescriptorHandle = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12RHI::Get().GetD3D12Device()->CreateShaderResourceView(m_Resource.Get(), nullptr, m_CpuDescriptorHandle);
}

void FTexture::LoadFromFile(const std::wstring& FileName, bool IsSRGB)
{
	DirectX::ScratchImage image;
	HRESULT hr;
	if (FileName.rfind(L".dds") != std::string::npos)
	{
		hr = DirectX::LoadFromDDSFile(FileName.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
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

	m_Width = (int)image.GetImages()->width;
	m_Height = (int)image.GetImages()->height;

	ThrowIfFailed(hr);
	ID3D12Device* Device = D3D12RHI::Get().GetD3D12Device().Get();
	ThrowIfFailed(DirectX::CreateTextureEx(Device, image.GetMetadata(), D3D12_RESOURCE_FLAG_NONE, IsSRGB, m_Resource.ReleaseAndGetAddressOf()));
	InitializeState(D3D12_RESOURCE_STATE_COPY_DEST);

	m_Resource->SetName(FileName.c_str());

	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	ThrowIfFailed(PrepareUpload(Device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), subresources));
	
	Assert(subresources.size() > 0);
	FCommandContext::InitializeTexture(*this, (UINT)subresources.size(), &subresources[0]);

	if (m_CpuDescriptorHandle.ptr == D3D12_CPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_CpuDescriptorHandle = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12RHI::Get().GetD3D12Device()->CreateShaderResourceView(m_Resource.Get(), nullptr, m_CpuDescriptorHandle);

}

void FTexture::SaveTexutre(const std::wstring& FileName)
{
	ScratchImage image;
	FCommandQueue& Queue = g_CommandListManager.GetQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

	HRESULT hr = DirectX::CaptureTexture(Queue.GetD3D12CommandQueue(), m_Resource.Get(), false/*isCubeMap*/, image, m_AllCurrentState[0], m_AllCurrentState[0]);
	if (SUCCEEDED(hr))
	{
		hr = DirectX::SaveToDDSFile(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DDS_FLAGS_NONE, FileName.c_str());
		if (FAILED(hr))
		{
			printf("Falied to save texture to dds file\n");
		}
	}
}


void FTextureArray::LoadFromFile(const std::wstring& FileName, bool IsSRGB /*= true*/)
{
	DirectX::ScratchImage image;
	HRESULT hr;
	if (FileName.rfind(L".dds") != std::string::npos)
	{
		hr = DirectX::LoadFromDDSFile(FileName.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
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

	m_Width = (int)image.GetImages()->width;
	m_Height = (int)image.GetImages()->height;

	ThrowIfFailed(hr);
	ID3D12Device* Device = D3D12RHI::Get().GetD3D12Device().Get();
	ThrowIfFailed(DirectX::CreateTextureEx(Device, image.GetMetadata(), D3D12_RESOURCE_FLAG_NONE, IsSRGB, m_Resource.ReleaseAndGetAddressOf()));
	InitializeState(D3D12_RESOURCE_STATE_COPY_DEST);

	m_Resource->SetName(FileName.c_str());

	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	ThrowIfFailed(PrepareUpload(Device, image.GetImages(), image.GetImageCount(), image.GetMetadata(), subresources));

	Assert(subresources.size() > 0);
	FCommandContext::InitializeTexture(*this, (UINT)subresources.size(), &subresources[0]);

	if (m_CpuDescriptorHandle.ptr == D3D12_CPU_VIRTUAL_ADDRESS_UNKNOWN)
		m_CpuDescriptorHandle = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, (UINT)image.GetMetadata().arraySize);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = m_Resource->GetDesc().Format;//视图的默认格式
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;//2D纹理数组
	srvDesc.Texture2DArray.ArraySize = m_Resource->GetDesc().DepthOrArraySize;//纹理数组长度
	srvDesc.Texture2DArray.MostDetailedMip = 0;//MipMap层级为0
	srvDesc.Texture2DArray.MipLevels = -1;//没有层级
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	D3D12RHI::Get().GetD3D12Device()->CreateShaderResourceView(m_Resource.Get(), &srvDesc, m_CpuDescriptorHandle);
}
