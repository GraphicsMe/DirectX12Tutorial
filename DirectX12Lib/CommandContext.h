#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <d3d12.h>

#include "LinearAllocator.h"

class FCommandContext;

class FContextManager
{
public:
	FContextManager() {}

	FCommandContext* AllocateContext(D3D12_COMMAND_LIST_TYPE Type);
	void FreeContext(FCommandContext* CommandContext);
	void DestroyAllContexts();

private:
	std::vector<std::unique_ptr<FCommandContext>> m_ContextPool[4];
	std::queue<FCommandContext*> m_AvailableContexts[4];
};
 

struct NonCopyable
{
	NonCopyable() = default;
	NonCopyable(const NonCopyable&) = delete;
	NonCopyable& operator=(const NonCopyable) = delete;
};

class FCommandContext : NonCopyable
{
	friend FContextManager;
private:
	FCommandContext(D3D12_COMMAND_LIST_TYPE Type);

	void Reset(void);

public:
	static void DestroyAllContexts();
	static FCommandContext& Begin(D3D12_COMMAND_LIST_TYPE Type=D3D12_COMMAND_LIST_TYPE_DIRECT, const std::wstring& ID = L"");

public:
	~FCommandContext();

	void Initialize();
	void SetID(const std::wstring& ID) { m_ID = ID; }
	uint64_t FinishFrame(bool WaitForCompletion = false);

	FAllocation ReserveUploadMemory(uint32_t SizeInBytes);

	void FlushResourceBarriers();
	ID3D12GraphicsCommandList* GetCommandList() { return m_CommandList; }


protected:
	std::wstring m_ID;
	ID3D12GraphicsCommandList* m_CommandList;
	ID3D12CommandAllocator* m_CurrentAllocator;

	ID3D12RootSignature* m_CurGraphicsRootSignature;
	ID3D12PipelineState* m_CurPipelineState;
	ID3D12RootSignature* m_CurComputeRootSignature;

	D3D12_RESOURCE_BARRIER m_ResourceBarrierBuffer[16];
	UINT m_NumBarriersToFlush;

	D3D12_COMMAND_LIST_TYPE m_Type;

	LinearAllocator m_CpuLinearAllocator;
	LinearAllocator m_GpuLinearAllocator;
};