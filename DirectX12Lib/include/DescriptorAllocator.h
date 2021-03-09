#pragma once

#include <set>
#include <vector>

#include <d3d12.h>
#include "Common.h"

class FDescriptorAllocator
{
public:
	FDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE Type);

	D3D12_CPU_DESCRIPTOR_HANDLE Allocate(uint32_t Count);

	static void DestroyAll();

protected:
	static const uint32_t sm_NumDescriptorsPerHeap = 256;
	static std::vector<ComPtr<ID3D12DescriptorHeap> > sm_DescriptorPool;
	static ID3D12DescriptorHeap* RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type);
 
	D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType;
	ID3D12DescriptorHeap* m_CurrentHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE m_CurrentCpuAddress;
	uint32_t m_DescriptorSize;
	uint32_t m_RemainingFreeHandles;
	 
};

