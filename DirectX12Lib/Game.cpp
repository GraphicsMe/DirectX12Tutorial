#include "Game.h"
#include "ApplicationWin32.h"

#include <iostream>

Game::Game(const GameDesc& Desc)
	: m_GameDesc(Desc)
{
}

Game::~Game()
{
}

void Game::OnStartup()
{
	std::cout << "OnStartup" << std::endl;
}

void Game::OnUpdate()
{
	ApplicationWin32::Get().Render();
}

void Game::OnShutdown()
{
	std::cout << "OnShutdown" << std::endl;
}
