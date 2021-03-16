#pragma once

#include "Common.h"
#include <queue>
#include <d3d12.h>

class FCommandQueue
{
public:
	FCommandQueue(D3D12_COMMAND_LIST_TYPE type);
	virtual ~FCommandQueue();

	void Create(ID3D12Device* Device);
	void Destroy();
	
	// Execute a command list.
    // Returns the fence value to wait for for this command list.
	uint64_t ExecuteCommandList(ComPtr<ID3D12GraphicsCommandList> commandList);

	uint64_t Signal();
	bool IsFenceComplete(uint64_t FenceValue);
	void WaitForFenceValue(uint64_t FenceValue);
	void Flush();
 
	ID3D12CommandQueue* GetD3D12CommandQueue() const { return m_d3d12CommandQueue.Get(); }
	ID3D12CommandAllocator* RequestAllocator();
	void DiscardAllocator(uint64_t FenceValue, ID3D12CommandAllocator* CommandAllocator);

protected:
	ID3D12CommandAllocator* CreateCommandAllocator();

private:
    // Keep track of command allocators that are "in-flight"
    struct CommandAllocatorEntry
    {
		uint64_t FenceValue;
		ID3D12CommandAllocator* CommandAllocator;
    };
 
    using CommandListQueue = std::queue< ComPtr<ID3D12GraphicsCommandList> >;
 
	D3D12_COMMAND_LIST_TYPE		m_CommandListType;
	ID3D12Device*				m_d3d12Device;
	ComPtr<ID3D12CommandQueue>	m_d3d12CommandQueue;
	ComPtr<ID3D12Fence>			m_d3d12Fence;
	HANDLE						m_FenceEvent;
	uint64_t					m_NextFenceValue;
	uint64_t					m_LastCompletedFenceValue;
 
	std::queue< CommandAllocatorEntry>		m_ReadyAllocators;
	std::vector<ID3D12CommandAllocator*>	m_AllocatorPool;

	CommandListQueue			m_CommandListQueue;
};

