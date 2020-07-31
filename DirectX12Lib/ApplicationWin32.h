#pragma once
#include "WindowWin32.h"


class FGame;
class D3D12RHI;

class ApplicationWin32
{
public:
	static ApplicationWin32& Get();

	void Run(FGame* game);

	
private:
	//ApplicationWin32() = default;
	bool Initialize(FGame* game);
	void Terminate();
};

