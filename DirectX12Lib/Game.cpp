#include "Game.h"
#include "ApplicationWin32.h"

#include <iostream>

FGame::FGame(const GameDesc& Desc)
	: m_GameDesc(Desc)
{
}

FGame::~FGame()
{
}

void FGame::OnStartup()
{
	std::cout << "OnStartup" << std::endl;
}

void FGame::OnUpdate()
{
}

void FGame::OnRender()
{
}

void FGame::OnShutdown()
{
	std::cout << "OnShutdown" << std::endl;
}
