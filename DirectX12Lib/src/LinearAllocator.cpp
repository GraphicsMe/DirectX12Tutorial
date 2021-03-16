#include "LinearAllocator.h"
#include "MathLib.h"
#include "D3D12RHI.h"
#include "CommandQueue.h"
#include "CommandListManager.h"

extern FCommandListManager g_CommandListManager;

ELinearAllocatorType LinearAllocationPagePageManager::ms_TypeCounter = GpuExclusive;

LinearAllocationPagePageManager LinearAllocator::ms_PageManager[2];

LinearAllocationPage::LinearAllocationPage(ID3D12Resource* Resource, D3D12_RESOURCE_STATES State, size_t SizeInBytes)
	: FD3D12Resource(Resource, State)
	, m_PageSize(SizeInBytes)
	, m_CpuAddress(nullptr)
	, m_FenceValue(0)
{
	this->Map();
	GpuAddress = Resource->GetGPUVirtualAddress();
}

LinearAllocationPage::~LinearAllocationPage()
{
	this->Unmap();
}

void LinearAllocationPage::Map()
{
	if (m_CpuAddress == nullptr)
	{
		m_Resource->Map(0, nullptr, &m_CpuAddress);
		Assert(m_CpuAddress != nullptr);
	}
}

void LinearAllocationPage::Unmap()
{
	if (m_CpuAddress)
	{
		m_Resource->Unmap(0, nullptr);
		m_CpuAddress = nullptr;
	}
}

LinearAllocator::LinearAllocator(ELinearAllocatorType Type)
	: m_AllocatorType(Type)
	, m_CurrentPage(nullptr)
	, m_CurrentOffset(0)
{
	Assert(Type > ELinearAllocatorType::InvalidAllocator && Type < ELinearAllocatorType::NumAllocatorTypes);
	m_PageSize = (Type == ELinearAllocatorType::GpuExclusive ? GpuAllocatorPageSize : CpuAllocatorPageSize);
}

FAllocation LinearAllocator::Allocate(size_t SizeInBytes, size_t Alignment /*= DEFAULT_ALIGN*/)
{
	const size_t AlignmentMask = Alignment - 1;
	Assert((AlignmentMask & Alignment) == 0);
	const size_t AlignedSize = AlignUpWithMask(SizeInBytes, AlignmentMask);

	if (AlignedSize > m_PageSize)
		return AllocateLargePage(AlignedSize);

	m_CurrentOffset = AlignUp(m_CurrentOffset, Alignment);
	if (m_CurrentOffset + AlignedSize > m_PageSize)
	{
		Assert(m_CurrentPage != nullptr);
		// make sure current page is in UsingPages
		m_StandardPages.push_back(m_CurrentPage);
		m_CurrentPage = nullptr;
	}

	if (m_CurrentPage == nullptr)
	{
		m_CurrentPage = ms_PageManager[m_AllocatorType].RequestPage();
		Assert(m_CurrentPage != nullptr);
		m_CurrentOffset = 0;
	}

	Assert (m_CurrentPage != nullptr);
	FAllocation allocation;
	allocation.D3d12Resource = m_CurrentPage->GetResource();
	allocation.Offset = m_CurrentOffset;
	allocation.CPU = (uint8_t*)m_CurrentPage->m_CpuAddress + m_CurrentOffset;
	allocation.GpuAddress = m_CurrentPage->GpuAddress + m_CurrentOffset;
	m_CurrentOffset += AlignedSize;
	return allocation;
}

void LinearAllocator::CleanupUsedPages(uint64_t FenceID)
{
	if (m_CurrentPage != nullptr)
	{
		m_StandardPages.push_back(m_CurrentPage);
		m_CurrentPage = nullptr;
		m_CurrentOffset = 0;
	}
	for (auto Iter = m_StandardPages.begin(); Iter != m_StandardPages.end(); ++Iter)
	{
		(*Iter)->SetFenceValue(FenceID);
	}

	ms_PageManager[m_AllocatorType].DiscardStandardPages(FenceID, m_StandardPages);
	m_StandardPages.clear();

	ms_PageManager[m_AllocatorType].DiscardLargePages(FenceID, m_LargePages);
	m_LargePages.clear();
}

