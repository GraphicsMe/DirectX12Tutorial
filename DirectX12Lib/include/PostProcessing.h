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

	extern bool g_EnableSSR;
	extern bool g_DebugSSR;
	extern bool g_UseHiZ;
	extern bool g_UseMinMaxZ;
	extern float g_Thickness;
	extern float g_CompareTolerance;
	extern int g_NumRays;

	void Initialize();
	void Destroy();

	void Render(FCommandContext& CommandContext);
	void GenerateBloom(FCommandContext& CommandContext);
	void ToneMapping(FCommandContext& CommandContext);

	void BuildHZB(FCommandContext& GfxContext);
	void GenerateSSR(FCommandContext& GfxContext, FCamera& Camera, FCubeBuffer& CubeBuffer);
}