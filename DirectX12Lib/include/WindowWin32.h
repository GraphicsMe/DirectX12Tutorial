#pragma once

#include <string>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

class FGame;

class WindowWin32
{
public:
	static const TCHAR AppWindowClass[];

private:
	WindowWin32();

public:
	bool Initialize(FGame* Game);
	~WindowWin32();

	static WindowWin32& Get();
	void Destroy();

	int GetWidth() const { return Width; }
	int GetHeight() const { return Height; }
	HWND GetWindowHandle() const { return m_hwnd; }


private:
	static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	HWND m_hwnd;
	int Width, Height;
};

