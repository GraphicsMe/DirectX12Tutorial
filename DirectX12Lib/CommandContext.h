#pragma once

#include <vector>
#include <queue>
#include <memory>
#include <d3d12.h>

#include "LinearAllocator.h"

class FCommandContext;
class FRootSignature;
class FD3D12Resource;

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
	static void InitializeBuffer(FD3D12Resource& Dest, const void* Data, uint32_t NumBytes, size_t Offset = 0);

public:
	~FCommandContext();

	void Initialize();
	void SetID(const std::wstring& ID) { m_ID = ID; }
	uint64_t FinishFrame(bool WaitForCompletion = false);

	FAllocation ReserveUploadMemory(uint32_t SizeInBytes);

	void FlushResourceBarriers();
	ID3D12GraphicsCommandList* GetCommandList() { return m_CommandList; }

	void TransitionResource(FD3D12Resource& Resource, D3D12_RESOURCE_STATES NewState, bool Flush = false);

	void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* HeapPtr);
	void SetDescriptorHeaps(UINT HeapCount, D3D12_DESCRIPTOR_HEAP_TYPE Type[], ID3D12DescriptorHeap* HeapPtrs[]);

	void SetRootSignature(const FRootSignature& RootSignature);
	void SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY Topology);
	void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& View);
	void SetVertexBuffer(UINT Slot, const D3D12_VERTEX_BUFFER_VIEW& View);
	void SetVertexBuffers(UINT StartSlot, UINT Count, const D3D12_VERTEX_BUFFER_VIEW View[]);

	void SetViewport(const D3D12_VIEWPORT& vp);
	void SetScissor(const D3D12_RECT& rect);
	void SetScissor(UINT left, UINT top, UINT right, UINT bottom);
	void SetViewport(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT minDepth = 0.0f, FLOAT maxDepth = 1.0f);
	void SetViewportAndScissor(UINT x, UINT y, UINT w, UINT h);

protected:
	void BindDescriptorHeaps();

protected:
	std::wstring m_ID;
	ID3D12GraphicsCommandList* m_CommandList;
	ID3D12CommandAllocator* m_CurrentAllocator;

	ID3D12RootSignature* m_CurGraphicsRootSignature;
	ID3D12PipelineState* m_CurPipelineState;
	ID3D12RootSignature* m_CurComputeRootSignature;

	D3D12_RESOURCE_BARRIER m_ResourceBarrierBuffer[16];
	UINT m_NumBarriersToFlush;

	ID3D12DescriptorHeap* m_CurrentDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	D3D12_COMMAND_LIST_TYPE m_Type;

	LinearAllocator m_CpuLinearAllocator;
	LinearAllocator m_GpuLinearAllocator;
};