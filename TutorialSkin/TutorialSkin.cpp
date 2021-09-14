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
#include "ScreenSpaceSubsurface.h"
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

enum EShowMode
{
	SM_ScreenSpaceSurfaceScater,
	SM_PreIntegratedSkin,
};

class TutorialSkin : public FGame
{
public:
	TutorialSkin(const GameDesc& Desc) : FGame(Desc)
	{
	}

	void OnStartup()
	{
		SetupMesh();
		SetupShaders();
		SetupPipelineState();
		SetupCameraLight();

		PreintegratedSkinLut();

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

		m_Mesh->Update();
		if (m_RotateMesh)
		{
			m_RotateY += delta * 0.0005f * 2;
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
			ImGui::RadioButton("ScreenSpaceSurfaceScater", &m_ShowMode, SM_ScreenSpaceSurfaceScater);
			ImGui::RadioButton("PreIntegratedSkin", &m_ShowMode, SM_PreIntegratedSkin);
			ImGui::EndGroup();

			if (m_ShowMode == SM_ScreenSpaceSurfaceScater)
			{
				ImGui::SliderInt("DebugFlag,1:OnlyDiffuse,2:OnlySpecular", &ScreenSpaceSubsurface::g_DebugFlag, 0, 2);
				ImGui::SliderFloat("sss Width", &ScreenSpaceSubsurface::g_sssWidth, 1.f, 80.f);
				ImGui::SliderFloat("sss Strength", &ScreenSpaceSubsurface::g_sssStr, 0.f, 3.f);
				ImGui::SliderFloat("SpecularSmooth", &m_SpecularSmooth, 0.001f, 1.f);
				ImGui::SliderFloat("SpecularScale", &m_SpecularScale, 0.001f, 1.f);
			}
			else if(m_ShowMode == SM_PreIntegratedSkin)
			{
				ImGui::SliderInt("DebugFlag,1:OnlyDiffuse,2:OnlySpecular", &m_DebugFlag, 0, 2);
				ImGui::SliderFloat("CurveFactor", &m_CurveFactor, 0.001f, 1.f);
				ImGui::Checkbox("UseBlurNoaml", &m_UseBlurNoaml);
				if (m_UseBlurNoaml)
				{
					ImGui::SliderFloat3("TuneNormalBlur", &m_TuneNormalBlur.x, 0.f, 1.f);
				}
				ImGui::SliderFloat("SpecularSmooth", &m_SpecularSmooth, 0.001f, 1.f);
				ImGui::SliderFloat("SpecularScale", &m_SpecularScale, 0.001f, 1.f);
			}
			
			ImGui::SliderFloat("m_LightDir.x", &m_LightDir.x, -1, 1);
			m_LightDir = m_LightDir.Normalize();
		}
		ImGui::End();

		ImguiManager::Get().Render(CommandContext, RenderWindow::Get());
	}

	void OnRender()
	{
		FCommandContext& CommandContext = FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE_DIRECT, L"3D Queue");

		if (m_ShowMode == SM_ScreenSpaceSurfaceScater)
		{
			BasePass(CommandContext, true);
			SkinLightingPass(CommandContext);
			ScreenSpaceSubsurface::Render(CommandContext, m_Camera);
		}
		else if (m_ShowMode == SM_PreIntegratedSkin)
		{
			PrePreintegratedSkinRendering(CommandContext, true);
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
		m_Mesh = std::make_unique<FModel>("../Resources/Models/HumanHead/HumanHead.obj", true, false);
		m_Mesh->SetRotation(FMatrix::RotateY(m_RotateY));

		m_PreintegratedSkinLut.Create(L"PreintegratedSkinLut", PREFILTERED_SIZE, PREFILTERED_SIZE, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);

		m_BlurNormalMap.LoadFromFile(L"../Resources/Models/HumanHead/textures/Head_NM_blur.tga", false);
		m_SpecularBRDF.LoadFromFile(L"../Resources/Models/HumanHead/textures/zirmayKalosSpecularBRDF.png", false);
	}

	void SetupShaders()
	{
		// Mesh
		m_MeshVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PBR.hlsl", "VS_PBR", "vs_5_1");
		m_MeshPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PBR.hlsl", "PS_PBR", "ps_5_1");

		// Lighting
		m_ScreenQuadVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PostProcess.hlsl", "VS_ScreenQuad", "vs_5_1");
		m_SkinLightingPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/SkinPBR.hlsl", "PS_SkinLighting", "ps_5_1");

		// PreintegratedSkin
		m_PreintegratedSkinLutPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PreIntegratedSkinLut.hlsl", "PS_PreIntegratedSkinLut", "ps_5_1");

		m_PreintegratedSkinShadingVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PreIntegratedSkinShading.hlsl", "VS_PreIntegratedSkin", "vs_5_1");
		m_PreintegratedSkinShadingPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PreIntegratedSkinShading.hlsl", "PS_PreIntegratedSkin", "ps_5_1");
	}

