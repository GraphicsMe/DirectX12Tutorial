#pragma once

#include <string>

class FCamera;


struct GameDesc
{
	std::wstring Caption = L"The Practice of Direct3D 12 Programming";
	int Width = 1024;
	int Height = 768;
};


class Game
{
public:
	Game(const GameDesc& Desc);
	virtual ~Game();

	const GameDesc& GetDesc() const { return m_GameDesc; }

	virtual void OnStartup();
	virtual void LoadContent();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnShutdown();

private:
	FCamera* m_Camera;
	GameDesc m_GameDesc;
};

