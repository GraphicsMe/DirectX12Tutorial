﻿#include "LinearAllocator.h"
#include "MathLib.h"
#include "D3D12RHI.h"
#include "CommandQueue.h"

LinearAllocationPage::LinearAllocationPage(ComPtr<ID3D12Resource> Resource, uint32_t SizeInBytes)
	: m_d3d12Resource(Resource)
	, m_PageSize(SizeInBytes)
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
		m_d3d12Resource->Map(0, nullptr, &m_CpuAddress);
	}
}

void LinearAllocationPage::Unmap()
{
	if (m_CpuAddress)
	{
		m_d3d12Resource->Unmap(0, nullptr);
		m_CpuAddress = nullptr;
	}
}

LinearAllocator::LinearAllocator(ELinearAllocatorType Type)
	: m_AllocatorType(Type)
{
	Assert(Type > ELinearAllocatorType::InvalidAllocator && Type < ELinearAllocatorType::NumAllocatorTypes);
	m_PageSize = (Type == ELinearAllocatorType::GpuExclusive ? GpuAllocatorPageSize : CpuAllocatorPageSize);
}

FAllocation LinearAllocator::Allocate(uint32_t SizeInBytes, uint32_t Alignment /*= DEFAULT_ALIGN*/)
{
	const uint32_t AlignmentMask = Alignment - 1;
	Assert((AlignmentMask & Alignment) == 0);
	const uint32_t AlignedSize = AlignUpWithMask(SizeInBytes, AlignmentMask);

	Assert(AlignedSize <= m_PageSize);
	//if (AlignedSize > m_PageSize)
	//	return AllocateLargepage(AlignedSize);

	m_CurrentOffset = AlignUp(m_CurrentOffset, Alignment);
	if (m_CurrentOffset + AlignedSize > m_PageSize)
	{
		Assert(m_CurrentPage != nullptr);
		// make sure current page is in UsingPages
		m_UsingPages.push_back(m_CurrentPage);
		m_CurrentPage = nullptr;
	}

	if (m_CurrentPage == nullptr)
	{
		m_CurrentPage = RequestPage(m_PageSize);
		m_CurrentOffset = 0;
	}

	Assert (m_CurrentPage != nullptr);
	FAllocation allocation;
	allocation.D3d12Resource = m_CurrentPage->m_d3d12Resource.Get();
	allocation.Offset = m_CurrentOffset;
	allocation.CPU = (uint8_t*)m_CurrentPage->m_CpuAddress + m_CurrentOffset;
	allocation.GpuAddress = m_CurrentPage->GpuAddress + m_CurrentOffset;
	m_CurrentOffset += AlignedSize;
	return allocation;
}

void LinearAllocator::Destroy()
{
	while (!m_RetiredPages.empty())
	{
		LinearAllocationPage* Page = m_RetiredPages.front();
		m_RetiredPages.pop_front();
		delete Page;
	}

	while (!m_UsingPages.empty())
	{
		LinearAllocationPage* Page = m_UsingPages.front();
		m_UsingPages.pop_front();
		delete Page;
	}

	if (m_CurrentPage)
	{
		delete m_CurrentPage;
		m_CurrentPage = nullptr;
	}
}

LinearAllocationPage* LinearAllocator::AllocateLargePage(uint32_t SizeInBytes)
{
	return nullptr;
}

LinearAllocationPage* LinearAllocator::RequestPage(uint32_t SizeInBytes)
{
	CommandQueue* commandQueue = D3D12RHI::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_COPY);
	LinearAllocationPage* Page = nullptr;
	while (!m_RetiredPages.empty() && commandQueue->IsFenceComplete(m_RetiredPages.front()->GetFenceValue()))
	{
		Page = m_RetiredPages.front();
		m_RetiredPages.pop_front();
	}
	if (!Page)
	{
		Page = CreateNewPage(SizeInBytes);
	}

	return Page;
}

LinearAllocationPage* LinearAllocator::CreateNewPage(uint32_t PageSize)
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

	ComPtr<ID3D12Resource> pBuffer;
	ThrowIfFailed(D3D12RHI::Get().GetD3D12Device()->CreateCommittedResource(
		&HeapProps,
		D3D12_HEAP_FLAG_NONE,
		&ResourceDesc,
		DefaultUsage,
		nullptr,
		IID_PPV_ARGS(&pBuffer)));

	pBuffer->SetName(L"LinearAllocatorPage");

	return new LinearAllocationPage(pBuffer, PageSize);
}