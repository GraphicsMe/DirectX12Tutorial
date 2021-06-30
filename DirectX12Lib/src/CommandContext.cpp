#include "CommandContext.h"
#include "CommandListManager.h"
#include "RootSignature.h"
#include "DepthBuffer.h"
#include "ColorBuffer.h"
#include "CubeBuffer.h"
#include "PipelineState.h"

#include "d3dx12.h"

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
	}
}

void FCommandContext::DestroyAllContexts()
{
	LinearAllocator::DestroyAll();
	g_ContextManager.DestroyAllContexts();
}

FCommandContext& FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE Type, const std::wstring& ID /*= L""*/)
{
	FCommandContext* NewContext = g_ContextManager.AllocateContext(Type);
	NewContext->SetID(ID);
	return *NewContext;
}

void FCommandContext::InitializeBuffer(FD3D12Resource& Dest, const void* Data, uint32_t NumBytes, size_t Offset /*= 0*/)
{
	FCommandContext& CommandContext = FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE_DIRECT);

	FAllocation Allocation = CommandContext.ReserveUploadMemory(NumBytes);
	memcpy(Allocation.CPU, Data, NumBytes);

	CommandContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST, true);
	CommandContext.m_CommandList->CopyBufferRegion(Dest.GetResource(), Offset, Allocation.D3d12Resource, 0, NumBytes);
	CommandContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_GENERIC_READ, true);

	CommandContext.Finish(true);
}

void FCommandContext::InitializeTexture(FD3D12Resource& Dest, UINT NumSubResources, D3D12_SUBRESOURCE_DATA SubData[])
{
	size_t UploadBufferSize = (size_t)GetRequiredIntermediateSize(Dest.GetResource(), 0, NumSubResources);

	FCommandContext& CommandContext = FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE_DIRECT);
	FAllocation Allocation = CommandContext.ReserveUploadMemory(UploadBufferSize);
	UpdateSubresources(CommandContext.m_CommandList, Dest.GetResource(), Allocation.D3d12Resource, 0, 0, NumSubResources, SubData);
	CommandContext.TransitionResource(Dest,D3D12_RESOURCE_STATE_GENERIC_READ);
	CommandContext.Finish(true);
}

FCommandContext::~FCommandContext()
{
	if (m_CommandList)
	{
		m_CommandList->Release();
		m_CommandList = nullptr;
	}
}

void FCommandContext::Initialize()
{
	g_CommandListManager.CreateNewCommandList(m_Type, &m_CommandList, &m_CurrentAllocator);
}

void FCommandContext::SetID(const std::wstring& ID)
{
	m_ID = ID;
	m_CommandList->SetName(m_ID.c_str());
}

FCommandContext::FCommandContext(D3D12_COMMAND_LIST_TYPE Type)
	: m_Type(Type)
	, m_CpuLinearAllocator(ELinearAllocatorType::CpuWritable)
	, m_GpuLinearAllocator(ELinearAllocatorType::GpuExclusive)
	, m_DynamicSamplerDescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
	, m_DynamicViewDescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
{
	ZeroMemory(m_CurrentDescriptorHeaps, sizeof(m_CurrentDescriptorHeaps));
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

	BindDescriptorHeaps();
}

uint64_t FCommandContext::Flush(bool WaitForCompletion /*= false*/)
{
	FlushResourceBarriers();
	
	Assert(m_CurrentAllocator != nullptr);

	uint64_t FenceValue = g_CommandListManager.GetQueue(m_Type).ExecuteCommandList(m_CommandList);
	if (WaitForCompletion)
	{
		g_CommandListManager.WaitForFence(FenceValue);
	}

	m_CommandList->Reset(m_CurrentAllocator, nullptr);
	if (m_CurGraphicsRootSignature)
	{
		m_CommandList->SetGraphicsRootSignature(m_CurGraphicsRootSignature);
	}
	if (m_CurComputeRootSignature)
	{
		m_CommandList->SetComputeRootSignature(m_CurComputeRootSignature);
	}
	if (m_CurPipelineState)
	{
		m_CommandList->SetPipelineState(m_CurPipelineState);
	}
	BindDescriptorHeaps();
	return FenceValue;
}

