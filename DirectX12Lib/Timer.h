#pragma once

class FTimer
{
public:
	static double InitTiming();

	static double GetSeconds();

	static double GetSecondsPerCycle() { return SecondsPerCycle; }

protected:
	static double SecondsPerCycle;
};