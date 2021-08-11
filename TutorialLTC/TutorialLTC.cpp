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
#include "Geometry.h"
#include "GameInput.h"
#include "ImguiManager.h"
#include "GenerateMips.h"
#include "BufferManager.h"
#include "PostProcessing.h"

#include <d3d12.h>
#include <dxgi1_4.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include<sstream>       //istringstream 必须包含这个头文件

extern FCommandListManager g_CommandListManager;

using namespace BufferManager;

class TutorialLTC : public FGame
{
public:
	TutorialLTC(const GameDesc& Desc) : FGame(Desc)
	{
	}

	void OnStartup()
	{
		SetupMesh();
		SetupShaders();
		SetupPipelineState();
		SetupCameraLight();

		PostProcessing::g_EnableBloom = false;
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

		m_Floor->Update();
		m_LightPolygon->Update();

		if (GameInput::IsKeyDown('F'))
			SetupCameraLight();

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
			ImGui::SliderInt("DebugFlag,1:OnlyDiffuse,2:OnlySpecular", &m_DebugFlag, 0, 2);
			ImGui::SliderFloat("Light Intensity", &m_LightIntensity, 0, 10.0f);
			ImGui::Checkbox("Is Two Side Light", &m_TwoSideLight);
			ImGui::SliderFloat("Floor Roughness", &m_Roughness, 0.01f, 1.0f);
			ImGui::ColorEdit3("Floor DiffuseColor", &m_FloorDiffuseColor.x);
			ImGui::ColorEdit3("Floor SpecularColor", &m_SpecularColor.x);
		}
		ImGui::End();

		ImguiManager::Get().Render(CommandContext, RenderWindow::Get());
	}

	void OnRender()
	{
		FCommandContext& CommandContext = FCommandContext::Begin(D3D12_COMMAND_LIST_TYPE_DIRECT, L"3D Queue");

		RenderLightPolygon(CommandContext, true);
		RenderFloor(CommandContext, false);

		PostProcessing::Render(CommandContext);

		OnGUI(CommandContext);

		CommandContext.TransitionResource(RenderWindow::Get().GetBackBuffer(), D3D12_RESOURCE_STATE_PRESENT);
		CommandContext.Finish(true);

		RenderWindow::Get().Present();
	}