uint64_t FCommandContext::Finish(bool WaitForCompletion /*= false*/)
{
	FlushResourceBarriers();

	Assert(m_CurrentAllocator != nullptr);

	FCommandQueue& Queue = g_CommandListManager.GetQueue(m_Type);

	uint64_t FenceValue = Queue.ExecuteCommandList(m_CommandList);
	Queue.DiscardAllocator(FenceValue, m_CurrentAllocator);
	m_CurrentAllocator = nullptr;

	m_CpuLinearAllocator.CleanupUsedPages(FenceValue);
	m_GpuLinearAllocator.CleanupUsedPages(FenceValue);
	m_DynamicViewDescriptorHeap.CleanupUsedHeaps(FenceValue);
	m_DynamicSamplerDescriptorHeap.CleanupUsedHeaps(FenceValue);
	
	if (WaitForCompletion)
	{
		g_CommandListManager.WaitForFence(FenceValue);
	}
	
	g_ContextManager.FreeContext(this);
	return FenceValue;
}

FAllocation FCommandContext::ReserveUploadMemory(size_t SizeInBytes)
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

void FCommandContext::TransitionResource(FD3D12Resource& Resource, D3D12_RESOURCE_STATES NewState, bool Flush /*= false*/)
{
	D3D12_RESOURCE_STATES OldState = Resource.m_CurrentState;

	if (OldState != NewState)
	{
		Assert(m_NumBarriersToFlush < 16);
		D3D12_RESOURCE_BARRIER& BarrierDesc = m_ResourceBarrierBuffer[m_NumBarriersToFlush++];

		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource = Resource.GetResource();
		BarrierDesc.Transition.StateBefore = OldState;
		BarrierDesc.Transition.StateAfter = NewState;
		BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		
		Resource.m_CurrentState = NewState;
	}

	if (Flush || m_NumBarriersToFlush == 16)
	{
		FlushResourceBarriers();
	}
}

void FCommandContext::TransitionSubResource(FD3D12Resource& Resource, D3D12_RESOURCE_STATES OldState,D3D12_RESOURCE_STATES NewState, uint32_t Subresource)
{
	//@todo: more control on subresource state
	//D3D12_RESOURCE_STATES OldState = Resource.m_CurrentState;
	//if (OldState != NewState)
	{
		Assert(m_NumBarriersToFlush < 16);
		D3D12_RESOURCE_BARRIER& BarrierDesc = m_ResourceBarrierBuffer[m_NumBarriersToFlush++];

		BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		BarrierDesc.Transition.pResource = Resource.GetResource();
		BarrierDesc.Transition.StateBefore = OldState;
		BarrierDesc.Transition.StateAfter = NewState;
		BarrierDesc.Transition.Subresource = Subresource;

		Resource.m_CurrentState = NewState;
	}

	//@todo
	//if (Flush || m_NumBarriersToFlush == 16)
	{
		FlushResourceBarriers();
	}
}

void FCommandContext::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* HeapPtr)
{
	if (m_CurrentDescriptorHeaps[Type] != HeapPtr)
	{
		m_CurrentDescriptorHeaps[Type] = HeapPtr;
		BindDescriptorHeaps();
	}
}

void FCommandContext::SetDescriptorHeaps(UINT HeapCount, D3D12_DESCRIPTOR_HEAP_TYPE Type[], ID3D12DescriptorHeap* HeapPtrs[])
{
	bool Changed = false;
	for (uint32_t i = 0; i < HeapCount; ++i)
	{
		if (m_CurrentDescriptorHeaps[Type[i]] != HeapPtrs[i])
		{
			m_CurrentDescriptorHeaps[Type[i]] = HeapPtrs[i];
			Changed = true;
		}
	}
	if (Changed)
	{
		BindDescriptorHeaps();
	}
}

void FCommandContext::SetDynamicDescriptor(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle)
{
	SetDynamicDescriptors(RootIndex, Offset, 1, &Handle);
}

