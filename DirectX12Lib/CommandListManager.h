#pragma once

#include "CommandQueue.h"

class FCommandListManager
{
	friend class FCommandContext;

public:
	FCommandListManager();
	~FCommandListManager();

	void Create(ComPtr<ID3D12Device> Device);
	void Destroy();

	CommandQueue& GetGraphicsQueue() { return m_GraphicsQueue; }
	CommandQueue& GetComputeQueue() { return m_ComputeQueue; }
	CommandQueue& GetCopyQueue() { return m_CopyQueue; }

	CommandQueue& GetQueue(D3D12_COMMAND_LIST_TYPE Type = D3D12_COMMAND_LIST_TYPE_DIRECT);

	void CreateNewCommandList(
		D3D12_COMMAND_LIST_TYPE Type, 
		ID3D12GraphicsCommandList** List, 
		ID3D12CommandAllocator** Allocator);

	bool IsFenceComplete() const;


private:
	ComPtr<ID3D12Device> Device;
	CommandQueue m_GraphicsQueue;
	CommandQueue m_ComputeQueue;
	CommandQueue m_CopyQueue;
};