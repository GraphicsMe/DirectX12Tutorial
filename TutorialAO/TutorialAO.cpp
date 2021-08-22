#include "ApplicationWin32.h"
#include "Game.h"
#include "Common.h"
#include "MathLib.h"
#include "Camera.h"
#include "CommandQueue.h"
#include "D3D12RHI.h"
#include "d3dx12.h"
#include "RenderWindow.h"
#include "CommandListManager.h"
#include "CommandContext.h"
#include "RootSignature.h"
#include "GpuBuffer.h"
#include "PipelineState.h"
#include "DirectXTex.h"
#include "SamplerManager.h"
#include "Model.h"
#include "ShadowBuffer.h"
#include "Light.h"
#include "CubeBuffer.h"
#include "GameInput.h"
#include "ImguiManager.h"
#include "GenerateMips.h"
#include "TemporalEffects.h"
#include "BufferManager.h"
#include "MotionBlur.h"
#include "PostProcessing.h"
#include "DepthOfField.h"
#include "UserMarkers.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include<sstream>

extern FCommandListManager g_CommandListManager;

const int PREFILTERED_SIZE = 256;

using namespace BufferManager;

class TutorialAO : public FGame
{
public:
	TutorialAO(const GameDesc& Desc) : FGame(Desc)
	{
	}

	void OnStartup()
	{
		SetupMesh();
		SetupShaders();
		SetupPipelineState();
		SetupCameraLight();

		PostProcessing::g_EnableBloom = false;
		PostProcessing::g_EnableSSR = false;
	}

	void OnShutdown()
	{
	}

	void OnUpdate()
	{
		tEnd = std::chrono::high_resolution_clock::now();
		float delta = std::chrono::duration<float, std::milli>(tEnd - tStart).count();
		tStart = std::chrono::high_resolution_clock::now();
		m_Camera.Update(delta);
		
		// update lights
		static float lightAnimTime = 0.0f;
		//if (false)
		{
			lightAnimTime += static_cast<float>(delta) * 0.0005f * MATH_PI;
		}

		static const Vector3f LightColors[] = { Vector3f(1,1,1), Vector3f(1,0.325f,0.286f), Vector3f(0,0,1) };
		const float radius = 1.0f;
		float       directionalLightOffset = m_NumDirectionalLights > 0 ? 2.0f * MATH_PI / m_NumDirectionalLights : 0;

		m_DirectionalLights.resize(m_NumDirectionalLights);
		for (int i = 0; i < m_NumDirectionalLights; ++i)
		{
			FDirectionalLight& l = m_DirectionalLights[i];

			float angle = lightAnimTime + directionalLightOffset * i;

			Vector3f positionWS(
				-static_cast<float>(std::cos(angle)) * radius,
				-static_cast<float>(std::sin(angle)) * radius,
				-radius
			);

			Vector3f directionWS = positionWS.Normalize();
			l.SetDirection(directionWS);
			l.SetColor(LightColors[i]);
		}


		m_Mesh->Update();
		if (m_RotateMesh)
		{
			m_RotateY += delta * 0.0005f;
			m_RotateY = fmodf(m_RotateY, MATH_2PI);
			m_Mesh->SetRotation(FMatrix::RotateY(m_RotateY));
		}

		if (GameInput::IsKeyDown('F'))
			SetupCameraLight();

		if (GameInput::IsKeyDown(32))
		{
			if (m_RotateMesh)
			{
				m_RotateMesh = false;
			}
			else
			{
				m_RotateMesh = true;
			}
		}

		m_MainViewport.TopLeftX = 0.0f;
		m_MainViewport.TopLeftY = 0.0f;

		m_MainViewport.Width = (float)RenderWindow::Get().GetBackBuffer().GetWidth();
		m_MainViewport.Height = (float)RenderWindow::Get().GetBackBuffer().GetHeight();
		m_MainViewport.MinDepth = 0.0f;
		m_MainViewport.MaxDepth = 1.0f;

		m_MainScissor.left = 0;
		m_MainScissor.top = 0;
		m_MainScissor.right = (LONG)RenderWindow::Get().GetBackBuffer().GetWidth();
		m_MainScissor.bottom = (LONG)RenderWindow::Get().GetBackBuffer().GetHeight();

		TemporalEffects::Update();
	}

