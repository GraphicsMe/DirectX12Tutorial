#pragma once

#include "ColorBuffer.h"
#include "DepthBuffer.h"

namespace BufferManager
{
	extern FColorBuffer g_SceneColorBuffer;

	void InitializeRenderingBuffers(uint32_t NativeWidth, uint32_t NativeHeight);
	void DestroyRenderingBuffers();

}



