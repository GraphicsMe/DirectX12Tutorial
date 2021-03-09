#pragma once

#include "PixelBuffer.h"
#include "MathLib.h"


class FColorBuffer : public FPixelBuffer
{
public:
	FColorBuffer(const Vector3f& Color = Vector3f(0.2f))
		: m_ClearColor(Color)
		, m_NumMipMaps(0)
		, m_SampleCount(1)
	{
		m_RTVHandle.ptr = 0;
		m_SRVHandle.ptr = 0;
	}

	void CreateFromSwapChain(const std::wstring& Name, ID3D12Resource* BaseResource);
	void Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips,
		DXGI_FORMAT Format);

	const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV(void) const { return m_SRVHandle; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetRTV(void) const { return m_RTVHandle; }

	void SetClearColor(const Vector3f& Color) { m_ClearColor = Color; }
	const Vector3f& GetClearColor() const { return m_ClearColor; }

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
	Vector3f m_ClearColor;
	uint32_t m_NumMipMaps;
	uint32_t m_SampleCount;

	D3D12_CPU_DESCRIPTOR_HANDLE m_RTVHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE m_SRVHandle;
};