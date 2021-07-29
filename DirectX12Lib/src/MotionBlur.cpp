#include "MotionBlur.h"
#include "CommandContext.h"
#include "BufferManager.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "SamplerManager.h"
#include "D3D12RHI.h"
#include "Camera.h"

using namespace MotionBlur;
using namespace BufferManager;

namespace MotionBlur
{
	// R10G10B10  (3D velocity)
	FColorBuffer			g_VelocityBuffer;

	FRootSignature			s_RootSignature;

	ComPtr<ID3DBlob>		s_CameraVelocityShader;

	FComputePipelineState	s_CameraVelocityPSO;
}

void MotionBlur::Initialize(void)
{
	// Buffer
	uint32_t bufferWidth = g_SceneColorBuffer.GetWidth();
	uint32_t bufferHeight = g_SceneColorBuffer.GetHeight();

	//g_VelocityBuffer.Create(L"Motion Vectors", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R32_UINT);
	g_VelocityBuffer.Create(L"Motion Vectors", bufferWidth, bufferHeight, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);

	// RootSignatre
	s_RootSignature.Reset(3, 0);
	s_RootSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_ALL);
	s_RootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	s_RootSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
	s_RootSignature.Finalize(L"Motion Blur");

	// Shader
	s_CameraVelocityShader = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/CameraVelocityCS.hlsl", "cs_main", "cs_5_1");

	// PSO
	s_CameraVelocityPSO.SetRootSignature(s_RootSignature);
	s_CameraVelocityPSO.SetComputeShader(CD3DX12_SHADER_BYTECODE(s_CameraVelocityShader.Get()));
	s_CameraVelocityPSO.Finalize();
}

void ClearVelocityBuffer(FCommandContext& Context)
{
	Context.TransitionResource(g_VelocityBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	Context.ClearColor(g_VelocityBuffer);
}

void MotionBlur::GenerateCameraVelocityBuffer(FCommandContext& Context, const FCamera& camera)
{
	MotionBlur::GenerateCameraVelocityBuffer(Context, camera.GetReprojectionMatrix(), camera.GetNearClip(), camera.GetFarClip());
}

void MotionBlur::GenerateCameraVelocityBuffer(FCommandContext& BaseContext, const FMatrix& reprojectionMatrix, float nearClip, float farClip)
{
	FComputeContext& Context = BaseContext.GetComputeContext();

	uint32_t Width = g_SceneColorBuffer.GetWidth();
	uint32_t Height = g_SceneColorBuffer.GetHeight();

	float RcpHalfDimX = 2.0f / Width;
	float RcpHalfDimY = 2.0f / Height;
	float RcpZMagic = nearClip / (farClip - nearClip);

	// sreen space -> ndc
	FMatrix preMult = FMatrix(
		Vector4f(RcpHalfDimX, 0, 0, 0),
		Vector4f(0, -RcpHalfDimY, 0, 0),
		Vector4f(0, 0, 1, 0),
		Vector4f(-1, 1, 0, 1)
	);

	// ndc -> screen space without perspectiveDivide
	FMatrix postMult = FMatrix(
		Vector4f(1.0f / RcpHalfDimX, 0.0f, 0.0f, 0.0f),
		Vector4f(0.0f, -1.0f / RcpHalfDimY, 0.0f, 0.0f),
		Vector4f(0.0f, 0.0f, 1.0f, 0.0f),
		Vector4f(1.0f / RcpHalfDimX, 1.0f / RcpHalfDimY, 0.0f, 1.0f));

	FMatrix CurToPrevXForm = preMult * reprojectionMatrix * postMult;

	Context.SetRootSignature(s_RootSignature);
	Context.SetPipelineState(s_CameraVelocityPSO);

	Context.SetDynamicConstantBufferView(0, sizeof(CurToPrevXForm), &CurToPrevXForm);
	Context.TransitionResource(g_SceneDepthZ, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
	Context.TransitionResource(g_VelocityBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	// srv
	Context.SetDynamicDescriptor(1, 0, g_SceneDepthZ.GetSRV());
	// uav
	Context.SetDynamicDescriptor(2, 0, g_VelocityBuffer.GetUAV());

	Context.Dispatch2D(Width, Height);
}

void MotionBlur::Destroy(void)
{
	g_VelocityBuffer.Destroy();
}
