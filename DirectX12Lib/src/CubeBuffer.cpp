#include "CubeBuffer.h"
#include "D3D12RHI.h"
#include "DirectXTex.h"
#include "DirectXTexP.h"
#include "CommandQueue.h"
#include "CommandListManager.h"
#include "CommandContext.h"
#include "Texture.h"

using namespace DirectX;

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

class Vertex
{
public:
	Vector3f pos, color;
};
void RandomSample(const DirectX::ScratchImage& InputImage, int Width, int Height, int SampleNum, std::vector<Vertex>& OutSamples)
{
	DirectX::ScratchImage dstSImg;
	dstSImg.Initialize2D(DXGI_FORMAT_R32G32B32A32_FLOAT, InputImage.GetMetadata().width, InputImage.GetMetadata().height, InputImage.GetMetadata().arraySize, InputImage.GetMetadata().mipLevels);
	for (int i = 0; i < InputImage.GetImageCount(); i++)
	{
		DirectX::_ConvertFromR16G16B16A16(InputImage.GetImages()[i], dstSImg.GetImages()[i]);
		
		//const Image img = dstSImg.GetImages()[i];
		//{
		//	float* dst = (float*)(img.pixels);
		//	size_t rowPitch = 0;
		//	size_t slicePitch = 0;
		//	ComputePitch(img.format, img.width, img.height, rowPitch, slicePitch);
		//	for (int rowIndex = 0; rowIndex < img.height; rowIndex++)
		//	{
		//		float* dst = (float*)(img.pixels + rowPitch * rowIndex);
		//		for (int colIndex = 0; colIndex < img.width; colIndex++)
		//		{
		//		}
		//	}
		//}
	}

	//HRESULT hr = DirectX::SaveToDDSFile(dstSImg.GetImages(), dstSImg.GetImageCount(), dstSImg.GetMetadata(), DDS_FLAGS_NONE, L"test_image.dds");

	OutSamples.clear();
	OutSamples.resize(SampleNum);

	for (int i = 0; i < SampleNum; ++i)
	{
		float x, y, z;
		do {
			x = NormalRandom();
			y = NormalRandom();
			z = NormalRandom();
		} while (x == 0 && y == 0 && z == 0);

		Vertex vex;

		Vector3f pos(x, y, z);
		vex.pos = pos.Normalize();

		CubeUV cubeuv = XYZ2CubeUV(pos);

		int colIndex = (int)(cubeuv.u * (Width - 1));
		int rowIndex = (int)((1.f - cubeuv.v) * (Height - 1));

		const DirectX::Image* images = dstSImg.GetImages();
		int Lod0FaceIndex = cubeuv.index * dstSImg.GetMetadata().mipLevels;
		const DirectX::Image image = images[Lod0FaceIndex];

		size_t rowPitch = 0;
		size_t slicePitch = 0;
		ComputePitch(image.format, image.width, image.height, rowPitch, slicePitch);

		float* dst = (float*)(image.pixels + rowPitch * rowIndex);
		float R = dst[colIndex * 4 + 0];
		float G = dst[colIndex * 4 + 1];
		float B = dst[colIndex * 4 + 2];

		//printf("[%f,%f,%f]\n", R, G, B);
		vex.color = { R,G,B };
		//vex.color = { 1,0,0 };
		if (vex.color.x < 0 || vex.color.y < 0 || vex.color.z < 0)
		{
			int xxx = 0;
		}

		OutSamples[i] = vex;
	}
}