	void OnGUI(FCommandContext& CommandContext)
	{
		static bool ShowConfig = true;
		if (!ShowConfig)
			return;

		ImguiManager::Get().NewFrame();

		ImGui::SetNextWindowPos(ImVec2(1, 1));
		ImGui::SetNextWindowCollapsed(true, ImGuiCond_Once);
		if (ImGui::Begin("Config", &ShowConfig, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::BeginGroup();
			ImGui::Text("Show Mode");
			ImGui::Indent(20);
			ImGui::EndGroup();
		}
		ImGui::End();

		ImguiManager::Get().Render(CommandContext, RenderWindow::Get());
	}

	void OnRender()
	{
		FCommandContext& CommandContext = FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE_DIRECT, L"3D Queue");

		BasePass(CommandContext, true);
		
		for (int i = 0; i < m_NumDirectionalLights; ++i)
		{
			FDirectionalLight& l = m_DirectionalLights[i];
			LightingPass(CommandContext, l.GetDirection(), 1.0, l.GetColor(), true);
		}

		PostProcessing::Render(CommandContext);

		OnGUI(CommandContext);

		CommandContext.TransitionResource(RenderWindow::Get().GetBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);
		CommandContext.Finish(true);

		RenderWindow::Get().Present();
	}

private:
	void SetupCameraLight()
	{
		m_Camera = FCamera(Vector3f(0.0f, 0, -0.5f), Vector3f(0.f, 0.0f, 0.f), Vector3f(0.f, 1.f, 0.f));
		m_Camera.SetMouseMoveSpeed(1e-3f);
		m_Camera.SetMouseRotateSpeed(1e-4f);

		const float FovVertical = MATH_PI / 4.f;
		m_Camera.SetPerspectiveParams(FovVertical, (float)GetDesc().Width / GetDesc().Height, 0.1f, 100.f);
	}

	void ParsePath()
	{
		size_t lastPeriodIndex = m_HDRFilePath.find_last_of('.');

		if (lastPeriodIndex == m_HDRFilePath.npos)
		{
			printf("Input HDR file pathv is wrong\n");
			exit(-1);
		}

		m_HDRFileName = m_HDRFilePath.substr(0, lastPeriodIndex);

		m_IrradianceMapPath = m_HDRFileName + std::wstring(L"_IrradianceMap.dds");
		m_PrefilteredMapPath = m_HDRFileName + std::wstring(L"_PrefilteredMap.dds");
		m_SHCoeffsPath = m_HDRFileName + std::wstring(L"_SHCoeffs.txt");
		m_PreIntegrateBRDFPath = std::wstring(L"../Resources/HDR/PreIntegrateBRDF.dds");

		// check file exit
		bool isExist = true;
		isExist &= CheckFileExist(m_IrradianceMapPath);
		isExist &= CheckFileExist(m_PrefilteredMapPath);
		isExist &= CheckFileExist(m_SHCoeffsPath);
		isExist &= CheckFileExist(m_PreIntegrateBRDFPath);
		if (!isExist)
		{
			printf("this input HDR file do not precompute to generate IBL maps\n");
			exit(-1);
		}
	}

	bool CheckFileExist(const std::wstring& name)
	{
		std::ifstream f(name.c_str());
		const bool isExist = f.good();
		f.close();
		return isExist;
	}

	void SetupMesh()
	{
		// https://sketchfab.com/3d-models/sponza-scene-725bc7dc88b049a5b38bfa718b218909
		m_Mesh = std::make_unique<FModel>("../Resources/Models/SponzaPBR_dds2tga/SponzaPBR.obj", false, false, false);
		m_Mesh->SetPosition(0, 1.5, 0);
	}

	void SetupShaders()
	{
		// Mesh
		m_MeshVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PBR.hlsl", "VS_PBR", "vs_5_1");
		m_MeshPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PBR.hlsl", "PS_PBR", "ps_5_1");

		// Lighting
		m_ScreenQuadVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PostProcess.hlsl", "VS_ScreenQuad", "vs_5_1");
		m_PBRLightingPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/SponzaPBR.hlsl", "PS_SponzaPBR", "ps_5_1");
	}

