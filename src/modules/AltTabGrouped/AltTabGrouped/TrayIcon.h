#pragma once

#include <Windows.h>
#include <shellapi.h>

// A minimal notification-area (system tray) icon used only when the switcher
// runs as a standalone app (outside the PowerToys runner). It gives the user a
// way to quit gracefully, since the app otherwise silently owns Alt+Tab.
class TrayIcon
{
public:
    bool Create(HINSTANCE hinstance, DWORD mainThreadId);
    ~TrayIcon();

private:
    static LRESULT CALLBACK WndProcStatic(HWND, UINT, WPARAM, LPARAM);
    LRESULT WndProc(HWND, UINT, WPARAM, LPARAM);
    void ShowMenu();

    HINSTANCE m_hinstance = nullptr;
    DWORD m_mainThreadId = 0;
    HWND m_hwnd = nullptr;
    NOTIFYICONDATAW m_nid{};
};
