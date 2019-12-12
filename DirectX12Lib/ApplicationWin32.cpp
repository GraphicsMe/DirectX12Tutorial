#include "ApplicationWin32.h"
#include "Game.h"
#include "D3D12RHI.h"
#include <iostream>


ApplicationWin32::ApplicationWin32()
{
}

ApplicationWin32::~ApplicationWin32()
{
	delete m_rhi;
	delete m_window;
	std::cout << "~Win32Application" << std::endl;
}

ApplicationWin32& ApplicationWin32::ApplicationWin32::Get()
{
	static ApplicationWin32 Singleton;
	return Singleton;
}

void ApplicationWin32::Run(Game* game)
{
	Assert(game != nullptr);
	m_window = new WindowWin32(game->GetDesc());
	m_rhi = new D3D12RHI(m_window);

	game->OnStartup();

	while (!m_window->IsClosed())
	{
		MSG msg = {};
		while(::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}

		game->OnUpdate();
	}
	game->OnShutdown();
}

void ApplicationWin32::Render()
{
	m_rhi->Render();
}
