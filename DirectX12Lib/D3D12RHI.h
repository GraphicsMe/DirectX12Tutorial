#pragma once

#include <chrono>

#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include "Common.h"
#include "MathLib.h"
#include "LinearAllocator.h"

class WindowWin32;
class FCommandQueue;
class FD3D12Device;
class FD3D12Adapter;

class FD3D12AdapterChild
{
protected:
	FD3D12Adapter* ParentAdapter;

public:
	FD3D12AdapterChild(FD3D12Adapter* InParent) : ParentAdapter(InParent) {}

	FD3D12Adapter* GetParentAdapter() const
	{
		Assert(ParentAdapter != nullptr);
		return ParentAdapter;
	}

	// To be used with delayed setup
	inline void SetParentAdapter(FD3D12Adapter* InParent)
	{
		Assert(ParentAdapter == nullptr);
		ParentAdapter = InParent;
	}
};

class FD3D12DeviceChild
{
protected:
	FD3D12Device* Parent;

public:
	FD3D12DeviceChild(FD3D12Device* InParent) : Parent(InParent) {}

	FD3D12Device* GetParentDevice() const
	{
		Assert(Parent != nullptr);
		return Parent;
	}

	FD3D12Device* GetParentDevice_Unsafe() const
	{
		return Parent;
	}

	// To be used with delayed setup
	inline void SetParentDevice(FD3D12Device* InParent)
	{
		Assert(Parent == nullptr);
		Parent = InParent;
	}
};


class FD3D12Adapter
{
public:
	ID3D12Device* GetD3DDevice() const { return RootDevice.Get(); }
	FD3D12Device* GetDevice() { return Device; }

protected:
	FD3D12Device* Device;
	ComPtr<ID3D12Device> RootDevice;
	ComPtr<IDXGIFactory> DxgiFactory;
	ComPtr<IDXGIFactory2> DxgiFactory2;
	ComPtr<IDXGIAdapter> DxgiAdapter;
};


class FD3D12Device : public FD3D12AdapterChild
{
public:
	FD3D12Device(FD3D12Adapter* InAdapter);
	virtual ~FD3D12Device();

	void Initialize();
	virtual void Cleanup();

	ID3D12Device* GetDevice();

};

class D3D12RHI
{
private:
	D3D12RHI();

public:
	~D3D12RHI() = default;

	static D3D12RHI& Get();

public:
	bool Initialize();
	void Destroy();

	ComPtr<ID3D12Device> GetD3D12Device() { return m_device; }
	IDXGIFactory4* GetDXGIFactory() { return m_dxgiFactory.Get(); }
	ComPtr<ID3DBlob> CreateShader(const std::wstring& ShaderFile, const std::string& EntryPoint, const std::string& TargetModel);
	void SetResourceBarrier(ComPtr<ID3D12GraphicsCommandList> commandList, ComPtr<ID3D12Resource> resource, 
			D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter);;
	ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, D3D12_DESCRIPTOR_HEAP_FLAGS flag, uint32_t numDescriptors);

	FAllocation ReserveUploadMemory(uint32_t SizeInBytes);

private:
	void EnableDebugLayer();
	ComPtr<IDXGIFactory4> CreateDXGIFactory();
	ComPtr<IDXGIAdapter1> ChooseAdapter(ComPtr<IDXGIFactory4> factory);
	ComPtr<ID3D12Device> CreateDevice(ComPtr<IDXGIAdapter1> adapter);
	
	
private:
	ComPtr<IDXGIFactory4> m_dxgiFactory;
	ComPtr<ID3D12Device> m_device;

	LinearAllocator m_CpuLinearAllocator;
	LinearAllocator m_GpuLinearAllocator;
};

