#pragma once

class FCamera;
class FCubeBuffer;
class FColorBuffer;
class FCommandContext;

namespace PostProcessing
{
	extern bool g_EnableBloom;
	extern float g_BloomIntensity;
	extern float g_BloomThreshold;

	void Initialize();
	void Destroy();

	void Render(FCommandContext& CommandContext);
	void GenerateBloom(FCommandContext& CommandContext);
	void ToneMapping(FCommandContext& CommandContext);

	void BuildHZB(FCommandContext& GfxContext);
	void GenerateSSR(FCommandContext& GfxContext, FCamera& Camera, FCubeBuffer& CubeBuffer);
}