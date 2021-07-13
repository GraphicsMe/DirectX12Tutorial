#pragma once

#include "PipelineState.h"
#include "RootSignature.h"

class FCubeBuffer;
class FCommandContext;

class FGenerateMips
{
public:
	static void Initialize();
	static void Destroy();

	static void GenerateForCube(FCubeBuffer& CubeBuffer, FCommandContext& CommandContext);
	static void GeneratePerCubeFace(FCubeBuffer& CubeBuffer, FCommandContext& CommandContext);

private:
	static FRootSignature m_GenMipSignature;
	static FGraphicsPipelineState m_GenMipPSO;
	static ComPtr<ID3DBlob> m_ScreenQuadVS, m_GenerateMipPS;

	static FGraphicsPipelineState m_GenMipPSOCube;
	static ComPtr<ID3DBlob> m_CubeVS, m_CubePS;
};