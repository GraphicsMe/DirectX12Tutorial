#pragma once
#include "WindowWin32.h"


class Game;
class D3D12RHI;

class ApplicationWin32
{
public:
	static ApplicationWin32& Get();

	void Run(Game* game);

	
private:
	//ApplicationWin32() = default;
	bool Initialize(Game* game);
	void Terminate();
};