	void SetupPipelineState()
	{
		FSamplerDesc DefaultSamplerDesc(D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
		// Mesh
		m_MeshSignature.Reset(3, 1);
		m_MeshSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
		m_MeshSignature[1].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
		m_MeshSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10); //pbr 7, irradiance, prefiltered, preintegratedGF
		m_MeshSignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_MeshSignature.Finalize(L"Mesh RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		std::vector<D3D12_INPUT_ELEMENT_DESC> MeshLayout;
		m_Mesh->GetMeshLayout(MeshLayout);
		Assert(MeshLayout.size() > 0);
		m_MeshPSO.SetInputLayout((UINT)MeshLayout.size(), &MeshLayout[0]);

		m_MeshPSO.SetRootSignature(m_MeshSignature);

		D3D12_RASTERIZER_DESC CullBackRS;
		CullBackRS = FPipelineState::RasterizerTwoSided;
		CullBackRS.CullMode = D3D12_CULL_MODE_BACK;
		m_MeshPSO.SetRasterizerState(CullBackRS);
		m_MeshPSO.SetBlendState(FPipelineState::BlendDisable);

		D3D12_DEPTH_STENCIL_DESC DSS = FPipelineState::DepthStateReadWrite;
		DSS.StencilEnable = TRUE;
		DSS.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
		DSS.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
		DSS.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
		DSS.FrontFace.StencilPassOp = D3D12_STENCIL_OP_REPLACE;
		DSS.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
		DSS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
		DSS.BackFace = DSS.FrontFace;
		m_MeshPSO.SetDepthStencilState(DSS);

		m_MeshPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		DXGI_FORMAT RTFormats[] = {
			g_SceneColorBuffer.GetFormat(), g_GBufferA.GetFormat(), g_GBufferB.GetFormat(), g_GBufferC.GetFormat(), MotionBlur::g_VelocityBuffer.GetFormat(),
		};
		m_MeshPSO.SetRenderTargetFormats(5, RTFormats, g_SceneDepthZ.GetFormat());
		m_MeshPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_MeshVS.Get()));
		m_MeshPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_MeshPS.Get()));
		m_MeshPSO.Finalize();

		// lighting pass
		FSamplerDesc LinearSamplerDesc(D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		m_PBRLightingSignature.Reset(3, 1);
		m_PBRLightingSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
		m_PBRLightingSignature[1].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
		m_PBRLightingSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
		m_PBRLightingSignature.InitStaticSampler(0, LinearSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_PBRLightingSignature.Finalize(L"Lighting RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_PBRLightingPSO.SetRootSignature(m_PBRLightingSignature);
		m_PBRLightingPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);

		D3D12_BLEND_DESC BlendAdd = FPipelineState::BlendTraditional;
		BlendAdd.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		BlendAdd.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		BlendAdd.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		BlendAdd.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		m_PBRLightingPSO.SetBlendState(BlendAdd);
		m_PBRLightingPSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
		// no need to set input layout
		m_PBRLightingPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

		m_PBRLightingPSO.SetRenderTargetFormats(1, &g_SceneColorBuffer.GetFormat(), g_SceneDepthZ.GetFormat());

		m_PBRLightingPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_ScreenQuadVS.Get()));
		m_PBRLightingPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_PBRLightingPS.Get()));
		m_PBRLightingPSO.Finalize();
	}

	void BasePass(FCommandContext& GfxContext, bool Clear)
	{
		UserMarker GPUMaker(GfxContext, "Base Pass");

		// Set necessary state.
		GfxContext.SetRootSignature(m_MeshSignature);
		GfxContext.SetPipelineState(m_MeshPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

		FColorBuffer& SceneBuffer = g_SceneColorBuffer;
		FDepthBuffer& DepthBuffer = g_SceneDepthZ;

		// Indicate that the back buffer will be used as a render target.
		GfxContext.TransitionResource(SceneBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(g_GBufferA, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(g_GBufferB, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(g_GBufferC, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(MotionBlur::g_VelocityBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(g_SceneDepthZ, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

		D3D12_CPU_DESCRIPTOR_HANDLE RTVs[] = {
			SceneBuffer.GetRTV(), g_GBufferA.GetRTV(), g_GBufferB.GetRTV(), g_GBufferC.GetRTV(), MotionBlur::g_VelocityBuffer.GetRTV(),
		};
		GfxContext.SetRenderTargets(5, RTVs, g_SceneDepthZ.GetDSV());

		if (Clear)
		{
			GfxContext.ClearColor(SceneBuffer);
			GfxContext.ClearColor(g_GBufferA);
			GfxContext.ClearColor(g_GBufferB);
			GfxContext.ClearColor(g_GBufferC);
			GfxContext.ClearColor(MotionBlur::g_VelocityBuffer);
			GfxContext.ClearDepth(DepthBuffer);
		}

		__declspec(align(16)) struct
		{
			FMatrix ModelMatrix;
			FMatrix ViewProjMatrix;
			FMatrix PreviousModelMatrix;
			FMatrix PreviousViewProjMatrix;
			Vector2f ViewportSize;
		} BasePass_VSConstants;

		BasePass_VSConstants.ModelMatrix = m_Mesh->GetModelMatrix();
		BasePass_VSConstants.ViewProjMatrix = m_Camera.GetViewMatrix() * m_Camera.GetProjectionMatrix();
		BasePass_VSConstants.PreviousModelMatrix = m_Mesh->GetPreviousModelMatrix();
		BasePass_VSConstants.PreviousViewProjMatrix = m_Camera.GetPreviousViewProjMatrix();
		BasePass_VSConstants.ViewportSize = Vector2f(m_MainViewport.Width, m_MainViewport.Height);

		GfxContext.SetDynamicConstantBufferView(0, sizeof(BasePass_VSConstants), &BasePass_VSConstants);

		__declspec(align(16)) struct
		{
			float		Exposure;
			Vector3f	CameraPos;
			Vector3f	BaseColor;
			float		Metallic;
			float		Roughness;
			int			MaxMipLevel;
			int			bSHDiffuse;
			int			Degree;
			FMatrix		InvViewProj;
			Vector4f	TemporalAAJitter;
			Vector4f	Coeffs[16];
		} BasePass_PSConstants;

		BasePass_PSConstants.CameraPos = m_Camera.GetPosition();

		Vector4f TemporalAAJitter;
		TemporalEffects::GetJitterOffset(TemporalAAJitter, m_MainViewport.Width, m_MainViewport.Height);
		BasePass_PSConstants.TemporalAAJitter = TemporalAAJitter;

		GfxContext.SetDynamicConstantBufferView(1, sizeof(BasePass_PSConstants), &BasePass_PSConstants);

		GfxContext.SetStencilRef(0x1);
		m_Mesh->Draw(GfxContext);
	}

	void LightingPass(FCommandContext& GfxContext,Vector3f LightPos,float LightScale,Vector3f LightColor,bool isDirectionalLight)
	{
		UserMarker GPUMaker(GfxContext, "Lighting Pass");

		// Set necessary state.
		GfxContext.SetRootSignature(m_PBRLightingSignature);
		GfxContext.SetPipelineState(m_PBRLightingPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

		// Indicate that the back buffer will be used as a render target.
		GfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(g_GBufferA, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(g_GBufferB, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(g_GBufferC, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(g_SceneDepthZ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
		
		GfxContext.SetRenderTargets(1, &g_SceneColorBuffer.GetRTV(), g_SceneDepthZ.GetDSV());

		__declspec(align(16)) struct
		{
		} VSConstants;

		GfxContext.SetDynamicConstantBufferView(0, sizeof(VSConstants), &VSConstants);

		__declspec(align(16)) struct
		{
			FMatrix		InvViewProj;
			Vector3f	CameraPos;
			float		pad;
			Vector3f	LightPos;
			float		LightScale;
			Vector3f	LightColor;
			float		IsDirectionalLight;
		} LightingPass_PSConstants;

		LightingPass_PSConstants.InvViewProj = m_Camera.GetViewProjMatrix().Inverse();
		LightingPass_PSConstants.CameraPos = m_Camera.GetPosition();
		LightingPass_PSConstants.LightPos = LightPos;
		LightingPass_PSConstants.LightScale = LightScale;
		LightingPass_PSConstants.LightColor = LightColor;
		LightingPass_PSConstants.IsDirectionalLight = isDirectionalLight;

		GfxContext.SetDynamicConstantBufferView(1, sizeof(LightingPass_PSConstants), &LightingPass_PSConstants);
		GfxContext.SetDynamicDescriptor(2, 0, g_GBufferA.GetSRV());
		GfxContext.SetDynamicDescriptor(2, 1, g_GBufferB.GetSRV());
		GfxContext.SetDynamicDescriptor(2, 2, g_GBufferC.GetSRV());
		GfxContext.SetDynamicDescriptor(2, 3, g_SceneDepthZ.GetSRV());

		GfxContext.Draw(3);
	}

private:
	std::wstring m_HDRFilePath;
	std::wstring m_HDRFileName;
	std::wstring m_IrradianceMapPath;
	std::wstring m_PrefilteredMapPath;
	std::wstring m_SHCoeffsPath;
	std::wstring m_PreIntegrateBRDFPath;

private:
	ComPtr<ID3DBlob>			m_MeshVS;
	ComPtr<ID3DBlob>			m_MeshPS;
	FRootSignature				m_MeshSignature;
	FGraphicsPipelineState		m_MeshPSO;

	// Lighting Pass
	ComPtr<ID3DBlob>			m_ScreenQuadVS;
	ComPtr<ID3DBlob>			m_PBRLightingPS;
	FRootSignature				m_PBRLightingSignature;
	FGraphicsPipelineState		m_PBRLightingPSO;

	// LightInfo
	float						m_LightScale;
	Vector3f					m_LightColor;
	Vector3f					m_LightDir = Vector3f(1, 0, 1);

	int							m_DebugFlag = 0;

	int							m_ShowMode = 0;

	// 
	D3D12_VIEWPORT		m_MainViewport;
	D3D12_RECT			m_MainScissor;
	FCamera m_Camera;

	std::vector<FDirectionalLight>	m_DirectionalLights;
	const int						m_NumDirectionalLights = 3;

	std::unique_ptr<FModel> m_Mesh;
	float m_RotateY = MATH_PI / 2;
	bool m_RotateMesh = false;

	std::chrono::high_resolution_clock::time_point tStart, tEnd;
};

int main()
{
	ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	GameDesc Desc;
	Desc.Caption = L"Ambient Occlusion";
	TutorialAO tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	CoUninitialize();
	return 0;
}