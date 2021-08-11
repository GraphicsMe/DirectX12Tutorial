#include "ColorBuffer.h"
#include "D3D12RHI.h"
#include "DirectXTex.h"
#include "CommandContext.h"
#include "CommandListManager.h"
#include "CommandContext.h"

using namespace DirectX;
extern FCommandListManager g_CommandListManager;

void FColorBuffer::CreateFromSwapChain(const std::wstring& Name, ID3D12Resource* BaseResource)
{
	ID3D12Device* Device = D3D12RHI::Get().GetD3D12Device().Get();
	AssociateWithResource(Device, Name, BaseResource, D3D12_RESOURCE_STATE_PRESENT);

	m_RTVHandle = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	Device->CreateRenderTargetView(m_Resource.Get(), nullptr, m_RTVHandle);
}

void FColorBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips, DXGI_FORMAT Format)
{
	m_NumMipMaps = (NumMips == 0) ? ComputeNumMips(Width, Height) : NumMips;
	
	D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags();
	D3D12_RESOURCE_DESC ResDesc = DescribeTex2D(Width, Height, 1, m_NumMipMaps, Format, Flags);

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
	CreateDerivedViews(Device, Format, 1, m_NumMipMaps);
}

const D3D12_CPU_DESCRIPTOR_HANDLE FColorBuffer::GetMipSRV(int Mip) const
{
	uint32_t DescriptorSize = D3D12RHI::Get().GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE Result = m_SRVHandleMips;
	Result.ptr += DescriptorSize * Mip;
	return Result;
}

const D3D12_CPU_DESCRIPTOR_HANDLE FColorBuffer::GetMipUAV(int Mip) const
{
	uint32_t DescriptorSize = D3D12RHI::Get().GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE Result = m_UAVHandleMips;
	Result.ptr += DescriptorSize * Mip;
	return Result;
}

void FColorBuffer::CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips /*= 1*/)
{
	(ArraySize);
	D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};

	RTVDesc.Format = Format;
	RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	RTVDesc.Texture2D.MipSlice = 0;

	if (m_SRVHandle.ptr == 0)
	{
		m_RTVHandle = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		m_SRVHandle = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	if (m_UAVHandle.ptr == 0)
	{
		m_UAVHandle = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	SRVDesc.Format = Format;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = NumMips;
	SRVDesc.Texture2D.MostDetailedMip = 0;
	Device->CreateShaderResourceView(m_Resource.Get(), &SRVDesc, m_SRVHandle);

	UAVDesc.Format = Format;
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	UAVDesc.Texture2D.MipSlice = 0;
	Device->CreateUnorderedAccessView(m_Resource.Get(), nullptr, &UAVDesc, m_UAVHandle);
	if (NumMips > 1)
	{
		m_SRVHandleMips = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NumMips);
		m_UAVHandleMips = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, NumMips);

		uint32_t SRVUAVDescriptorSize = D3D12RHI::Get().GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		D3D12_CPU_DESCRIPTOR_HANDLE CurrentSRVHandle = m_SRVHandleMips;
		D3D12_CPU_DESCRIPTOR_HANDLE CurrentUAVHandle = m_UAVHandleMips;
		for (uint32_t i = 0; i < NumMips; ++i)
		{
			SRVDesc.Texture2DArray.ArraySize = 1;
			SRVDesc.Texture2DArray.FirstArraySlice = 0;
			SRVDesc.Texture2DArray.MipLevels = 1;
			SRVDesc.Texture2DArray.MostDetailedMip = i;
			SRVDesc.Texture2DArray.PlaneSlice = 0;
			SRVDesc.Texture2DArray.ResourceMinLODClamp = 0.f;
			Device->CreateShaderResourceView(m_Resource.Get(), &SRVDesc, CurrentSRVHandle);
			CurrentSRVHandle.ptr += SRVUAVDescriptorSize;

			UAVDesc.Texture2DArray.ArraySize = 1;
			UAVDesc.Texture2DArray.FirstArraySlice = 0;
			UAVDesc.Texture2DArray.MipSlice = i;
			UAVDesc.Texture2DArray.PlaneSlice = 0;
			Device->CreateUnorderedAccessView(m_Resource.Get(), nullptr, &UAVDesc, CurrentUAVHandle);
			CurrentUAVHandle.ptr += SRVUAVDescriptorSize;
		}
	}

	Device->CreateRenderTargetView(m_Resource.Get(), &RTVDesc, m_RTVHandle);
}

void FColorBuffer::SaveColorBuffer(const std::wstring& FileName)
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