void LinearAllocator::DestroyAll()
{
	ms_PageManager[0].Destroy();
	ms_PageManager[1].Destroy();
}

FAllocation LinearAllocator::AllocateLargePage(size_t SizeInBytes)
{
	LinearAllocationPage* Page = ms_PageManager[m_AllocatorType].CreateNewPage(SizeInBytes);
	m_LargePages.push_back(Page);

	FAllocation allocation;
	allocation.D3d12Resource = Page->GetResource();
	allocation.Offset = 0;
	allocation.CPU = (uint8_t*)Page->m_CpuAddress;
	allocation.GpuAddress = Page->GpuAddress;
	return allocation;
}

LinearAllocationPagePageManager::LinearAllocationPagePageManager()
{
	m_AllocatorType = ms_TypeCounter;
	ms_TypeCounter = (ELinearAllocatorType)(ms_TypeCounter + 1);
	Assert(ms_TypeCounter <= NumAllocatorTypes);
}

LinearAllocationPage* LinearAllocationPagePageManager::RequestPage()
{
	LinearAllocationPage* Page = nullptr;
	if (!m_RetiredPages.empty() && g_CommandListManager.IsFenceComplete(m_RetiredPages.front()->GetFenceValue()))
	{
		Page = m_RetiredPages.front();
		m_RetiredPages.pop();
	}
	else
	{
		Page = CreateNewPage();
		m_StandardPagePool.push(Page);
	}
	return Page;
}

void LinearAllocationPagePageManager::DiscardStandardPages(uint64_t FenceID, const std::vector<LinearAllocationPage*>& Pages)
{
	for (auto Iter = Pages.begin(); Iter != Pages.end(); ++Iter)
	{
		(*Iter)->SetFenceValue(FenceID);
		m_RetiredPages.push(*Iter);
	}
}

void LinearAllocationPagePageManager::DiscardLargePages(uint64_t FenceID, const std::vector<LinearAllocationPage*>& Pages)
{
	while (!m_LargePagePool.empty() && g_CommandListManager.IsFenceComplete(m_LargePagePool.front()->GetFenceValue()))
	{
		delete m_LargePagePool.front();
		m_LargePagePool.pop();
	}
	for (auto Iter = Pages.begin(); Iter != Pages.end(); ++Iter)
	{
		(*Iter)->SetFenceValue(FenceID);
		m_LargePagePool.push(*Iter);
	}
}

LinearAllocationPage* LinearAllocationPagePageManager::CreateNewPage(size_t PageSize)
{
	D3D12_HEAP_PROPERTIES HeapProps;
	HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProps.CreationNodeMask = 1;
	HeapProps.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC ResourceDesc;
	ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	ResourceDesc.Alignment = 0;
	ResourceDesc.Height = 1;
	//uboResourceDesc.Width = (sizeof(m_uboVS) + 255) & ~255;
	ResourceDesc.DepthOrArraySize = 1;
	ResourceDesc.MipLevels = 1;
	ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.SampleDesc.Quality = 0;
	ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_RESOURCE_STATES DefaultUsage;
	if (m_AllocatorType == ELinearAllocatorType::GpuExclusive)
	{
		HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		ResourceDesc.Width = PageSize == 0 ? GpuAllocatorPageSize : PageSize;
		ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		DefaultUsage = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	}
	else
	{
		HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		ResourceDesc.Width = PageSize == 0 ? CpuAllocatorPageSize : PageSize;
		ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
		DefaultUsage = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	ID3D12Resource* pBuffer = nullptr;
	ThrowIfFailed(D3D12RHI::Get().GetD3D12Device()->CreateCommittedResource(
		&HeapProps,
		D3D12_HEAP_FLAG_NONE,
		&ResourceDesc,
		DefaultUsage,
		nullptr,
		IID_PPV_ARGS(&pBuffer)));

	pBuffer->SetName(L"LinearAllocatorPage");

	return new LinearAllocationPage(pBuffer, DefaultUsage, PageSize);
}

void LinearAllocationPagePageManager::Destroy()
{
	while (!m_LargePagePool.empty())
	{
		delete m_LargePagePool.front();
		m_LargePagePool.pop();
	}
	while (!m_StandardPagePool.empty())
	{
		delete m_StandardPagePool.front();
		m_StandardPagePool.pop();
	}
}

