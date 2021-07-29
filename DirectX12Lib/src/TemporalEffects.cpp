#include "TemporalEffects.h"
#include "CommandContext.h"
#include "BufferManager.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "SamplerManager.h"
#include "D3D12RHI.h"
#include "MotionBlur.h"


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
	SamplerLinearBorderDesc.SetLinearBorderDesc(Vector4f(0.f));

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
		const float* Offset = nullptr;
		float Scale = 1.f;

		bool EnableMSFTTAA = false;
		if (EnableMSFTTAA)
		{
			static const float Halton23[8][2] =
			{
				{ 0.0f / 8.0f, 0.0f / 9.0f }, { 4.0f / 8.0f, 3.0f / 9.0f },
				{ 2.0f / 8.0f, 6.0f / 9.0f }, { 6.0f / 8.0f, 1.0f / 9.0f },
				{ 1.0f / 8.0f, 4.0f / 9.0f }, { 5.0f / 8.0f, 7.0f / 9.0f },
				{ 3.0f / 8.0f, 2.0f / 9.0f }, { 7.0f / 8.0f, 5.0f / 9.0f }
			};
			Offset = Halton23[s_FrameIndex % 8];
		}
		else
		{
			// following work of Vaidyanathan et all: https://software.intel.com/content/www/us/en/develop/articles/coarse-pixel-shading-with-temporal-supersampling.html
			static const float Halton23_16[16][2] = { 
				{ 0.0f, 0.0f }, { 0.5f, 0.333333f }, { 0.25f, 0.666667f }, { 0.75f, 0.111111f }, 
				{ 0.125f, 0.444444f }, { 0.625f, 0.777778f }, { 0.375f ,0.222222f }, { 0.875f ,0.555556f },
				{ 0.0625f, 0.888889f }, { 0.562500f,0.037037f }, { 0.3125f, 0.37037f }, { 0.8125f, 0.703704f }, 
				{ 0.1875f,0.148148f }, { 0.6875f, 0.481481f }, { 0.4375f ,0.814815f }, { 0.9375f ,0.259259f }
			};

			static const float BlueNoise_16[16][2] = { 
				{ 1.5f, 0.59375f }, { 1.21875f, 1.375f }, { 1.6875f, 1.90625f }, { 0.375f, 0.84375f },
				{ 1.125f, 1.875f }, { 0.71875f, 1.65625f }, { 1.9375f ,0.71875f }, { 0.65625f ,0.125f }, 
				{ 0.90625f, 0.9375f }, { 1.65625f, 1.4375f }, { 0.5f, 1.28125f }, { 0.21875f, 0.0625f },
				{ 1.843750,0.312500 }, { 1.09375f, 0.5625f }, { 0.0625f, 1.21875f }, { 0.28125f, 1.65625f },
			};

			Scale = 1.f;
			Offset = Halton23_16[s_FrameIndex % 16];
		}

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

FColorBuffer& TemporalEffects::GetHistoryBuffer()
{
	return g_TemporalColor[s_FrameIndexMod2];
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
	uint32_t Src = s_FrameIndexMod2;
	uint32_t Dst = Src ^ 1;
	
	Context.SetRootSignature(s_RootSignature);
	Context.SetPipelineState(s_TemporalBlendPSO);

	__declspec(align(16)) struct ConstantBuffer
	{
		Vector4f	Resolution;//width, height, 1/width, 1/height
		float		CombinedJitter[2];
		int			FrameIndex;
	};

	const float width = static_cast<float>(g_SceneColorBuffer.GetWidth());
	const float height = static_cast<float>(g_SceneColorBuffer.GetHeight());
	const float rcpWidth = 1.f / width;
	const float rcpHeight = 1.f / height;

	ConstantBuffer cbv;
	cbv.Resolution = Vector4f(width, height, rcpWidth, rcpHeight);
	cbv.CombinedJitter[0] = s_JitterDeltaX;
	cbv.CombinedJitter[1] = s_JitterDeltaY;
	cbv.FrameIndex = s_FirstFrame ? 1 : s_FrameIndex;

	Context.SetDynamicConstantBufferView(0, sizeof(cbv), &cbv);
	Context.TransitionResource(SceneColor, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(g_SceneDepthZ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(g_VelocityBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(g_TemporalColor[Src], D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(g_TemporalColor[Dst], D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// srv
	Context.SetDynamicDescriptor(1, 0, SceneColor.GetSRV());
	Context.SetDynamicDescriptor(1, 1, g_TemporalColor[Src].GetSRV());
	Context.SetDynamicDescriptor(1, 2, g_VelocityBuffer.GetSRV());
	Context.SetDynamicDescriptor(1, 3, g_SceneDepthZ.GetSRV());
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
