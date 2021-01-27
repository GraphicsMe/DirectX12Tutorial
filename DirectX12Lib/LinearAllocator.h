#pragma once

#include "Common.h"
#include <deque>
#include <memory>
#include <d3d12.h>

const static uint32_t DEFAULT_ALIGN = 256;
const static uint32_t GpuAllocatorPageSize = 0x10000;	// 64k
const static uint32_t CpuAllocatorPageSize = 0x200000;	// 2MB


enum ELinearAllocatorType
{
	InvalidAllocator = -1,
	GpuExclusive = 0,
	CpuWritable = 1,
	NumAllocatorTypes,
};


struct FAllocation
{
	ID3D12Resource* D3d12Resource;
	uint64_t Offset;
	void* CPU;
	D3D12_GPU_VIRTUAL_ADDRESS GpuAddress;
};

class LinearAllocationPage
{
	friend class LinearAllocator;

public:
	LinearAllocationPage(ID3D12Resource* pResource, uint32_t SizeInBytes);
	~LinearAllocationPage();

	bool HasSpace(uint32_t SizeInBytes, uint32_t Alignment);
	FAllocation Allocate(uint32_t SizeInBytes, uint32_t Alignment);
	void Reset();
	uint64_t GetFenceValue() const { return m_FenceValue; }

private:
	void Map();
	void Unmap();
	
private:
	uint64_t m_FenceValue;
	uint32_t m_PageSize;
	ComPtr<ID3D12Resource> m_d3d12Resource;
	void* m_CpuAddress;
	D3D12_GPU_VIRTUAL_ADDRESS GpuAddress;
	
};

class LinearAllocator
{
public:
	LinearAllocator(ELinearAllocatorType Type);

	FAllocation Allocate(uint32_t SizeInBytes, uint32_t Alignment = DEFAULT_ALIGN);

	void CleanupUsedPages(uint64_t FenceID);
	void Destroy();

private:
	LinearAllocationPage* AllocateLargePage(uint32_t SizeInBytes);
	LinearAllocationPage* RequestPage(uint32_t SizeInBytes);
	LinearAllocationPage* CreateNewPage(uint32_t PageSize);

	using PagePool = std::deque<LinearAllocationPage* >;

	ELinearAllocatorType m_AllocatorType;
	uint32_t m_PageSize;
	uint32_t m_CurrentOffset = 0;

	PagePool m_UsingPages;
	PagePool m_RetiredPages;

	LinearAllocationPage* m_CurrentPage;
};