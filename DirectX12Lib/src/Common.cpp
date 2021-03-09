#pragma once

#include "Common.h"
#include <wchar.h>

#define MAX_STRING_LEN 512

void DoAssert(bool success, const wchar_t* file_name, int line)
{
	if (success)
	{
		return;
	}

	static wchar_t assert_str[MAX_STRING_LEN];
	static wchar_t module_path[MAX_PATH];
	::GetModuleFileName(NULL, module_path, MAX_PATH);
	swprintf(assert_str, MAX_STRING_LEN, L"Debug Assertion Failed!  \n\nModule: %s \nFile: %s \n Line: %d \n\nTerminate or not? press Cancel to enter debuger.\n",
		module_path, file_name, line);
	int result = ::MessageBox(NULL, assert_str, L"DirectX 12 Tutorial", MB_ICONSTOP | MB_YESNOCANCEL);
	if (result == IDYES)
	{
		::ExitProcess(3);
	}
	else if (result == IDCANCEL)
	{
		__debugbreak();
	}
}