void FCommandContext::SetDynamicDescriptors(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
{
	m_DynamicViewDescriptorHeap.SetGraphicsDescriptorHandles(RootIndex, Offset, Count, Handles);
}

void FCommandContext::SetPipelineState(const FPipelineState& PipelineState)
{
	ID3D12PipelineState* PipelineStateObj = PipelineState.GetPipelineStateObject();
	if (PipelineStateObj == m_CurPipelineState)
		return;

	m_CommandList->SetPipelineState(PipelineStateObj);
	m_CurPipelineState = PipelineStateObj;
}

void FCommandContext::SetRootSignature(const FRootSignature& RootSignature)
{
	if (RootSignature.GetSignature() == m_CurGraphicsRootSignature)
		return;
	m_CurGraphicsRootSignature = RootSignature.GetSignature();
	m_CommandList->SetGraphicsRootSignature(m_CurGraphicsRootSignature);

	m_DynamicViewDescriptorHeap.ParseGraphicsRootSignature(RootSignature);
	m_DynamicSamplerDescriptorHeap.ParseGraphicsRootSignature(RootSignature);
}

void FCommandContext::SetPrimitiveTopology(D3D12_PRIMITIVE_TOPOLOGY Topology)
{
	m_CommandList->IASetPrimitiveTopology(Topology);
}

void FCommandContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW& View)
{
	m_CommandList->IASetIndexBuffer(&View);
}

void FCommandContext::SetVertexBuffer(UINT Slot, const D3D12_VERTEX_BUFFER_VIEW& View)
{
	SetVertexBuffers(Slot, 1, &View);
}

void FCommandContext::SetVertexBuffers(UINT StartSlot, UINT Count, const D3D12_VERTEX_BUFFER_VIEW View[])
{
	m_CommandList->IASetVertexBuffers(StartSlot, Count, View);
}

void FCommandContext::SetScissor(UINT left, UINT top, UINT right, UINT bottom)
{
	SetScissor(CD3DX12_RECT(left, top, right, bottom));
}

void FCommandContext::SetScissor(const D3D12_RECT& rect)
{
	m_CommandList->RSSetScissorRects(1, &rect);
}

void FCommandContext::SetViewport(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT minDepth /*= 0.0f*/, FLOAT maxDepth /*= 1.0f*/)
{
	D3D12_VIEWPORT vp;
	vp.Width = w;
	vp.Height = h;
	vp.MinDepth = minDepth;
	vp.MaxDepth = maxDepth;
	vp.TopLeftX = x;
	vp.TopLeftY = y;
	SetViewport(vp);
}

void FCommandContext::SetViewport(const D3D12_VIEWPORT& vp)
{
	m_CommandList->RSSetViewports(1, &vp);
}

void FCommandContext::SetViewportAndScissor(UINT x, UINT y, UINT w, UINT h)
{
	SetViewport((float)x, (float)y, (float)w, (float)h);
	SetScissor(x, y, x+w, y+h);
}

void FCommandContext::SetViewportAndScissor(const D3D12_VIEWPORT& Viewport, const D3D12_RECT& Scissor)
{
	m_CommandList->RSSetViewports(1, &Viewport);
	m_CommandList->RSSetScissorRects(1, &Scissor);
}

void FCommandContext::ClearColor(FColorBuffer& Target)
{
	m_CommandList->ClearRenderTargetView(Target.GetRTV(), Target.GetClearColor().data, 0, nullptr);
}

void FCommandContext::ClearColor(FCubeBuffer& Target, int Face, int Mip)
{
	m_CommandList->ClearRenderTargetView(Target.GetRTV(Face, Mip), Target.GetClearColor().data, 0, nullptr);
}

void FCommandContext::ClearDepth(FDepthBuffer& Target)
{
	m_CommandList->ClearDepthStencilView(Target.GetDSV(), D3D12_CLEAR_FLAG_DEPTH, Target.GetClearDepth(), Target.GetClearStencil(), 0, nullptr);
}

void FCommandContext::SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[], D3D12_CPU_DESCRIPTOR_HANDLE DSV)
{
	m_CommandList->OMSetRenderTargets(NumRTVs, RTVs, true, &DSV);
}

void FCommandContext::SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[])
{
	m_CommandList->OMSetRenderTargets(NumRTVs, RTVs, true, nullptr);
}

void FCommandContext::SetDepthStencilTarget(D3D12_CPU_DESCRIPTOR_HANDLE DSV)
{
	SetRenderTargets(0, nullptr, DSV);
}

void FCommandContext::SetConstantArray(UINT RootIndex, UINT NumConstants, const void* Contents)
{
	m_CommandList->SetGraphicsRoot32BitConstants(RootIndex, NumConstants, Contents, 0);
}