	void SetupPipelineState()
	{
		FSamplerDesc DefaultSamplerDesc(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
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
		m_SkinLightingSignature.Reset(3, 1);
		m_SkinLightingSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
		m_SkinLightingSignature[1].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
		m_SkinLightingSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
		m_SkinLightingSignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_SkinLightingSignature.Finalize(L"SkinLighting RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_SkinLightingPSO.SetRootSignature(m_SkinLightingSignature);
		m_SkinLightingPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);

		D3D12_BLEND_DESC BlendAdd = FPipelineState::BlendTraditional;
		BlendAdd.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		BlendAdd.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		BlendAdd.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		BlendAdd.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		m_SkinLightingPSO.SetBlendState(BlendAdd);
		m_SkinLightingPSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
		// no need to set input layout
		m_SkinLightingPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);

		DXGI_FORMAT SkinLight_RTFormats[] = {
			ScreenSpaceSubsurface::g_DiffuseTerm.GetFormat(),
			ScreenSpaceSubsurface::g_SpecularTerm.GetFormat(),
		};

		m_SkinLightingPSO.SetRenderTargetFormats(2, SkinLight_RTFormats, g_SceneDepthZ.GetFormat());

		m_SkinLightingPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_ScreenQuadVS.Get()));
		m_SkinLightingPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_SkinLightingPS.Get()));
		m_SkinLightingPSO.Finalize();
		
		// PreintegratedSkinLut
		m_PreintegratedSkinLutSignature.Reset(0, 0);
		m_PreintegratedSkinLutSignature.Finalize(L"PreintegratedSkinLut RootSignature");

		m_PreintegratedSkinLutPSO.SetRootSignature(m_PreintegratedSkinLutSignature);
		m_PreintegratedSkinLutPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_PreintegratedSkinLutPSO.SetBlendState(FPipelineState::BlendDisable);
		m_PreintegratedSkinLutPSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
		// no need to set input layout
		m_PreintegratedSkinLutPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_PreintegratedSkinLutPSO.SetRenderTargetFormat(DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_UNKNOWN);
		m_PreintegratedSkinLutPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_ScreenQuadVS.Get()));
		m_PreintegratedSkinLutPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_PreintegratedSkinLutPS.Get()));
		m_PreintegratedSkinLutPSO.Finalize();

		// PreintegratedSkinShading
		m_PreintegratedSkinShadingSignature.Reset(3, 1);
		m_PreintegratedSkinShadingSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
		m_PreintegratedSkinShadingSignature[1].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
		m_PreintegratedSkinShadingSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
		m_PreintegratedSkinShadingSignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_PreintegratedSkinShadingSignature.Finalize(L"PreintegratedSkinShading RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_PreintegratedSkinShadingPSO.SetRootSignature(m_PreintegratedSkinShadingSignature);
		m_PreintegratedSkinShadingPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_PreintegratedSkinShadingPSO.SetBlendState(FPipelineState::BlendDisable);
		m_PreintegratedSkinShadingPSO.SetDepthStencilState(DSS);
		m_PreintegratedSkinShadingPSO.SetInputLayout((UINT)MeshLayout.size(), &MeshLayout[0]);
		m_PreintegratedSkinShadingPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_PreintegratedSkinShadingPSO.SetRenderTargetFormats(1, &g_SceneColorBuffer.GetFormat(), g_SceneDepthZ.GetFormat());

		m_PreintegratedSkinShadingPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_PreintegratedSkinShadingVS.Get()));
		m_PreintegratedSkinShadingPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_PreintegratedSkinShadingPS.Get()));
		m_PreintegratedSkinShadingPSO.Finalize();
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

	void SkinLightingPass(FCommandContext& GfxContext)
	{
		UserMarker GPUMaker(GfxContext, "Skin Lighting Pass");

		// Set necessary state.
		GfxContext.SetRootSignature(m_SkinLightingSignature);
		GfxContext.SetPipelineState(m_SkinLightingPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

		// Indicate that the back buffer will be used as a render target.
		GfxContext.TransitionResource(ScreenSpaceSubsurface::g_DiffuseTerm, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(ScreenSpaceSubsurface::g_SpecularTerm, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(g_GBufferA, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(g_GBufferB, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(g_GBufferC, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(m_SpecularBRDF, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(g_SceneDepthZ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);

		D3D12_CPU_DESCRIPTOR_HANDLE RTVs[] = {
			ScreenSpaceSubsurface::g_DiffuseTerm.GetRTV(), ScreenSpaceSubsurface::g_SpecularTerm.GetRTV(), 
		};
		GfxContext.SetRenderTargets(2, RTVs, g_SceneDepthZ.GetDSV());

		GfxContext.ClearColor(ScreenSpaceSubsurface::g_DiffuseTerm);
		GfxContext.ClearColor(ScreenSpaceSubsurface::g_SpecularTerm);

		__declspec(align(16)) struct
		{
		} SkinLightingPass_VSConstants;

		GfxContext.SetDynamicConstantBufferView(0, sizeof(SkinLightingPass_VSConstants), &SkinLightingPass_VSConstants);

		__declspec(align(16)) struct
		{
			FMatrix		InvViewProj;
			Vector3f	CameraPos;
			float		pad;
			Vector3f	LightDir;
			float		LightScale;
			Vector3f	LightColor;
			int			DebugFlag;
			float		Smooth;
			float		SpecularScale;
			Vector2f	pad2;
			Vector3f	SubsurfaceColor;
		} SkinLightingPass_PSConstants;

		SkinLightingPass_PSConstants.InvViewProj = m_Camera.GetViewProjMatrix().Inverse();
		SkinLightingPass_PSConstants.CameraPos = m_Camera.GetPosition();
		SkinLightingPass_PSConstants.LightDir = m_LightDir;
		SkinLightingPass_PSConstants.LightScale = m_LightScale;
		SkinLightingPass_PSConstants.LightColor = m_LightColor;
		SkinLightingPass_PSConstants.DebugFlag = m_DebugFlag;
		SkinLightingPass_PSConstants.Smooth = m_SpecularSmooth;
		SkinLightingPass_PSConstants.SpecularScale = m_SpecularScale;
		SkinLightingPass_PSConstants.SubsurfaceColor = Vector3f(0.655000f, 0.559480f, 0.382083f);

		GfxContext.SetDynamicConstantBufferView(1, sizeof(SkinLightingPass_PSConstants), &SkinLightingPass_PSConstants);
		GfxContext.SetDynamicDescriptor(2, 0, g_GBufferA.GetSRV());
		GfxContext.SetDynamicDescriptor(2, 1, g_GBufferB.GetSRV());
		GfxContext.SetDynamicDescriptor(2, 2, g_GBufferC.GetSRV());
		GfxContext.SetDynamicDescriptor(2, 3, g_SceneDepthZ.GetSRV());
		GfxContext.SetDynamicDescriptor(2, 4, m_SpecularBRDF.GetSRV());

		GfxContext.Draw(3);
	}

	void PreintegratedSkinLut()
	{
		FCommandContext& GfxContext = FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE_DIRECT, L"3D Queue");
		
		UserMarker GPUMaker(GfxContext, "Pre-Preintegrated Skin Lut");

		GfxContext.SetRootSignature(m_PreintegratedSkinLutSignature);
		GfxContext.SetPipelineState(m_PreintegratedSkinLutPSO);
		GfxContext.SetViewportAndScissor(0, 0, PREFILTERED_SIZE, PREFILTERED_SIZE); // very important
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GfxContext.TransitionResource(m_PreintegratedSkinLut, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		GfxContext.SetRenderTargets(1, &m_PreintegratedSkinLut.GetRTV());

		GfxContext.ClearColor(m_PreintegratedSkinLut);

		GfxContext.Draw(3);
		GfxContext.Flush(true);

		m_PreintegratedSkinLut.SaveColorBuffer(L"../Resources/HDR/PreintegratedSkinLut.dds");
	}
	
	void PrePreintegratedSkinRendering(FCommandContext& GfxContext,bool Clear)
	{
		UserMarker GPUMaker(GfxContext, "Pre-Preintegrated Skin Shading");
		
		// Set necessary state.
		GfxContext.SetRootSignature(m_PreintegratedSkinShadingSignature);
		GfxContext.SetPipelineState(m_PreintegratedSkinShadingPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

		// Indicate that the back buffer will be used as a render target.
		GfxContext.TransitionResource(m_SpecularBRDF, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(m_BlurNormalMap, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(m_PreintegratedSkinLut, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(g_SceneColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(g_SceneDepthZ, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

		GfxContext.SetRenderTargets(1, &g_SceneColorBuffer.GetRTV(), g_SceneDepthZ.GetDSV());

		if (Clear)
		{
			GfxContext.ClearColor(g_SceneColorBuffer);
			GfxContext.ClearDepth(g_SceneDepthZ);
		}

		__declspec(align(16)) struct
		{
			FMatrix ModelMatrix;
			FMatrix ViewProjMatrix;
		} VSConstants;

		VSConstants.ModelMatrix = m_Mesh->GetModelMatrix();
		VSConstants.ViewProjMatrix = m_Camera.GetViewMatrix() * m_Camera.GetProjectionMatrix();

		GfxContext.SetDynamicConstantBufferView(0, sizeof(VSConstants), &VSConstants);

		__declspec(align(16)) struct
		{
			Vector3f	CameraPos;
			float		CurveFactor;
			Vector3f	LightDir;
			int			DebugFlag;
			Vector3f	SubsurfaceColor;
			float		UseBlurNoaml;
			Vector3f	TuneNormalBlur;
			float		Smooth;
			float		SpecularScale;
		} PSConstants;

		PSConstants.CameraPos = m_Camera.GetPosition();
		PSConstants.CurveFactor = m_CurveFactor;
		PSConstants.LightDir = m_LightDir;
		PSConstants.DebugFlag = m_DebugFlag;
		PSConstants.SubsurfaceColor = Vector3f(0.655000f, 0.559480f, 0.382083f);
		PSConstants.UseBlurNoaml = m_UseBlurNoaml;
		PSConstants.TuneNormalBlur = m_TuneNormalBlur;
		PSConstants.Smooth = m_SpecularSmooth;
		PSConstants.SpecularScale = m_SpecularScale;

		GfxContext.SetDynamicConstantBufferView(1, sizeof(PSConstants), &PSConstants);

		GfxContext.SetDynamicDescriptor(2, 7, m_PreintegratedSkinLut.GetSRV());
		GfxContext.SetDynamicDescriptor(2, 8, m_BlurNormalMap.GetSRV());
		GfxContext.SetDynamicDescriptor(2, 9, m_SpecularBRDF.GetSRV());

		GfxContext.SetStencilRef(0x1);
		m_Mesh->Draw(GfxContext);
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
	ComPtr<ID3DBlob>			m_SkinLightingPS;
	FRootSignature				m_SkinLightingSignature;
	FGraphicsPipelineState		m_SkinLightingPSO;

	// PreintegratedSkin
	FColorBuffer				m_PreintegratedSkinLut;
	ComPtr<ID3DBlob>			m_PreintegratedSkinLutPS;
	FRootSignature				m_PreintegratedSkinLutSignature;
	FGraphicsPipelineState		m_PreintegratedSkinLutPSO;
	
	FTexture					m_BlurNormalMap;
	FTexture					m_SpecularBRDF;

	ComPtr<ID3DBlob>			m_PreintegratedSkinShadingVS;
	ComPtr<ID3DBlob>			m_PreintegratedSkinShadingPS;
	FRootSignature				m_PreintegratedSkinShadingSignature;
	FGraphicsPipelineState		m_PreintegratedSkinShadingPSO;

	// LightInfo
	float						m_LightScale;
	Vector3f					m_LightColor;
	Vector3f					m_LightDir = Vector3f(1, 0, 1);

	/*
	* DebugFlag
	* 0: None
	* 1: OnlyDiffuse
	* 2: OnlySpecular
	*/
	int							m_DebugFlag = 0;

	//
	float						m_CurveFactor = 1.f;
	bool						m_UseBlurNoaml = true;
	Vector3f					m_TuneNormalBlur = Vector3f(0.9f, 0.8f, 0.7f);

	float						m_SpecularSmooth = 0.65f;
	float						m_SpecularScale = 0.30f;

	// ShowMode
	int							m_ShowMode = SM_ScreenSpaceSurfaceScater;

	// 
	D3D12_VIEWPORT		m_MainViewport;
	D3D12_RECT			m_MainScissor;
	FCamera m_Camera;
	FDirectionalLight m_DirectionLight;
	std::unique_ptr<FModel> m_Mesh;
	float m_RotateY = MATH_PI;
	bool m_RotateMesh = false;

	std::chrono::high_resolution_clock::time_point tStart, tEnd;
};

int main()
{
	ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	GameDesc Desc;
	Desc.Caption = L"Skin Rendering";
	TutorialSkin tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	CoUninitialize();
	return 0;
}