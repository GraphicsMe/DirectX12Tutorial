#pragma once

class FCamera;
class FCubeBuffer;
class FColorBuffer;
class FCommandContext;

namespace ScreenSpaceSubsurface
{
	extern bool						g_SSSSkinEnable;
	extern float					g_sssStretchAlpha;
	extern float					g_sssWidth;
	extern float					g_sssStr;

	extern int						g_DebugFlag;

	extern 	FColorBuffer			g_DiffuseTerm;
	extern	FColorBuffer			g_SpecularTerm;
				
	void Initialize();
	void Destroy();
	void Render(FCommandContext& CommandContext, FCamera& Camera);
}