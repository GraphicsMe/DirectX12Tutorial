#include "CommandListManager.h"

FCommandListManager::FCommandListManager()
	: m_Device(nullptr)
	, m_GraphicsQueue(D3D12_COMMAND_LIST_TYPE_DIRECT)
	, m_ComputeQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE)
	, m_CopyQueue(D3D12_COMMAND_LIST_TYPE_COPY)
{

}

FCommandListManager::~FCommandListManager()
{
	Destroy();
}

void FCommandListManager::Create(ID3D12Device* Device)
{
	Assert(Device != nullptr);
	m_Device = Device;
	m_GraphicsQueue.Create(Device);
	m_ComputeQueue.Create(Device);
	m_CopyQueue.Create(Device);
}

void FCommandListManager::Destroy()
{
	m_GraphicsQueue.Destroy();
	m_ComputeQueue.Destroy();
	m_CopyQueue.Destroy();
}

FCommandQueue& FCommandListManager::GetQueue(D3D12_COMMAND_LIST_TYPE Type /*= D3D12_COMMAND_LIST_TYPE_DIRECT*/)
{
	switch (Type)
	{
	case D3D12_COMMAND_LIST_TYPE_COMPUTE: return m_ComputeQueue;
	case D3D12_COMMAND_LIST_TYPE_COPY: return m_CopyQueue;
	default: return m_GraphicsQueue;
	}
}

void FCommandListManager::CreateNewCommandList(D3D12_COMMAND_LIST_TYPE Type, ID3D12GraphicsCommandList** List, ID3D12CommandAllocator** Allocator)
{
	switch (Type)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		*Allocator = m_GraphicsQueue.RequestAllocator();
		break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		*Allocator = m_ComputeQueue.RequestAllocator();
		break;
	case D3D12_COMMAND_LIST_TYPE_COPY:
		*Allocator = m_CopyQueue.RequestAllocator();
		break;
	}
	ThrowIfFailed(m_Device->CreateCommandList(1, Type, *Allocator, nullptr, IID_PPV_ARGS(List)));
	(*List)->SetName(L"CommandList");
}

bool FCommandListManager::IsFenceComplete(uint64_t FenceValue)
{
	return GetQueue(D3D12_COMMAND_LIST_TYPE(FenceValue >> 56)).IsFenceComplete(FenceValue);
}

