#pragma once
#include <stdint.h>
#include "ColorBuffer.h"

class FCommandContext;

namespace DepthOfField
{
	extern bool		g_EnableDepthOfField;
	extern float	g_FoucusDistance;
	extern float    g_FoucusRange;
	extern float    g_BokehRadius;

	extern float	g_Aperture;
	extern float	g_FocalLength;
	extern float	g_MaxBlurSize;

	void Initialize(void);
	void Destroy(void);
	void Render(FCommandContext& BaseContext, float NearClipDist = 0.01f, float FarClipDist = 1000.0f);
}