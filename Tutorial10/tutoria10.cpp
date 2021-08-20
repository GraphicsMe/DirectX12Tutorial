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

#include <d3d12.h>
#include <dxgi1_4.h>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sys/stat.h>

extern FCommandListManager g_CommandListManager;

const int CUBE_MAP_SIZE = 1024;
const int IRRADIANCE_SIZE = 256;
const int PREFILTERED_SIZE = 256;
const bool CUBEMAP_DEBUG_VIEW = true;

using namespace BufferManager;

class Tutorial10 : public FGame
{
public:
	Tutorial10(const GameDesc& Desc) : FGame(Desc)
	{
	}

	void OnStartup()
	{
		m_HDRFilePath = std::wstring(L"../Resources/HDR/spruit_sunrise_2k.hdr");
		ParsePath();

		// Setup
		SetupMesh();
		SetupShaders();
		SetupPipelineState();

		// Generate CubeMap for Precompute
		GenerateCubeMap();

		// Precompute
		GenerateIrradianceMap();
		GeneratePrefilteredMap();
		GenerateSHCoeffs();

		// Save Result
		SaveCubeMap();
		SaveSHCoeffs();

		// BRDF-LUT
		m_PreIntegrateBRDFPath = std::wstring(L"../Resources/HDR/PreIntegrateBRDF.dds");
		if (!CheckFileExist(m_PreIntegrateBRDFPath))
		{
			PreIntegrateBRDF();
			m_PreintegratedGF.SaveTexutre(m_PreIntegrateBRDFPath);
		}
		exit(0);
	}

private:

	void ParsePath()
	{
		size_t lastPeriodIndex = m_HDRFilePath.find_last_of('.');

		if (lastPeriodIndex == m_HDRFilePath.npos)
		{
			printf("Input HDR file pathv is wrong\n");
			exit(-1);
		}

		m_HDRFileName = m_HDRFilePath.substr(0, lastPeriodIndex); // ../Resources/HDR/spruit_sunrise_2k

		m_IrradianceMapPath = m_HDRFileName + std::wstring(L"_IrradianceMap.dds");
		m_PrefilteredMapPath = m_HDRFileName + std::wstring(L"_PrefilteredMap.dds");
		m_SHCoeffsPath = m_HDRFileName + std::wstring(L"_SHCoeffs.txt");
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
		m_TextureLongLat.LoadFromFile(m_HDRFilePath.c_str());
		m_SkyBox = std::make_unique<FSkyBox>();
	}

