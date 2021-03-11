#pragma once

#include <vector>
#include <d3d12.h>
#include "Common.h"

class FRootParameter
{
	friend class FRootSignature;

public:
	FRootParameter()
	{
		m_RootParam.ParameterType = (D3D12_ROOT_PARAMETER_TYPE)-1;
	}
	~FRootParameter()
	{
		Clear();
	}
	
	void Clear()
	{
		if (m_RootParam.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			delete [] m_RootParam.DescriptorTable.pDescriptorRanges;
		}
		m_RootParam.ParameterType = (D3D12_ROOT_PARAMETER_TYPE)-1;
	}

	void InitAsConstants(UINT Register, UINT NumDwords, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		m_RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		m_RootParam.ShaderVisibility = Visibility;
		m_RootParam.Constants.Num32BitValues = NumDwords;
		m_RootParam.Constants.ShaderRegister = Register;
		m_RootParam.Constants.RegisterSpace = 0;
	}

	void InitAsBufferCBV(UINT Register, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		m_RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
		m_RootParam.ShaderVisibility = Visibility;
		m_RootParam.Descriptor.ShaderRegister = Register;
		m_RootParam.Descriptor.RegisterSpace = 0;
	}

	void InitAsBufferSRV(UINT Register, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		m_RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
		m_RootParam.ShaderVisibility = Visibility;
		m_RootParam.Descriptor.ShaderRegister = Register;
		m_RootParam.Descriptor.RegisterSpace = 0;
	}

	void InitAsBufferUAV(UINT Register, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		m_RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
		m_RootParam.ShaderVisibility = Visibility;
		m_RootParam.Descriptor.ShaderRegister = Register;
		m_RootParam.Descriptor.RegisterSpace = 0;
	}

	void InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE Type, UINT Register, UINT Count, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		InitAsDescriptorTable(1, Visibility);
		SetTableRange(0, Type, Register, Count);
	}

	void InitAsDescriptorTable(UINT RangeCount, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL)
	{
		m_RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		m_RootParam.ShaderVisibility = Visibility;
		m_RootParam.DescriptorTable.NumDescriptorRanges = RangeCount;
		m_RootParam.DescriptorTable.pDescriptorRanges = new D3D12_DESCRIPTOR_RANGE[RangeCount];
	}

	void SetTableRange(UINT RangeIndex, D3D12_DESCRIPTOR_RANGE_TYPE Type, UINT Register, UINT Count, UINT Space = 0)
	{
		D3D12_DESCRIPTOR_RANGE* range = const_cast<D3D12_DESCRIPTOR_RANGE*>(m_RootParam.DescriptorTable.pDescriptorRanges + RangeIndex);
		range->RangeType = Type;
		range->NumDescriptors = Count;
		range->BaseShaderRegister = Register;
		range->RegisterSpace = Space;
		range->OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	}

protected:
	D3D12_ROOT_PARAMETER m_RootParam;
};


class FRootSignature
{
public:
	FRootSignature(UINT NumRootParams = 0, UINT NumStaticSamplers = 0)
	{
		Reset(NumRootParams, NumStaticSamplers);
	}

	~FRootSignature()
	{
		if (m_D3DRootSignature)
		{
			m_D3DRootSignature->Release();
			m_D3DRootSignature = nullptr;
		}
	}

	void Reset(UINT NumRootParams, UINT NumStaticSamplers)
	{
		m_ParamArray.resize(NumRootParams);
		m_NumParameters = NumRootParams;

		m_StaticSamplerArray.clear();
		m_NumStaticSamplers = NumStaticSamplers;
	}

	FRootParameter& operator[] (size_t EntryIndex)
	{
		Assert(EntryIndex < m_NumParameters);
		return m_ParamArray[EntryIndex];
	}

	const FRootParameter& operator[] (size_t EntryIndex) const
	{
		Assert(EntryIndex < m_NumParameters);
		return m_ParamArray[EntryIndex];
	}

	void InitStaticSampler(UINT Register, const D3D12_SAMPLER_DESC& SamplerDesc, D3D12_SHADER_VISIBILITY Visibility = D3D12_SHADER_VISIBILITY_ALL);

	void Finalize(const std::wstring& name, D3D12_ROOT_SIGNATURE_FLAGS Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE);

	ID3D12RootSignature* GetSignature() const { return m_D3DRootSignature; }

	UINT GetNumParameters() const { return m_NumParameters; }

	uint32_t GetSamplerTableBitMap() const { return m_SamplerTableBitMap; }
	uint32_t GetDescriptorTableBitMap() const { return m_DescriptorTableBitMap; }
	uint32_t GetDescriptorTableSize(uint32_t RootIndex) const { return m_DescriptorTableSize[RootIndex]; }


protected:
	bool m_Finalized = false;
	UINT m_NumParameters = 0;
	UINT m_NumStaticSamplers = 0;

	uint32_t m_DescriptorTableBitMap = 0;
	uint32_t m_SamplerTableBitMap = 0;
	uint32_t m_DescriptorTableSize[16] = {};

	std::vector<FRootParameter> m_ParamArray;
	std::vector< D3D12_STATIC_SAMPLER_DESC> m_StaticSamplerArray;
	ID3D12RootSignature* m_D3DRootSignature = nullptr;
};