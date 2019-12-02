#pragma once

#include <string>
#include <Windows.h>

struct WindowDesc
{
	std::wstring Caption = L"The Practice of Direct3D 12 Programming";
	int Width = 1024;
	int Height = 768;
};

class WindowWin32
{
public:
	static const TCHAR AppWindowClass[];

public:
	WindowWin32(const WindowDesc& Desc);
	~WindowWin32();

	bool IsClosed() const;

private:
	static LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	
private:
	HWND m_hwnd;
	bool m_bIsClosed;
};

