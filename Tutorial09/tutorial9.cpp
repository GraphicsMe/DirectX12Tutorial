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
#include "TemporalEffects.h"
#include "BufferManager.h"
#include "MotionBlur.h"
#include "PostProcessing.h"
#include "DepthOfField.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include<sstream>       //istringstream 必须包含这个头文件

extern FCommandListManager g_CommandListManager;

const int CUBE_MAP_SIZE = 1024;
const int IRRADIANCE_SIZE = 256;
const int PREFILTERED_SIZE = 256;
const bool CUBEMAP_DEBUG_VIEW = true;

using namespace BufferManager;

enum EShowMode
{
	SM_LongLat,
	SM_CubeMapCross,
	SM_Irradiance,
	SM_Prefiltered,
	SM_SphericalHarmonics,
	SM_PreintegratedGF,
	SM_PBR,
};

class Tutorial9 : public FGame
{
public:
	Tutorial9(const GameDesc& Desc) : FGame(Desc)
	{
	}

	void OnStartup()
	{
		SetupMesh();
		SetupShaders();
		SetupPipelineState();
		SetupCameraLight();

		//GenerateCubeMap();
		//GenerateIrradianceMap();
		//GeneratePrefilteredMap();

		//SaveCubeMap();
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
		}
		m_Mesh->SetRotation(FMatrix::RotateY(m_RotateY));

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
			ImGui::ColorEdit3("Clear Color", &m_ClearColor.x);
			ImGui::SliderFloat("Exposure", &m_Exposure, 0.f, 10.f, "%.1f");
			
			ImGui::Separator();

			ImGui::BeginGroup();
			ImGui::Text("Show Mode");
			ImGui::Indent(20);
			ImGui::RadioButton("Long-Lat View", &m_ShowMode, SM_LongLat);
			ImGui::RadioButton("Cube Cross", &m_ShowMode, SM_CubeMapCross);
			ImGui::RadioButton("Irradiance", &m_ShowMode, SM_Irradiance);
			ImGui::RadioButton("Prefiltered", &m_ShowMode, SM_Prefiltered);
			ImGui::RadioButton("SphericalHarmonics", &m_ShowMode, SM_SphericalHarmonics);
			ImGui::RadioButton("PreintegratedGF", &m_ShowMode, SM_PreintegratedGF);
			ImGui::RadioButton("PBR Mesh", &m_ShowMode, SM_PBR);
			ImGui::EndGroup();

			if (m_ShowMode == SM_CubeMapCross)
			{
				ImGui::SliderInt("Mip Level", &m_MipLevel, 0, m_CubeBuffer.GetNumMips()-1);
			}
			else if (m_ShowMode == SM_Prefiltered)
			{
				ImGui::SliderInt("Mip Level", &m_MipLevel, 0, m_PrefilteredCube.GetNumMips() - 1);
			}
			else if (m_ShowMode == SM_SphericalHarmonics)
			{
				ImGui::SliderInt("SH Degree", &m_SHDegree, 1, 4);
			}
			else if (m_ShowMode == SM_PBR)
			{
				ImGui::Checkbox("TAA", &TemporalEffects::g_EnableTAA);

				ImGui::Checkbox("SHDiffuse", &m_bSHDiffuse);
				ImGui::Checkbox("Rotate Mesh", &m_RotateMesh);
				ImGui::SameLine();
				ImGui::SliderFloat("RotateY", &m_RotateY, 0, MATH_2PI);

				ImGui::Checkbox("Enable Bloom", &PostProcessing::g_EnableBloom);
				if (PostProcessing::g_EnableBloom)
				{
					ImGui::Indent(20);
					ImGui::SliderFloat("Bloom Intensity", &PostProcessing::g_BloomIntensity, 0.f, 5.f);
					ImGui::SliderFloat("Bloom Threshold", &PostProcessing::g_BloomThreshold, 0.f, 10.f);
					ImGui::Indent(-20);
				}

				ImGui::Checkbox("Enable DOF", &DepthOfField::g_EnableDepthOfField);
				if (DepthOfField::g_EnableDepthOfField)
				{
					ImGui::Indent(20);
					ImGui::SliderFloat("FoucusDistance", &DepthOfField::g_FoucusDistance, 0.f, 10.f);
					ImGui::SliderFloat("FoucusRange", &DepthOfField::g_FoucusRange, 0.f, 5.f);
					ImGui::SliderFloat("BokehRadius", &DepthOfField::g_BokehRadius, 1.f, 10.f);
					ImGui::Indent(-20);
				}

				ImGui::Text("Floor PBR Parameters");
				// floor
				{
					ImGui::Indent(20);
					ImGui::ColorEdit3("Base Color", m_FloorColor.data);
					ImGui::SliderFloat("Metallic", &m_FloorMetallic, 0.f, 1.f);
					ImGui::SliderFloat("Roughness", &m_FloorRoughness, 0.f, 1.f);
					ImGui::Indent(-20);
				}
			}

