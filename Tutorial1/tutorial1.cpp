#include "ApplicationWin32.h"
#include "Game.h"


int main()
{
	GameDesc Desc;
	Desc.Caption = L"Tutorial 1 - Create A Window";
	FGame tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	return 0;
}