std::vector<float> Basis(const int Degree , const Vector3f& pos)
{
	int n = Degree * Degree;
	std::vector<float> Y(n);
	Vector3f normal = pos.Normalize();
	float x = normal.x;
	float y = normal.y;
	float z = normal.z;

	if (Degree >= 1)
	{
		Y[0] = 1.f / 2.f * sqrt(1.f / MATH_PI);
	}
	if (Degree >= 2)
	{
		Y[1] = sqrt(3.f / (4.f * MATH_PI)) * z;
		Y[2] = sqrt(3.f / (4.f * MATH_PI)) * y;
		Y[3] = sqrt(3.f / (4.f * MATH_PI)) * x;
	}
	if (Degree >= 3)
	{
		Y[4] = 1.f / 2.f * sqrt(15.f / MATH_PI) * x * z;
		Y[5] = 1.f / 2.f * sqrt(15.f / MATH_PI) * z * y;
		Y[6] = 1.f / 4.f * sqrt(5.f / MATH_PI) * (-x * x - z * z + 2 * y * y);
		Y[7] = 1.f / 2.f * sqrt(15.f / MATH_PI) * y * x;
		Y[8] = 1.f / 4.f * sqrt(15.f / MATH_PI) * (x * x - z * z);
	}
	if (Degree >= 4)
	{
		Y[9] = 1.f / 4.f * sqrt(35.f / (2.f * MATH_PI)) * (3 * x * x - z * z) * z;
		Y[10] = 1.f / 2.f * sqrt(105.f / MATH_PI) * x * z * y;
		Y[11] = 1.f / 4.f * sqrt(21.f / (2.f * MATH_PI)) * z * (4 * y * y - x * x - z * z);
		Y[12] = 1.f / 4.f * sqrt(7.f / MATH_PI) * y * (2 * y * y - 3 * x * x - 3 * z * z);
		Y[13] = 1.f / 4.f * sqrt(21.f / (2.f * MATH_PI)) * x * (4 * y * y - x * x - z * z);
		Y[14] = 1.f / 4.f * sqrt(105.f / MATH_PI) * (x * x - z * z) * y;
		Y[15] = 1.f / 4.f * sqrt(35.f / (2 * MATH_PI)) * (x * x - 3 * z * z) * x;
	}
	return Y;
}

std::vector<Vector3f> FCubeBuffer::GenerateSHcoeffs(int Degree, int SampleNum)
{
	std::vector<Vector3f> SHcoeffs;
	HRESULT hr;

#if 0
	DirectX::ScratchImage image;
	FCommandQueue& Queue = g_CommandListManager.GetQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
	hr = DirectX::CaptureTexture(Queue.GetD3D12CommandQueue(), m_Resource.Get(), true/*isCubeMap*/, image, m_CurrentState, m_CurrentState);
#else
	DirectX::ScratchImage image_posx;
	DirectX::ScratchImage image_negx;
	DirectX::ScratchImage image_posy;
	DirectX::ScratchImage image_negy;
	DirectX::ScratchImage image_posz;
	DirectX::ScratchImage image_negz;

	hr = DirectX::LoadFromWICFile(L"../Resources/CubeMap/CNTower/posx.jpg", DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image_posx);
	hr = DirectX::LoadFromWICFile(L"../Resources/CubeMap/CNTower/negx.jpg", DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image_negx);
	hr = DirectX::LoadFromWICFile(L"../Resources/CubeMap/CNTower/posy.jpg", DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image_posy);
	hr = DirectX::LoadFromWICFile(L"../Resources/CubeMap/CNTower/negy.jpg", DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image_negy);
	hr = DirectX::LoadFromWICFile(L"../Resources/CubeMap/CNTower/posz.jpg", DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image_posz);
	hr = DirectX::LoadFromWICFile(L"../Resources/CubeMap/CNTower/negz.jpg", DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image_negz);

	DirectX::ScratchImage image;
	image.Initialize2D(DXGI_FORMAT_R16G16B16A16_FLOAT, image_posx.GetMetadata().width, image_posx.GetMetadata().height, 6, 1);
	DirectX::ScratchImage temp;
	temp.Initialize2D(DXGI_FORMAT_R16G16B16A16_FLOAT, image_posx.GetMetadata().width, image_posx.GetMetadata().height, 1, 1);

	hr = DirectX::_ConvertToR16G16B16A16(image_posx.GetImages()[0], temp);
	hr = DirectX::_ConvertFromR16G16B16A16(temp.GetImages()[0], image.GetImages()[0]);

	hr = DirectX::_ConvertToR16G16B16A16(image_negx.GetImages()[0], temp);
	hr = DirectX::_ConvertFromR16G16B16A16(temp.GetImages()[0], image.GetImages()[1]);

	hr = DirectX::_ConvertToR16G16B16A16(image_posy.GetImages()[0], temp);
	hr = DirectX::_ConvertFromR16G16B16A16(temp.GetImages()[0], image.GetImages()[2]);

	hr = DirectX::_ConvertToR16G16B16A16(image_negy.GetImages()[0], temp);
	hr = DirectX::_ConvertFromR16G16B16A16(temp.GetImages()[0], image.GetImages()[3]);

	hr = DirectX::_ConvertToR16G16B16A16(image_posz.GetImages()[0], temp);
	hr = DirectX::_ConvertFromR16G16B16A16(temp.GetImages()[0], image.GetImages()[4]);

	hr = DirectX::_ConvertToR16G16B16A16(image_negz.GetImages()[0], temp);
	hr = DirectX::_ConvertFromR16G16B16A16(temp.GetImages()[0], image.GetImages()[5]);
#endif

	if (SUCCEEDED(hr))
	{
		std::vector<Vertex> Samples;
		RandomSample(image, image.GetMetadata().width, image.GetMetadata().height, SampleNum, Samples);

		int n = Degree * Degree;
		SHcoeffs.resize(n);
		memset(SHcoeffs.data(), 0, sizeof(Vector3f) * SHcoeffs.size());

		for (const Vertex& v : Samples)
		{
			std::vector<float> Y = Basis(Degree, v.pos);
			for (int i = 0; i < n; ++i)
			{
				SHcoeffs[i] = SHcoeffs[i] + Y[i] * v.color; 
			}
		}
		for (Vector3f& coef : SHcoeffs)
		{
			coef = 4 * MATH_PI * coef / (float)Samples.size();
		}
		
		//...Convolve with SH - projected cosinus lobe
		float ConvolveCosineLobeBandFactor[16] =
		{
			MATH_PI,
			2.0f * MATH_PI / 3.0f, 2.0f * MATH_PI / 3.0f, 2.0f * MATH_PI / 3.0f,
			MATH_PI / 4.0f, MATH_PI / 4.0f, MATH_PI / 4.0f, MATH_PI / 4.0f, MATH_PI / 4.0f,
			0,0,0,0,0,0,0
		};
		for (int i = 0; i < Degree*Degree; i++)
		{
			SHcoeffs[i] = SHcoeffs[i] * ConvolveCosineLobeBandFactor[i];
		}
	}
	
	// render test
	//RenderCubemap(Degree, SHcoeffs, image.GetMetadata().width, image.GetMetadata().height);

	image.Release();
	return SHcoeffs;
}

