#include "TemporalEffects.h"
#include "CommandContext.h"
#include "BufferManager.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "SamplerManager.h"
#include "D3D12RHI.h"
#include "MotionBlur.h"
#include "Camera.h"

using namespace TemporalEffects;
using namespace BufferManager;
using namespace MotionBlur;

namespace TemporalEffects
{
	bool					g_EnableTAA = true;
	bool					s_FirstFrame = false;

	uint32_t				s_FrameIndex = 0;
	uint32_t				s_FrameIndexMod2 = 0;

	float					s_JitterX = 0;
	float					s_JitterY = 0;
	float					s_PrevJitterX = 0;
	float					s_PrevJitterY = 0;
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

/** [ Halton 1964, "Radical-inverse quasi-random point sequence" ] */
inline float Halton(int32_t Index, int32_t Base)
{
	float Result = 0.0f;
	float InvBase = 1.0f / Base;
	float Fraction = InvBase;
	while (Index > 0)
	{
		Result += (Index % Base) * Fraction;
		Index /= Base;
		Fraction *= InvBase;
	}
	return Result;
}

void TemporalEffects::Update(void)
{
	s_FrameIndex++;
	s_FrameIndexMod2 = s_FrameIndex % 2;

	// 对抖动进行更新
	if (g_EnableTAA)
	{
		s_PrevJitterX = s_JitterX;
		s_PrevJitterY = s_JitterY;

		// Uniformly distribute temporal jittering in [-.5; .5], because there is no longer any alignement of input and output pixels.
		//s_JitterX = Halton(s_FrameIndex , 2) - 0.5f;
		//s_JitterX = Halton(s_FrameIndex , 3) - 0.5f;
		float u1 = Halton(s_FrameIndex, 2);
		float u2 = Halton(s_FrameIndex, 3);

		// Generates samples in normal distribution
		// exp( x^2 / Sigma^2 )
		float FilterSize = 1;

		// Scale distribution to set non-unit variance
		// Variance = Sigma^2
		float Sigma = 0.47f * FilterSize;

		// Window to [-0.5, 0.5] output
		// Without windowing we could generate samples far away on the infinite tails.
		float OutWindow = 0.5f;
		float InWindow = std::exp(-0.5f * (float)std::pow(OutWindow / Sigma, 2));

		// Box-Muller transform
		float Theta = 2.0f * MATH_PI * u2;
		float r = Sigma * std::sqrt(-2.0f * std::log((1.0f - u1) * InWindow + u1));

		s_JitterX = r * std::cos(Theta);
		s_JitterY = r * std::sin(Theta);
	}
	else
	{
		s_JitterX = 0;
		s_JitterY = 0;
	}
}

uint32_t TemporalEffects::GetFrameIndexMod2(void)
{
	return s_FrameIndexMod2;
}

void TemporalEffects::GetJitterOffset(Vector4f& TemporalAAJitter,float Width,float Height)
{
	TemporalAAJitter.x = s_JitterX * 2.0f / Width;
	TemporalAAJitter.y = s_JitterY * -2.0f / Height;
	TemporalAAJitter.z = s_PrevJitterX * 2.0f / Width;
	TemporalAAJitter.w = s_PrevJitterY * -2.0f / Height;
}

FMatrix TemporalEffects::HackAddTemporalAAProjectionJitter(const FCamera& Camera, float Width, float Height, bool PrevFrame /*= false*/)
{
	//Assert(abs(s_JitterX <= 0.5f) && abs(s_JitterY) <= 0.5f);
	FMatrix ProjectMatrix;
	Vector2f TemporalAAProjectionJitter;
	if (PrevFrame)
	{
		ProjectMatrix = FMatrix(Camera.GetPreviousProjectionMatrix());
		TemporalAAProjectionJitter.x = s_PrevJitterX * 2.0f / Width;
		TemporalAAProjectionJitter.y = s_PrevJitterY * -2.0f / Height;
	}
	else
	{
		ProjectMatrix = FMatrix(Camera.GetProjectionMatrix());
		TemporalAAProjectionJitter.x = s_JitterX * 2.0f / Width;
		TemporalAAProjectionJitter.y = s_JitterY * -2.0f / Height;
	}

	ProjectMatrix.r2[0] += TemporalAAProjectionJitter.x;
	ProjectMatrix.r2[1] += TemporalAAProjectionJitter.y;

	return ProjectMatrix;
}

FColorBuffer& TemporalEffects::GetHistoryBuffer()
{
	return g_TemporalColor[s_FrameIndexMod2];
}

uint32_t TemporalEffects::GetFrameIndex()
{
	return s_FrameIndex;
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
	cbv.CombinedJitter[0] = 0;
	cbv.CombinedJitter[1] = 0;
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
