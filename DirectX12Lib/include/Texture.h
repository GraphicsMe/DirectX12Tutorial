#pragma once

#include "D3D12Resource.h"

class FTexture : public FD3D12Resource
{
public:
	FTexture() : m_Width(0), m_Height(0) { m_CpuDescriptorHandle.ptr = D3D12_CPU_VIRTUAL_ADDRESS_UNKNOWN; }
	FTexture(D3D12_CPU_DESCRIPTOR_HANDLE Handle) : m_CpuDescriptorHandle(Handle) {}

	void Create(uint32_t Width, uint32_t Height, DXGI_FORMAT Format, const void* InitialData);
	void LoadFromFile(const std::wstring& FileName, bool IsSRGB = true);
	void SaveTexutre(const std::wstring& Path);

	int GetWidth() const { return m_Width; }
	int GetHeight() const { return m_Height; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const { return m_CpuDescriptorHandle; }

protected:
	int m_Width, m_Height;
	D3D12_CPU_DESCRIPTOR_HANDLE m_CpuDescriptorHandle;
};