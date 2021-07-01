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
#include "SkyBox.h"
#include "CubeMapCross.h"
#include "GameInput.h"
#include "ImguiManager.h"
#include "GenerateMips.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <chrono>
#include <iostream>


extern FCommandListManager g_CommandListManager;

const int CUBE_MAP_SIZE = 1024;
const int IRRADIANCE_SIZE = 256;
const bool CUBEMAP_DEBUG_VIEW = true;

enum EShowMode
{
	SM_LongLat,
	SM_SkyBox,
	SM_CubeMapCross,
	SM_Irradiance,
	SM_Prefiltered,
	SM_SphericalHarmonics,
};

class Tutorial9 : public FGame
{
public:
	Tutorial9(const GameDesc& Desc) : FGame(Desc)
	{
	}

	void OnStartup()
	{
		SetupCameraLight();
		SetupMesh();
		SetupShaders();
		SetupPipelineState();

		GenerateCubeMap();
		GenerateIrradianceMap();

		//SaveCubeMap();
		GenerateSHcoeffs();
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

		if (GameInput::IsKeyDown('F'))
			SetupCameraLight();
	}

	void OnGUI(FCommandContext& CommandContext)
	{
		ImguiManager::Get().NewFrame();

		ImGui::SetNextWindowPos(ImVec2(1, 1));
		
		static bool ShowConfig = true;
		if (ImGui::Begin("Config", &ShowConfig, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::ColorEdit3("Clear Color", &m_ClearColor.x);
			ImGui::SliderFloat("Exposure", &m_Exposure, 0.f, 10.f, "%.1f");

			ImGui::BeginGroup();
			ImGui::Text("Show Mode");
			ImGui::Indent(20);
			ImGui::RadioButton("Long-Lat View", &m_ShowMode, SM_LongLat);
			ImGui::RadioButton("Cube Box", &m_ShowMode, SM_SkyBox);
			ImGui::RadioButton("Cube Cross", &m_ShowMode, SM_CubeMapCross);
			ImGui::RadioButton("Irradiance", &m_ShowMode, SM_Irradiance);
			ImGui::RadioButton("Prefiltered", &m_ShowMode, SM_Prefiltered);
			ImGui::RadioButton("SphericalHarmonics", &m_ShowMode, SM_SphericalHarmonics);
			ImGui::EndGroup();

			if (m_ShowMode == SM_CubeMapCross)
			{
				ImGui::SliderInt("Mip Level", &m_MipLevel, 0, m_CubeBuffer.GetNumMips()-1);
			}

			if (m_ShowMode == SM_SphericalHarmonics)
			{
				ImGui::SliderInt("SH Degree", &m_SHDegree, 1, 4);
			}

			//static bool ShowDemo = false;
			//ImGui::Checkbox("Show Demo", &ShowDemo);
			//if (ShowDemo)
			//	ImGui::ShowDemoWindow(&ShowDemo);

			ImGui::End();
		}

		ImguiManager::Get().Render(CommandContext, RenderWindow::Get());
	}

	void OnRender()
	{
		FCommandContext& CommandContext = FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE_DIRECT, L"3D Queue");

		switch (m_ShowMode)
		{
		case SM_LongLat:
			ShowTexture2D(CommandContext, m_TextureLongLat);
			break;
		case SM_SkyBox:
			SkyPass(CommandContext);
			break;
		case SM_CubeMapCross:
			ShowCubeMapDebugView(CommandContext, m_CubeBuffer);
			break;
		case SM_Irradiance:
		case SM_Prefiltered:
			ShowCubeMapDebugView(CommandContext, m_IrradianceCube);
			break;
		case SM_SphericalHarmonics:
			ShowSHCubeMapDebugView(CommandContext, m_CubeBuffer);
			break;
		}

		OnGUI(CommandContext);

		CommandContext.TransitionResource(RenderWindow::Get().GetBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);
		CommandContext.Finish(true);

		RenderWindow::Get().Present();
	}


private:
	void SetupCameraLight()
	{
		m_Camera = FCamera(Vector3f(0.f, 0.f, -1.f), Vector3f(0.f, 0.f, 0.f), Vector3f(0.f, 1.f, 0.f));
		m_Camera.SetMouseMoveSpeed(1e-4f);
		m_Camera.SetMouseRotateSpeed(1e-4f);

		const float FovVertical = MATH_PI / 4.f;
		m_Camera.SetPerspectiveParams(FovVertical, (float)GetDesc().Width / GetDesc().Height, 0.1f, 100.f);
	}

