#include "DescriptorAllocator.h"
#include "D3D12RHI.h"

std::vector<ComPtr<ID3D12DescriptorHeap> > FDescriptorAllocator::sm_DescriptorPool;

FDescriptorAllocator::FDescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE Type)
	: m_HeapType(Type)
{
}

D3D12_CPU_DESCRIPTOR_HANDLE FDescriptorAllocator::Allocate(uint32_t Count)
{
	if (m_CurrentHeap == nullptr || m_RemainingFreeHandles < Count)
	{
		m_CurrentHeap = RequestNewHeap(m_HeapType);
		m_CurrentCpuAddress = m_CurrentHeap->GetCPUDescriptorHandleForHeapStart();
		m_RemainingFreeHandles = sm_NumDescriptorsPerHeap;
		if (m_DescriptorSize == 0)
		{
			m_DescriptorSize = D3D12RHI::Get().GetD3D12Device()->GetDescriptorHandleIncrementSize(m_HeapType);
		}
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Result = m_CurrentCpuAddress;
	m_CurrentCpuAddress.ptr += Count * m_DescriptorSize;
	m_RemainingFreeHandles -= Count;

	return Result;
}

void FDescriptorAllocator::DestroyAll()
{
	sm_DescriptorPool.clear();
}

ID3D12DescriptorHeap* FDescriptorAllocator::RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
	D3D12_DESCRIPTOR_HEAP_DESC Desc = {};
	Desc.NumDescriptors = sm_NumDescriptorsPerHeap;
	Desc.Type = Type;
	Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	Desc.NodeMask = 1;

	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	ThrowIfFailed(D3D12RHI::Get().GetD3D12Device()->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&descriptorHeap)));
	sm_DescriptorPool.emplace_back(descriptorHeap);
	return descriptorHeap.Get();
}
