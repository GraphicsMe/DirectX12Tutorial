#include "Game.h"
#include "ApplicationWin32.h"

#include <iostream>

Game::Game(const GameDesc& Desc)
	: m_GameDesc(Desc)
	, m_Camera(nullptr)
{
}

Game::~Game()
{
}

void Game::OnStartup()
{
	std::cout << "OnStartup" << std::endl;
}

void Game::LoadContent()
{
}

void Game::OnUpdate()
{
}

void Game::OnRender()
{
}

void Game::OnShutdown()
{
	std::cout << "OnShutdown" << std::endl;
}