void FCubeBuffer::RenderCubemap(int Degree,std::vector<Vector3f> SHCoeffs, int width, int height)
{
	DirectX::ScratchImage dstSImg;
	dstSImg.Initialize2D(DXGI_FORMAT_R32G32B32A32_FLOAT, width, height, 6, 1);

	int n = Degree * Degree;

	for (int k = 0; k < 6; k++)
	{
		const Image img = dstSImg.GetImages()[k];
		
		float* dst = (float*)(img.pixels);
		size_t rowPitch = 0;
		size_t slicePitch = 0;
		ComputePitch(img.format, img.width, img.height, rowPitch, slicePitch);

		for (int rowIndex = 0; rowIndex < height; rowIndex++)
		{
			float* dst = (float*)(img.pixels + rowPitch * rowIndex);
			for (int colIndex = 0; colIndex < width; colIndex++)
			{
				float u = (float)colIndex / (width - 1);
				float v = 1.f - (float)rowIndex / (height - 1);

				Vector3f pos = CubeUV2XYZ({ k, u, v });
				pos = pos.Normalize();
				std::vector<float> Y = Basis(Degree,pos);

				Vector3f color(0, 0, 0);
				for (int i = 0; i < n; i++)
				{
					color = color + Y[i] * SHCoeffs[i];
				}
				
				dst[colIndex * 4 + 0] = color.x;
				dst[colIndex * 4 + 1] = color.y;
				dst[colIndex * 4 + 2] = color.z;
				dst[colIndex * 4 + 3] = 255;

				if (color.x < 0 || color.y < 0 || color.z < 0)
				{
					int xxx = 0;
				}

			}
		}
	}

	//HRESULT hr = DirectX::SaveToDDSFile(dstSImg.GetImages(), dstSImg.GetImageCount(), dstSImg.GetMetadata(), DDS_FLAGS_NONE, L"reconstruct_test_image.dds");
	dstSImg.Release();
}

