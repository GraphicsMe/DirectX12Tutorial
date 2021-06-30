#include "CubeBuffer.h"
#include "D3D12RHI.h"
#include "DirectXTex.h"
#include "CommandQueue.h"
#include "CommandListManager.h"
#include "CommandContext.h"

extern FCommandListManager g_CommandListManager;

FMatrix FCubeBuffer::GetViewMatrix(int Face)
{
	static Vector3f CameraTargets[6] = {
		Vector3f(1.f, 0.f, 0.f),	// +x
		Vector3f(-1.f, 0.f, 0.f),	// -x
		Vector3f(0.f, 1.f, 0.f),	// +y
		Vector3f(0.f, -1.f, 0.f),	// -y
		Vector3f(0.f, 0.f, 1.f),	// +z
		Vector3f(0.f, 0.f, -1.f),	// -z
	};
	static Vector3f CameraUps[6] = {
		Vector3f(0.f, 1.f, 0.f),	// +x
		Vector3f(0.f, 1.f, 0.f),	// -x
		Vector3f(0.f, 0.f, -1.f),	// +y
		Vector3f(0.f, 0.f, 1.f),	// -y
		Vector3f(0.f, 1.f, 0.f),	// +z
		Vector3f(0.f, 1.f, 0.f),	// -z
	};

	return FMatrix::MatrixLookAtLH(Vector3f(0.f), CameraTargets[Face], CameraUps[Face]);
}



FMatrix FCubeBuffer::GetProjMatrix()
{
	return FMatrix::MatrixPerspectiveFovLH(MATH_PI_HALF, 1.f, 0.0f, 10.f);
}

FMatrix FCubeBuffer::GetViewProjMatrix(int Face)
{
	return GetViewMatrix(Face) * GetProjMatrix();
}

void FCubeBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips, DXGI_FORMAT Format)
{
	m_NumMipMaps = (NumMips == 0) ? ComputeNumMips(Width, Height) : NumMips;
	Assert(Width == Height);
	m_Width = m_Height = Width;

	D3D12_RESOURCE_FLAGS Flags = CombineResourceFlags();
	D3D12_RESOURCE_DESC ResDesc = DescribeTex2D(Width, Height, 6, m_NumMipMaps, Format, Flags); //ArraySize=6

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
	CreateDerivedViews(Device, Format, 6, m_NumMipMaps);
}

D3D12_CPU_DESCRIPTOR_HANDLE FCubeBuffer::GetRTV(int Face, int Mip) const
{
	uint32_t DescriptorSize = D3D12RHI::Get().GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE Result = m_RTVHandle;
	Result.ptr += DescriptorSize * GetSubresourceIndex(Face, Mip);
	return Result;
}


D3D12_CPU_DESCRIPTOR_HANDLE FCubeBuffer::GetFaceMipSRV(int Face, int Mip) const
{
	uint32_t DescriptorSize = D3D12RHI::Get().GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE Result = m_FaceMipSRVHandle;
	Result.ptr += DescriptorSize * GetSubresourceIndex(Face, Mip);
	return Result;
}

uint32_t FCubeBuffer::GetSubresourceIndex(int Face, int Mip) const
{
	//Face0: Mip0, Mip1, Mip2, ...
	//Face1: Mip0, Mip1, Mip2, ...
	//...
	//Face5: Mip0, Mip1, Mip2, ...
	return Face * m_NumMipMaps + Mip;
}


void FCubeBuffer::CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips /*= 1*/)
{
	Assert(ArraySize == 6);
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = Format;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	SRVDesc.TextureCube.MipLevels = NumMips;
	SRVDesc.TextureCube.MostDetailedMip = 0;
	SRVDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	m_CubeSRVHandle = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
	Device->CreateShaderResourceView(m_Resource.Get(), &SRVDesc, m_CubeSRVHandle);

	D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
	RTVDesc.Format = Format;
	RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
	RTVDesc.Texture2DArray.PlaneSlice = 0;

	m_RTVHandle = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, ArraySize * NumMips);
	m_FaceMipSRVHandle = D3D12RHI::Get().AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, ArraySize * NumMips);

	uint32_t RTVDescriptorSize = D3D12RHI::Get().GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	uint32_t SRVDescriptorSize = D3D12RHI::Get().GetDescriptorSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentRTVHandle = m_RTVHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentSRVHandle = m_FaceMipSRVHandle;
	for (uint32_t Face = 0; Face < ArraySize; ++Face)
	{
		for (uint32_t Mip = 0; Mip < NumMips; ++Mip)
		{
			RTVDesc.Texture2DArray.MipSlice = Mip;
			RTVDesc.Texture2DArray.FirstArraySlice = Face;
			RTVDesc.Texture2DArray.ArraySize = 1;
			Device->CreateRenderTargetView(m_Resource.Get(), &RTVDesc, CurrentRTVHandle);
			CurrentRTVHandle.ptr += RTVDescriptorSize;
		
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
			SRVDesc.Texture2DArray.MostDetailedMip = Mip;
			SRVDesc.Texture2DArray.MipLevels = 1;
			SRVDesc.Texture2DArray.FirstArraySlice = Face;
			SRVDesc.Texture2DArray.ArraySize = 1;
			SRVDesc.Texture2DArray.PlaneSlice = 0;
			SRVDesc.Texture2DArray.ResourceMinLODClamp = 0.f;
			Device->CreateShaderResourceView(m_Resource.Get(), &SRVDesc, CurrentSRVHandle);
			CurrentSRVHandle.ptr += SRVDescriptorSize;
		}
	}
}


void FCubeBuffer::SaveCubeMap(const std::wstring& FileName)
{
	using namespace DirectX;
	ScratchImage image;

	FCommandQueue& Queue = g_CommandListManager.GetQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

	HRESULT hr = DirectX::CaptureTexture(Queue.GetD3D12CommandQueue(), m_Resource.Get(), true/*isCubeMap*/, image, m_CurrentState, m_CurrentState);
	if (SUCCEEDED(hr))
	{
		hr = DirectX::SaveToDDSFile(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DDS_FLAGS_NONE, FileName.c_str());
		if (FAILED(hr))
		{
			printf("Falied to save cubemap to dds file\n");
		}
	}
}
