#pragma once

#include <string>

class FCamera;


struct GameDesc
{
	std::wstring Caption = L"The Practice of Direct3D 12 Programming";
	int Width = 1024;
	int Height = 768;
};


class FGame
{
public:
	FGame(const GameDesc& Desc);
	virtual ~FGame();

	const GameDesc& GetDesc() const { return m_GameDesc; }

	virtual void OnStartup();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnShutdown();

	virtual void OnKeyDown(uint8_t Key) {}
	virtual void OnKeyUp(uint8_t Key) {}

	int GetWidth() const { return m_GameDesc.Width; }
	int GetHeight() const { return m_GameDesc.Height; }
	std::wstring GetWindowTitle() const { return m_GameDesc.Caption; }

protected:
	FCamera* m_Camera = nullptr;
	GameDesc m_GameDesc;
};

