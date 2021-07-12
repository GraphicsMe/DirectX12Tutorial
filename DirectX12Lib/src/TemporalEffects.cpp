#include "TemporalEffects.h"
#include "CommandContext.h"
#include "BufferManager.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "SamplerManager.h"
#include "D3D12RHI.h"
#include "MotionBlur.h"
#include "RenderWindow.h"

using namespace TemporalEffects;
using namespace BufferManager;
using namespace MotionBlur;

namespace TemporalEffects
{
	bool					g_EnableTAA = true;
	bool					s_FirstFrame = false;

	uint32_t				s_FrameIndex = 0;
	uint32_t				s_FrameIndexMod2 = 0;

	float					s_JitterX = 0.5f;
	float					s_JitterY = 0.5f;
	float					s_JitterDeltaX = 0.0f;
	float					s_JitterDeltaY = 0.0f;

	FColorBuffer			g_TemporalColor[2];

	FRootSignature			s_RootSignature;

	ComPtr<ID3DBlob>		s_TemporalBlendShader;
	FComputePipelineState	s_TemporalBlendPSO;

	ComPtr<ID3DBlob>		s_ResolveTAAShader;
	FComputePipelineState	s_ResolveTAAPSO;

}

void TemporalEffects::Initialize(void)
{
	uint32_t bufferWidth = g_SceneColorBuffer.GetWidth();
	uint32_t bufferHeight = g_SceneColorBuffer.GetHeight();

	// Buffers
	g_TemporalColor[0].Create(L"Temporal Color 0", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);
	g_TemporalColor[1].Create(L"Temporal Color 1", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);

	// RootSignature
	FSamplerDesc SamplerLinearBorderDesc;
	SamplerLinearBorderDesc.SetLinearBorderDesc();

	s_RootSignature.Reset(3, 1);
	s_RootSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_ALL);
	s_RootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
	s_RootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 10);
	s_RootSignature.InitStaticSampler(0, SamplerLinearBorderDesc, D3D12_SHADER_VISIBILITY_ALL);
	s_RootSignature.Finalize(L"Temporal RS");

	// PSO
	s_TemporalBlendShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/TemporalBlendCS.hlsl", "cs_main", "cs_5_1");
	s_ResolveTAAShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/ResolveTAACS.hlsl", "cs_main", "cs_5_1");

	s_TemporalBlendPSO.SetRootSignature(s_RootSignature);
	s_TemporalBlendPSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(s_TemporalBlendShader.Get()));
	s_TemporalBlendPSO.Finalize();

	s_ResolveTAAPSO.SetRootSignature(s_RootSignature);
	s_ResolveTAAPSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(s_ResolveTAAShader.Get()));
	s_ResolveTAAPSO.Finalize();
}

void TemporalEffects::Destroy(void)
{
	g_TemporalColor[0].Destroy();
	g_TemporalColor[1].Destroy();
}

void TemporalEffects::Update(void)
{
	s_FrameIndex++;
	s_FrameIndexMod2 = s_FrameIndex % 2;

	// 对抖动进行更新
	if (g_EnableTAA)
	{
		static const float Halton23[8][2] =
		{
			{ 0.0f / 8.0f, 0.0f / 9.0f }, { 4.0f / 8.0f, 3.0f / 9.0f },
			{ 2.0f / 8.0f, 6.0f / 9.0f }, { 6.0f / 8.0f, 1.0f / 9.0f },
			{ 1.0f / 8.0f, 4.0f / 9.0f }, { 5.0f / 8.0f, 7.0f / 9.0f },
			{ 3.0f / 8.0f, 2.0f / 9.0f }, { 7.0f / 8.0f, 5.0f / 9.0f }
		};

		const float* Offset = Halton23[s_FrameIndex % 8];

		s_JitterDeltaX = s_JitterX - Offset[0];
		s_JitterDeltaY = s_JitterY - Offset[1];
		s_JitterX = Offset[0];
		s_JitterY = Offset[1];
	}
	else
	{
		s_JitterDeltaX = s_JitterX - 0.5f;
		s_JitterDeltaY = s_JitterY - 0.5f;
		s_JitterX = 0.5f;
		s_JitterY = 0.5f;
	}
}

uint32_t TemporalEffects::GetFrameIndexMod2(void)
{
	return s_FrameIndexMod2;
}

