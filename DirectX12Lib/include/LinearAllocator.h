#pragma once

#include <deque>
#include <memory>
#include <d3d12.h>
#include "Common.h"
#include "D3D12Resource.h"

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
	size_t Offset;
	void* CPU;
	D3D12_GPU_VIRTUAL_ADDRESS GpuAddress;
};

class LinearAllocationPage : public FD3D12Resource
{
	friend class LinearAllocator;

public:
	LinearAllocationPage(ID3D12Resource* pResource, D3D12_RESOURCE_STATES State, size_t SizeInBytes);
	~LinearAllocationPage();

	uint64_t GetFenceValue() const { return m_FenceValue; }
	void SetFenceValue(uint64_t FenceValue) { m_FenceValue = FenceValue; }

private:
	void Map();
	void Unmap();
	
private:
	uint64_t m_FenceValue;
	size_t m_PageSize;
	void* m_CpuAddress;
	D3D12_GPU_VIRTUAL_ADDRESS GpuAddress;
	
};

class LinearAllocator
{
public:
	LinearAllocator(ELinearAllocatorType Type);

	FAllocation Allocate(size_t SizeInBytes, size_t Alignment = DEFAULT_ALIGN);

	void CleanupUsedPages(uint64_t FenceID);
	void Destroy();

private:
	LinearAllocationPage* AllocateLargePage(uint32_t SizeInBytes);
	LinearAllocationPage* RequestPage(size_t SizeInBytes);
	LinearAllocationPage* CreateNewPage(size_t PageSize);

	using PagePool = std::deque<LinearAllocationPage* >;

	ELinearAllocatorType m_AllocatorType;
	size_t m_PageSize;
	size_t m_CurrentOffset;

	PagePool m_UsingPages;
	PagePool m_RetiredPages;

	LinearAllocationPage* m_CurrentPage;
};