void FCommandContext::SetDynamicConstantBufferView(UINT RootIndex, size_t BufferSize, const void* BufferData)
{
	Assert(BufferData != nullptr && IsAligned(BufferSize, 16));
	FAllocation Alloc = m_CpuLinearAllocator.Allocate(BufferSize);
	memcpy(Alloc.CPU, BufferData, BufferSize);
	m_CommandList->SetGraphicsRootConstantBufferView(RootIndex, Alloc.GpuAddress);
}

void FCommandContext::Draw(UINT VertexCount, UINT VertexStartOffset /*= 0*/)
{
	DrawInstanced(VertexCount, 1, VertexStartOffset, 0);
}

void FCommandContext::DrawIndexed(UINT IndexCount, UINT StartIndexLocation /*= 0*/, INT BaseVertexLocation /*= 0*/)
{
	DrawIndexedInstanced(IndexCount, 1, StartIndexLocation, BaseVertexLocation, 0);
}

void FCommandContext::DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation /*= 0*/, UINT StartInstanceLocation /*= 0*/)
{
	FlushResourceBarriers();
	m_DynamicViewDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
	m_DynamicSamplerDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
	m_CommandList->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

void FCommandContext::DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation)
{
	FlushResourceBarriers();
	m_DynamicViewDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
	m_DynamicSamplerDescriptorHeap.CommitGraphicsRootDescriptorTables(m_CommandList);
	m_CommandList->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

void FCommandContext::BindDescriptorHeaps()
{
	uint32_t NonNullHeaps = 0;
	ID3D12DescriptorHeap* HeapsToBind[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	for (uint32_t i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		if (m_CurrentDescriptorHeaps[i] != nullptr)
		{
			HeapsToBind[NonNullHeaps++] = m_CurrentDescriptorHeaps[i];
		}
	}
	if (NonNullHeaps > 0)
	{
		m_CommandList->SetDescriptorHeaps(NonNullHeaps, HeapsToBind);
	}
}

FComputeContext& FComputeContext::Begin(const std::wstring& ID /*= L""*/)
{
	FComputeContext& NewContext = g_ContextManager.AllocateContext(D3D12_COMMAND_LIST_TYPE_COMPUTE)->GetComputeContext();
	NewContext.SetID(ID);
	return NewContext;
}

void FComputeContext::SetRootSignature(const FRootSignature& RootSignature)
{
	if (RootSignature.GetSignature() == m_CurComputeRootSignature)
		return;
	m_CurComputeRootSignature = RootSignature.GetSignature();
	m_CommandList->SetComputeRootSignature(m_CurComputeRootSignature);

	m_DynamicViewDescriptorHeap.ParseComputeRootSignature(RootSignature);
	m_DynamicSamplerDescriptorHeap.ParseComputeRootSignature(RootSignature);
}

void FComputeContext::SetDynamicDescriptor(UINT RootIndex, UINT Offset, D3D12_CPU_DESCRIPTOR_HANDLE Handle)
{
	SetDynamicDescriptors(RootIndex, Offset, 1, &Handle);
}

void FComputeContext::SetDynamicDescriptors(UINT RootIndex, UINT Offset, UINT Count, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
{
	m_DynamicViewDescriptorHeap.SetComputeDescriptorHandles(RootIndex, Offset, Count, Handles);
}

void FComputeContext::SetConstantArray(UINT RootIndex, UINT NumConstants, const void* Contents)
{
	m_CommandList->SetComputeRoot32BitConstants(RootIndex, NumConstants, Contents, 0);
}

void FComputeContext::SetDynamicConstantBufferView(UINT RootIndex, size_t BufferSize, const void* BufferData)
{
	Assert(BufferData != nullptr && IsAligned(BufferSize, 16));
	FAllocation Alloc = m_CpuLinearAllocator.Allocate(BufferSize);
	memcpy(Alloc.CPU, BufferData, BufferSize);
	m_CommandList->SetComputeRootConstantBufferView(RootIndex, Alloc.GpuAddress);
}

void FComputeContext::Dispatch(size_t GroupCountX, size_t GroupCountY, size_t GroupCountZ)
{
	//Assert(m_Type == D3D12_COMMAND_LIST_TYPE_COMPUTE);

	FlushResourceBarriers();
	m_DynamicViewDescriptorHeap.CommitComputeRootDescriptorTables(m_CommandList);
	m_DynamicSamplerDescriptorHeap.CommitComputeRootDescriptorTables(m_CommandList);
	m_CommandList->Dispatch((UINT)GroupCountX, (UINT)GroupCountY, (UINT)GroupCountZ);
}
