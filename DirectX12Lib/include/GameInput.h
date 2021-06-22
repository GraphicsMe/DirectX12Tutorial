#pragma once

#include "MathLib.h"

enum EKeyState
{
	Up,
	Down,
};

namespace GameInput
{
	void Initialize();
	void Reset();
	bool IsKeyUp(int KeyCode);
	bool IsKeyDown(int KeyCode);
	void SetKeyState(int KeyCode, EKeyState State);

	void MouseStart(int MouseX, int MouseY);
	void MouseStop();
	void MouseMove(int MouseX, int MouseY);

	Vector2i GetMoveDelta();
	void MouseMoveProcessed();

	void MouseZoom(int ZoomValue);
	float ConsumeMouseZoom();
}