#include "PipelineState.h"
#include "RootSignature.h"
#include "MathLib.h"
#include "D3D12RHI.h"


void FPipelineState::Initialize()
{
	// Default rasterizer states
	RasterizerDefault.FillMode = D3D12_FILL_MODE_SOLID;
	RasterizerDefault.CullMode = D3D12_CULL_MODE_BACK;
	RasterizerDefault.FrontCounterClockwise = FALSE;
	RasterizerDefault.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	RasterizerDefault.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	RasterizerDefault.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	RasterizerDefault.DepthClipEnable = TRUE;
	RasterizerDefault.MultisampleEnable = FALSE;
	RasterizerDefault.AntialiasedLineEnable = FALSE;
	RasterizerDefault.ForcedSampleCount = 0;
	RasterizerDefault.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	RasterizerTwoSided = RasterizerDefault;
	RasterizerTwoSided.CullMode = D3D12_CULL_MODE_NONE;

	RasterizerShadow = RasterizerDefault;
	RasterizerShadow.DepthBias = -100;
	RasterizerShadow.SlopeScaledDepthBias = -1.5f;

	// blend state
	D3D12_BLEND_DESC alphaBlend = {};
	alphaBlend.IndependentBlendEnable = FALSE;
	alphaBlend.RenderTarget[0].BlendEnable = FALSE;
	alphaBlend.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	alphaBlend.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	alphaBlend.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	alphaBlend.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	alphaBlend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	alphaBlend.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	alphaBlend.RenderTarget[0].RenderTargetWriteMask = 0;
	BlendNoColorWrite = alphaBlend;

	alphaBlend.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	BlendDisable = alphaBlend;

	alphaBlend.RenderTarget[0].BlendEnable = TRUE;
	BlendTraditional = alphaBlend;

	// depth stencil state
	DepthStateDisabled.DepthEnable = FALSE;
	DepthStateDisabled.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	DepthStateDisabled.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	DepthStateDisabled.StencilEnable = FALSE;
	DepthStateDisabled.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	DepthStateDisabled.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
	DepthStateDisabled.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	DepthStateDisabled.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	DepthStateDisabled.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	DepthStateDisabled.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	DepthStateDisabled.BackFace = DepthStateDisabled.FrontFace;

	DepthStateReadWrite = DepthStateDisabled;
	DepthStateReadWrite.DepthEnable = TRUE;
	DepthStateReadWrite.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	DepthStateReadWrite.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

	DepthStateReadOnly = DepthStateReadWrite;
	DepthStateReadOnly.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
}

void FPipelineState::DestroyAll()
{
	ms_GraphicsPSHashMap.clear();
	ms_ComputePSHashMap.clear();
}

D3D12_RASTERIZER_DESC FPipelineState::RasterizerDefault;
D3D12_RASTERIZER_DESC FPipelineState::RasterizerTwoSided;
D3D12_RASTERIZER_DESC FPipelineState::RasterizerShadow;

D3D12_BLEND_DESC FPipelineState::BlendNoColorWrite;
D3D12_BLEND_DESC FPipelineState::BlendDisable;
D3D12_BLEND_DESC FPipelineState::BlendTraditional;

D3D12_DEPTH_STENCIL_DESC FPipelineState::DepthStateDisabled;
D3D12_DEPTH_STENCIL_DESC FPipelineState::DepthStateReadWrite;
D3D12_DEPTH_STENCIL_DESC FPipelineState::DepthStateReadOnly;

std::map<size_t, ComPtr<ID3D12PipelineState>> FPipelineState::ms_GraphicsPSHashMap;
std::map<size_t, ComPtr<ID3D12PipelineState>> FPipelineState::ms_ComputePSHashMap;

FGraphicsPipelineState::FGraphicsPipelineState()
	: m_InputLayouts(nullptr)
{
	ZeroMemory(&m_PSDesc, sizeof(m_PSDesc));
	m_PSDesc.NodeMask = 1;
	m_PSDesc.SampleMask = (UINT)-1;
	m_PSDesc.SampleDesc.Count = 1;
	m_PSDesc.InputLayout.NumElements = 0;
}

void FGraphicsPipelineState::SetBlendState(const D3D12_BLEND_DESC& BlendDesc)
{
	m_PSDesc.BlendState = BlendDesc;	
}

void FGraphicsPipelineState::SetRasterizerState(const D3D12_RASTERIZER_DESC& RasterizerDesc)
{
	m_PSDesc.RasterizerState = RasterizerDesc;
}

