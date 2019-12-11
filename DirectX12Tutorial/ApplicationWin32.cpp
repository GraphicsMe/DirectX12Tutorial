#include "ApplicationWin32.h"
#include "D3D12RHI.h"
#include <iostream>


ApplicationWin32::ApplicationWin32(const WindowDesc& Desc)
	: m_windowDesc(Desc)
{
	
}

ApplicationWin32::~ApplicationWin32()
{
	delete m_window;
	std::cout << "~Win32Application" << std::endl;
}

void ApplicationWin32::Run()
{
	m_window = new WindowWin32(m_windowDesc);
	m_rhi = new D3D12RHI(m_window);

	this->OnStartup();

	while (!m_window->IsClosed())
	{
		MSG msg = {};
		while(::PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}

		this->OnUpdate();
	}
	this->OnShutdown();
}

void ApplicationWin32::OnStartup()
{
	std::cout << "OnStartup" << std::endl;
}

void ApplicationWin32::OnUpdate()
{
	m_rhi->Render();
}

void ApplicationWin32::OnShutdown()
{
	std::cout << "OnShutdown" << std::endl;
}
