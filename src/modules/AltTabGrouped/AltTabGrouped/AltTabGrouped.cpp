#include "pch.h"
#include "AltTabGrouped.h"
#include "WindowEnumerator.h"
#include "AppGrouping.h"
#include "trace.h"

#include <common/logger/logger.h>

namespace
{
    const wchar_t ControlWindowClassName[] = L"PowerToys_AltTabGrouped_Control";

    // Control messages posted from the keyboard hook to the control window.
    enum ControlMessage : UINT
    {
        WM_ATG_TAB = WM_APP + 1, // wParam: shift held
        WM_ATG_NAV_GROUP, // wParam: forward
        WM_ATG_NAV_WINDOW, // wParam: forward
        WM_ATG_EXPAND,
        WM_ATG_COMMIT,
        WM_ATG_CANCEL,
    };

    bool ShiftDown()
    {
        return (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    }
}

AltTabGrouped* AltTabGrouped::s_instance = nullptr;

AltTabGrouped::AltTabGrouped(HINSTANCE hinstance, DWORD mainThreadId) :
    m_hinstance(hinstance), m_mainThreadId(mainThreadId)
{
    s_instance = this;

    // Hidden control window that marshals work off the hook callback.
    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = ControlWndProcStatic;
    wc.hInstance = hinstance;
    wc.lpszClassName = ControlWindowClassName;
    RegisterClassExW(&wc);
    m_controlWnd = CreateWindowExW(0, ControlWindowClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, hinstance, this);

    m_switcher.Initialize(hinstance);

    m_keyboardHook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardHookProc, hinstance, 0);
    if (!m_keyboardHook)
    {
        Logger::error(L"Failed to install the low-level keyboard hook for AltTabGrouped");
    }
    else
    {
        Logger::info(L"AltTabGrouped keyboard hook installed");
    }
}

AltTabGrouped::~AltTabGrouped()
{
    if (m_keyboardHook)
    {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = nullptr;
    }
    if (m_controlWnd)
    {
        DestroyWindow(m_controlWnd);
        m_controlWnd = nullptr;
    }
    s_instance = nullptr;
}

LRESULT CALLBACK AltTabGrouped::KeyboardHookProc(int code, WPARAM wParam, LPARAM lParam)
{
    if (code == HC_ACTION && s_instance)
    {
        const auto* info = reinterpret_cast<const KBDLLHOOKSTRUCT*>(lParam);
        if (s_instance->HandleKey(wParam, *info))
        {
            return 1; // swallow: the shell never sees this key
        }
    }
    return CallNextHookEx(nullptr, code, wParam, lParam);
}

bool AltTabGrouped::HandleKey(WPARAM message, const KBDLLHOOKSTRUCT& info)
{
    const bool keyDown = (message == WM_KEYDOWN || message == WM_SYSKEYDOWN);
    const bool keyUp = (message == WM_KEYUP || message == WM_SYSKEYUP);
    const bool altDown = (info.flags & LLKHF_ALTDOWN) != 0;

    // Releasing Alt commits the current selection.
    if (keyUp && (info.vkCode == VK_LMENU || info.vkCode == VK_RMENU))
    {
        if (m_active)
        {
            PostMessageW(m_controlWnd, WM_ATG_COMMIT, 0, 0);
        }
        return false; // let Alt-up flow normally
    }

    if (!keyDown)
    {
        return false;
    }

    // Alt+Tab starts or advances the switcher.
    if (info.vkCode == VK_TAB && (altDown || m_active))
    {
        PostMessageW(m_controlWnd, WM_ATG_TAB, ShiftDown() ? 1 : 0, 0);
        return true;
    }

    if (!m_active)
    {
        return false;
    }

    // While the switcher is up, these keys drive the two-level navigation.
    switch (info.vkCode)
    {
    case VK_OEM_3: // the ` / ~ key above Tab: cycle within the focused app
    case VK_DOWN:
        PostMessageW(m_controlWnd, WM_ATG_NAV_WINDOW, ShiftDown() ? 0 : 1, 0);
        return true;
    case VK_UP:
        PostMessageW(m_controlWnd, WM_ATG_NAV_WINDOW, 0, 0);
        return true;
    case VK_RIGHT:
        PostMessageW(m_controlWnd, WM_ATG_NAV_GROUP, 1, 0);
        return true;
    case VK_LEFT:
        PostMessageW(m_controlWnd, WM_ATG_NAV_GROUP, 0, 0);
        return true;
    case VK_RETURN:
        PostMessageW(m_controlWnd, WM_ATG_COMMIT, 0, 0);
        return true;
    case VK_ESCAPE:
        PostMessageW(m_controlWnd, WM_ATG_CANCEL, 0, 0);
        return true;
    default:
        return false;
    }
}

