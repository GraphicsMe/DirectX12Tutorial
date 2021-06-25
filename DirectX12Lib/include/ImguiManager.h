#pragma once

#include "imgui.h"
#include "Texture.h"
#include "RootSignature.h"
#include "PipelineState.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>  // For HWND

#include <memory>  // For std::shared_ptr

class CommandList;
class Device;
class PipelineStateObject;
class RenderWindow;
class RootSignature;
class ShaderResourceView;
class Texture;
class FCommandContext;

class ImguiManager
{
public:
	static LRESULT WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	
	static ImguiManager& Get();

	void Initialize();
	void Destroy();
    
    void NewFrame();
    void Render(FCommandContext& CommandContext, RenderWindow& renderTarget);

    void SetScaling( float scale );

protected:
	ImguiManager() = default;
	~ImguiManager() = default;

private:
    ImGuiContext* m_pImGuiCtx = nullptr;
	FTexture m_FontTexture;
	FRootSignature m_RootSignature;
	ComPtr<ID3DBlob> m_ImGUIVS, m_ImGUIPS;
	FGraphicsPipelineState m_ImGUIPSO;
};