			ImGui::Separator();

			//static bool ShowDemo = false;
			//ImGui::Checkbox("Show Demo", &ShowDemo);
			//if (ShowDemo)
			//	ImGui::ShowDemoWindow(&ShowDemo);

		}
		ImGui::End();

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
		case SM_CubeMapCross:
			ShowCubeMapDebugView(CommandContext, m_CubeBuffer);
			break;
		case SM_Irradiance:
			ShowCubeMapDebugView(CommandContext, m_IrradianceCube);
			break;
		case SM_Prefiltered:
			ShowCubeMapDebugView(CommandContext, m_PrefilteredCube);
			break;
		case SM_SphericalHarmonics:
			ShowSHCubeMapDebugView(CommandContext, m_CubeBuffer);
			break;
		case SM_PreintegratedGF:
			ShowTexture2D(CommandContext, m_PreintegratedGF);
			break;
		case SM_PBR:
			//SkyPass(CommandContext, true);
			BasePass(CommandContext, true);
			PostProcessing::GenerateSSR(CommandContext, m_Camera, m_CubeBuffer);
			IBLPass(CommandContext);
			// DOF
			{
				DepthOfField::Render(CommandContext, m_Camera.GetNearClip(), m_Camera.GetFarClip());
			}
			// TAA
			{
				//MotionBlur::GenerateCameraVelocityBuffer(CommandContext, m_Camera);
				TemporalEffects::ResolveImage(CommandContext, g_SceneColorBuffer);
			}
			PostProcessing::Render(CommandContext);

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
		m_Camera = FCamera(Vector3f(1.5f, 0.6f, 0.3f), Vector3f(0.f, 0.3f, 0.f), Vector3f(0.f, 1.f, 0.f));
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
		m_HDRFilePath = L"../Resources/HDR/spruit_sunrise_2k.hdr";
		ParsePath();

		m_TextureLongLat.LoadFromFile(m_HDRFilePath.c_str()); //spruit_sunrise_2k.hdr, newport_loft.hdr, delta_2_2k
		m_SkyBox = std::make_unique<FSkyBox>();
		m_CubeMapCross = std::make_unique<FCubeMapCross>();
		m_Mesh = std::make_unique<FModel>("../Resources/Models/harley/harley.obj", true, false, true);
	
		m_FloorAlpha.LoadFromFile(L"../Resources/Models/harley/textures/Floor_Alpha.jpg", false);
		m_FloorAlbedo.LoadFromFile(L"../Resources/Models/harley/textures/default.png", true);

		// IBL
		m_PreintegratedGF.LoadFromFile(L"../Resources/HDR/PreIntegrateBRDF.dds");
		m_IrradianceCube.LoadFromFile(m_IrradianceMapPath.c_str(), false);
		m_PrefilteredCube.LoadFromFile(m_PrefilteredMapPath.c_str(), false);

		// 
		m_SHCoeffs.resize(16);
		std::ifstream ifs;;
		ifs.open(m_SHCoeffsPath.c_str());
		for (int i = 0; i < 4 * 4; ++i) 
		{
			std::string str;
			getline(ifs, str);

			std::istringstream is(str);
			std::string s;
			is >> s;
			m_SHCoeffs[i].x = std::stof(s);
			is >> s;
			m_SHCoeffs[i].y = std::stof(s);
			is >> s;
			m_SHCoeffs[i].z = std::stof(s);
		}
		ifs.close();
	}

	void SetupShaders()
	{
		m_LongLatToCubeVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "VS_LongLatToCube", "vs_5_1");
		m_LongLatToCubePS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_LongLatToCube", "ps_5_1");
		m_SkyVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "VS_SkyCube", "vs_5_1");
		m_SkyPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_SkyCube", "ps_5_1");
		m_CubeMapCrossVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "VS_CubeMapCross", "vs_5_1");
		m_CubeMapCrossPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_CubeMapCross", "ps_5_1");
		m_ShowTexture2DVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "VS_ShowTexture2D", "vs_5_1");
		m_ShowTexture2DPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_ShowTexture2D", "ps_5_1");
		m_GenIrradiancePS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_GenIrradiance", "ps_5_1");
		m_GenPrefilterPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_GenPrefiltered", "ps_5_1");
		m_MeshVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PBR.hlsl", "VS_PBR", "vs_5_1");
		m_MeshPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PBR.hlsl", "PS_PBR", "ps_5_1");

		m_SphericalHarmonicsPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_SphericalHarmonics", "ps_5_1");

		m_PBRFloorVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PBR.hlsl", "VS_PBR_Floor", "vs_5_1");
		m_PBRFloorPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PBR.hlsl", "PS_PBR_Floor", "ps_5_1");

		m_ScreenQuadVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PostProcess.hlsl", "VS_ScreenQuad", "vs_5_1");
		m_IBLPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/PBR.hlsl", "PS_IBL", "ps_5_1");
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
		
		std::vector<D3D12_INPUT_ELEMENT_DESC> SkyBoxLayout;
		m_SkyBox->GetMeshLayout(SkyBoxLayout);
		Assert(SkyBoxLayout.size() > 0);
		m_GenCubePSO.SetInputLayout((UINT)SkyBoxLayout.size(), &SkyBoxLayout[0]);

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
		m_SkyPSO.SetDepthStencilState(FPipelineState::DepthStateReadOnly);
		m_SkyPSO.SetInputLayout((UINT)SkyBoxLayout.size(), &SkyBoxLayout[0]);
		m_SkyPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_SkyPSO.SetRenderTargetFormats(1, &g_SceneColorBuffer.GetFormat(), g_SceneDepthZ.GetFormat());
		m_SkyPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_SkyVS.Get()));
		m_SkyPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_SkyPS.Get()));
		m_SkyPSO.Finalize();

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
		m_Show2DTexturePSO.SetRenderTargetFormats(1, &RenderWindow::Get().GetColorFormat(), g_SceneDepthZ.GetFormat());
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
		m_CubeMapCrossViewPSO.SetRenderTargetFormats(1, &RenderWindow::Get().GetColorFormat(), g_SceneDepthZ.GetFormat());
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
		m_SphericalHarmonicsCrossViewPSO.SetRenderTargetFormats(1, &RenderWindow::Get().GetColorFormat(), g_SceneDepthZ.GetFormat());
		m_SphericalHarmonicsCrossViewPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_CubeMapCrossVS.Get()));
		m_SphericalHarmonicsCrossViewPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_SphericalHarmonicsPS.Get()));
		m_SphericalHarmonicsCrossViewPSO.Finalize();

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
		m_MeshPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_MeshPSO.SetBlendState(FPipelineState::BlendDisable);
		m_MeshPSO.SetDepthStencilState(FPipelineState::DepthStateReadWrite);
		m_MeshPSO.SetInputLayout((UINT)MeshLayout.size(), &MeshLayout[0]);
		m_MeshPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		DXGI_FORMAT RTFormats[] = {
			g_SceneColorBuffer.GetFormat(), g_GBufferA.GetFormat(), g_GBufferB.GetFormat(), g_GBufferC.GetFormat(), MotionBlur::g_VelocityBuffer.GetFormat(),
		};
		m_MeshPSO.SetRenderTargetFormats(5, RTFormats, g_SceneDepthZ.GetFormat());
		m_MeshPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_MeshVS.Get()));
		m_MeshPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_MeshPS.Get()));
		m_MeshPSO.Finalize();

		m_FloorPSO.SetRootSignature(m_MeshSignature);
		m_FloorPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_FloorPSO.SetBlendState(FPipelineState::BlendTraditional);
		m_FloorPSO.SetDepthStencilState(FPipelineState::DepthStateReadWrite);
		// no need to set input layout
		m_FloorPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_FloorPSO.SetRenderTargetFormats(5, RTFormats, g_SceneDepthZ.GetFormat());
		m_FloorPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_PBRFloorVS.Get()));
		m_FloorPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_PBRFloorPS.Get()));
		m_FloorPSO.Finalize();

		m_FloorPSO.SetRootSignature(m_MeshSignature);
		m_FloorPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_FloorPSO.SetBlendState(FPipelineState::BlendTraditional);
		m_FloorPSO.SetDepthStencilState(FPipelineState::DepthStateReadWrite);
		// no need to set input layout
		m_FloorPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_FloorPSO.SetRenderTargetFormats(5, RTFormats, g_SceneDepthZ.GetFormat());
		m_FloorPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_PBRFloorVS.Get()));
		m_FloorPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_PBRFloorPS.Get()));
		m_FloorPSO.Finalize();

		m_IBLSignature.Reset(3, 1);
		m_IBLSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
		m_IBLSignature[1].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
		m_IBLSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10); //scenecolor, normal, metallSpecularRoughness, AlbedoAO, velocity, irradiance, prefiltered, preintegratedGF
		m_IBLSignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
		m_IBLSignature.Finalize(L"IBL RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		m_IBLPSO.SetRootSignature(m_IBLSignature);
		m_IBLPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		D3D12_BLEND_DESC BlendAdd = FPipelineState::BlendTraditional;
		BlendAdd.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
		BlendAdd.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		BlendAdd.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		BlendAdd.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		m_IBLPSO.SetBlendState(BlendAdd);
		m_IBLPSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
		// no need to set input layout
		m_IBLPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_IBLPSO.SetRenderTargetFormat(g_SceneColorBuffer.GetFormat(), DXGI_FORMAT_UNKNOWN);
		m_IBLPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_ScreenQuadVS.Get()));
		m_IBLPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_IBLPS.Get()));
		m_IBLPSO.Finalize();
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
		FGenerateMips::GenerateForCube(m_CubeBuffer, GfxContext);
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

	void GeneratePrefilteredMap()
	{
		FCommandContext& GfxContext = FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE_DIRECT, L"3D Queue");

		GfxContext.SetRootSignature(m_SkySignature);
		GfxContext.SetPipelineState(m_GenPrefilterPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		
		GfxContext.TransitionResource(m_CubeBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(m_PrefilteredCube, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		GfxContext.SetDynamicDescriptor(2, 0, m_CubeBuffer.GetCubeSRV());

		m_VSConstants.ModelMatrix = FMatrix(); // identity
		uint32_t NumMips = m_PrefilteredCube.GetNumMips();
		m_PSConstants.MaxMipLevel = NumMips;
		for (uint32_t MipLevel = 0; MipLevel < NumMips; ++MipLevel)
		{
			uint32_t Size = PREFILTERED_SIZE >> MipLevel;
			GfxContext.SetViewportAndScissor(0, 0, Size, Size);

			m_PSConstants.MipLevel = MipLevel;
			GfxContext.SetDynamicConstantBufferView(1, sizeof(m_PSConstants), &m_PSConstants);

			for (int i = 0; i < 6; ++i)
			{
				GfxContext.SetRenderTargets(1, &m_PrefilteredCube.GetRTV(i, MipLevel));
				GfxContext.ClearColor(m_PrefilteredCube, i, MipLevel);

				m_VSConstants.ViewProjMatrix = m_PrefilteredCube.GetViewProjMatrix(i);
				GfxContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstants), &m_VSConstants);

				m_SkyBox->Draw(GfxContext);
			}
		}
		
		GfxContext.Finish(true);
	}

	void SaveCubeMap()
	{
		m_CubeBuffer.SaveCubeMap(L"spruit_sunrise_2k_cubemap.dds");
	}

	void SkyPass(FCommandContext& GfxContext, bool Clear)
	{
		// Set necessary state.
		GfxContext.SetRootSignature(m_SkySignature);
		GfxContext.SetPipelineState(m_SkyPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GfxContext.SetViewportAndScissor(0, 0, m_GameDesc.Width, m_GameDesc.Height);

		FColorBuffer& BackBuffer = g_SceneColorBuffer;
		FDepthBuffer& DepthBuffer = g_SceneDepthZ;

		// Indicate that the back buffer will be used as a render target.
		GfxContext.TransitionResource(m_CubeBuffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(DepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

		GfxContext.SetRenderTargets(1, &BackBuffer.GetRTV());
		GfxContext.SetRenderTargets(1, &BackBuffer.GetRTV(), DepthBuffer.GetDSV());

		if (Clear)
		{
			//BackBuffer.SetClearColor(m_ClearColor);
			GfxContext.ClearColor(BackBuffer);
			GfxContext.ClearDepth(DepthBuffer);
		}

		m_VSConstants.ModelMatrix = FMatrix::TranslateMatrix(m_Camera.GetPosition()); // move with camera
		m_VSConstants.ViewProjMatrix = m_Camera.GetViewMatrix() * m_Camera.GetProjectionMatrix();
		GfxContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstants), &m_VSConstants);

		m_PSConstants.Exposure = m_Exposure;
		GfxContext.SetDynamicConstantBufferView(1, sizeof(m_PSConstants), &m_PSConstants);

		GfxContext.SetDynamicDescriptor(2, 0, m_CubeBuffer.GetCubeSRV());

		m_SkyBox->Draw(GfxContext);
	}

	void BasePass(FCommandContext& GfxContext, bool Clear)
	{
		// Set necessary state.
		GfxContext.SetRootSignature(m_MeshSignature);
		GfxContext.SetPipelineState(m_MeshPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		// jitter offset
		GfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

		FColorBuffer& SceneBuffer = g_SceneColorBuffer;
		FDepthBuffer& DepthBuffer = g_SceneDepthZ;

		// Indicate that the back buffer will be used as a render target.
		GfxContext.TransitionResource(m_IrradianceCube, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(m_PrefilteredCube, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		
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

		if (TemporalEffects::g_EnableTAA)
		{
			m_VSConstants.ModelMatrix = m_Mesh->GetModelMatrix();
			m_VSConstants.ViewProjMatrix = m_Camera.GetViewMatrix() * TemporalEffects::HackAddTemporalAAProjectionJitter(m_Camera, m_MainViewport.Width, m_MainViewport.Height, false);
			m_VSConstants.PreviousModelMatrix = m_Mesh->GetPreviousModelMatrix();
			m_VSConstants.PreviousViewProjMatrix = m_Camera.GetPreviousViewMatrix() * TemporalEffects::HackAddTemporalAAProjectionJitter(m_Camera, m_MainViewport.Width, m_MainViewport.Height, true);
			m_VSConstants.ViewportSize = Vector2f(m_MainViewport.Width, m_MainViewport.Height);
		}
		else
		{
			m_VSConstants.ModelMatrix = m_Mesh->GetModelMatrix();
			m_VSConstants.ViewProjMatrix = m_Camera.GetViewMatrix() * m_Camera.GetProjectionMatrix();
			m_VSConstants.PreviousModelMatrix = m_Mesh->GetPreviousModelMatrix();
			m_VSConstants.PreviousViewProjMatrix = m_Camera.GetPreviousViewProjMatrix();
			m_VSConstants.ViewportSize = Vector2f(m_MainViewport.Width, m_MainViewport.Height);
		}

		GfxContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstants), &m_VSConstants);

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
		} PBR_Constants;

		PBR_Constants.Exposure = m_Exposure;
		PBR_Constants.MaxMipLevel = m_PrefilteredCube.GetNumMips();
		PBR_Constants.CameraPos = m_Camera.GetPosition();
		PBR_Constants.BaseColor = m_FloorColor;
		PBR_Constants.Metallic = m_FloorMetallic;
		PBR_Constants.Roughness = m_FloorRoughness;

		PBR_Constants.bSHDiffuse = m_bSHDiffuse;
		PBR_Constants.Degree = m_SHDegree;

		Vector4f TemporalAAJitter;
		TemporalEffects::GetJitterOffset(TemporalAAJitter, m_MainViewport.Width, m_MainViewport.Height);
		PBR_Constants.TemporalAAJitter = TemporalAAJitter;

		for (int i = 0; i < m_SHCoeffs.size(); ++i)
		{
			PBR_Constants.Coeffs[i] = m_SHCoeffs[i];
		}

		GfxContext.SetDynamicConstantBufferView(1, sizeof(PBR_Constants), &PBR_Constants);

		GfxContext.SetDynamicDescriptor(2, 7, m_IrradianceCube.GetCubeSRV());
		GfxContext.SetDynamicDescriptor(2, 8, m_PrefilteredCube.GetCubeSRV());
		GfxContext.SetDynamicDescriptor(2, 9, m_PreintegratedGF.GetSRV());

		m_Mesh->Draw(GfxContext);

		GfxContext.SetPipelineState(m_FloorPSO);

		// draw floor
		m_VSConstants.ModelMatrix = FMatrix::ScaleMatrix(5.f);
		GfxContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstants), &m_VSConstants);

		GfxContext.SetDynamicDescriptor(2, 0, m_FloorAlbedo.GetSRV());
		GfxContext.SetDynamicDescriptor(2, 1, m_FloorAlpha.GetSRV());
		GfxContext.Draw(6);
	}

	void IBLPass(FCommandContext& GfxContext)
	{
		// Set necessary state.
		GfxContext.SetRootSignature(m_IBLSignature);
		GfxContext.SetPipelineState(m_IBLPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		// jitter offset
		GfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

		FColorBuffer& SceneBuffer = g_SceneColorBuffer;

		// Indicate that the back buffer will be used as a render target.
		GfxContext.TransitionResource(m_IrradianceCube, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(m_PrefilteredCube, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(SceneBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(g_GBufferA, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(g_GBufferB, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(g_GBufferC, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(g_SceneDepthZ, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);

		GfxContext.SetRenderTargets(1, &SceneBuffer.GetRTV());

		m_VSConstants.ModelMatrix = m_Mesh->GetModelMatrix();
		m_VSConstants.ViewProjMatrix = m_Camera.GetViewMatrix() * m_Camera.GetProjectionMatrix();
		m_VSConstants.PreviousModelMatrix = m_Mesh->GetPreviousModelMatrix();
		m_VSConstants.PreviousViewProjMatrix = m_Camera.GetPreviousViewProjMatrix();
		m_VSConstants.ViewportSize = Vector2f(m_MainViewport.Width, m_MainViewport.Height);

		GfxContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstants), &m_VSConstants);

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
		} PBR_Constants;

		PBR_Constants.Exposure = m_Exposure;
		PBR_Constants.MaxMipLevel = m_PrefilteredCube.GetNumMips();
		PBR_Constants.CameraPos = m_Camera.GetPosition();
		PBR_Constants.BaseColor = m_FloorColor;
		PBR_Constants.Metallic = m_FloorMetallic;
		PBR_Constants.Roughness = m_FloorRoughness;
		PBR_Constants.InvViewProj = m_Camera.GetViewProjMatrix().Inverse();

		PBR_Constants.bSHDiffuse = m_bSHDiffuse;
		PBR_Constants.Degree = m_SHDegree;
		for (int i = 0; i < m_SHCoeffs.size(); ++i)
		{
			PBR_Constants.Coeffs[i] = m_SHCoeffs[i];
		}

		GfxContext.SetDynamicConstantBufferView(1, sizeof(PBR_Constants), &PBR_Constants);
		GfxContext.SetDynamicDescriptor(2, 0, g_GBufferA.GetSRV());
		GfxContext.SetDynamicDescriptor(2, 1, g_GBufferB.GetSRV());
		GfxContext.SetDynamicDescriptor(2, 2, g_GBufferC.GetSRV());
		GfxContext.SetDynamicDescriptor(2, 3, g_SceneDepthZ.GetSRV());

		GfxContext.SetDynamicDescriptor(2, 7, m_IrradianceCube.GetCubeSRV());
		GfxContext.SetDynamicDescriptor(2, 8, m_PrefilteredCube.GetCubeSRV());
		GfxContext.SetDynamicDescriptor(2, 9, m_PreintegratedGF.GetSRV());

		GfxContext.Draw(3);
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
		GfxContext.TransitionResource(BackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

		GfxContext.SetRenderTargets(1, &BackBuffer.GetRTV());

		BackBuffer.SetClearColor(m_ClearColor);
		GfxContext.ClearColor(BackBuffer);

		m_VSConstants.ModelMatrix = FMatrix(); // identity
		m_VSConstants.ViewProjMatrix = FMatrix::MatrixOrthoLH(1.f, 1.f, -1.f, 1.f);
		GfxContext.SetDynamicConstantBufferView(0, sizeof(m_VSConstants), &m_VSConstants);

		m_PSConstants.Exposure = m_Exposure;
		GfxContext.SetDynamicConstantBufferView(1, sizeof(m_PSConstants), &m_PSConstants);

		GfxContext.SetDynamicDescriptor(2, 0, Texture2D.GetSRV());

		GfxContext.Draw(3);
	}

	void ShowCubeMapDebugView(FCommandContext& GfxContext, FCubeBuffer& CubeBuffer)
	{
		// Set necessary state.
		GfxContext.SetRootSignature(m_CubeMapCrossViewSignature);
		GfxContext.SetPipelineState(m_CubeMapCrossViewPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		uint32_t Size = std::min(m_GameDesc.Width, m_GameDesc.Height);
		//Size = std::min(Size, CubeBuffer.GetWidth());
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
	std::wstring m_HDRFilePath;
	std::wstring m_HDRFileName;
	std::wstring m_IrradianceMapPath;
	std::wstring m_PrefilteredMapPath;
	std::wstring m_SHCoeffsPath;
	std::wstring m_PreIntegrateBRDFPath;

private:
	__declspec(align(16)) struct
	{
		FMatrix ModelMatrix;
		FMatrix ViewProjMatrix;
		FMatrix PreviousModelMatrix;
		FMatrix PreviousViewProjMatrix;
		Vector2f ViewportSize;
	} m_VSConstants;

	__declspec(align(16)) struct
	{
		float		Exposure;
		int			MipLevel;
		int			MaxMipLevel;
		int			NumSamplesPerDir;
		int			Degree;
		Vector3f	CameraPos;
		int			bSHDiffuse;
		Vector2f    ViewportSize;
		float		pad;
		Vector4f	Coeffs[16];
	} m_PSConstants;

	int	m_SHDegree = 4;
	int m_SHSampleNum = 10000;
	std::vector<Vector3f> m_SHCoeffs;
	bool m_bSHDiffuse = true;

	bool m_bTAA = true;

	FRootSignature m_GenCubeSignature;
	FRootSignature m_SkySignature;
	FRootSignature m_Show2DTextureSignature;
	FRootSignature m_CubeMapCrossViewSignature;
	FRootSignature m_MeshSignature;
	FRootSignature m_IBLSignature;

	FGraphicsPipelineState m_GenCubePSO;
	FGraphicsPipelineState m_SkyPSO;
	FGraphicsPipelineState m_Show2DTexturePSO;
	FGraphicsPipelineState m_CubeMapCrossViewPSO;
	FGraphicsPipelineState m_GenIrradiancePSO;
	FGraphicsPipelineState m_GenPrefilterPSO;
	FGraphicsPipelineState m_SphericalHarmonicsCrossViewPSO;
	FGraphicsPipelineState m_MeshPSO;
	FGraphicsPipelineState m_FloorPSO;
	FGraphicsPipelineState m_IBLPSO;

	FTexture m_FloorAlbedo, m_FloorAlpha;
	FTexture m_TextureLongLat, m_PreintegratedGF;
	FCubeBuffer m_CubeBuffer, m_IrradianceCube, m_PrefilteredCube;

	ComPtr<ID3DBlob> m_LongLatToCubeVS;
	ComPtr<ID3DBlob> m_SkyPS, m_SkyVS;
	ComPtr<ID3DBlob> m_CubeMapCrossPS, m_CubeMapCrossVS;
	ComPtr<ID3DBlob> m_SphericalHarmonicsPS;
	ComPtr<ID3DBlob> m_ShowTexture2DVS, m_ShowTexture2DPS;
	ComPtr<ID3DBlob> m_LongLatToCubePS;
	ComPtr<ID3DBlob> m_GenIrradiancePS;
	ComPtr<ID3DBlob> m_GenPrefilterPS;
	ComPtr<ID3DBlob> m_MeshPS, m_MeshVS;
	ComPtr<ID3DBlob> m_PBRFloorVS, m_PBRFloorPS;
	ComPtr<ID3DBlob> m_ScreenQuadVS, m_IBLPS;

	D3D12_VIEWPORT		m_MainViewport;
	D3D12_RECT			m_MainScissor;
	FCamera m_Camera;
	FDirectionalLight m_DirectionLight;
	std::unique_ptr<FModel> m_Mesh, m_SkyBox, m_CubeMapCross;

	int m_ShowMode = SM_PBR;
	Vector3f m_ClearColor = Vector3f(0.2f);
	float m_Exposure = 1.f;
	int m_MipLevel = 0;
	int m_NumSamplesPerDir = 10;
	bool m_RotateMesh = false;
	float m_RotateY = 0.f;//3.984f;
	// floor PBR parameters
	Vector3f m_FloorColor = Vector3f(0.3f);
	float m_FloorMetallic = 0.f;
	float m_FloorRoughness = 1.f;
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