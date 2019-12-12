#pragma once

#include <wrl.h>
#include <exception>
#include <system_error>

using namespace Microsoft::WRL;

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		__debugbreak();
		//throw std::system_error(hr, std::system_category());
	}
}

void DoAssert(bool success, const wchar_t* file_name, int line);

#define WIDE2(x) L##x
#define WIDE1(x) WIDE2(x)
#define WFILE WIDE1(__FILE__)

#ifdef _DEBUG
#define Assert(s) DoAssert(s, WFILE, __LINE__)
#else
#define Assert(s)
#endif