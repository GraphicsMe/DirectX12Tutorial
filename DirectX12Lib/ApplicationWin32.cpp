#include "ApplicationWin32.h"
#include "Game.h"
#include "D3D12RHI.h"
#include <iostream>


bool ApplicationWin32::Initialize(Game* game)
{
	WindowWin32::Get().Initialize(game->GetDesc());
	D3D12RHI::Get().Initialize();
	return true;
}

ApplicationWin32::~ApplicationWin32()
{
	D3D12RHI::Get().Destroy();
	WindowWin32::Get().Destroy();
}

ApplicationWin32& ApplicationWin32::ApplicationWin32::Get()
{
	static ApplicationWin32 Singleton;
	return Singleton;
}

void ApplicationWin32::Run(Game* game)
{
	Assert(game != nullptr);

	if (!Initialize(game))
	{
		return;
	}

	game->OnStartup();
	game->LoadContent();

	WindowWin32& Window = WindowWin32::Get();
	Window.Show();

	while (!Window.IsClosed())
	{
		MSG msg = {};
		while(::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}

		game->OnUpdate();
		game->OnRender();
	}
	game->OnShutdown();
}