#pragma once

#include <string>
#include <Windows.h>
#include "Game.h"


class WindowWin32
{
public:
	static const TCHAR AppWindowClass[];

private:
	WindowWin32();

public:
	bool Initialize(const GameDesc& Desc);
	~WindowWin32();

	static WindowWin32& Get();
	void Destroy();

	bool IsClosed() const;
	int GetWidth() const { return m_GameDesc.Width; }
	int GetHeight() const { return m_GameDesc.Height; }
	HWND GetWindowHandle() const { return m_hwnd; }
	void Show();


private:
	static LRESULT WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	LRESULT HandleMessage(UINT uMsg, WPARAM wParam, LPARAM lParam);
	
private:
	HWND m_hwnd;
	bool m_bIsClosed;
	GameDesc m_GameDesc;
};