	void SetupShaders()
	{
		m_LongLatToCubeVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "VS_LongLatToCube", "vs_5_1");
		m_LongLatToCubePS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_LongLatToCube", "ps_5_1");
		m_SkyVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "VS_SkyCube", "vs_5_1");
		m_SkyPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_SkyCube", "ps_5_1");
		m_GenIrradiancePS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_GenIrradiance", "ps_5_1");
		m_GenPrefilterPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/EnvironmentShaders.hlsl", "PS_GenPrefiltered", "ps_5_1");
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

		m_GenIrradiancePSO.SetRootSignature(m_SkySignature);
		m_GenIrradiancePSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_GenIrradiancePSO.SetBlendState(FPipelineState::BlendDisable);
		m_GenIrradiancePSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
		m_GenIrradiancePSO.SetInputLayout((UINT)SkyBoxLayout.size(), &SkyBoxLayout[0]);
		m_GenIrradiancePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_GenIrradiancePSO.SetRenderTargetFormats(1, &CubeFormat, DXGI_FORMAT_UNKNOWN);
		m_GenIrradiancePSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_SkyVS.Get()));
		m_GenIrradiancePSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_GenIrradiancePS.Get()));
		m_GenIrradiancePSO.Finalize();

		m_IrradianceCube.Create(L"Irradiance Map", IRRADIANCE_SIZE, IRRADIANCE_SIZE, 1, DXGI_FORMAT_R16G16B16A16_FLOAT);

		m_GenPrefilterPSO.SetRootSignature(m_SkySignature);
		m_GenPrefilterPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
		m_GenPrefilterPSO.SetBlendState(FPipelineState::BlendDisable);
		m_GenPrefilterPSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
		m_GenPrefilterPSO.SetInputLayout((UINT)SkyBoxLayout.size(), &SkyBoxLayout[0]);
		m_GenPrefilterPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
		m_GenPrefilterPSO.SetRenderTargetFormats(1, &CubeFormat, DXGI_FORMAT_UNKNOWN);
		m_GenPrefilterPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_SkyVS.Get()));
		m_GenPrefilterPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_GenPrefilterPS.Get()));
		m_GenPrefilterPSO.Finalize();

		m_PrefilteredCube.Create(L"Prefiltered Map", PREFILTERED_SIZE, PREFILTERED_SIZE, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);

		FSamplerDesc PointSamplerDesc(D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);
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
		m_IrradianceCube.SaveCubeMap(m_IrradianceMapPath);
		m_PrefilteredCube.SaveCubeMap(m_PrefilteredMapPath);
	}

	void GenerateSHCoeffs()
	{
		m_SHCoeffs = m_CubeBuffer.GenerateSHcoeffs(4, m_SHSampleNum);
	}

	void SaveSHCoeffs()
	{
		//for (int i = 0; i < m_SHCoeffs.size(); ++i)
		//{
		//	printf("[%f,%f,%f]\n", m_SHCoeffs[i].x, m_SHCoeffs[i].y, m_SHCoeffs[i].z);
		//}

		std::ofstream Outputfile;
		Outputfile.open(m_SHCoeffsPath.c_str());

		for (int i = 0; i < m_SHCoeffs.size(); ++i)
		{
			Outputfile << m_SHCoeffs[i].x << " " << m_SHCoeffs[i].y << " " << m_SHCoeffs[i].z << std::endl;
		}
		
		Outputfile.close();
	}

	float G1(float k, float NoV) { return NoV / (NoV * (1.0f - k) + k); }

	// Geometric Shadowing function
	float G_Smith(float NoL, float NoV, float roughness)
	{
		float k = (roughness * roughness) * 0.5f;
		return G1(k, NoL) * G1(k, NoV);
	}

	// Appoximation of joint Smith term for GGX
	// [Heitz 2014, "Understanding the Masking-Shadowing Function in Microfacet-Based BRDFs"]
	float G_SmithJointApprox(float a2, float NoV, float NoL)
	{
		float a = sqrt(a2);
		float Vis_SmithV = NoL * (NoV * (1 - a) + a);
		float Vis_SmithL = NoV * (NoL * (1 - a) + a);
		return 0.5f / (Vis_SmithV + Vis_SmithL);
	}

	void PreIntegrateBRDF()
	{
		int width = 128; //NoV
		int height = 32; //Roughness
		std::vector<Vector2f> ImageData(width * height * sizeof(Vector2f));

		for (int y = 0; y < height; ++y)
		{
			float Roughness = (float)(y + 0.5f) / height;
			float m = Roughness * Roughness;
			float m2 = m * m;

			for (int x = 0; x < width; ++x)
			{
				float NoV = (float)(x + 0.5f) / width;

				Vector3f V;
				V.x = sqrt(1.0f - NoV * NoV);	// sin
				V.y = 0.0f;
				V.z = NoV;						// cos

				float A = 0.0f;
				float B = 0.0f;

				const uint32_t NumSamples = 128;
				for (uint32_t i = 0; i < NumSamples; i++)
				{
					float E1 = (float)i / NumSamples;
					float E2 = (float)ReverseBits(i) / (float)0x100000000LL;

					{
						float Phi = 2.0f * MATH_PI * E1;
						float CosPhi = cos(Phi);
						float SinPhi = sin(Phi);
						float CosTheta = sqrt((1.0f - E2) / (1.0f + (m2 - 1.0f) * E2));
						float SinTheta = sqrt(1.0f - CosTheta * CosTheta);

						Vector3f H(SinTheta * cos(Phi), SinTheta * sin(Phi), CosTheta);
						Vector3f L = 2.0f * V.Dot(H) * H - V;

						float NoL = std::max(L.z, 0.0f);
						float NoH = std::max(H.z, 0.0f);
						float VoH = std::max(V.Dot(H), 0.0f);

						if (NoL > 0.0f)
						{
							float Vis = G_SmithJointApprox(m2, NoV, NoL);
							float NoL_Vis_PDF = NoL * Vis * (4.f * VoH / NoH);
							float Fc = pow(1.0f - VoH, 5.f);
							A += NoL_Vis_PDF * (1.0f - Fc);
							B += NoL_Vis_PDF * Fc;
						}
					}
				}

				Vector2f& Texel = ImageData[y * width + x];
				Texel.x = A / NumSamples;
				Texel.y = B / NumSamples;
			}
		}

		m_PreintegratedGF.Create(width, height, DXGI_FORMAT_R32G32_FLOAT, ImageData.data());
	}


private:
	std::wstring m_HDRFilePath;
	std::wstring m_HDRFileName;
	std::wstring m_IrradianceMapPath;
	std::wstring m_PrefilteredMapPath;
	std::wstring m_SHCoeffsPath;
	std::wstring m_PreIntegrateBRDFPath;

	// SH
	int	m_SHDegree = 4;
	int m_SHSampleNum = 10000;
	std::vector<Vector3f> m_SHCoeffs;

	// Irradiance
	int m_NumSamplesPerDir = 10;

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

	FRootSignature m_GenCubeSignature;
	FRootSignature m_SkySignature;

	FGraphicsPipelineState m_GenCubePSO;
	FGraphicsPipelineState m_SkyPSO;
	FGraphicsPipelineState m_GenIrradiancePSO;
	FGraphicsPipelineState m_GenPrefilterPSO;

	FTexture m_TextureLongLat, m_PreintegratedGF;
	FCubeBuffer m_CubeBuffer, m_IrradianceCube, m_PrefilteredCube;

	ComPtr<ID3DBlob> m_LongLatToCubeVS;
	ComPtr<ID3DBlob> m_LongLatToCubePS;
	ComPtr<ID3DBlob> m_SkyPS, m_SkyVS;
	ComPtr<ID3DBlob> m_GenIrradiancePS;
	ComPtr<ID3DBlob> m_GenPrefilterPS;

	std::unique_ptr<FModel> m_SkyBox;
};

int main()
{
	ThrowIfFailed(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	GameDesc Desc;
	Desc.Caption = L"IBL Precompute";
	Tutorial10 tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	CoUninitialize();
	return 0;
}