#include "DepthOfField.h"
#include "CommandContext.h"
#include "BufferManager.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "SamplerManager.h"
#include "D3D12RHI.h"
#include "TemporalEffects.h"
#include "RenderWindow.h"

using namespace BufferManager;
using namespace TemporalEffects;

namespace DepthOfField
{
	/*
	* Whether enbale DOF effect
	*/
	bool	g_EnableDepthOfField  = true;

	/*
	* the distance to the point of focus 
	* 聚焦位置
	* 单位米
	*/
	float	g_FoucusDistance = 1.55f;
	/*
	* 聚焦位置
	* 单位米
	*/
	float   g_FoucusRange = .62f;

	/*
	* 散景模糊半径
	*/
	float   g_BokehRadius = 10.0f;

	/*
	* 光圈大小
	*/
	float   g_Aperture = 5.6f;

	/*
	* the distance between the lens and the film. The larger the value is, the shallower the depth of field is.
	* 焦距
	* 单位毫米
	*/
	float   g_FocalLength = 50.0f;

	/*
	* Select the convolution kernel size of the bokeh filter from the dropdown.
	* This setting determines the maximum radius of bokeh. It also affects the performance.
	* The larger the kernel is, the longer the GPU time is required.
	*/
	float	g_MaxBlurSize;

	FColorBuffer			g_CoCBuffer;
	FColorBuffer			g_PrefilterColor;
	FColorBuffer			g_BokehColor;
	FColorBuffer			g_TempSceneColor;

	FRootSignature			s_RootSignature;

	ComPtr<ID3DBlob>		s_FragCoCShader;
	FComputePipelineState	s_FragCoCPSO;

	ComPtr<ID3DBlob>		s_FragPrefilterShader;
	FComputePipelineState	s_FragPrefilterPSO;

	ComPtr<ID3DBlob>		s_FragBokenhFilterShader;
	FComputePipelineState	s_FragBokenhFilterPSO;

	ComPtr<ID3DBlob>		s_FragPostfilterShader;
	FComputePipelineState	s_FragPostfilterPSO;

	ComPtr<ID3DBlob>		s_FragCombineShader;
	FComputePipelineState	s_FragCombinePSO;
}

void DepthOfField::Initialize(void)
{
	// Buffers
	uint32_t bufferWidth = g_SceneColorBuffer.GetWidth();
	uint32_t bufferHeight = g_SceneColorBuffer.GetHeight();

	uint32_t halfWidth = bufferWidth >> 1;
	uint32_t halfHeight = bufferHeight >> 1;

	g_CoCBuffer.Create(L"COC Buffer", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R16_FLOAT);

	g_TempSceneColor.Create(L"TempSceneColor", bufferWidth, bufferHeight, 1, g_SceneColorBuffer.GetFormat());

	g_PrefilterColor.Create(L"PrefilterColor", halfWidth, halfHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);

	g_BokehColor.Create(L"BokehColor", halfWidth, halfHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);

	//
	FSamplerDesc DefaultSamplerDesc(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	s_RootSignature.Reset(3, 1);
	s_RootSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_ALL);
	s_RootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
	s_RootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 10);
	s_RootSignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_ALL);
	s_RootSignature.Finalize(L"DOF");

	// coc
	s_FragCoCShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/DepthOfField_CoC.hlsl", "cs_FragCoC", "cs_5_1");
	s_FragCoCPSO.SetRootSignature(s_RootSignature);
	s_FragCoCPSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(s_FragCoCShader.Get()));
	s_FragCoCPSO.Finalize();

	// Prefilter
	s_FragPrefilterShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/DepthOfField_Prefilter.hlsl", "cs_FragPrefilter", "cs_5_1");
	s_FragPrefilterPSO.SetRootSignature(s_RootSignature);
	s_FragPrefilterPSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(s_FragPrefilterShader.Get()));
	s_FragPrefilterPSO.Finalize();

	// BokenhFilter
	s_FragBokenhFilterShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/DepthOfField_BokehFilter.hlsl", "cs_FragBokehFilter", "cs_5_1");
	s_FragBokenhFilterPSO.SetRootSignature(s_RootSignature);
	s_FragBokenhFilterPSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(s_FragBokenhFilterShader.Get()));
	s_FragBokenhFilterPSO.Finalize();

	// PostFilter
	s_FragPostfilterShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/DepthOfField_Postfilter.hlsl", "cs_FragPostfilter", "cs_5_1");
	s_FragPostfilterPSO.SetRootSignature(s_RootSignature);
	s_FragPostfilterPSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(s_FragPostfilterShader.Get()));
	s_FragPostfilterPSO.Finalize();

	// Combine
	s_FragCombineShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/DepthOfField_Combine.hlsl", "cs_FragCombine", "cs_5_1");
	s_FragCombinePSO.SetRootSignature(s_RootSignature);
	s_FragCombinePSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(s_FragCombineShader.Get()));
	s_FragCombinePSO.Finalize();
}

