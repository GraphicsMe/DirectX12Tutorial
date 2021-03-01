﻿#include "GpuBuffer.h"
#include "D3D12RHI.h"
#include "CommandContext.h"

void FGpuBuffer::Create(const std::wstring& Name, uint32_t NumElements, uint32_t ElementSize, const void* InitData /*= nullptr*/)
{
	Destroy();

	m_ElementCount = NumElements;
	m_ElementSize = ElementSize;
	m_BufferSize = NumElements * ElementSize;

	D3D12_RESOURCE_DESC ResDesc = DescribeBuffer();

	m_CurrentState = D3D12_RESOURCE_STATE_COMMON;

	D3D12_HEAP_PROPERTIES HeapProps;
	HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	HeapProps.CreationNodeMask = 1;
	HeapProps.VisibleNodeMask = 1;

	ThrowIfFailed(D3D12RHI::Get().GetD3D12Device()->CreateCommittedResource(
		&HeapProps, D3D12_HEAP_FLAG_NONE,
		&ResDesc, m_CurrentState, nullptr, IID_PPV_ARGS(&m_Resource)
	));

	m_GpuAddress = m_Resource->GetGPUVirtualAddress();
	if (InitData)
	{
		FCommandContext::InitializeBuffer(*this, InitData, m_BufferSize);
	}
}

D3D12_VERTEX_BUFFER_VIEW FGpuBuffer::VertexBufferView(size_t Offset, uint32_t Size, uint32_t Stride) const
{
	D3D12_VERTEX_BUFFER_VIEW VBView;
	VBView.BufferLocation = m_GpuAddress + Offset;
	VBView.SizeInBytes = Size;
	VBView.StrideInBytes = Stride;
	return VBView;
}

D3D12_INDEX_BUFFER_VIEW FGpuBuffer::IndexBufferView(size_t Offset, uint32_t Size, bool b32Bit /*= false*/) const
{
	D3D12_INDEX_BUFFER_VIEW IBView;
	IBView.BufferLocation = m_GpuAddress + Offset;
	IBView.Format = b32Bit ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
	IBView.SizeInBytes = Size;
	return IBView;
}

D3D12_RESOURCE_DESC FGpuBuffer::DescribeBuffer()
{
	Assert(m_BufferSize != 0);

	D3D12_RESOURCE_DESC Desc = {};
	Desc.Alignment = 0;
	Desc.DepthOrArraySize = 1;
	Desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	Desc.Flags = m_ResourceFlags;
	Desc.Format = DXGI_FORMAT_UNKNOWN;
	Desc.Width = (UINT64)m_BufferSize;
	Desc.Height = 1;
	Desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	Desc.MipLevels = 1;
	Desc.SampleDesc.Count = 1;
	Desc.SampleDesc.Quality = 0;
	return Desc;
}

