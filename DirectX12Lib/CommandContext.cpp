#include "CommandContext.h"
#include "CommandListManager.h"

extern FContextManager g_ContextManager;
extern FCommandListManager g_CommandListManager;

FCommandContext* FContextManager::AllocateContext(D3D12_COMMAND_LIST_TYPE Type)
{
	FCommandContext* Result = nullptr;

	std::queue<FCommandContext*>& AvailableContexts = m_AvailableContexts[Type];
	if (AvailableContexts.empty())
	{
		Result = new FCommandContext(Type);
		m_ContextPool[Type].emplace_back(Result);
		Result->Initialize();
	}
	else
	{
		Result = AvailableContexts.front();
		AvailableContexts.pop();
		Result->Reset();
	}
	Assert(Result != nullptr);
	Assert(Result->m_Type == Type);
	return Result;
}

void FContextManager::FreeContext(FCommandContext* CommandContext)
{
	Assert(CommandContext != nullptr);
	m_AvailableContexts[CommandContext->m_Type].push(CommandContext);
}

void FContextManager::DestroyAllContexts()
{
	for (uint32_t i = 0; i < 4; ++i)
	{
		m_ContextPool[i].clear();
		Assert(m_AvailableContexts[i].empty());
	}
}

void FCommandContext::DestroyAllContexts()
{

}

FCommandContext& FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE Type, const std::wstring& ID /*= L""*/)
{
	FCommandContext* NewContext = g_ContextManager.AllocateContext(Type);
	NewContext->SetID(ID);
	return *NewContext;
}

FCommandContext::~FCommandContext()
{
	if (m_CommandList)
	{
		m_CommandList->Release();
		m_CommandList = nullptr;
	}
	m_CpuLinearAllocator.Destroy();
	m_GpuLinearAllocator.Destroy();
}

void FCommandContext::Initialize()
{
	g_CommandListManager.CreateNewCommandList(m_Type, &m_CommandList, &m_CurrentAllocator);
}

FCommandContext::FCommandContext(D3D12_COMMAND_LIST_TYPE Type)
	: m_Type(Type)
	, m_CpuLinearAllocator(ELinearAllocatorType::CpuWritable)
	, m_GpuLinearAllocator(ELinearAllocatorType::GpuExclusive)
{
	m_CommandList = nullptr;
	m_CurrentAllocator = nullptr;
	
	m_CurGraphicsRootSignature = nullptr;
	m_CurPipelineState = nullptr;
	m_CurComputeRootSignature = nullptr;
	m_NumBarriersToFlush = 0;
}

void FCommandContext::Reset(void)
{
	Assert(m_CommandList != nullptr && m_CurrentAllocator == nullptr);
	m_CurrentAllocator = g_CommandListManager.GetQueue(m_Type).RequestAllocator();
	m_CommandList->Reset(m_CurrentAllocator, nullptr);

	m_CurGraphicsRootSignature = nullptr;
	m_CurPipelineState = nullptr;
	m_CurComputeRootSignature = nullptr;
	m_NumBarriersToFlush = 0;

	//todo: bind descriptor heaps
}

uint64_t FCommandContext::FinishFrame(bool WaitForCompletion /*= false*/)
{
	FlushResourceBarriers();

	Assert(m_CurrentAllocator != nullptr);

	FCommandQueue& Queue = g_CommandListManager.GetQueue(m_Type);

	uint64_t FenceValue = Queue.ExecuteCommandList(m_CommandList);
	Queue.DiscardAllocator(FenceValue, m_CurrentAllocator);
	m_CurrentAllocator = nullptr;

	m_CpuLinearAllocator.CleanupUsedPages(FenceValue);
	m_GpuLinearAllocator.CleanupUsedPages(FenceValue);
	
	if (WaitForCompletion)
	{
		g_CommandListManager.WaitForFence(FenceValue);
	}
	
	// todo: free context
	return FenceValue;
}

FAllocation FCommandContext::ReserveUploadMemory(uint32_t SizeInBytes)
{
	return m_CpuLinearAllocator.Allocate(SizeInBytes);
}

void FCommandContext::FlushResourceBarriers()
{
	if (m_NumBarriersToFlush > 0)
	{
		m_CommandList->ResourceBarrier(m_NumBarriersToFlush, m_ResourceBarrierBuffer);
		m_NumBarriersToFlush = 0;
	}
}
