#pragma once
#include "WindowWin32.h"


class D3D12RHI;

class ApplicationWin32
{
public:
	ApplicationWin32(const WindowDesc& Desc);
	virtual ~ApplicationWin32();

	void Run();

	virtual void OnStartup();
	virtual void OnUpdate();
	virtual void OnShutdown();

private:
	D3D12RHI* m_rhi;
	WindowWin32* m_window;
	WindowDesc m_windowDesc;
};