private:
	void SetupCameraLight()
	{
		m_Camera = FCamera(Vector3f(0.0f, 0.5f, -7.0f), Vector3f(0.f, 0.0f, 0.f), Vector3f(0.f, 1.f, 0.f));
		m_Camera.SetMouseMoveSpeed(1e-3f);
		m_Camera.SetMouseRotateSpeed(1e-4f);

		const float FovVertical = MATH_PI / 4.f;
		m_Camera.SetPerspectiveParams(FovVertical, (float)GetDesc().Width / GetDesc().Height, 0.1f, 100.f);
	}

	void SetupMesh()
	{
		m_LightPolygon = std::make_unique<FQuad>();
		m_Floor = std::make_unique<FModel>("../Resources/Models/primitive/Plane.obj", true, true);
		m_Floor->SetPosition(0, -0.6f, 0);
		m_Floor->SetScale(10, 10, 10);

		m_LightTexture.LoadFromFile(L"../Resources/Textures/white.png", false);
		
		// LTC Textures
		m_LTC_MatrixTexture.LoadFromFile(L"../Resources/Textures/LTC/ltc_1.dds", false);
		m_LTC_MagnitueTexture.LoadFromFile(L"../Resources/Textures/LTC/ltc_2.dds", false);
	}

	void SetupShaders()
	{
		m_FloorVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/LTC_Floor.hlsl", "VS_Floor", "vs_5_1");
		m_FloorPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/LTC_Floor.hlsl", "PS_Floor", "ps_5_1");

		m_LightPolygonVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/LTC_LightPolygon.hlsl", "VS_LightPolygon", "vs_5_1");
		m_LightPolygonPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/LTC_LightPolygon.hlsl", "PS_LightPolygon", "ps_5_1");
	}

	void SetupPipelineState()
	{
		FSamplerDesc DefaultSamplerDesc(D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		// Floor
		{
			m_FloorSignature.Reset(3, 1);
			m_FloorSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
			m_FloorSignature[1].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
			m_FloorSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 10);
			m_FloorSignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
			m_FloorSignature.Finalize(L"Plane RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			std::vector<D3D12_INPUT_ELEMENT_DESC> MeshLayout;
			m_Floor->GetMeshLayout(MeshLayout);
			Assert(MeshLayout.size() > 0);
			m_FloorPSO.SetInputLayout((UINT)MeshLayout.size(), &MeshLayout[0]);
			m_FloorPSO.SetRootSignature(m_FloorSignature);
			m_FloorPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
			m_FloorPSO.SetBlendState(FPipelineState::BlendDisable);
			m_FloorPSO.SetDepthStencilState(FPipelineState::DepthStateReadWrite);
			m_FloorPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			m_FloorPSO.SetRenderTargetFormats(1, &g_SceneColorBuffer.GetFormat(), g_SceneDepthZ.GetFormat());
			m_FloorPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_FloorVS.Get()));
			m_FloorPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_FloorPS.Get()));
			m_FloorPSO.Finalize();
		}

		// LightPolygon
		{
			m_LightPolygonSignature.Reset(3, 1);
			m_LightPolygonSignature[0].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_VERTEX);
			m_LightPolygonSignature[1].InitAsBufferCBV(0, D3D12_SHADER_VISIBILITY_PIXEL);
			m_LightPolygonSignature[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
			m_LightPolygonSignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
			m_LightPolygonSignature.Finalize(L"LightPolygon RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

			std::vector<D3D12_INPUT_ELEMENT_DESC> MeshLayout;
			m_LightPolygon->GetMeshLayout(MeshLayout);
			Assert(MeshLayout.size() > 0);
			m_LightPolygonPSO.SetInputLayout((UINT)MeshLayout.size(), &MeshLayout[0]);

			m_LightPolygonPSO.SetRootSignature(m_LightPolygonSignature);
			m_LightPolygonPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
			m_LightPolygonPSO.SetDepthStencilState(FPipelineState::DepthStateReadWrite);
			m_LightPolygonPSO.SetBlendState(FPipelineState::BlendDisable);
			m_LightPolygonPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
			m_LightPolygonPSO.SetRenderTargetFormats(1, &g_SceneColorBuffer.GetFormat(), g_SceneDepthZ.GetFormat());
			m_LightPolygonPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_LightPolygonVS.Get()));
			m_LightPolygonPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_LightPolygonPS.Get()));
			m_LightPolygonPSO.Finalize();
		}

	}

	void GetLightPolygonalVertexPosSet(std::vector<Vector4f>& PolygonalLightVertexPos)
	{
		m_LightPolygon->GetLightPolygonalVertexPosSet(PolygonalLightVertexPos);
	}

	void RenderFloor(FCommandContext& GfxContext, bool Clear = false)
	{
		// Set necessary state.
		GfxContext.SetRootSignature(m_FloorSignature);
		GfxContext.SetPipelineState(m_FloorPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

		FColorBuffer& SceneBuffer = g_SceneColorBuffer;
		FDepthBuffer& DepthBuffer = g_SceneDepthZ;

		GfxContext.TransitionResource(m_LTC_MatrixTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(m_LTC_MagnitueTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(m_LightTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
		GfxContext.TransitionResource(SceneBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(g_SceneDepthZ, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

		GfxContext.SetRenderTargets(1, &SceneBuffer.GetRTV(), g_SceneDepthZ.GetDSV());

		if (Clear)
		{
			GfxContext.ClearColor(SceneBuffer);
			GfxContext.ClearDepth(DepthBuffer);
		}

		__declspec(align(16)) struct
		{
			FMatrix ModelMatrix;
			FMatrix ViewProjMatrix;
		} VSConstants;

		VSConstants.ModelMatrix = m_Floor->GetModelMatrix();
		VSConstants.ViewProjMatrix = m_Camera.GetViewMatrix() * m_Camera.GetProjectionMatrix();

		GfxContext.SetDynamicConstantBufferView(0, sizeof(VSConstants), &VSConstants);

		__declspec(align(16)) struct
		{
			Vector3f	CameraPosInWorldSpace;
			float		Roughness;
			Vector3f	DiffuseColor;
			float		bTwoSideLight;
			Vector3f	SpecularColor;
			float		LightIntensity;
			int			DebugFlag;
			Vector3f	pad;
			Vector4f	PolygonalLightVertexPos[4];
		}PSConstants;

		PSConstants.CameraPosInWorldSpace = m_Camera.GetPosition();
		PSConstants.Roughness = m_Roughness;
		PSConstants.DiffuseColor = m_FloorDiffuseColor;
		PSConstants.SpecularColor = m_SpecularColor;
		PSConstants.bTwoSideLight = m_TwoSideLight;
		PSConstants.LightIntensity = m_LightIntensity;

		std::vector<Vector4f> PolygonalLightVertexPosSet;
		GetLightPolygonalVertexPosSet(PolygonalLightVertexPosSet);

		for (int i = 0; i < 4; i++)
		{
			PSConstants.PolygonalLightVertexPos[i] = PolygonalLightVertexPosSet[i];
		}

		PSConstants.DebugFlag = m_DebugFlag;

		GfxContext.SetDynamicConstantBufferView(1, sizeof(PSConstants), &PSConstants);

		GfxContext.SetDynamicDescriptor(2, 0, m_LightTexture.GetSRV());
		GfxContext.SetDynamicDescriptor(2, 1, m_LTC_MatrixTexture.GetSRV());
		GfxContext.SetDynamicDescriptor(2, 2, m_LTC_MagnitueTexture.GetSRV());

		m_Floor->Draw(GfxContext, false);
	}

	void RenderLightPolygon(FCommandContext& GfxContext, bool Clear = false)
	{
		// Set necessary state.
		GfxContext.SetRootSignature(m_LightPolygonSignature);
		GfxContext.SetPipelineState(m_LightPolygonPSO);
		GfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		GfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

		FColorBuffer& SceneBuffer = g_SceneColorBuffer;
		FDepthBuffer& DepthBuffer = g_SceneDepthZ;

		GfxContext.TransitionResource(m_LightTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, true);
		GfxContext.TransitionResource(SceneBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
		GfxContext.TransitionResource(g_SceneDepthZ, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

		GfxContext.SetRenderTargets(1, &SceneBuffer.GetRTV(), g_SceneDepthZ.GetDSV());

		if (Clear)
		{
			GfxContext.ClearColor(SceneBuffer);
			GfxContext.ClearDepth(DepthBuffer);
		}

		__declspec(align(16)) struct
		{
			FMatrix ModelMatrix;
			FMatrix ViewProjMatrix;
		} VSConstants;

		VSConstants.ModelMatrix = m_LightPolygon->GetModelMatrix();
		VSConstants.ViewProjMatrix = m_Camera.GetViewMatrix() * m_Camera.GetProjectionMatrix();

		GfxContext.SetDynamicConstantBufferView(0, sizeof(VSConstants), &VSConstants);

		__declspec(align(16)) struct
		{
			float LightIntensity;
		}PSConstants;

		PSConstants.LightIntensity = m_LightIntensity;

		GfxContext.SetDynamicConstantBufferView(1, sizeof(PSConstants), &PSConstants);

		GfxContext.SetDynamicDescriptor(2, 0, m_LightTexture.GetSRV());

		m_LightPolygon->Draw(GfxContext, false);
	}

private:
	std::unique_ptr<FModel>		m_Floor;
	ComPtr<ID3DBlob>			m_FloorVS;
	ComPtr<ID3DBlob>			m_FloorPS;
	FRootSignature				m_FloorSignature;
	FGraphicsPipelineState		m_FloorPSO;
	
	std::unique_ptr<FQuad>		m_LightPolygon;
	ComPtr<ID3DBlob>			m_LightPolygonVS;
	ComPtr<ID3DBlob>			m_LightPolygonPS;
	FRootSignature				m_LightPolygonSignature;
	FGraphicsPipelineState		m_LightPolygonPSO;

	FTexture					m_LightTexture;
	FTexture					m_LTC_MatrixTexture;
	FTexture					m_LTC_MagnitueTexture;

private:
	int					m_DebugFlag = 0;
	float				m_Roughness = 0.4f;
	float				m_LightIntensity = 4.0f;
	bool				m_TwoSideLight = false;
	Vector3f			m_FloorDiffuseColor = Vector3f(1, 1, 1);
	Vector3f			m_SpecularColor = Vector3f(0.172f, 0.172f, 0.172f);

private:
	D3D12_VIEWPORT		m_MainViewport;
	D3D12_RECT			m_MainScissor;
	FCamera				m_Camera;
	std::chrono::high_resolution_clock::time_point tStart, tEnd;
};

int main()
{
	ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	GameDesc Desc;
	Desc.Caption = L"LTC";
	TutorialLTC tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	CoUninitialize();
	return 0;
}