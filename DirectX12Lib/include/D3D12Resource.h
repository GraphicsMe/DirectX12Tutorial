#pragma once

#include <vector>
#include <d3d12.h>
#include "Common.h"

class FD3D12Resource
{
	friend class FCommandContext;

public:
	FD3D12Resource()
		: m_GpuAddress(0)
	{
	}

	FD3D12Resource(ID3D12Resource* Resource, D3D12_RESOURCE_STATES State)
		: m_GpuAddress(0)
	{
		m_Resource.Attach(Resource);
		
	}

	void InitializeState(D3D12_RESOURCE_STATES State)
	{
		D3D12_RESOURCE_DESC Desc = m_Resource->GetDesc();
		m_AllCurrentState.resize(Desc.MipLevels * Desc.DepthOrArraySize, State);
	}

	virtual void Destroy()
	{
		m_Resource = nullptr;
		m_GpuAddress = 0;
		m_AllCurrentState.resize(0);
	}

	ID3D12Resource* GetResource() { return m_Resource.Get(); }
	const ID3D12Resource* GetResource() const { return  m_Resource.Get(); }
	D3D12_GPU_VIRTUAL_ADDRESS GetGpuAddress() const { return m_GpuAddress; }

protected:
	ComPtr<ID3D12Resource> m_Resource;
	std::vector<D3D12_RESOURCE_STATES> m_AllCurrentState;
	D3D12_GPU_VIRTUAL_ADDRESS m_GpuAddress;
};