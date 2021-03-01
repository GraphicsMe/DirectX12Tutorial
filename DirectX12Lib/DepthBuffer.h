#pragma once

#include "PixelBuffer.h"

class FDepthBuffer : public FPixelBuffer
{
public:
	FDepthBuffer(float ClearDepth = 1.f, uint8_t ClearStencil = 0)
		: m_ClearDepth(ClearDepth), m_ClearStencil(ClearStencil)
	{
		m_hDSV.ptr = 0;
		m_hDepthSRV.ptr = 0;
	}

	void Create(const std::wstring& Name, uint32_t Width, uint32_t Height, DXGI_FORMAT Format);
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV() const { return m_hDSV; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetDepthSRV() const { return m_hDepthSRV; }

	float GetClearDepth() const { return m_ClearDepth; }
	uint8_t GetClearStencil() const { return m_ClearStencil; }

protected:
	void CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format);
	DXGI_FORMAT GetDepthSRVFormat(DXGI_FORMAT DSVFormat);


	float m_ClearDepth;
	uint8_t m_ClearStencil;
	D3D12_CPU_DESCRIPTOR_HANDLE m_hDSV;
	D3D12_CPU_DESCRIPTOR_HANDLE m_hDepthSRV;
};