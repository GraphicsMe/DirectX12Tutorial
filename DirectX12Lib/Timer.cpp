#include "Timer.h"
#include <Windows.h>

double FTimer::SecondsPerCycle = 0.0;

double FTimer::InitTiming()
{
	LARGE_INTEGER Frequency;
	QueryPerformanceFrequency(&Frequency);

	SecondsPerCycle = 1.0 / Frequency.QuadPart;

	return GetSeconds();
}

double FTimer::GetSeconds()
{
	LARGE_INTEGER Cycles;
	QueryPerformanceCounter(&Cycles);

	return Cycles.QuadPart * GetSecondsPerCycle() + 16777216.0;
}

