#pragma once
#include "WindowWin32.h"


class Game;
class D3D12RHI;

class ApplicationWin32
{
public:
	virtual ~ApplicationWin32();

	static ApplicationWin32& Get();

	void Run(Game* game);
	void Render();

private:
	ApplicationWin32();

private:
	D3D12RHI* m_rhi;
	WindowWin32* m_window;
};