void DepthOfField::Destroy(void)
{
	g_CoCBuffer.Destroy();
	g_PrefilterColor.Destroy();
	g_BokehColor.Destroy();
	g_TempSceneColor.Destroy();
}

void DepthOfField::Render(FCommandContext& BaseContext, float NearClipDist, float FarClipDist)
{
	if (!g_EnableDepthOfField)
	{
		return;
	}

	static bool s_EnableDOF = false;

	if (s_EnableDOF != g_EnableDepthOfField)
	{
		//BaseContext.TransitionResource(g_CoCBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		//BaseContext.ClearColor(g_CoCBuffer);
	}

	RenderWindow& renderWindow = RenderWindow::Get();
	FDepthBuffer& SceneDepthBuffer = renderWindow.GetDepthBuffer();

	FComputeContext& Context = BaseContext.GetComputeContext();

	// CoC
	{
		Context.SetRootSignature(s_RootSignature);
		Context.SetPipelineState(s_FragCoCPSO);

		__declspec(align(16)) struct DoFCoCConstantBuffer
		{
			float		FoucusDistance;
			float		FoucusRange;
			float		FocalLength;
			float		Aperture;
			Vector2f	ClipSpaceNearFar;
			float		BokehRadius;
			float		RcpBokehRadius;
		};

		const float width = static_cast<float>(g_CoCBuffer.GetWidth());
		const float height = static_cast<float>(g_CoCBuffer.GetHeight());
		const float rcpWidth = 1.f / width;
		const float rcpHeight = 1.f / height;

		DoFCoCConstantBuffer cbv;
		cbv.FoucusDistance = g_FoucusDistance;
		cbv.FoucusRange = g_FoucusRange;
		cbv.FocalLength = g_FocalLength;
		cbv.Aperture = g_Aperture;
		cbv.ClipSpaceNearFar[0] = NearClipDist;
		cbv.ClipSpaceNearFar[1] = FarClipDist;
		cbv.BokehRadius = g_BokehRadius;
		cbv.RcpBokehRadius = 1.0f / g_BokehRadius;

		Context.SetDynamicConstantBufferView(0, sizeof(cbv), &cbv);

		Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(SceneDepthBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_CoCBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		Context.TransitionResource(g_TempSceneColor, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		// srv
		Context.SetDynamicDescriptor(1, 0, g_SceneColorBuffer.GetSRV());
		Context.SetDynamicDescriptor(1, 1, SceneDepthBuffer.GetSRV());
		// uav
		Context.SetDynamicDescriptor(2, 0, g_CoCBuffer.GetUAV());
		Context.SetDynamicDescriptor(2, 1, g_TempSceneColor.GetUAV());

		Context.Dispatch2D(g_CoCBuffer.GetWidth(), g_CoCBuffer.GetHeight(), 8, 8);
	}

	// DownSample and Prefilter
	{
		Context.SetRootSignature(s_RootSignature);
		Context.SetPipelineState(s_FragPrefilterPSO);

		__declspec(align(16)) struct DoFPrefilterConstantBuffer
		{
			Vector4f	BokehRadius;
		};

		DoFPrefilterConstantBuffer cbv;
		cbv.BokehRadius = g_BokehRadius;
		Context.SetDynamicConstantBufferView(0, sizeof(cbv), &cbv);

		Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_CoCBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_PrefilterColor, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		// srv
		Context.SetDynamicDescriptor(1, 0, g_SceneColorBuffer.GetSRV());
		Context.SetDynamicDescriptor(1, 1, g_CoCBuffer.GetSRV());
		// uav
		Context.SetDynamicDescriptor(2, 0, g_PrefilterColor.GetUAV());

		Context.Dispatch2D(g_PrefilterColor.GetWidth(), g_PrefilterColor.GetHeight(), 8, 8);
	}

	// Bokeh Filter
	{
		Context.SetRootSignature(s_RootSignature);
		Context.SetPipelineState(s_FragBokenhFilterPSO);

		__declspec(align(16)) struct DoFBokenhFilterConstantBuffer
		{
			float	BokehRadius;
		};

		DoFBokenhFilterConstantBuffer cbv;
		cbv.BokehRadius = g_BokehRadius;

		Context.SetDynamicConstantBufferView(0, sizeof(cbv), &cbv);

		Context.TransitionResource(g_PrefilterColor, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_BokehColor, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		// srv
		Context.SetDynamicDescriptor(1, 0, g_PrefilterColor.GetSRV());
		// uav
		Context.SetDynamicDescriptor(2, 0, g_BokehColor.GetUAV());

		Context.Dispatch2D(g_BokehColor.GetWidth(), g_BokehColor.GetHeight(), 8, 8);
	}

	// Postfilter
	{
		Context.SetRootSignature(s_RootSignature);
		Context.SetPipelineState(s_FragPostfilterPSO);

		__declspec(align(16)) struct DoFPostfilterConstantBuffer
		{
		};

		DoFPostfilterConstantBuffer cbv;
		Context.SetDynamicConstantBufferView(0, sizeof(cbv), &cbv);

		Context.TransitionResource(g_BokehColor, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		// resue prefilterBuffer
		Context.TransitionResource(g_PrefilterColor, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		// srv
		Context.SetDynamicDescriptor(1, 0, g_BokehColor.GetSRV());
		// uav
		Context.SetDynamicDescriptor(2, 0, g_PrefilterColor.GetUAV());

		Context.Dispatch2D(g_PrefilterColor.GetWidth(), g_PrefilterColor.GetHeight(), 8, 8);
	}

	// Combine
	{
		Context.SetRootSignature(s_RootSignature);
		Context.SetPipelineState(s_FragCombinePSO);

		__declspec(align(16)) struct DoFDOFCombineConstantBuffer
		{
			float	BokehRadius;
		};

		DoFDOFCombineConstantBuffer cbv;
		cbv.BokehRadius = g_BokehRadius;
		Context.SetDynamicConstantBufferView(0, sizeof(cbv), &cbv);

		Context.TransitionResource(g_TempSceneColor, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_CoCBuffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_PrefilterColor, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		Context.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		// srv
		Context.SetDynamicDescriptor(1, 0, g_TempSceneColor.GetSRV());
		Context.SetDynamicDescriptor(1, 1, g_CoCBuffer.GetSRV());
		Context.SetDynamicDescriptor(1, 2, g_PrefilterColor.GetSRV());
		// uav
		Context.SetDynamicDescriptor(2, 0, g_SceneColorBuffer.GetUAV());

		Context.Dispatch2D(g_SceneColorBuffer.GetWidth(), g_SceneColorBuffer.GetHeight(), 8, 8);
	}
}