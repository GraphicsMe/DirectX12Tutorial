#pragma once

#include "PixelBuffer.h"
#include "MathLib.h"
#include <vector>


class FCubeBuffer : public FPixelBuffer
{
public:
	FCubeBuffer(const Vector4f& Color = Vector4f(0.2f))
		: m_ClearColor(Color)
		, m_NumMipMaps(1)
		, m_SampleCount(1)
	{
		m_CubeSRVHandle.ptr = 0;
		m_FaceMipSRVHandle.ptr = 0;
		m_RTVHandle.ptr = 0;
	}

	static FMatrix GetProjMatrix();
	static FMatrix GetViewMatrix(int Face);
	static FMatrix GetViewProjMatrix(int Face);

	void Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips, DXGI_FORMAT Format = DXGI_FORMAT_R16G16B16A16_FLOAT);

	D3D12_CPU_DESCRIPTOR_HANDLE GetRTV(int Face, int Mip) const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetCubeSRV(void) const { return m_CubeSRVHandle; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetFaceMipSRV(int Face, int Mip) const;

	void SetClearColor(const Vector4f& Color) { m_ClearColor = Color; }
	const Vector4f& GetClearColor() const { return m_ClearColor; }

	void SaveCubeMap(const std::wstring& FileName);

	uint32_t GetMipWidth(int Mip) const { return m_Width >> Mip; }
	uint32_t GetNumMips() const { return m_NumMipMaps; }
	uint32_t GetSubresourceIndex(int Face, int Mip) const;

protected:
	D3D12_RESOURCE_FLAGS CombineResourceFlags(void) const
	{
		D3D12_RESOURCE_FLAGS Flags = D3D12_RESOURCE_FLAG_NONE;

		if (Flags == D3D12_RESOURCE_FLAG_NONE && m_SampleCount == 1)
			Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

		return D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | Flags;
	}

	static inline uint32_t ComputeNumMips(uint32_t Width, uint32_t Height)
	{
		uint32_t HighBit;
		_BitScanReverse((unsigned long*)&HighBit, Width | Height);
		return HighBit + 1;
	}

	void CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips = 1);


protected:
	Vector4f m_ClearColor;
	uint32_t m_NumMipMaps;
	uint32_t m_SampleCount;

	D3D12_CPU_DESCRIPTOR_HANDLE m_CubeSRVHandle, m_FaceMipSRVHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE m_RTVHandle;
};