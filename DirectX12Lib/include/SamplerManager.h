#pragma once

#include "Common.h"

class FSamplerDesc : public D3D12_SAMPLER_DESC
{
public:
	FSamplerDesc()
	{
		Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		MipLODBias = 0.f;
		MaxAnisotropy = 1;
		ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		BorderColor[0] = BorderColor[1] = BorderColor[2] = BorderColor[3] = 1.f;
		MinLOD = 0.0f;
		MaxLOD = D3D12_FLOAT32_MAX;
	}

	void SetShadowMapDesc()
	{
		Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
		AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		MipLODBias = 0.f;
		MaxAnisotropy = 1;
		ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		BorderColor[0] = BorderColor[1] = BorderColor[2] = BorderColor[3] = 1.f;
		MinLOD = 0.0f;
		MaxLOD = D3D12_FLOAT32_MAX;
	}

	void SetShadowMapPCFDesc()
	{
		Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		MipLODBias = 0.f;
		MaxAnisotropy = 1;
		ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		BorderColor[0] = BorderColor[1] = BorderColor[2] = BorderColor[3] = 1.f;
		MinLOD = 0.0f;
		MaxLOD = D3D12_FLOAT32_MAX;
	}
};