#include "CommandQueue.h"


CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE type)
	: m_d3d12Device(nullptr)
	, m_CommandListType(type)
	, m_FenceValue(0)
{

}

CommandQueue::~CommandQueue()
{
	Destroy();
}

void CommandQueue::Create(ID3D12Device* Device)
{
	m_d3d12Device = Device;

	D3D12_COMMAND_QUEUE_DESC Desc = {};
	Desc.Type = m_CommandListType;
	Desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	Desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	Desc.NodeMask = 0;

	ThrowIfFailed(m_d3d12Device->CreateCommandQueue(&Desc, IID_PPV_ARGS(&m_d3d12CommandQueue)));
	ThrowIfFailed(m_d3d12Device->CreateFence(m_FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_d3d12Fence)));

	m_FenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

	Assert(m_FenceEvent && "Failed to create fence event handle.");
}

void CommandQueue::Destroy()
{
	if (!m_FenceEvent)
	{
		return;
	}

	Flush();
	m_CommandListQueue.empty();
	while (!m_CommandAllocatorQueue.empty())
	{
		m_CommandAllocatorQueue.front().CommandAllocator->Release();
		m_CommandAllocatorQueue.pop();
	}
	m_CommandAllocatorQueue.empty();

	::CloseHandle(m_FenceEvent);
	m_FenceEvent = nullptr;
}

ComPtr<ID3D12GraphicsCommandList> CommandQueue::GetCommandList()
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

uint64_t CommandQueue::ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList> commandList)
{
	commandList->Close();  // finish recording command, should be called before ExecuteCommandLists
 
	ID3D12CommandAllocator* commandAllocator;
	UINT dataSize = sizeof(commandAllocator);
	ThrowIfFailed(commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &commandAllocator));
 
	ID3D12CommandList* const ppCommandLists[] = {
		commandList.Get()
	};
 
	m_d3d12CommandQueue->ExecuteCommandLists(1, ppCommandLists);
	uint64_t fenceValue = Signal();
 
	m_CommandAllocatorQueue.emplace(CommandAllocatorEntry{ fenceValue, commandAllocator });

	//can be reused the next time the GetCommandList method is called.
	m_CommandListQueue.push(commandList);

	commandAllocator->Release();

	return fenceValue;
}

uint64_t CommandQueue::Signal()
{
    uint64_t fenceValueForSignal = ++m_FenceValue;
    ThrowIfFailed(m_d3d12CommandQueue->Signal(m_d3d12Fence.Get(), fenceValueForSignal));
 
    return fenceValueForSignal;
}

bool CommandQueue::IsFenceComplete(uint64_t fenceValue)
{
	return m_d3d12Fence->GetCompletedValue() >= fenceValue;
}

void CommandQueue::WaitForFenceValue(uint64_t fenceValue)
{
	if (m_d3d12Fence->GetCompletedValue() < fenceValue)
    {
        ThrowIfFailed(m_d3d12Fence->SetEventOnCompletion(fenceValue, m_FenceEvent));
        ::WaitForSingleObject(m_FenceEvent, INFINITE);
    }
}

void CommandQueue::Flush()
{
	uint64_t fenceValueForSignal = Signal();
    WaitForFenceValue(fenceValueForSignal);
}

ID3D12CommandAllocator* CommandQueue::RequestAllocator()
{
	ID3D12CommandAllocator* Allocator = nullptr;
	if (!m_CommandAllocatorQueue.empty() && IsFenceComplete(m_CommandAllocatorQueue.front().FenceValue))
	{
		Allocator = m_CommandAllocatorQueue.front().CommandAllocator;
		m_CommandAllocatorQueue.pop();

		ThrowIfFailed(Allocator->Reset());
	}
	else
	{
		Allocator = CreateCommandAllocator();
	}
	return Allocator;
}

ID3D12CommandAllocator* CommandQueue::CreateCommandAllocator()
{
	ID3D12CommandAllocator* CommandAllocator;
	ThrowIfFailed(m_d3d12Device->CreateCommandAllocator(m_CommandListType, IID_PPV_ARGS(&CommandAllocator)));
	return CommandAllocator;
}

Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CommandQueue::CreateCommandList(ComPtr<ID3D12CommandAllocator> allocator)
{
	ComPtr<ID3D12GraphicsCommandList> commandList;
	ThrowIfFailed(m_d3d12Device->CreateCommandList(0, m_CommandListType, allocator.Get(), nullptr, IID_PPV_ARGS(&commandList)));
	return commandList;
}
