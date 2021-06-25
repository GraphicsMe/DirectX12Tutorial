#include "ApplicationWin32.h"
#include "Game.h"
#include "GameInput.h"
#include "D3D12RHI.h"
#include "Timer.h"
#include "ImguiManager.h"
#include <iostream>


bool ApplicationWin32::Initialize(FGame* game)
{
	GameInput::Initialize();
	WindowWin32::Get().Initialize(game);
	D3D12RHI::Get().Initialize();
	ImguiManager::Get().Initialize();

	return true;
}

void ApplicationWin32::Terminate()
{
	ImguiManager::Get().Destroy();
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