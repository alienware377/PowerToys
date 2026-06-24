#include "pch.h"
#include "WindowEnumerator.h"

#include <dwmapi.h>

// DWMWA_CLOAKED is only available when targeting Win8+; guard for older SDKs.
#ifndef DWMWA_CLOAKED
#define DWMWA_CLOAKED 14
#endif

namespace
{
    // Returns the "last visible active popup" of the owner chain, mirroring the
    // heuristic the shell uses to decide which window of an owner chain is the
    // one that belongs in Alt+Tab.
    HWND GetLastVisibleActivePopup(HWND hwnd)
    {
        HWND root = GetAncestor(hwnd, GA_ROOTOWNER);
        HWND lastPopup = root;
        HWND walk = root;
        while (walk)
        {
            HWND popup = GetLastActivePopup(walk);
            if (popup == walk)
            {
                break;
            }
            if (IsWindowVisible(popup))
            {
                lastPopup = popup;
                break;
            }
            walk = popup;
        }
        return lastPopup;
    }
}

bool WindowEnumerator::IsCloaked(HWND hwnd)
{
    int cloaked = 0;
    if (SUCCEEDED(DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked))))
    {
        return cloaked != 0;
    }
    return false;
}

bool WindowEnumerator::IsAltTabWindow(HWND hwnd)
{
    if (!IsWindowVisible(hwnd))
    {
        return false;
    }

    // Must have a non-empty title.
    if (GetWindowTextLengthW(hwnd) == 0)
    {
        return false;
    }

    // The window has to be the representative window of its owner chain.
    if (GetLastVisibleActivePopup(hwnd) != hwnd)
    {
        return false;
    }

    // Cloaked windows are on another virtual desktop or are suspended UWP apps.
    if (IsCloaked(hwnd))
    {
        return false;
    }

    const LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    // App windows are always shown; tool windows never are.
    if (exStyle & WS_EX_APPWINDOW)
    {
        return true;
    }
    if (exStyle & WS_EX_TOOLWINDOW)
    {
        return false;
    }

    // Skip windows owned by another window (dialogs etc.) unless they opted in
    // via WS_EX_APPWINDOW above.
    if (GetWindow(hwnd, GW_OWNER) != nullptr)
    {
        return false;
    }

    return true;
}

std::vector<WindowInfo> WindowEnumerator::Enumerate()
{
    std::vector<WindowInfo> result;

    EnumWindows(
        [](HWND hwnd, LPARAM lParam) -> BOOL {
            auto* out = reinterpret_cast<std::vector<WindowInfo>*>(lParam);
            if (!IsAltTabWindow(hwnd))
            {
                return TRUE;
            }

            WindowInfo info{};
            info.hwnd = hwnd;
            GetWindowThreadProcessId(hwnd, &info.processId);

            wchar_t title[512] = {};
            GetWindowTextW(hwnd, title, ARRAYSIZE(title));
            info.title = title;

            out->push_back(std::move(info));
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&result));

    return result;
}
