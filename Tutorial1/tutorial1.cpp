#include "ApplicationWin32.h"
#include "Game.h"


int main()
{
	GameDesc Desc;
	Desc.Caption = L"Tutorial 1 - Create A Window";
	Game tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	return 0;
}