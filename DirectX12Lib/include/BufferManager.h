#pragma once

#include "ColorBuffer.h"
#include "DepthBuffer.h"

namespace BufferManager
{
	extern FColorBuffer g_SceneColorBuffer;
	extern FDepthBuffer g_SceneDepthZ;
	extern FColorBuffer g_GBufferA;
	extern FColorBuffer g_GBufferB;
	extern FColorBuffer g_GBufferC;
	extern FColorBuffer g_SSRBuffer;

	void InitializeRenderingBuffers(uint32_t NativeWidth, uint32_t NativeHeight);
	void DestroyRenderingBuffers();
}



