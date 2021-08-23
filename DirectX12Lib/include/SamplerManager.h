#pragma once

#include "Common.h"
#include "MathLib.h"

class FSamplerDesc : public D3D12_SAMPLER_DESC
{
public:
	FSamplerDesc(D3D12_FILTER FilterMode=D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE AddressMode=D3D12_TEXTURE_ADDRESS_MODE_WRAP)
	{
		Filter = FilterMode;
		AddressU = AddressMode;
		AddressV = AddressMode;
		AddressW = AddressMode;
		MipLODBias = 0.f;
		MaxAnisotropy = 16;
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
		AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		MipLODBias = 0.f;
		MaxAnisotropy = 1;
		ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		BorderColor[0] = BorderColor[1] = BorderColor[2] = BorderColor[3] = 1.f;
		MinLOD = 0.0f;
		MaxLOD = D3D12_FLOAT32_MAX;
	}

	void SetLinearBorderDesc(const Vector4f& Border)
	{
		Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		MipLODBias = 0.f;
		MaxAnisotropy = 1;
		ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		BorderColor[0] = Border.x;
		BorderColor[1] = Border.y;
		BorderColor[2] = Border.z;
		BorderColor[3] = Border.w;
		MinLOD = 0.0f;
		MaxLOD = D3D12_FLOAT32_MAX;
	}

	void SetPointBorderDesc(const Vector4f& Border)
	{
		Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
		AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		MipLODBias = 0.f;
		MaxAnisotropy = 1;
		ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		BorderColor[0] = Border.x;
		BorderColor[1] = Border.y;
		BorderColor[2] = Border.z;
		BorderColor[3] = Border.w;
		MinLOD = 0.0f;
		MaxLOD = D3D12_FLOAT32_MAX;
	}
};