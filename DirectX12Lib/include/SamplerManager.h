#pragma once

#include "Common.h"

class FSamplerDesc : public D3D12_SAMPLER_DESC
{
public:
	FSamplerDesc()
	{
		Filter = D3D12_FILTER_ANISOTROPIC;
		AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		MipLODBias = 0;
		MaxAnisotropy = 16;
		ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		BorderColor[0] = 1.f;
		BorderColor[1] = 1.f;
		BorderColor[2] = 1.f;
		BorderColor[3] = 1.f;
		MinLOD = 0.0f;
		MaxLOD = D3D12_FLOAT32_MAX;
	}
};