#include "DescriptorAllocator.h"
#include "D3D12RHI.h"

bool DescriptorAllocation::IsNull() const
{
	return true;
}


DescriptorAllocatorPage::DescriptorAllocatorPage(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t Num)
	: m_HeapType(Type)
	, m_NumDescriptorsInHeap(Num)
{
	ComPtr<ID3D12Device> device = D3D12RHI::Get().GetD3D12Device();
 
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = m_HeapType;
    heapDesc.NumDescriptors = m_NumDescriptorsInHeap;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
 
    ThrowIfFailed( device->CreateDescriptorHeap( &heapDesc, IID_PPV_ARGS( &m_d3d12DescriptorHeap ) ) );
 
    m_BaseDescriptor = m_d3d12DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_DescriptorHandleIncrementSize = device->GetDescriptorHandleIncrementSize( m_HeapType );
    m_NumFreeHandles = m_NumDescriptorsInHeap;
}

void DescriptorAllocatorPage::AddNewBlock(uint32_t offset, uint32_t numDescriptors)
{

}

DescriptorAllocator::DescriptorAllocator(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t NumPerHeap /*= 256*/)
	: m_HeapType(Type)
	, m_NumDescriptorsPerHeap(NumPerHeap)
{
}

DescriptorAllocation DescriptorAllocator::Allocate(uint32_t Count /*= 1*/)
{
	DescriptorAllocation allocation;
	for (auto iter = m_AvailableHeaps.begin(); iter != m_AvailableHeaps.end(); ++iter)
	{
		auto AllocatorPage = m_HeapPool[*iter];
		allocation = AllocatorPage->Allocate(Count);
	}
}

DescriptorAllocatorPage DescriptorAllocator::CreateAllocatorPage()
{
	DescriptorAllocatorPage* NewPage = new DescriptorAllocatorPage(m_HeapType, m_NumDescriptorsPerHeap);
	m_HeapPool.emplace_back(NewPage);
	m_AvailableHeaps.insert(m_HeapPool.size() - 1);
 
	return m_HeapPool.back();
}
