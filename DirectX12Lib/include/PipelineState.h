#pragma once

#include <map>
#include <d3d12.h>
#include "d3dx12.h"
#include "Common.h"

class FRootSignature;

class FPipelineState
{
public:
	FPipelineState() : m_RootSignature(nullptr), m_PipelineState(nullptr) {}
	
	static void Initialize();
	static void DestroyAll();

	void SetRootSignature(const FRootSignature& RootSignature)
	{
		m_RootSignature = &RootSignature;
	}

	const FRootSignature& GetRootSignature() const { return *m_RootSignature; }
	
	ID3D12PipelineState* GetPipelineStateObject() const { return m_PipelineState; }

public:
	static D3D12_RASTERIZER_DESC RasterizerDefault;
	static D3D12_RASTERIZER_DESC RasterizerTwoSided;

	static D3D12_BLEND_DESC BlendNoColorWrite;
	static D3D12_BLEND_DESC BlendDisable;
	static D3D12_BLEND_DESC BlendTraditional;

	static D3D12_DEPTH_STENCIL_DESC DepthStateDisabled;
	static D3D12_DEPTH_STENCIL_DESC DepthStateReadWrite;
	static D3D12_DEPTH_STENCIL_DESC DepthStateReadOnly;


protected:
	const FRootSignature* m_RootSignature;
	
	ID3D12PipelineState* m_PipelineState;

	static std::map<size_t, ComPtr<ID3D12PipelineState>> ms_GraphicsPSHashMap;
	static std::map<size_t, ComPtr<ID3D12PipelineState>> ms_ComputePSHashMap;
};


class FGraphicsPipelineState : public FPipelineState
{
public:
	FGraphicsPipelineState();

	void SetBlendState(const D3D12_BLEND_DESC& BlendDesc);
	void SetRasterizerState(const D3D12_RASTERIZER_DESC& RasterizerDesc);
	void SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilDesc);

	void SetSampleMask(UINT SampleMask);
	void SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE Type);
	void SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MsaaCount = 1, UINT MsaaQuality = 0);
	void SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount = 1, UINT MsaaQuality = 0);
	void SetInputLayout(UINT NumElements, const D3D12_INPUT_ELEMENT_DESC* InputElementDescs);
	
	void SetVertexShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSDesc.VS = Binary; }
	void SetPixelShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSDesc.PS = Binary; }
	void SetGeometryShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSDesc.GS = Binary; }
	void SetHullShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSDesc.HS = Binary; }
	void SetDomainShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSDesc.DS = Binary; }

	void SetVertexShader(const void* Binary, size_t Size) { m_PSDesc.VS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
	void SetPixelShader(const void* Binary, size_t Size) { m_PSDesc.PS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
	void SetGeometryShader(const void* Binary, size_t Size) { m_PSDesc.GS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
	void SetHullShader(const void* Binary, size_t Size) { m_PSDesc.HS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
	void SetDomainShader(const void* Binary, size_t Size) { m_PSDesc.DS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }

	void Finalize();

private:
	D3D12_INPUT_ELEMENT_DESC* m_InputLayouts;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC m_PSDesc;
};


class FComputePipelineState : public FPipelineState
{
public:
	FComputePipelineState();

	void SetComputeShader(const void* Binary, size_t Size) { m_PSDesc.CS = CD3DX12_SHADER_BYTECODE(const_cast<void*>(Binary), Size); }
	void SetComputeShader(const D3D12_SHADER_BYTECODE& Binary) { m_PSDesc.CS = Binary; }

	void Finalize();

protected:
	D3D12_COMPUTE_PIPELINE_STATE_DESC m_PSDesc;
};