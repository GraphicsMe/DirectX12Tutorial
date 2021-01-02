#pragma once

#include <set>
#include <vector>

#include "d3dx12.h"
#include "Common.h"

class DescriptorAllocation
{
public:
	bool IsNull() const;
};

class DescriptorAllocatorPage
{
public:
	DescriptorAllocatorPage(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t Num);

	uint32_t NumFreeDescriptors() const;

private:
	void AddNewBlock( uint32_t offset, uint32_t numDescriptors );

	D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType;
	ComPtr<ID3D12DescriptorHeap> m_d3d12DescriptorHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE m_BaseDescriptor;
	uint32_t m_DescriptorHandleIncrementSize;
	uint32_t m_NumFreeHandles;
	uint32_t m_NumDescriptorsInHeap;
};

class DescriptorAllocator
{
public:
	DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t NumPerHeap = 256);
	virtual ~DescriptorAllocator();

	DescriptorAllocation Allocate(uint32_t Count = 1);

	void ReleaseStaleDescriptors(uint64_t FrameNumber);

private:
	using DescriptorHeapPool = std::vector<DescriptorAllocatorPage*>;
 
	// Create a new heap with a specific number of descriptors.
	DescriptorAllocatorPage* CreateAllocatorPage();
 
	D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType;
	uint32_t m_NumDescriptorsPerHeap;
 
	DescriptorHeapPool m_HeapPool;
	// Indices of available heaps in the heap pool.
	std::set<size_t> m_AvailableHeaps;
 
	//std::mutex m_AllocationMutex;
};

