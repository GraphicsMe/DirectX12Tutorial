#include <Windows.h>
#include <iostream>
#include <algorithm>

#include "WindowWin32.h"


const TCHAR WindowWin32::AppWindowClass[] = L"DirectX12TutorialWindow";

WindowWin32::WindowWin32(const WindowDesc& Desc)
	: m_hwnd(nullptr)
	, m_bIsClosed(false)
	, m_windowDesc(Desc)
{
	HINSTANCE InstanceHandle = GetModuleHandle(nullptr);
	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = InstanceHandle;
	wc.lpszClassName = AppWindowClass;

	int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);
	int windowX = std::max<int>(0, (screenWidth - Desc.Width) / 2);
	int windowY = std::max<int>(0, (screenHeight - Desc.Height) / 2);

	RegisterClassEx(&wc);

	RECT windowRect = { 0, 0, static_cast<LONG>(Desc.Width), static_cast<LONG>(Desc.Height) };
    AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	m_hwnd = CreateWindowEx(
		0,  // Optional window styles
		AppWindowClass,  // Window class
		Desc.Caption.c_str(),  // Window title
		WS_OVERLAPPEDWINDOW,  // Window style
		windowX, windowY, Desc.Width, Desc.Height,  // Position and Size
		NULL,  // Parent window    
		NULL,  // Menu
		InstanceHandle,  // Instance handle
		NULL  // Additional application data
	);

	::SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

	::ShowWindow(m_hwnd, SW_SHOWNORMAL);
}


WindowWin32::~WindowWin32()
{

}

bool WindowWin32::IsClosed() const
{
	return m_bIsClosed;
}

LRESULT WindowWin32::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	WindowWin32* win = reinterpret_cast<WindowWin32*>(::GetWindowLongPtr(hWnd, GWLP_USERDATA));

	if (win != nullptr)
	{
		return win->HandleMessage(uMsg, wParam, lParam);
	}
	else
	{
		return ::DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}

LRESULT WindowWin32::HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		m_bIsClosed = true;
		::PostQuitMessage(0);
		return 0;
	}

	return ::DefWindowProc(m_hwnd, uMsg, wParam, lParam);
}