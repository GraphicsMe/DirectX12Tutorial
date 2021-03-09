#include "ApplicationWin32.h"
#include "Game.h"
#include "D3D12RHI.h"
#include "Timer.h"
#include <iostream>


bool ApplicationWin32::Initialize(FGame* game)
{
	WindowWin32::Get().Initialize(game);
	D3D12RHI::Get().Initialize();
	return true;
}

void ApplicationWin32::Terminate()
{
	D3D12RHI::Get().Destroy();
	WindowWin32::Get().Destroy();
}

ApplicationWin32& ApplicationWin32::ApplicationWin32::Get()
{
	static ApplicationWin32 Singleton;
	return Singleton;
}

void ApplicationWin32::Run(FGame* game)
{
	FTimer::InitTiming();

	if (!Initialize(game))
	{
		return;
	}

	game->OnStartup();

	MSG msg = {};
	do
	{
		bool GotMessage = ::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
		if (GotMessage)
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}

		game->OnUpdate();
		game->OnRender();
	}while(msg.message != WM_QUIT);

	game->OnShutdown();

	this->Terminate();
}