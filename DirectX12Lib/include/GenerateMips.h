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

	static void Generate(FCubeBuffer& CubeBuffer, FCommandContext& CommandContext);

private:
	static FRootSignature m_GenMipSignature;
	static FGraphicsPipelineState m_GenMipPSO;
	static ComPtr<ID3DBlob> m_ScreenQuadVS;
	static ComPtr<ID3DBlob> m_GenerateMipPS;
};