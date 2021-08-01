#pragma once

class FCamera;
class FCubeBuffer;
class FColorBuffer;
class FCommandContext;

namespace ScreenSpaceSubsurface
{
	extern bool						g_SSSSkinEnable;
	extern float					g_sssStrength;
	extern float					g_sssWidth;
	extern float					g_sssClampScale;
	extern float					g_EffectStr;
				
	void Initialize();
	void Destroy();
	void Render(FCommandContext& CommandContext);
}