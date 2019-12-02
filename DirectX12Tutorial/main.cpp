#include "ApplicationWin32.h"


int main()
{
	WindowDesc Desc;
	Desc.Caption = L"Tutorial 1 - Create A Window";
	ApplicationWin32 App(Desc);
	App.Run();
	return 0;
}