#include "ApplicationWin32.h"
#include "Game.h"

class Tutorial1 : public Game
{
public:
	Tutorial1(const GameDesc& Desc) : Game(Desc) {}
};

int main()
{
	GameDesc Desc;
	Desc.Caption = L"Tutorial 1 - Create A Window";
	Tutorial1 tutorial(Desc);
	ApplicationWin32::Get().Run(&tutorial);
	return 0;
}