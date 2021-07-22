#include "d3dx12.h"
#include "ImguiManager.h"
#include "Common.h"
#include "RootSignature.h"
#include "CommandContext.h"
#include "RenderWindow.h"
#include "WindowWin32.h"
#include "D3D12RHI.h"
#include "SamplerManager.h"
#include "DirectXTex.h"
#include "BufferManager.h"


// Include compiled shaders for ImGui.

#include "imgui_impl_win32.h"

// Root parameters for the ImGui root signature.
enum RootParameters
{
    MatrixCB,     // cbuffer vertexBuffer : register(b0)
    FontTexture,  // Texture2D texture0 : register(t0);
    NumRootParameters
};

void GetSurfaceInfo(size_t width, size_t height, DXGI_FORMAT fmt, size_t* outNumBytes,
                    size_t* outRowBytes, size_t* outNumRows );

// Windows message handler for ImGui.
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

LRESULT ImguiManager::WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
}

ImguiManager& ImguiManager::Get()
{
	static ImguiManager	Manager;
	return Manager;
}

void ImguiManager::Initialize()
{
    m_pImGuiCtx = ImGui::CreateContext();
    ImGui::SetCurrentContext( m_pImGuiCtx );
	
	HWND hWnd = WindowWin32::Get().GetWindowHandle();
    if ( !ImGui_ImplWin32_Init(hWnd) )
    {
        throw std::exception( "Failed to initialize ImGui" );
    }

    ImGuiIO& io = ImGui::GetIO();

    io.FontGlobalScale = ::GetDpiForWindow(hWnd) / 96.0f;
    // Allow user UI scaling using CTRL+Mouse Wheel scrolling
    io.FontAllowUserScaling = true;

    // Build texture atlas
    unsigned char* pixelData = nullptr;
    int            width, height;
    io.Fonts->GetTexDataAsRGBA32( &pixelData, &width, &height );

    auto fontTextureDesc = CD3DX12_RESOURCE_DESC::Tex2D( DXGI_FORMAT_R8G8B8A8_UNORM, width, height );

    size_t rowPitch, slicePitch;
    GetSurfaceInfo( width, height, DXGI_FORMAT_R8G8B8A8_UNORM, &slicePitch, &rowPitch, nullptr );

	//m_FontTexture->SetName( L"ImGui Font Texture" );
	m_FontTexture.Create(width, height, DXGI_FORMAT_R8G8B8A8_UNORM, pixelData);

    // Allow input layout and deny unnecessary access to certain pipeline stages.
    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                                                    D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                                                    D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                                                    D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	FSamplerDesc DefaultSamplerDesc;
	DefaultSamplerDesc.BorderColor[0] = 0.f;
	DefaultSamplerDesc.BorderColor[1] = 0.f;
	DefaultSamplerDesc.BorderColor[2] = 0.f;
	DefaultSamplerDesc.BorderColor[3] = 0.f;

	m_RootSignature.Reset(2, 1);
	m_RootSignature[0].InitAsConstants(0, sizeof(FMatrix) / 4, D3D12_SHADER_VISIBILITY_VERTEX);
	m_RootSignature[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	m_RootSignature.InitStaticSampler(0, DefaultSamplerDesc, D3D12_SHADER_VISIBILITY_PIXEL);
	m_RootSignature.Finalize(L"IMGUI", rootSignatureFlags);

    // clang-format off
    const D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof( ImDrawVert, pos ), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof( ImDrawVert, uv ), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof( ImDrawVert, col ), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

	m_ImGUIVS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/ImGUI.hlsl", "VS_Main", "vs_5_1");
	m_ImGUIPS = D3D12RHI::Get().CreateShader(L"../Resources/Shaders/ImGUI.hlsl", "PS_Main", "ps_5_1");

	m_ImGUIPSO.SetRootSignature(m_RootSignature);
	m_ImGUIPSO.SetRasterizerState(FPipelineState::RasterizerTwoSided);
	m_ImGUIPSO.SetBlendState(FPipelineState::BlendTraditional);
	m_ImGUIPSO.SetDepthStencilState(FPipelineState::DepthStateDisabled);
	m_ImGUIPSO.SetInputLayout(3, &inputLayout[0]);
	m_ImGUIPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	RenderWindow& RenderTarget = RenderWindow::Get();
	m_ImGUIPSO.SetRenderTargetFormats(1, &RenderWindow::Get().GetColorFormat(), BufferManager::g_SceneDepthZ.GetFormat());
	m_ImGUIPSO.SetVertexShader(CD3DX12_SHADER_BYTECODE(m_ImGUIVS.Get()));
	m_ImGUIPSO.SetPixelShader(CD3DX12_SHADER_BYTECODE(m_ImGUIPS.Get()));
	m_ImGUIPSO.Finalize();
}

void ImguiManager::Destroy()
{
    if (m_newFrame)
    {
        ImGui::EndFrame();
    }
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext(m_pImGuiCtx);
	m_pImGuiCtx = nullptr;
}

void ImguiManager::NewFrame()
{
    ImGui::SetCurrentContext( m_pImGuiCtx );
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    m_newFrame = true;
}

void ImguiManager::Render(FCommandContext& CommandContext, RenderWindow& renderWindow )
{
    ImGui::SetCurrentContext( m_pImGuiCtx );
    ImGui::Render();

    ImGuiIO&    io       = ImGui::GetIO();
    ImDrawData* drawData = ImGui::GetDrawData();

    // Check if there is anything to render.
    if ( !drawData || drawData->CmdListsCount == 0 )
        return;

    ImVec2 displayPos = drawData->DisplayPos;

	FColorBuffer& BackBuffer = renderWindow.GetBackBuffer();

	CommandContext.SetPipelineState(m_ImGUIPSO);
	CommandContext.SetRootSignature(m_RootSignature);
	CommandContext.SetRenderTargets(1, &BackBuffer.GetRTV());

    // Set root arguments.
    //    DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixOrthographicRH( drawData->DisplaySize.x,
    //    drawData->DisplaySize.y, 0.0f, 1.0f );
    float L         = drawData->DisplayPos.x;
    float R         = drawData->DisplayPos.x + drawData->DisplaySize.x;
    float T         = drawData->DisplayPos.y;
    float B         = drawData->DisplayPos.y + drawData->DisplaySize.y;
    float mvp[4][4] = {
        { 2.0f / ( R - L ), 0.0f, 0.0f, 0.0f },
        { 0.0f, 2.0f / ( T - B ), 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.5f, 0.0f },
        { ( R + L ) / ( L - R ), ( T + B ) / ( B - T ), 0.5f, 1.0f },
    };

	CommandContext.SetConstantArray(RootParameters::MatrixCB, sizeof(mvp) / 4, mvp);
    CommandContext.SetDynamicDescriptor(RootParameters::FontTexture, 0, m_FontTexture.GetSRV());

    D3D12_VIEWPORT viewport = {};
    viewport.Width          = drawData->DisplaySize.x;
    viewport.Height         = drawData->DisplaySize.y;
    viewport.MinDepth       = 0.0f;
    viewport.MaxDepth       = 1.0f;

	CommandContext.SetViewport(viewport);
	CommandContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const DXGI_FORMAT indexFormat = sizeof( ImDrawIdx ) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

    // It may happen that ImGui doesn't actually render anything. In this case,
    // any pending resource barriers in the commandList will not be flushed (since
    // resource barriers are only flushed when a draw command is executed).
    // In that case, manually flushing the resource barriers will ensure that
    // they are properly flushed before exiting this function.
    //commandList->FlushResourceBarriers();

    for ( int i = 0; i < drawData->CmdListsCount; ++i )
    {
        const ImDrawList* drawList = drawData->CmdLists[i];

		size_t VertexSize = sizeof(ImDrawVert);
		size_t BufferSize = drawList->VtxBuffer.size() * sizeof(ImDrawVert);
		FAllocation Allocation = CommandContext.ReserveUploadMemory(BufferSize);
		memcpy(Allocation.CPU, drawList->VtxBuffer.Data, BufferSize);

		D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
		vertexBufferView.BufferLocation = Allocation.GpuAddress;
		vertexBufferView.SizeInBytes = static_cast<UINT>(BufferSize);
		vertexBufferView.StrideInBytes = static_cast<UINT>(VertexSize);

		CommandContext.SetVertexBuffer(0, vertexBufferView);

		size_t IndexSizeInBytes = indexFormat == DXGI_FORMAT_R16_UINT ? 2 : 4;
		size_t IndexBufferSize = IndexSizeInBytes * drawList->IdxBuffer.size();
		Allocation = CommandContext.ReserveUploadMemory(IndexBufferSize);
		memcpy(Allocation.CPU, drawList->IdxBuffer.Data, IndexBufferSize);
		D3D12_INDEX_BUFFER_VIEW IBView;
		IBView.BufferLocation = Allocation.GpuAddress;
		IBView.Format = indexFormat == DXGI_FORMAT_R32_UINT ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
		IBView.SizeInBytes = (UINT)IndexBufferSize;
		CommandContext.SetIndexBuffer(IBView);

        int indexOffset = 0;
        for ( int j = 0; j < drawList->CmdBuffer.size(); ++j )
        {
            const ImDrawCmd& drawCmd = drawList->CmdBuffer[j];
            if ( drawCmd.UserCallback )
            {
                drawCmd.UserCallback( drawList, &drawCmd );
            }
            else
            {
                ImVec4     clipRect = drawCmd.ClipRect;
                D3D12_RECT scissorRect;
                scissorRect.left   = static_cast<LONG>( clipRect.x - displayPos.x );
                scissorRect.top    = static_cast<LONG>( clipRect.y - displayPos.y );
                scissorRect.right  = static_cast<LONG>( clipRect.z - displayPos.x );
                scissorRect.bottom = static_cast<LONG>( clipRect.w - displayPos.y );

                if ( scissorRect.right - scissorRect.left > 0.0f && scissorRect.bottom - scissorRect.top > 0.0 )
                {
					CommandContext.SetScissor(scissorRect);
					CommandContext.DrawIndexed(drawCmd.ElemCount, indexOffset);
                }
            }
            indexOffset += drawCmd.ElemCount;
        }
    }
}

void ImguiManager::SetScaling( float scale )
{
    ImGuiIO& io        = ImGui::GetIO();
    io.FontGlobalScale = scale;
}



void GetSurfaceInfo(size_t width, size_t height, DXGI_FORMAT fmt, size_t* outNumBytes,
                      size_t* outRowBytes, size_t* outNumRows )
{
    size_t numBytes = 0;
    size_t rowBytes = 0;
    size_t numRows  = 0;

    bool   bc     = false;
    bool   packed = false;
    bool   planar = false;
    size_t bpe    = 0;
    switch ( fmt )
    {
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        bc  = true;
        bpe = 8;
        break;

    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        bc  = true;
        bpe = 16;
        break;

    case DXGI_FORMAT_R8G8_B8G8_UNORM:
    case DXGI_FORMAT_G8R8_G8B8_UNORM:
    case DXGI_FORMAT_YUY2:
        packed = true;
        bpe    = 4;
        break;

    case DXGI_FORMAT_Y210:
    case DXGI_FORMAT_Y216:
        packed = true;
        bpe    = 8;
        break;

    case DXGI_FORMAT_NV12:
    case DXGI_FORMAT_420_OPAQUE:
        planar = true;
        bpe    = 2;
        break;

    case DXGI_FORMAT_P010:
    case DXGI_FORMAT_P016:
        planar = true;
        bpe    = 4;
        break;
    }

    if ( bc )
    {
        size_t numBlocksWide = 0;
        if ( width > 0 )
        {
            numBlocksWide = std::max<size_t>( 1, ( width + 3 ) / 4 );
        }
        size_t numBlocksHigh = 0;
        if ( height > 0 )
        {
            numBlocksHigh = std::max<size_t>( 1, ( height + 3 ) / 4 );
        }
        rowBytes = numBlocksWide * bpe;
        numRows  = numBlocksHigh;
        numBytes = rowBytes * numBlocksHigh;
    }
    else if ( packed )
    {
        rowBytes = ( ( width + 1 ) >> 1 ) * bpe;
        numRows  = height;
        numBytes = rowBytes * height;
    }
    else if ( fmt == DXGI_FORMAT_NV11 )
    {
        rowBytes = ( ( width + 3 ) >> 2 ) * 4;
        numRows  = height * 2;  // Direct3D makes this simplifying assumption, although it is larger than the 4:1:1 data
        numBytes = rowBytes * numRows;
    }
    else if ( planar )
    {
        rowBytes = ( ( width + 1 ) >> 1 ) * bpe;
        numBytes = ( rowBytes * height ) + ( ( rowBytes * height + 1 ) >> 1 );
        numRows  = height + ( ( height + 1 ) >> 1 );
    }
    else
    {
        size_t bpp = DirectX::BitsPerPixel( fmt );
        rowBytes   = ( width * bpp + 7 ) / 8;  // round up to nearest byte
        numRows    = height;
        numBytes   = rowBytes * height;
    }

    if ( outNumBytes )
    {
        *outNumBytes = numBytes;
    }
    if ( outRowBytes )
    {
        *outRowBytes = rowBytes;
    }
    if ( outNumRows )
    {
        *outNumRows = numRows;
    }
}
