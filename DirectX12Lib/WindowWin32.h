#pragma once

#include <string>
#include <Windows.h>
#include "Game.h"


class WindowWin32
{
public:
	static const TCHAR AppWindowClass[];

public:
	WindowWin32(const GameDesc& Desc);
	~WindowWin32();

	bool IsClosed() const;
	int GetWidth() const { return m_GameDesc.Width; }
	int GetHeight() const { return m_GameDesc.Height; }
	HWND GetWindowHandle() const { return m_hwnd; }


private:
	static LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	
private:
	HWND m_hwnd;
	bool m_bIsClosed;
	GameDesc m_GameDesc;
};

