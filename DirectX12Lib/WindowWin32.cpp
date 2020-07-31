#include <Windows.h>
#include <iostream>
#include <algorithm>

#include "WindowWin32.h"
#include "Game.h"


const TCHAR WindowWin32::AppWindowClass[] = L"DirectX12TutorialWindow";

WindowWin32::WindowWin32()
	: m_hwnd(nullptr)
	, Width(0)
	, Height(0)
{
}

WindowWin32& WindowWin32::Get()
{
	static WindowWin32 Singleton;
	return Singleton;
}

void WindowWin32::Destroy()
{
	if (m_hwnd)
	{
		::DestroyWindow(m_hwnd);
		m_hwnd = nullptr;
	}
}

WindowWin32::~WindowWin32()
{
	Destroy();
}

bool WindowWin32::Initialize(FGame* Game)
{
	HINSTANCE InstanceHandle = GetModuleHandle(nullptr);
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = InstanceHandle;
	wc.lpszClassName = AppWindowClass;

	Width = Game->GetWidth();
	Height = Game->GetHeight();
	int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);
	int windowX = std::max<int>(0, (screenWidth - Width) / 2);
	int windowY = std::max<int>(0, (screenHeight - Height) / 2);

	RegisterClassEx(&wc);

	RECT windowRect = { 0, 0, static_cast<LONG>(Width), static_cast<LONG>(Height) };
	AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	m_hwnd = CreateWindowEx(
		0,									// Optional window styles
		AppWindowClass,						// Window class
		Game->GetWindowTitle().c_str(),		// Window title
		WS_OVERLAPPEDWINDOW,				// Window style
		windowX, windowY,					// Window Position
		windowRect.right-windowRect.left,	// Width
		windowRect.bottom - windowRect.top,	// Height
		NULL,								// Parent window
		NULL,								// Menu
		InstanceHandle,						// Instance handle
		Game								// Additional application data
	);

	::ShowWindow(m_hwnd, SW_SHOWNORMAL);

	return true;
}

LRESULT WindowWin32::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	FGame* Game = nullptr;
	if (uMsg == WM_CREATE)
	{
		LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
		SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
		Game = reinterpret_cast<FGame*>(pCreateStruct->lpCreateParams);
	}
	else
	{
		Game = reinterpret_cast<FGame*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));
	}
	
	switch (uMsg)
	{
	case WM_KEYDOWN:
		Game->OnKeyDown(static_cast<uint8_t>(wParam));
		if (wParam == VK_ESCAPE)
		{
			::PostQuitMessage(0);
		}
		break;

	case WM_KEYUP:
		Game->OnKeyUp(static_cast<uint8_t>(wParam));
		break;

	case WM_DESTROY:
		::PostQuitMessage(0);
		break;
	}

	return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
}