LRESULT CALLBACK AltTabGrouped::ControlWndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* self = (msg == WM_NCCREATE)
                     ? reinterpret_cast<AltTabGrouped*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams)
                     : reinterpret_cast<AltTabGrouped*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE)
    {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self)
    {
        return self->ControlWndProc(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT AltTabGrouped::ControlWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_ATG_TAB:
        if (!m_active)
        {
            StartSwitcher(wParam != 0);
        }
        else if (wParam != 0)
        {
            m_switcher.PrevGroup();
        }
        else
        {
            m_switcher.NextGroup();
        }
        return 0;

    case WM_ATG_NAV_GROUP:
        if (m_active)
        {
            wParam != 0 ? m_switcher.NextGroup() : m_switcher.PrevGroup();
        }
        return 0;

    case WM_ATG_NAV_WINDOW:
        if (m_active)
        {
            wParam != 0 ? m_switcher.NextWindow() : m_switcher.PrevWindow();
        }
        return 0;

    case WM_ATG_EXPAND:
        if (m_active)
        {
            m_switcher.ExpandFocusedGroup();
        }
        return 0;

    case WM_ATG_COMMIT:
        Commit();
        return 0;

    case WM_ATG_CANCEL:
        Cancel();
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void AltTabGrouped::StartSwitcher(bool shift)
{
    auto windows = WindowEnumerator::Enumerate();
    if (windows.empty())
    {
        return;
    }
    auto groups = AppGrouping::Group(std::move(windows));
    if (groups.empty())
    {
        return;
    }

    m_cancelled = false;
    m_active = true;
    Trace::AltTabGrouped::Invoked(groups.size());
    m_switcher.Show(std::move(groups), /*initialAdvance*/ !shift);
}

void AltTabGrouped::Commit()
{
    if (!m_active)
    {
        return;
    }
    HWND target = m_cancelled ? nullptr : m_switcher.SelectedTarget();
    m_switcher.Hide();
    m_active = false;
    m_cancelled = false;

    if (target && IsWindow(target))
    {
        ActivateWindow(target);
    }
}

void AltTabGrouped::Cancel()
{
    m_cancelled = true;
    Commit();
}

// Robustly bring a window to the foreground, working around the OS foreground
// lock by briefly attaching to the currently-foreground thread's input queue.
void AltTabGrouped::ActivateWindow(HWND hwnd)
{
    if (IsIconic(hwnd))
    {
        ShowWindow(hwnd, SW_RESTORE);
    }

    HWND foreground = GetForegroundWindow();
    DWORD foregroundThread = GetWindowThreadProcessId(foreground, nullptr);
    DWORD thisThread = GetCurrentThreadId();
    DWORD targetThread = GetWindowThreadProcessId(hwnd, nullptr);

    if (foregroundThread != thisThread)
    {
        AttachThreadInput(thisThread, foregroundThread, TRUE);
    }
    if (targetThread != thisThread && targetThread != foregroundThread)
    {
        AttachThreadInput(thisThread, targetThread, TRUE);
    }

    AllowSetForegroundWindow(ASFW_ANY);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);

    if (targetThread != thisThread && targetThread != foregroundThread)
    {
        AttachThreadInput(thisThread, targetThread, FALSE);
    }
    if (foregroundThread != thisThread)
    {
        AttachThreadInput(thisThread, foregroundThread, FALSE);
    }
}
