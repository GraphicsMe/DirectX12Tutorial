#include "BufferManager.h"

using namespace BufferManager;



namespace BufferManager
{
	FColorBuffer g_SceneColorBuffer;
}


void BufferManager::InitializeRenderingBuffers(uint32_t NativeWidth, uint32_t NativeHeight)
{
	g_SceneColorBuffer.Create(L"g_SceneColorBuffer", NativeWidth, NativeHeight, 1, DXGI_FORMAT_R11G11B10_FLOAT);
}


void BufferManager::DestroyRenderingBuffers()
{
	g_SceneColorBuffer.Destroy();
}