	void SetupMesh()
	{
		m_TextureLongLat.LoadFromFile(L"../Resources/HDR/spruit_sunrise_2k.hdr"); //spruit_sunrise_2k.hdr, newport_loft.hdr
		m_SkyBox = new FSkyBox();
		m_CubeMapCross = new FCubeMapCross();
	}

	void SetupShaders()
	{
		m_LongLatToCubeVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "VS_LongLatToCube", "vs_5_1");
		m_LongLatToCubePS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_LongLatToCube", "ps_5_1");
		m_SkyVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "VS_SkyCube", "vs_5_1");
		m_SkyPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_SkyCube", "ps_5_1");
		m_GenIrradiancePS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_SkyCube", "ps_5_1");
		m_CubeMapCrossVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "VS_CubeMapCross", "vs_5_1");
		m_CubeMapCrossPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_CubeMapCross", "ps_5_1");
		m_ShowTexture2DVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "VS_ShowTexture2D", "vs_5_1");
		m_ShowTexture2DPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_ShowTexture2D", "ps_5_1");
		m_GenIrradiancePS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_GenIrradiance", "ps_5_1");

		m_SphericalHarmonicsPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_SphericalHarmonics", "ps_5_1");

	}

	void SetupPipelineState()
	{
		FSamplerDesc DefaultSamplerDesc(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		m_GenCubeSignature.Reset(2, 1);
		m_GenCubeSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
		m_GenCubeSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		m_GenCubeSignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_GenCubeSignature.Finalize(L"Generate Cubemap RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_GenCubePSO.SetRootSignature(m_GenCubeSignature);
		m_GenCubePSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_GenCubePSO.SetBlendState(FPipelineState::BlendDisable);
		m_GenCubePSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
		
		std::vector<D3D12_INPUT_ELEMENT_DESC> MeshLayout;
		m_SkyBox->GetMeshLayout(MeshLayout);
		Assert(MeshLayout.size() > 0);
		m_GenCubePSO.SetInputLayout((UINT)MeshLayout.size(), &MeshLayout[0]);

		m_GenCubePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		DXGI_FORMAT CubeFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
		m_GenCubePSO.SetRenderTargetFormats(1, &CubeFormat, DXGI_FORMAT_UNKNOWN);
		m_GenCubePSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_LongLatToCubeVS.Get()));
		m_GenCubePSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_LongLatToCubePS.Get()));
		m_GenCubePSO.Finalize();

		m_CubeBuffer.Create(L"CubeMap", CUBE_MAP_SIZE, CUBE_MAP_SIZE, 0/*full mipmap chain*/, DXGI_FORMAT_R16G16B16A16_FLOAT);

		m_SkySignature.Reset(3, 1);
		m_SkySignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
		m_SkySignature[1].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
		m_SkySignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		m_SkySignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_SkySignature.Finalize(L"Sky RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_SkyPSO.SetRootSignature(m_SkySignature);
		m_SkyPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_SkyPSO.SetBlendState(FPipelineState::BlendDisable);
		m_SkyPSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
		m_SkyPSO.SetInputLayout((UINT)MeshLayout.size(), &MeshLayout[0]);
		m_SkyPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_SkyPSO.SetRenderTargetFormats(1, &RenderWindow::Get().GetColorFormat(), RenderWindow::Get().GetDepthFormat());
		m_SkyPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_SkyVS.Get()));
		m_SkyPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_SkyPS.Get()));
		m_SkyPSO.Finalize();

		m_GenIrradiancePSO.SetRootSignature(m_SkySignature);
		m_GenIrradiancePSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_GenIrradiancePSO.SetBlendState(FPipelineState::BlendDisable);
		m_GenIrradiancePSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
		m_GenIrradiancePSO.SetInputLayout((UINT)MeshLayout.size(), &MeshLayout[0]);
		m_GenIrradiancePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_GenIrradiancePSO.SetRenderTargetFormats(1, &CubeFormat, DXGI_FORMAT_UNKNOWN);
		m_GenIrradiancePSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_SkyVS.Get()));
		m_GenIrradiancePSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_GenIrradiancePS.Get()));
		m_GenIrradiancePSO.Finalize();

		m_IrradianceCube.Create(L"Irradiance Map", IRRADIANCE_SIZE, IRRADIANCE_SIZE, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);

		FSamplerDesc PointSamplerDesc(D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
		m_Show2DTextureSignature.Reset(3, 1);
		m_Show2DTextureSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
		m_Show2DTextureSignature[1].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
		m_Show2DTextureSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		m_Show2DTextureSignature.InitStaticSampler(0, PointSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_Show2DTextureSignature.Finalize(L"Show 2D Texture View RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_Show2DTexturePSO.SetRootSignature(m_Show2DTextureSignature);
		m_Show2DTexturePSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_Show2DTexturePSO.SetBlendState(FPipelineState::BlendDisable);
		m_Show2DTexturePSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
		// no need to set mesh layout
		m_Show2DTexturePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_Show2DTexturePSO.SetRenderTargetFormats(1, &RenderWindow::Get().GetColorFormat(), RenderWindow::Get().GetDepthFormat());
		m_Show2DTexturePSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_ShowTexture2DVS.Get()));
		m_Show2DTexturePSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_ShowTexture2DPS.Get()));
		m_Show2DTexturePSO.Finalize();

		m_CubeMapCrossViewSignature.Reset(3, 1);
		m_CubeMapCrossViewSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
		m_CubeMapCrossViewSignature[1].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
		m_CubeMapCrossViewSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
		m_CubeMapCrossViewSignature.InitStaticSampler(0, PointSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_CubeMapCrossViewSignature.Finalize(L"Cube Map Cross View RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_CubeMapCrossViewPSO.SetRootSignature(m_CubeMapCrossViewSignature);
		m_CubeMapCrossViewPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_CubeMapCrossViewPSO.SetBlendState(FPipelineState::BlendDisable);
		m_CubeMapCrossViewPSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);

		std::vector<D3D12_INPUT_ELEMENT_DESC> DebugMeshLayout;
		m_CubeMapCross->GetMeshLayout(DebugMeshLayout);
		Assert(DebugMeshLayout.size() > 0);
		m_CubeMapCrossViewPSO.SetInputLayout((UINT)DebugMeshLayout.size(), &DebugMeshLayout[0]);

		m_CubeMapCrossViewPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_CubeMapCrossViewPSO.SetRenderTargetFormats(1, &RenderWindow::Get().GetColorFormat(), RenderWindow::Get().GetDepthFormat());
		m_CubeMapCrossViewPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_CubeMapCrossVS.Get()));
		m_CubeMapCrossViewPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_CubeMapCrossPS.Get()));
		m_CubeMapCrossViewPSO.Finalize();

		// SH PSO
		m_SphericalHarmonicsCrossViewPSO.SetRootSignature(m_CubeMapCrossViewSignature);
		m_SphericalHarmonicsCrossViewPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_SphericalHarmonicsCrossViewPSO.SetBlendState(FPipelineState::BlendDisable);
		m_SphericalHarmonicsCrossViewPSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
		m_SphericalHarmonicsCrossViewPSO.SetInputLayout((UINT)DebugMeshLayout.size(), &DebugMeshLayout[0]);

		m_SphericalHarmonicsCrossViewPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_SphericalHarmonicsCrossViewPSO.SetRenderTargetFormats(1, &RenderWindow::Get().GetColorFormat(), RenderWindow::Get().GetDepthFormat());
		m_SphericalHarmonicsCrossViewPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_CubeMapCrossVS.Get()));
		m_SphericalHarmonicsCrossViewPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_SphericalHarmonicsPS.Get()));
		m_SphericalHarmonicsCrossViewPSO.Finalize();


	}

	void GenerateCubeMap()
	{
		FCommandContext& GfxContext = FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE_DIRECT, L"3D Queue");

		GfxContext.SetRootSignature(m_GenCubeSignature);
		GfxContext.SetPipelineState(m_GenCubePSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GfxContext.SetViewportAndScissor(0, 0, CUBE_MAP_SIZE, CUBE_MAP_SIZE); // very important, cubemap size

		GfxContext.TransitionResource(m_CubeBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
		
		GfxContext.SetDynamicDescriptor(1, 0, m_TextureLongLat.GetSRV());

		m_VSConstants.ModelMatrix = FMatrix(); // identity
		for (int i = 0; i < 6; ++i)
		{
			GfxContext.SetRenderTargets(1, &m_CubeBuffer.GetRTV(i, 0));
			GfxContext.ClearColor(m_CubeBuffer, i, 0);

			m_VSConstants.ViewProjMatrix = m_CubeBuffer.GetViewProjMatrix(i);
			GfxContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstants), &m_VSConstants);
			
			m_SkyBox->Draw(GfxContext);
		}
		GfxContext.Flush(true);
		FGenerateMips::Generate(m_CubeBuffer, GfxContext);
		GfxContext.Finish(true);
	}

	void GenerateIrradianceMap()
	{
		FCommandContext& GfxContext = FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE_DIRECT, L"3D Queue");

		GfxContext.SetRootSignature(m_SkySignature);
		GfxContext.SetPipelineState(m_GenIrradiancePSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GfxContext.SetViewportAndScissor(0, 0, IRRADIANCE_SIZE, IRRADIANCE_SIZE); // very important, cubemap size

		GfxContext.TransitionResource(m_CubeBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(m_IrradianceCube, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		GfxContext.SetDynamicDescriptor(2, 0, m_CubeBuffer.GetCubeSRV());

		m_PSConstants.NumSamplesPerDir = m_NumSamplesPerDir;
		GfxContext.SetDynamicConstantBufferView(1, sizeof(m_PSConstants), &m_PSConstants);

		m_VSConstants.ModelMatrix = FMatrix(); // identity
		for (int i = 0; i < 6; ++i)
		{
			GfxContext.SetRenderTargets(1, &m_IrradianceCube.GetRTV(i, 0));
			GfxContext.ClearColor(m_IrradianceCube, i, 0);

			m_VSConstants.ViewProjMatrix = m_IrradianceCube.GetViewProjMatrix(i);
			GfxContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstants), &m_VSConstants);

			m_SkyBox->Draw(GfxContext);
		}
		GfxContext.Finish(true);
	}

	void SaveCubeMap()
	{
		m_CubeBuffer.SaveCubeMap(L"spruit_sunrise_2k_cubemap.dds");
	}

	void GenerateSHcoeffs()
	{
		m_SHCoeffs = m_CubeBuffer.GenerateSHcoeffs(4, m_SHSampleNum);

		for (int i=0;i<m_SHCoeffs.size();++i)
		{
			printf("[%f,%f,%f]\n", m_SHCoeffs[i].x, m_SHCoeffs[i].y, m_SHCoeffs[i].z);
		}
	}

	void SkyPass(FCommandContext& GfxContext)
	{
		// Set necessary state.
		GfxContext.SetRootSignature(m_SkySignature);
		GfxContext.SetPipelineState(m_SkyPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GfxContext.SetViewportAndScissor(0, 0, m_GameDesc.Width, m_GameDesc.Height);

		RenderWindow& renderWindow = RenderWindow::Get();
		FColorBuffer& BackBuffer = renderWindow.GetBackBuffer();
		FDepthBuffer& DepthBuffer = renderWindow.GetDepthBuffer();

		// Indicate that the back buffer will be used as a render target.
		GfxContext.TransitionResource(m_CubeBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(DepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

		GfxContext.SetRenderTargets(1, &BackBuffer.GetRTV());
		GfxContext.SetRenderTargets(1, &BackBuffer.GetRTV(), DepthBuffer.GetDSV());

		BackBuffer.SetClearColor(m_ClearColor);
		GfxContext.ClearColor(BackBuffer);
		GfxContext.ClearDepth(DepthBuffer);

		m_VSConstants.ModelMatrix = FMatrix::TranslateMatrix(m_Camera.GetPosition()); // move with camera
		m_VSConstants.ViewProjMatrix = m_Camera.GetViewMatrix() * m_Camera.GetProjectionMatrix();
		GfxContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstants), &m_VSConstants);

		m_PSConstants.Exposure = m_Exposure;
		GfxContext.SetDynamicConstantBufferView(1, sizeof(m_PSConstants), &m_PSConstants);

		GfxContext.SetDynamicDescriptor(2, 0, m_CubeBuffer.GetCubeSRV());

		m_SkyBox->Draw(GfxContext);
	}

	void ShowTexture2D(FCommandContext& GfxContext, FTexture& Texture2D)
	{
		GfxContext.SetRootSignature(m_Show2DTextureSignature);
		GfxContext.SetPipelineState(m_Show2DTexturePSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		float AspectRatio = Texture2D.GetWidth() * 1.f / Texture2D.GetHeight();
		int Width = std::min(m_GameDesc.Width, Texture2D.GetWidth());
		int Height = std::min(m_GameDesc.Height, Texture2D.GetHeight());
		Width = std::min(Width, static_cast<int>(Height * AspectRatio));
		Height = std::min(Height, static_cast<int>(Width / AspectRatio));
		GfxContext.SetViewportAndScissor((m_GameDesc.Width - Width) / 2, (m_GameDesc.Height - Height) / 2, Width, Height);

		RenderWindow& renderWindow = RenderWindow::Get();
		FColorBuffer& BackBuffer = renderWindow.GetBackBuffer();

		// Indicate that the back buffer will be used as a render target.
		GfxContext.TransitionResource(m_CubeBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		GfxContext.SetRenderTargets(1, &BackBuffer.GetRTV());

		BackBuffer.SetClearColor(m_ClearColor);
		GfxContext.ClearColor(BackBuffer);

		m_VSConstants.ModelMatrix = FMatrix(); // identity
		m_VSConstants.ViewProjMatrix = FMatrix::MatrixOrthoLH(1.f, 1.f, -1.f, 1.f);
		GfxContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstants), &m_VSConstants);

		m_PSConstants.Exposure = m_Exposure;
		GfxContext.SetDynamicConstantBufferView(1, sizeof(m_PSConstants), &m_PSConstants);

		GfxContext.SetDynamicDescriptor(2, 0, m_TextureLongLat.GetSRV());

		GfxContext.Draw(3);
	}

	void ShowCubeMapDebugView(FCommandContext& GfxContext, FCubeBuffer& CubeBuffer)
	{
		// Set necessary state.
		GfxContext.SetRootSignature(m_CubeMapCrossViewSignature);
		GfxContext.SetPipelineState(m_CubeMapCrossViewPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		uint32_t Size = std::min(m_GameDesc.Width, m_GameDesc.Height);
		Size = std::min(Size, CubeBuffer.GetWidth());
		GfxContext.SetViewportAndScissor((m_GameDesc.Width - Size)/2, (m_GameDesc.Height - Size) / 2, Size, Size);

		RenderWindow& renderWindow = RenderWindow::Get();
		FColorBuffer& BackBuffer = renderWindow.GetBackBuffer();

		// Indicate that the back buffer will be used as a render target.
		GfxContext.TransitionResource(CubeBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		GfxContext.SetRenderTargets(1, &BackBuffer.GetRTV());

		BackBuffer.SetClearColor(m_ClearColor);
		GfxContext.ClearColor(BackBuffer);

		m_VSConstants.ModelMatrix = FMatrix(); // identity
		m_VSConstants.ViewProjMatrix = FMatrix::MatrixOrthoLH(1.f, 1.f, -1.f, 1.f);
		GfxContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstants), &m_VSConstants);

		m_PSConstants.Exposure = m_Exposure;
		m_PSConstants.MipLevel = m_MipLevel;
		GfxContext.SetDynamicConstantBufferView(1, sizeof(m_PSConstants), &m_PSConstants);

		GfxContext.SetDynamicDescriptor(2, 0, CubeBuffer.GetCubeSRV());

		m_CubeMapCross->Draw(GfxContext);
	}

	void ShowSHCubeMapDebugView(FCommandContext& GfxContext, FCubeBuffer& CubeBuffer)
	{
		// Set necessary state.
		GfxContext.SetRootSignature(m_CubeMapCrossViewSignature);
		GfxContext.SetPipelineState(m_SphericalHarmonicsCrossViewPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		uint32_t Size = std::min(m_GameDesc.Width, m_GameDesc.Height);
		Size = std::min(Size, CubeBuffer.GetWidth());
		GfxContext.SetViewportAndScissor((m_GameDesc.Width - Size) / 2, (m_GameDesc.Height - Size) / 2, Size, Size);

		RenderWindow& renderWindow = RenderWindow::Get();
		FColorBuffer& BackBuffer = renderWindow.GetBackBuffer();

		// Indicate that the back buffer will be used as a render target.
		GfxContext.TransitionResource(CubeBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		GfxContext.SetRenderTargets(1, &BackBuffer.GetRTV());

		BackBuffer.SetClearColor(m_ClearColor);
		GfxContext.ClearColor(BackBuffer);

		m_VSConstants.ModelMatrix = FMatrix(); // identity
		m_VSConstants.ViewProjMatrix = FMatrix::MatrixOrthoLH(1.f, 1.f, -1.f, 1.f);
		GfxContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstants), &m_VSConstants);

		m_PSConstants.Exposure = m_Exposure;
		m_PSConstants.Degree = m_SHDegree;

#if 0
		//Test
		{
			m_PSConstants.Coeffs[0] = Vector3f(0.695716f, 0.687074f, 0.850051f);
			m_PSConstants.Coeffs[1] = Vector3f(-0.0459522f, -0.0609238f, -0.100773f);
			m_PSConstants.Coeffs[2] = Vector3f(0.500634f, 0.520243f, 0.712448f);
			m_PSConstants.Coeffs[3] = Vector3f(-0.0575933f, -0.0329037f, -0.0265196f);
			m_PSConstants.Coeffs[4] = Vector3f(-0.0694421f, -0.0585204f, -0.0672366f);
			m_PSConstants.Coeffs[5] = Vector3f(-0.041728f, -0.0538929f, -0.084576);
			m_PSConstants.Coeffs[6] = Vector3f(0.00833671f, 0.0534902f, 0.112016f);
			m_PSConstants.Coeffs[7] = Vector3f(-0.104005f, -0.0725926f, -0.0730126f);
			m_PSConstants.Coeffs[8] = Vector3f(0.0135042f, 0.00762932f, 0.0148929f);
			m_PSConstants.Coeffs[9] = Vector3f(-0.015106f, -0.00712441f, -0.00698922);
			m_PSConstants.Coeffs[10] = Vector3f(-0.0862863f, -0.0746445f, -0.0934173f);
			m_PSConstants.Coeffs[11] = Vector3f(0.0382759f, 0.0347831f, 0.0428672f);
			m_PSConstants.Coeffs[12] = Vector3f(-0.141227f, -0.125088f, -0.157915f);
			m_PSConstants.Coeffs[13] = Vector3f(-0.0163175, -0.0125922f, -0.0148044f);
			m_PSConstants.Coeffs[14] = Vector3f(0.0471778f, 0.04255f, 0.0595879f);
			m_PSConstants.Coeffs[15] = Vector3f(0.0652876f, 0.0691427f, 0.0805324f);
		}
#else
		for (int i = 0; i < m_SHCoeffs.size(); ++i)
		{
			m_PSConstants.Coeffs[i] = m_SHCoeffs[i];
		}
#endif

		GfxContext.SetDynamicConstantBufferView(1, sizeof(m_PSConstants), &m_PSConstants);

		GfxContext.SetDynamicDescriptor(2, 0, CubeBuffer.GetCubeSRV());

		m_CubeMapCross->Draw(GfxContext);
	}


private:
	__declspec(align(16)) struct
	{
		FMatrix ModelMatrix;
		FMatrix ViewProjMatrix;
	} m_VSConstants;

	__declspec(align(16)) struct
	{
		float		Exposure;
		int			MipLevel;
		int			NumSamplesPerDir;
		int			Degree;
		Vector4f	Coeffs[16];
	} m_PSConstants;

	int	m_SHDegree = 4;
	int m_SHSampleNum = 10000;
	std::vector<Vector3f> m_SHCoeffs;

	FRootSignature m_GenCubeSignature;
	FRootSignature m_SkySignature;
	FRootSignature m_Show2DTextureSignature;
	FRootSignature m_CubeMapCrossViewSignature;

	FGraphicsPipelineState m_GenCubePSO;
	FGraphicsPipelineState m_SkyPSO;
	FGraphicsPipelineState m_Show2DTexturePSO;
	FGraphicsPipelineState m_CubeMapCrossViewPSO;
	FGraphicsPipelineState m_GenIrradiancePSO;
	FGraphicsPipelineState m_SphericalHarmonicsCrossViewPSO;

	FTexture m_TextureLongLat;
	FCubeBuffer m_CubeBuffer, m_IrradianceCube, m_PrefilteredCube;

	ComPtr<ID3DBlob> m_LongLatToCubeVS;
	ComPtr<ID3DBlob> m_ScreenQuadVS;
	ComPtr<ID3DBlob> m_SkyPS, m_SkyVS;
	ComPtr<ID3DBlob> m_CubeMapCrossPS, m_CubeMapCrossVS;
	ComPtr<ID3DBlob> m_SphericalHarmonicsPS;
	ComPtr<ID3DBlob> m_ShowTexture2DVS, m_ShowTexture2DPS;
	ComPtr<ID3DBlob> m_LongLatToCubePS;
	ComPtr<ID3DBlob> m_GenIrradiancePS;

	FCamera m_Camera;
	FDirectionalLight m_DirectionLight;
	FModel* m_SkyBox, *m_CubeMapCross;

	int m_ShowMode = SM_CubeMapCross;
	Vector3f m_ClearColor;
	float m_Exposure = 1.f;
	int m_MipLevel = 0;
	int m_NumSamplesPerDir = 10;
	std::chrono::high_resolution_clock::time_point tStart, tEnd;
};

int main()
{
	ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	GameDesc Desc;
	Desc.Caption = L"IBL Maps Generator";
	Tutorial9 tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	CoUninitialize();
	return 0;
}