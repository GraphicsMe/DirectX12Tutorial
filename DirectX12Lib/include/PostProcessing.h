#pragma once

class FCommandContext;

namespace PostProcessing
{
	extern bool g_EnableBloom;
	extern float g_BloomThreshold;

	void Initialize();
	void Destroy();

	void Render(FCommandContext& CommandContext);
	void GenerateBloom(FCommandContext& CommandContext);
	void ToneMapping(FCommandContext& CommandContext);
}