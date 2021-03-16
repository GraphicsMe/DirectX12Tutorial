#pragma once

#include <queue>
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

class LinearAllocationPagePageManager
{
public:
	LinearAllocationPagePageManager();
	LinearAllocationPage* RequestPage();
	void DiscardStandardPages(uint64_t FenceID, const std::vector<LinearAllocationPage*>& Pages);
	void DiscardLargePages(uint64_t FenceID, const std::vector<LinearAllocationPage*>& Pages);
	LinearAllocationPage* CreateNewPage(size_t SizeInBytes = 0);
	void Destroy();

private:
	using PagePool = std::queue<LinearAllocationPage* >;

	PagePool m_RetiredPages;
	PagePool m_LargePagePool;
	PagePool m_StandardPagePool;

	static ELinearAllocatorType ms_TypeCounter;
	ELinearAllocatorType m_AllocatorType;
};

class LinearAllocator
{
public:
	LinearAllocator(ELinearAllocatorType Type);

	FAllocation Allocate(size_t SizeInBytes, size_t Alignment = DEFAULT_ALIGN);

	void CleanupUsedPages(uint64_t FenceID);
	static void DestroyAll();

private:
	FAllocation AllocateLargePage(size_t SizeInBytes);

	ELinearAllocatorType m_AllocatorType;
	size_t m_PageSize;
	size_t m_CurrentOffset;

	std::vector<LinearAllocationPage*> m_StandardPages;
	std::vector<LinearAllocationPage*> m_LargePages;

	LinearAllocationPage* m_CurrentPage;

	static LinearAllocationPagePageManager ms_PageManager[2];
};