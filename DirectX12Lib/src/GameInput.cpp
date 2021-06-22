#include "GameInput.h"
#include "Common.h"

namespace 
{
	EKeyState s_KeyStates[255];
	bool s_MouseMoving;
	int s_MousePreX, s_MousePreY;
	int s_MouseCurrentX, s_MouseCurrentY;

	float s_MouseZoom;
}

void GameInput::Initialize()
{
	s_MouseZoom = 0.f;
	s_MouseMoving = false;
	ZeroMemory(s_KeyStates, sizeof(s_KeyStates));
}

void GameInput::Reset()
{
	GameInput::Initialize();
}

bool GameInput::IsKeyUp(int KeyCode)
{
	return s_KeyStates[KeyCode] == EKeyState::Up;
}

bool GameInput::IsKeyDown(int KeyCode)
{
	return s_KeyStates[KeyCode] == EKeyState::Down;
}

void GameInput::SetKeyState(int KeyCode, EKeyState State)
{
	s_KeyStates[KeyCode] = State;
}

void GameInput::MouseStart(int MouseX, int MouseY)
{
	s_MouseMoving = true;
	s_MousePreX = s_MouseCurrentX = MouseX;
	s_MousePreY = s_MouseCurrentY = MouseY;
	//printf("MouseStart %d %d\n", MouseX, MouseY);
}

void GameInput::MouseStop()
{
	s_MouseMoving = false;
}

void GameInput::MouseMove(int MouseX, int MouseY)
{
	if (s_MouseMoving)
	{
		s_MouseCurrentX = MouseX;
		s_MouseCurrentY = MouseY;
	}
}

void GameInput::MouseZoom(int ZoomValue)
{
	s_MouseZoom = (float)ZoomValue;
}

float GameInput::ConsumeMouseZoom()
{
	float Result = s_MouseZoom;
	s_MouseZoom = 0.f;
	return Result;
}

Vector2i GameInput::GetMoveDelta()
{
	Vector2i Result = Vector2i(s_MouseCurrentX - s_MousePreX, s_MouseCurrentY - s_MousePreY);
	return Result;
}

void GameInput::MouseMoveProcessed()
{
	s_MousePreX = s_MouseCurrentX;
	s_MousePreY = s_MouseCurrentY;
}