void FGraphicsPipelineState::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& DepthStencilDesc)
{
	m_PSDesc.DepthStencilState = DepthStencilDesc;
}

void FGraphicsPipelineState::SetSampleMask(UINT SampleMask)
{
	m_PSDesc.SampleMask = SampleMask;
}

void FGraphicsPipelineState::SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE Type)
{
	m_PSDesc.PrimitiveTopologyType = Type;
}

void FGraphicsPipelineState::SetRenderTargetFormat(DXGI_FORMAT RTVFormat, DXGI_FORMAT DSVFormat, UINT MsaaCount /*= 1*/, UINT MsaaQuality /*= 0*/)
{
	SetRenderTargetFormats(1, &RTVFormat, DSVFormat, MsaaCount, MsaaQuality);
}

void FGraphicsPipelineState::SetRenderTargetFormats(UINT NumRTVs, const DXGI_FORMAT* RTVFormats, DXGI_FORMAT DSVFormat, UINT MsaaCount /*= 1*/, UINT MsaaQuality /*= 0*/)
{
	Assert(NumRTVs != 0 || RTVFormats != nullptr);

	for (UINT i = 0; i < NumRTVs; ++i)
		m_PSDesc.RTVFormats[i] = RTVFormats[i];
	for (UINT i = NumRTVs; i < m_PSDesc.NumRenderTargets; ++i)
		m_PSDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;
	m_PSDesc.NumRenderTargets = NumRTVs;
	m_PSDesc.DSVFormat = DSVFormat;
	m_PSDesc.SampleDesc.Count = MsaaCount;
	m_PSDesc.SampleDesc.Quality = MsaaQuality;
}

void FGraphicsPipelineState::SetInputLayout(UINT NumElements, const D3D12_INPUT_ELEMENT_DESC* InputElementDescs)
{
	m_PSDesc.InputLayout.NumElements = NumElements;
	if (m_InputLayouts)
	{
		delete [] m_InputLayouts;
		m_InputLayouts = nullptr;
	}
	if (NumElements > 0)
	{
		m_InputLayouts = new D3D12_INPUT_ELEMENT_DESC[NumElements];
		memcpy(m_InputLayouts, InputElementDescs, NumElements * sizeof(D3D12_INPUT_ELEMENT_DESC));
	}
}

void FGraphicsPipelineState::Finalize()
{
	m_PSDesc.pRootSignature = m_RootSignature->GetSignature();
	Assert(m_PSDesc.pRootSignature != nullptr);

	m_PSDesc.InputLayout.pInputElementDescs = nullptr;
	size_t HashCode = HashState(&m_PSDesc);
	HashCode = HashState(m_InputLayouts, m_PSDesc.InputLayout.NumElements, HashCode);
	m_PSDesc.InputLayout.pInputElementDescs = m_InputLayouts;

	//todo: cache pso
	{
		auto iter = ms_GraphicsPSHashMap.find(HashCode);
		if (iter == ms_GraphicsPSHashMap.end())
		{
			ThrowIfFailed(D3D12RHI::Get().GetD3D12Device()->CreateGraphicsPipelineState(&m_PSDesc, IID_PPV_ARGS(&m_PipelineState)));
			ms_GraphicsPSHashMap[HashCode].Attach(m_PipelineState);
		}
		else
		{
			m_PipelineState = ms_GraphicsPSHashMap[HashCode].Get();
		}
	}
}

FComputePipelineState::FComputePipelineState()
{
	ZeroMemory(&m_PSDesc, sizeof(m_PSDesc));
	m_PSDesc.NodeMask = 1;
}

void FComputePipelineState::Finalize()
{
	m_PSDesc.pRootSignature = m_RootSignature->GetSignature();
	Assert(m_PSDesc.pRootSignature != nullptr);

	size_t HashCode = HashState(&m_PSDesc);

	//todo: cache pso
	{
		auto iter = ms_GraphicsPSHashMap.find(HashCode);
		if (iter == ms_GraphicsPSHashMap.end())
		{
			ThrowIfFailed(D3D12RHI::Get().GetD3D12Device()->CreateComputePipelineState(&m_PSDesc, IID_PPV_ARGS(&m_PipelineState)));
			ms_ComputePSHashMap[HashCode].Attach(m_PipelineState);
		}
		else
		{
			m_PipelineState = ms_ComputePSHashMap[HashCode].Get();
		}
	}
}