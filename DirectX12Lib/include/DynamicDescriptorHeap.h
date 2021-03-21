#pragma once
#include <d3d12.h>
#include <vector>
#include <queue>
#include "Common.h"

class FCommandContext;
class FRootSignature;

class FDescriptorHandle
{
public:
	FDescriptorHandle()
	{
		m_CpuHandle.ptr = D3D12_CPU_VIRTUAL_ADDRESS_UNKNOWN;
		m_GpuHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
	}

	FDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle)
		: m_CpuHandle(CpuHandle)
	{
		m_GpuHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
	}

	FDescriptorHandle(D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle)
		: m_CpuHandle(CpuHandle)
		, m_GpuHandle(GpuHandle)
	{}

	FDescriptorHandle(ID3D12DescriptorHeap* Heap)
	{
		m_CpuHandle = Heap->GetCPUDescriptorHandleForHeapStart();
		m_GpuHandle = Heap->GetGPUDescriptorHandleForHeapStart();
	}

	FDescriptorHandle operator + (INT Offset) const
	{
		FDescriptorHandle Result = *this;
		Result += Offset;
		return Result;
	}

	void operator += (INT Offset)
	{
		if (m_CpuHandle.ptr != D3D12_CPU_VIRTUAL_ADDRESS_UNKNOWN)
			m_CpuHandle.ptr += Offset;
		if (m_GpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			m_GpuHandle.ptr += Offset;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle() const { return m_CpuHandle; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle() const { return m_GpuHandle; }

	bool IsCpuNull() const { return m_CpuHandle.ptr == D3D12_CPU_VIRTUAL_ADDRESS_UNKNOWN; }
	bool IsShaderVisible() const { return m_GpuHandle.ptr != D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN; }

private:
	D3D12_CPU_DESCRIPTOR_HANDLE m_CpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE m_GpuHandle;
};


class FDynamicDescriptorHeap
{
public:
	FDynamicDescriptorHeap(FCommandContext& OwningContext, D3D12_DESCRIPTOR_HEAP_TYPE HeapType);

	static void DestroyAll()
	{
		ms_DescriptorHeapPool[0].clear();
		ms_DescriptorHeapPool[1].clear();
	}

	void ParseGraphicsRootSignature(const FRootSignature& RootSignature)
	{
		m_GraphicsHandleCache.ParseRootSignature(m_HeapType, RootSignature);
	}
	void SetGraphicsDescriptorHandles(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
	{
		m_GraphicsHandleCache.StageDescriptorHandles(RootIndex, Offset, Count, Handles);
	}

	void CommitGraphicsRootDescriptorTables(ID3D12GraphicsCommandList* CommandList)
	{
		if (m_GraphicsHandleCache.m_StaleRootParamsBitMap)
		{
			CopyAndBindStagedTables(m_GraphicsHandleCache, CommandList, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
		}
	}

	void CleanupUsedHeaps(uint64_t FenceValue);

private:
	bool HasSpace(uint32_t Count)
	{
		return (m_CurrentHeap != nullptr && m_CurrentOffset + Count <= NumDescriptorsPerHeap);
	}
	void RetireCurrentHeap();
	void RetireUsedHeaps(uint64_t FenceValue);
	void UnbindAllInvalid();
	ID3D12DescriptorHeap* GetHeapPointer();
	ID3D12DescriptorHeap* RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE HeapType);

private:
	static const uint32_t NumDescriptorsPerHeap = 256;
	static std::vector<ComPtr<ID3D12DescriptorHeap>> ms_DescriptorHeapPool[2]; //CBV_SRV_UAV, Sampler
	static std::queue<std::pair<uint64_t, ID3D12DescriptorHeap*>> ms_RetiredDescriptorHeaps[2];
	static std::queue<ID3D12DescriptorHeap*> ms_ReadyDescriptorHeaps[2];

	FCommandContext& m_OwningContext;
	ID3D12DescriptorHeap* m_CurrentHeap;
	const D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType;
	FDescriptorHandle m_FirstDescriptor;
	uint32_t m_DescriptorSize;
	uint32_t m_CurrentOffset;
	std::vector<ID3D12DescriptorHeap*> m_RetiredHeaps;


	struct FDescriptorTableCache
	{
		FDescriptorTableCache() 
			: AssignedHandlesBitMap(0)
			, TableSize(0)
			, TableStart(nullptr) {}

		uint32_t AssignedHandlesBitMap;
		uint32_t TableSize;
		D3D12_CPU_DESCRIPTOR_HANDLE* TableStart;
	};

	struct FDescriptorHandleCache
	{
		FDescriptorHandleCache()
		{
			ClearCache();
		}

		void ClearCache()
		{
			m_StaleRootParamsBitMap = 0;
			m_RootDescriptorTablesBitMap = 0;
			m_MaxCachedDescriptors = 0;
			ZeroMemory(m_HandleCache, sizeof(m_HandleCache));
		}

		void UnbindAllInvalid();

		uint32_t ComputeStagedSize();
		void ParseRootSignature(D3D12_DESCRIPTOR_HEAP_TYPE Type, const FRootSignature& RootSignature);
		void StageDescriptorHandles(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[]);
		void CopyAndBindStaleTables(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t DescriptorSize, FDescriptorHandle DestHandleStart, ID3D12GraphicsCommandList* CmdList,
			void (STDMETHODCALLTYPE ID3D12GraphicsCommandList::*SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE));

		uint32_t m_RootDescriptorTablesBitMap = 0;
		uint32_t m_StaleRootParamsBitMap = 0;
		uint32_t m_MaxCachedDescriptors = 0;

		static const uint32_t MaxNumDescriptors = 256;
		static const uint32_t MaxNumDescriptorTables = 16;
		FDescriptorTableCache m_RootDescriptorTable[MaxNumDescriptorTables] = {};
		D3D12_CPU_DESCRIPTOR_HANDLE m_HandleCache[MaxNumDescriptors] = {};
	};
	FDescriptorHandleCache m_GraphicsHandleCache;
	FDescriptorHandleCache m_ComputeHandleCache;

	FDescriptorHandle AllocateDescriptor(UINT Count)
	{
		Assert (m_CurrentHeap != nullptr);
		FDescriptorHandle Result = m_FirstDescriptor + m_CurrentOffset * m_DescriptorSize;
		m_CurrentOffset += Count;
		return Result;
	}

	void CopyAndBindStagedTables(FDescriptorHandleCache& HandleCache, ID3D12GraphicsCommandList* CommandList,
		void(STDMETHODCALLTYPE ID3D12GraphicsCommandList::*SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE));
};