void TemporalEffects::GetJitterOffset(float& JitterX, float& JitterY)
{
	JitterX = s_JitterX;
	JitterY = s_JitterY;
}

void TemporalEffects::ClearHistory(FCommandContext& Context)
{
	if (g_EnableTAA)
	{
		Context.TransitionResource(g_TemporalColor[0], D3D12_RESOURCE_STATE_RENDER_TARGET);
		Context.TransitionResource(g_TemporalColor[1], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		Context.ClearColor(g_TemporalColor[0]);
		Context.ClearColor(g_TemporalColor[1]);
	}
}

void TemporalEffects::ResolveImage(FCommandContext& GraphicsContext,FColorBuffer& SceneColor)
{
	static bool s_EnableTAA = false;

	// 二者不相同，清理缓冲
	if (s_EnableTAA != g_EnableTAA)
	{
		TemporalEffects::ClearHistory(GraphicsContext);
		s_EnableTAA = g_EnableTAA;
		s_FirstFrame = true;
	}
	else
	{
		s_FirstFrame = false;
	}

	// 双缓冲
	uint32_t Src = s_FrameIndexMod2;
	uint32_t Dst = Src ^ 1;

	FComputeContext& Context = GraphicsContext.GetComputeContext();

	if (g_EnableTAA)
	{
		ApplyTemporalAA(Context, SceneColor);
		SharpenImage(Context, SceneColor, g_TemporalColor[Dst]);
	}
}


void TemporalEffects::ApplyTemporalAA(FComputeContext& Context, FColorBuffer& SceneColor)
{
	RenderWindow& renderWindow = RenderWindow::Get();
	FDepthBuffer& SceneDepthBuffer = renderWindow.GetDepthBuffer();

	uint32_t Src = s_FrameIndexMod2;
	uint32_t Dst = Src ^ 1;
	
	Context.SetRootSignature(s_RootSignature);
	Context.SetPipelineState(s_TemporalBlendPSO);

	__declspec(align(16)) struct ConstantBuffer
	{
		float RcpBufferDim[2];
		float TemporalBlendFactor;
		float RcpSeedLimiter;
		float CombinedJitter[2];
	};

	ConstantBuffer cbv;
	cbv.RcpBufferDim[0] = 1.0f / g_SceneColorBuffer.GetWidth();
	cbv.RcpBufferDim[1] = 1.0f / g_SceneColorBuffer.GetHeight();
	cbv.TemporalBlendFactor = s_FirstFrame ? 1 : 0.02f;
	cbv.RcpSeedLimiter = 0;
	cbv.CombinedJitter[0] = s_JitterDeltaX;
	cbv.CombinedJitter[1] = s_JitterDeltaY;

	Context.SetDynamicConstantBufferView(0, sizeof(cbv), &cbv);
	Context.TransitionResource(SceneColor, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(SceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(g_VelocityBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(g_TemporalColor[Src], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(g_TemporalColor[Dst], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// srv
	Context.SetDynamicDescriptor(1, 0, SceneColor.GetSRV());
	Context.SetDynamicDescriptor(1, 1, g_TemporalColor[Src].GetSRV());
	Context.SetDynamicDescriptor(1, 2, g_VelocityBuffer.GetSRV());
	Context.SetDynamicDescriptor(1, 3, SceneDepthBuffer.GetDepthSRV());
	// uav
	Context.SetDynamicDescriptor(2, 0, g_TemporalColor[Dst].GetUAV());

	Context.Dispatch2D(SceneColor.GetWidth(), SceneColor.GetHeight(), 8, 8);
}

void TemporalEffects::SharpenImage(FComputeContext& Context, FColorBuffer& SceneColor, FColorBuffer& TemporalColor)
{
	//
	Context.SetRootSignature(s_RootSignature);
	Context.SetPipelineState(s_ResolveTAAPSO);

	Context.TransitionResource(SceneColor, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	Context.TransitionResource(TemporalColor, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	Context.SetDynamicDescriptor(1, 0, TemporalColor.GetSRV());
	Context.SetDynamicDescriptor(2, 0, SceneColor.GetUAV());

	Context.Dispatch2D(SceneColor.GetWidth(), SceneColor.GetHeight());
}
