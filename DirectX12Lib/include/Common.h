#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <wrl.h>
#include <exception>
#include <system_error>
#include <stdint.h>

#include <d3d12.h>

#define D3D12_GPU_VIRTUAL_ADDRESS_NULL      ((D3D12_GPU_VIRTUAL_ADDRESS)0)
#define D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN   ((D3D12_GPU_VIRTUAL_ADDRESS)-1)
#define D3D12_CPU_VIRTUAL_ADDRESS_UNKNOWN	((SIZE_T)-1)

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

inline std::wstring ToWideString(const std::string& str)
{
	int stringLength = ::MultiByteToWideChar(CP_ACP, 0, str.data(), (int)str.length(), 0, 0);
	std::wstring wstr(stringLength, 0);
	::MultiByteToWideChar(CP_ACP, 0, str.data(), (int)str.length(), &wstr[0], stringLength);
	return wstr;
}