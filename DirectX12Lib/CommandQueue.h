#pragma once

#include "Common.h"
#include <queue>
#include <d3d12.h>

class CommandQueue
{
public:
	CommandQueue(ComPtr<ID3D12Device> device, D3D12_COMMAND_LIST_TYPE type);
	virtual ~CommandQueue();
	
	ComPtr<ID3D12GraphicsCommandList> GetCommandList();

	// Execute a command list.
    // Returns the fence value to wait for for this command list.
	uint64_t ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList> commandList);

	uint64_t Signal();
	bool IsFenceComplete(uint64_t fenceValue);
	void WaitForFenceValue(uint64_t fenceValue);
	void Flush();
 
	ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const;
	ID3D12CommandAllocator* RequestAllocator();

protected:
	ID3D12CommandAllocator* CreateCommandAllocator();
	ComPtr<ID3D12GraphicsCommandList> CreateCommandList(ComPtr<ID3D12CommandAllocator> allocator);

private:
    // Keep track of command allocators that are "in-flight"
    struct CommandAllocatorEntry
    {
		uint64_t FenceValue;
		ID3D12CommandAllocator* CommandAllocator;
    };
 
    using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
    using CommandListQueue = std::queue< ComPtr<ID3D12GraphicsCommandList> >;
 
	D3D12_COMMAND_LIST_TYPE		m_CommandListType;
	ComPtr<ID3D12Device>		m_d3d12Device;
	ComPtr<ID3D12CommandQueue>	m_d3d12CommandQueue;
	ComPtr<ID3D12Fence>			m_d3d12Fence;
	HANDLE						m_FenceEvent;
	uint64_t					m_FenceValue;
 
	CommandAllocatorQueue		m_CommandAllocatorQueue;
	CommandListQueue			m_CommandListQueue;
};

