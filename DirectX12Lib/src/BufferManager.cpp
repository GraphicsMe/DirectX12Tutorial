#include "BufferManager.h"

using namespace BufferManager;



namespace BufferManager
{
	FColorBuffer g_SceneColorBuffer;
	FDepthBuffer g_SceneDepthZ;
	FColorBuffer g_GBufferA; //world normal
	FColorBuffer g_GBufferB; //metallic, specular, roughness
	FColorBuffer g_GBufferC; //diffuse, ao
	FColorBuffer g_SSRBuffer;
}


void BufferManager::InitializeRenderingBuffers(uint32_t NativeWidth, uint32_t NativeHeight)
{
	g_SSRBuffer.Create(L"SSR", NativeWidth, NativeHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	g_SceneColorBuffer.Create(L"SceneColorDeferred", NativeWidth, NativeHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	g_SceneDepthZ.Create(L"SceneDepthZ", NativeWidth, NativeHeight, DXGI_FORMAT_D32_FLOAT);
	g_GBufferA.Create(L"GBufferA", NativeWidth, NativeHeight, 1, DXGI_FORMAT_R10G10B10A2_UNORM);
	g_GBufferB.Create(L"GBufferB", NativeWidth, NativeHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
	g_GBufferC.Create(L"GBufferC", NativeWidth, NativeHeight, 1, DXGI_FORMAT_R8G8B8A8_UNORM);
}


void BufferManager::DestroyRenderingBuffers()
{
	g_SceneColorBuffer.Destroy();
}

