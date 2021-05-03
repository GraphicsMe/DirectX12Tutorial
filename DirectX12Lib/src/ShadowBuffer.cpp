#include "ShadowBuffer.h"
#include "CommandContext.h"

void FShadowBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height)
{
	FDepthBuffer::Create(Name, Width, Height, DXGI_FORMAT_D16_UNORM);

	m_Viewport.TopLeftX = 0.f;
	m_Viewport.TopLeftY = 0.f;
	m_Viewport.Width = (float)Width;
	m_Viewport.Height = (float)Height;
	m_Viewport.MinDepth = 0.f;
	m_Viewport.MaxDepth = 1.f;

	m_Scissor.left = 0;
	m_Scissor.top = 0;
	m_Scissor.right = (LONG)Width;
	m_Scissor.bottom = (LONG)Height;
}

void FShadowBuffer::BeginRendering(FCommandContext& CommandContext)
{
	CommandContext.TransitionResource(*this, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
	CommandContext.ClearDepth(*this);
	CommandContext.SetDepthStencilTarget(GetDSV());
	CommandContext.SetViewportAndScissor(m_Viewport, m_Scissor);
}

void FShadowBuffer::EndRendering(FCommandContext& CommandContext)
{
	CommandContext.TransitionResource(*this, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
}
