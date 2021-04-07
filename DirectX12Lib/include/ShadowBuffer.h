#pragma once

#include "DepthBuffer.h"

class FCommandContext;
class FShadowBuffer : public FDepthBuffer
{
public:
	FShadowBuffer() {}

	void Create(const std::wstring& Name, uint32_t Width, uint32_t Height);
	void BeginRendering(FCommandContext& CommandContext);
	void EndRendering(FCommandContext& CommandContext);

protected:
	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_Scissor;
};