#pragma once

#include "SwitcherWindow.h"

#include <Windows.h>

// The core of the grouped Alt+Tab switcher. It installs a low-level keyboard
// hook to intercept Alt+Tab before the shell sees it, drives a two-level
// selection (apps, then the focused app's windows) through a control window,
// and activates the chosen window when Alt is released.
//
// The hook callback itself only decides whether to swallow a key and posts the
// heavy work (window enumeration, grouping, rendering) to the control window so
// it never blocks long enough for Windows to silently drop the hook.
class AltTabGrouped
{
public:
    AltTabGrouped(HINSTANCE hinstance, DWORD mainThreadId);
    ~AltTabGrouped();

    AltTabGrouped(const AltTabGrouped&) = delete;
    AltTabGrouped& operator=(const AltTabGrouped&) = delete;

private:
    static LRESULT CALLBACK KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam);
    bool HandleKey(WPARAM message, const KBDLLHOOKSTRUCT& info); // returns true to swallow

    static LRESULT CALLBACK ControlWndProcStatic(HWND, UINT, WPARAM, LPARAM);
    LRESULT ControlWndProc(HWND, UINT, WPARAM, LPARAM);

    void StartSwitcher(bool shift);
    void Commit();
    void Cancel();
    static void ActivateWindow(HWND hwnd);

    HINSTANCE m_hinstance = nullptr;
    DWORD m_mainThreadId = 0;
    HHOOK m_keyboardHook = nullptr;
    HWND m_controlWnd = nullptr;

    SwitcherWindow m_switcher;
    bool m_active = false; // switcher is currently shown
    bool m_cancelled = false;

    static AltTabGrouped* s_instance;
};
