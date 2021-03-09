#pragma once

#include "CommandQueue.h"

class FCommandListManager
{
	friend class FCommandContext;

public:
	FCommandListManager();
	~FCommandListManager();

	void Create(ID3D12Device* Device);
	void Destroy();

	FCommandQueue& GetGraphicsQueue() { return m_GraphicsQueue; }
	FCommandQueue& GetComputeQueue() { return m_ComputeQueue; }
	FCommandQueue& GetCopyQueue() { return m_CopyQueue; }

	FCommandQueue& GetQueue(D3D12_COMMAND_LIST_TYPE Type = D3D12_COMMAND_LIST_TYPE_DIRECT);

	void CreateNewCommandList(
		D3D12_COMMAND_LIST_TYPE Type, 
		ID3D12GraphicsCommandList** List, 
		ID3D12CommandAllocator** Allocator);

	bool IsFenceComplete(uint64_t FenceValue);
	void WaitForFence(uint64_t FenceValue);


private:
	ID3D12Device* m_Device;
	FCommandQueue m_GraphicsQueue;
	FCommandQueue m_ComputeQueue;
	FCommandQueue m_CopyQueue;
};