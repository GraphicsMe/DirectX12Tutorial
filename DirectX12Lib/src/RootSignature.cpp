#include "RootSignature.h"
#include "D3D12RHI.h"


void FRootSignature::InitStaticSampler(UINT Register, const D3D12_SAMPLER_DESC& SamplerDesc, D3D12_SHADER_VISIBILITY Visibility /*= D3D12_SHADER_VISIBILITY_ALL*/)
{
	Assert (m_StaticSamplerArray.size() < m_NumStaticSamplers);

	D3D12_STATIC_SAMPLER_DESC StaticSamplerDesc;
	StaticSamplerDesc.Filter = SamplerDesc.Filter;
	StaticSamplerDesc.AddressU = SamplerDesc.AddressU;
	StaticSamplerDesc.AddressV = SamplerDesc.AddressV;
	StaticSamplerDesc.AddressW = SamplerDesc.AddressW;
	StaticSamplerDesc.MipLODBias = SamplerDesc.MipLODBias;
	StaticSamplerDesc.MaxAnisotropy = SamplerDesc.MaxAnisotropy;
	StaticSamplerDesc.ComparisonFunc = SamplerDesc.ComparisonFunc;
	StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	StaticSamplerDesc.MinLOD = SamplerDesc.MinLOD;
	StaticSamplerDesc.MaxLOD = SamplerDesc.MaxLOD;
	StaticSamplerDesc.ShaderRegister = Register;
	StaticSamplerDesc.RegisterSpace = 0;
	StaticSamplerDesc.ShaderVisibility = Visibility;

	if (SamplerDesc.BorderColor[3] == 1.0f)
	{
		if (SamplerDesc.BorderColor[0] == 1.0f)
			StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
		else
			StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
	}
	else
	{
		StaticSamplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	}
	m_StaticSamplerArray.emplace_back(StaticSamplerDesc);
}

void FRootSignature::Finalize(const std::wstring& name, D3D12_ROOT_SIGNATURE_FLAGS Flags)
{
	if (m_Finalized)
		return;

	Assert(m_StaticSamplerArray.size() == m_NumStaticSamplers);

	D3D12_ROOT_SIGNATURE_DESC RootDesc;
	RootDesc.Flags = Flags;
	RootDesc.NumParameters = m_NumParameters;
	if (m_NumParameters == 0)
		RootDesc.pParameters = nullptr;
	else
		RootDesc.pParameters = (const D3D12_ROOT_PARAMETER*)&m_ParamArray[0];
	RootDesc.NumStaticSamplers = m_NumStaticSamplers;
	if (m_NumStaticSamplers == 0)
		RootDesc.pStaticSamplers = nullptr;
	else
		RootDesc.pStaticSamplers = (const D3D12_STATIC_SAMPLER_DESC*)&m_StaticSamplerArray[0];

	m_DescriptorTableBitMap = 0;
	m_SamplerTableBitMap = 0;
	for (UINT i = 0; i < m_NumParameters; ++i)
	{
		const D3D12_ROOT_PARAMETER& RootParam = RootDesc.pParameters[i];
		m_DescriptorTableSize[i] = 0;

		if (RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			Assert(RootParam.DescriptorTable.pDescriptorRanges != nullptr);

			if (RootParam.DescriptorTable.pDescriptorRanges->RangeType == D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER)
				m_SamplerTableBitMap |= (1 << i);
			else
				m_DescriptorTableBitMap |= (1 << i);

			for (UINT TableRange = 0; TableRange < RootParam.DescriptorTable.NumDescriptorRanges; ++TableRange)
			{
				m_DescriptorTableSize[i] += RootParam.DescriptorTable.pDescriptorRanges[TableRange].NumDescriptors;
			}
		}
	}

	ComPtr<ID3DBlob> pOutBlob, pErrorBlob;

	ThrowIfFailed(D3D12SerializeRootSignature(&RootDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		pOutBlob.GetAddressOf(), pErrorBlob.GetAddressOf()));

	ThrowIfFailed(D3D12RHI::Get().GetD3D12Device()->CreateRootSignature(1, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS(&m_D3DRootSignature)));

	m_D3DRootSignature->SetName(name.c_str());
}

