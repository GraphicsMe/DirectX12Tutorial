#include "CommandQueue.h"

#define SET_FENCE(f) ((uint64_t)m_CommandListType << 56 | f)

FCommandQueue::FCommandQueue(D3D12_COMMAND_LIST_TYPE type)
	: m_d3d12Device(nullptr)
	, m_CommandListType(type)
	, m_NextFenceValue(SET_FENCE(1))
	, m_LastCompletedFenceValue(SET_FENCE(0))
{

}

FCommandQueue::~FCommandQueue()
{
	Destroy();
}

void FCommandQueue::Create(ID3D12Device* Device)
{
	m_d3d12Device = Device;

	D3D12_COMMAND_QUEUE_DESC Desc = {};
	Desc.Type = m_CommandListType;
	Desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	Desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	Desc.NodeMask = 0;

	ThrowIfFailed(m_d3d12Device->CreateCommandQueue(&Desc, IID_PPV_ARGS(&m_d3d12CommandQueue)));
	ThrowIfFailed(m_d3d12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_d3d12Fence)));

	m_FenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

	Assert(m_FenceEvent && "Failed to create fence event handle.");
}

void FCommandQueue::Destroy()
{
	if (!m_FenceEvent)
	{
		return;
	}

	Flush();
	m_CommandListQueue.empty();
	//Assert(m_ReadyAllocators.empty());
	for (uint32_t i = 0; i < m_AllocatorPool.size(); ++i)
	{
		m_AllocatorPool[i]->Release();
	}
	m_AllocatorPool.clear();

	::CloseHandle(m_FenceEvent);
	m_FenceEvent = nullptr;
}

ComPtr<ID3D12GraphicsCommandList> FCommandQueue::GetCommandList()
{
	ComPtr<ID3D12GraphicsCommandList> commandList;

	ID3D12CommandAllocator* Allocator = RequestAllocator();
	if (!m_CommandListQueue.empty())
	{
		commandList = m_CommandListQueue.front();
		m_CommandListQueue.pop();

		ThrowIfFailed(commandList->Reset(Allocator, nullptr));
	}
	else
	{
		commandList = CreateCommandList(Allocator);
	}
	ThrowIfFailed(commandList->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), Allocator));

	return commandList;
}

uint64_t FCommandQueue::ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList> commandList)
{
	commandList->Close();  // finish recording command, should be called before ExecuteCommandLists

	//ID3D12CommandAllocator* commandAllocator;
	//UINT dataSize = sizeof(commandAllocator);
	//ThrowIfFailed(commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &commandAllocator));

	ID3D12CommandList* const ppCommandLists[] = {
		commandList.Get()
	};

	m_d3d12CommandQueue->ExecuteCommandLists(1, ppCommandLists);
	uint64_t fenceValue = Signal();

	//m_ReadyAllocators.emplace(CommandAllocatorEntry{ fenceValue, commandAllocator });

	//can be reused the next time the GetCommandList method is called.
	m_CommandListQueue.push(commandList);

	//commandAllocator->Release();

	return fenceValue;
}

uint64_t FCommandQueue::Signal()
{
	ThrowIfFailed(m_d3d12CommandQueue->Signal(m_d3d12Fence.Get(), m_NextFenceValue));

	return m_NextFenceValue++;
}

bool FCommandQueue::IsFenceComplete(uint64_t FenceValue)
{
	if (FenceValue > m_LastCompletedFenceValue)
	{
		m_LastCompletedFenceValue = std::max(m_LastCompletedFenceValue, m_d3d12Fence->GetCompletedValue());
	}
	return FenceValue <= m_LastCompletedFenceValue;
}

void FCommandQueue::WaitForFenceValue(uint64_t FenceValue)
{
	if (IsFenceComplete(FenceValue))
		return;

	ThrowIfFailed(m_d3d12Fence->SetEventOnCompletion(FenceValue, m_FenceEvent));
	::WaitForSingleObject(m_FenceEvent, INFINITE);
	m_LastCompletedFenceValue = FenceValue;
}

void FCommandQueue::Flush()
{
	uint64_t fenceValueForSignal = Signal();
	WaitForFenceValue(fenceValueForSignal);
}

ID3D12CommandAllocator* FCommandQueue::RequestAllocator()
{
	ID3D12CommandAllocator* Allocator = nullptr;
	if (!m_ReadyAllocators.empty() && IsFenceComplete(m_ReadyAllocators.front().FenceValue))
	{
		Allocator = m_ReadyAllocators.front().CommandAllocator;
		m_ReadyAllocators.pop();

		ThrowIfFailed(Allocator->Reset());
	}
	else
	{
		Allocator = CreateCommandAllocator();
		m_AllocatorPool.push_back(Allocator);
	}
	return Allocator;
}

void FCommandQueue::DiscardAllocator(uint64_t FenceValue, ID3D12CommandAllocator* CommandAllocator)
{
	m_ReadyAllocators.emplace(CommandAllocatorEntry{ FenceValue, CommandAllocator });
}

ID3D12CommandAllocator* FCommandQueue::CreateCommandAllocator()
{
	ID3D12CommandAllocator* CommandAllocator;
	ThrowIfFailed(m_d3d12Device->CreateCommandAllocator(m_CommandListType, IID_PPV_ARGS(&CommandAllocator)));
	return CommandAllocator;
}

ComPtr<ID3D12GraphicsCommandList> FCommandQueue::CreateCommandList(ComPtr<ID3D12CommandAllocator> allocator)
{
	ComPtr<ID3D12GraphicsCommandList> commandList;
	ThrowIfFailed(m_d3d12Device->CreateCommandList(0, m_CommandListType, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
	return commandList;
}
