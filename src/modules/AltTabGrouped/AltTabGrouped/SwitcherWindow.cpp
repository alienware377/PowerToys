#include "pch.h"
#include "SwitcherWindow.h"

#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

namespace
{
    const wchar_t WindowClassName[] = L"PowerToys_AltTabGrouped_Switcher";

    constexpr int Pad = 16;
    constexpr int GroupRowH = 56;
    constexpr int GroupIcon = 40;
    constexpr int WindowRowH = 36;
    constexpr int WindowIcon = 22;
    constexpr int WindowIndent = 56;
    constexpr int MinWidth = 380;
    constexpr int MaxWidth = 620;
    constexpr int Corner = 16;

    int ClampWidth(int w) { return w < MinWidth ? MinWidth : (w > MaxWidth ? MaxWidth : w); }
}

SwitcherWindow::SwitcherWindow() = default;

SwitcherWindow::~SwitcherWindow()
{
    if (m_hwnd)
    {
        DestroyWindow(m_hwnd);
    }
}

bool SwitcherWindow::Initialize(HINSTANCE hinstance)
{
    m_hinstance = hinstance;

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = WndProcStatic;
    wc.hInstance = hinstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = WindowClassName;
    wc.hbrBackground = nullptr;
    RegisterClassExW(&wc);

    m_hwnd = CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        WindowClassName,
        L"",
        WS_POPUP,
        0, 0, 10, 10,
        nullptr, nullptr, hinstance, this);

    return m_hwnd != nullptr;
}

LRESULT CALLBACK SwitcherWindow::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    SwitcherWindow* self = nullptr;
    if (msg == WM_NCCREATE)
    {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = reinterpret_cast<SwitcherWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else
    {
        self = reinterpret_cast<SwitcherWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self)
    {
        return self->WndProc(hwnd, msg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT SwitcherWindow::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        Render();
        EndPaint(hwnd, &ps);
        return 0;
    }
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void SwitcherWindow::Show(std::vector<AppGroup> groups, bool initialAdvance)
{
    m_groups = std::move(groups);
    m_selectedGroup = 0;
    m_selectedWindow = 0;
    m_expanded = false;

    if (m_groups.empty())
    {
        return;
    }

    if (initialAdvance && m_groups.size() > 1)
    {
        m_selectedGroup = 1;
    }

    Layout();

    // Center on the monitor that currently has the foreground window.
    HMONITOR mon = MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    const int areaW = mi.rcWork.right - mi.rcWork.left;
    const int areaH = mi.rcWork.bottom - mi.rcWork.top;
    const int x = mi.rcWork.left + (areaW - m_width) / 2;
    const int y = mi.rcWork.top + (areaH - m_height) / 2;

    SetWindowPos(m_hwnd, HWND_TOPMOST, x, y, m_width, m_height, SWP_NOACTIVATE | SWP_SHOWWINDOW);

    // Rounded corners.
    HRGN rgn = CreateRoundRectRgn(0, 0, m_width + 1, m_height + 1, Corner, Corner);
    SetWindowRgn(m_hwnd, rgn, TRUE);

    m_visible = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
    UpdateWindow(m_hwnd);
}

void SwitcherWindow::Hide()
{
    if (m_hwnd)
    {
        ShowWindow(m_hwnd, SW_HIDE);
    }
    m_visible = false;
    m_groups.clear();
}

void SwitcherWindow::Layout()
{
    // Fixed width for now; titles ellipsize to fit. (A future pass can measure
    // the widest label and grow up to MaxWidth.)
    m_width = ClampWidth(MinWidth);

    int rows = static_cast<int>(m_groups.size()) * GroupRowH;
    if (m_expanded && m_selectedGroup >= 0 && m_selectedGroup < static_cast<int>(m_groups.size()))
    {
        rows += static_cast<int>(m_groups[m_selectedGroup].windows.size()) * WindowRowH;
    }
    m_height = Pad * 2 + rows;
}

void SwitcherWindow::NextGroup()
{
    if (m_groups.empty())
    {
        return;
    }
    m_expanded = false;
    m_selectedGroup = (m_selectedGroup + 1) % static_cast<int>(m_groups.size());
    m_selectedWindow = 0;
    Layout();
    SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, m_width, m_height, SWP_NOACTIVATE | SWP_NOMOVE);
    SetWindowRgn(m_hwnd, CreateRoundRectRgn(0, 0, m_width + 1, m_height + 1, Corner, Corner), TRUE);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void SwitcherWindow::PrevGroup()
{
    if (m_groups.empty())
    {
        return;
    }
    m_expanded = false;
    const int n = static_cast<int>(m_groups.size());
    m_selectedGroup = (m_selectedGroup - 1 + n) % n;
    m_selectedWindow = 0;
    Layout();
    SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, m_width, m_height, SWP_NOACTIVATE | SWP_NOMOVE);
    SetWindowRgn(m_hwnd, CreateRoundRectRgn(0, 0, m_width + 1, m_height + 1, Corner, Corner), TRUE);
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void SwitcherWindow::ExpandFocusedGroup()
{
    if (m_groups.empty())
    {
        return;
    }
    if (!m_expanded)
    {
        m_expanded = true;
        m_selectedWindow = 0;
        Layout();
        SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, m_width, m_height, SWP_NOACTIVATE | SWP_NOMOVE);
        SetWindowRgn(m_hwnd, CreateRoundRectRgn(0, 0, m_width + 1, m_height + 1, Corner, Corner), TRUE);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void SwitcherWindow::CollapseFocusedGroup()
{
    if (m_expanded)
    {
        m_expanded = false;
        Layout();
        SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, m_width, m_height, SWP_NOACTIVATE | SWP_NOMOVE);
        SetWindowRgn(m_hwnd, CreateRoundRectRgn(0, 0, m_width + 1, m_height + 1, Corner, Corner), TRUE);
        InvalidateRect(m_hwnd, nullptr, FALSE);
    }
}

void SwitcherWindow::NextWindow()
{
    if (m_groups.empty())
    {
        return;
    }
    ExpandFocusedGroup();
    const auto& windows = m_groups[m_selectedGroup].windows;
    if (windows.empty())
    {
        return;
    }
    m_selectedWindow = (m_selectedWindow + 1) % static_cast<int>(windows.size());
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void SwitcherWindow::PrevWindow()
{
    if (m_groups.empty())
    {
        return;
    }
    ExpandFocusedGroup();
    const auto& windows = m_groups[m_selectedGroup].windows;
    if (windows.empty())
    {
        return;
    }
    const int n = static_cast<int>(windows.size());
    m_selectedWindow = (m_selectedWindow - 1 + n) % n;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

HWND SwitcherWindow::SelectedTarget() const
{
    if (m_groups.empty() || m_selectedGroup < 0 || m_selectedGroup >= static_cast<int>(m_groups.size()))
    {
        return nullptr;
    }
    const auto& group = m_groups[m_selectedGroup];
    if (group.windows.empty())
    {
        return nullptr;
    }
    if (m_expanded && m_selectedWindow >= 0 && m_selectedWindow < static_cast<int>(group.windows.size()))
    {
        return group.windows[m_selectedWindow].hwnd;
    }
    return group.windows.front().hwnd; // most-recently-used window of the group
}

void SwitcherWindow::Render()
{
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    const int w = rc.right - rc.left;
    const int h = rc.bottom - rc.top;
    if (w <= 0 || h <= 0)
    {
        return;
    }

    HDC screenDc = GetDC(m_hwnd);
    HDC dc = CreateCompatibleDC(screenDc);
    HBITMAP bmp = CreateCompatibleBitmap(screenDc, w, h);
    HBITMAP oldBmp = static_cast<HBITMAP>(SelectObject(dc, bmp));

    {
        Graphics g(dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

        SolidBrush background(Color(244, 32, 32, 36));
        g.FillRectangle(&background, 0, 0, w, h);

        FontFamily family(L"Segoe UI");
        Font groupFont(&family, 13, FontStyleBold, UnitPixel);
        Font subFont(&family, 11, FontStyleRegular, UnitPixel);
        Font countFont(&family, 10, FontStyleRegular, UnitPixel);
        SolidBrush textBrush(Color(255, 240, 240, 240));
        SolidBrush dimBrush(Color(255, 170, 170, 175));
        SolidBrush accentBrush(Color(90, 0, 120, 215));
        SolidBrush windowSelBrush(Color(150, 0, 120, 215));
        StringFormat fmt;
        fmt.SetLineAlignment(StringAlignmentCenter);
        fmt.SetTrimming(StringTrimmingEllipsisCharacter);
        fmt.SetFormatFlags(StringFormatFlagsNoWrap);

        int y = Pad;
        for (int gi = 0; gi < static_cast<int>(m_groups.size()); ++gi)
        {
            const AppGroup& group = m_groups[gi];
            const bool groupSelected = (gi == m_selectedGroup);

            if (groupSelected)
            {
                g.FillRectangle(&accentBrush, Pad - 6, y, w - 2 * (Pad - 6), GroupRowH - 6);
            }

            if (group.icon)
            {
                DrawIconEx(dc, Pad, y + (GroupRowH - GroupIcon) / 2, group.icon, GroupIcon, GroupIcon, 0, nullptr, DI_NORMAL);
            }

            RectF nameRect(static_cast<REAL>(Pad + GroupIcon + 12),
                           static_cast<REAL>(y),
                           static_cast<REAL>(w - (Pad + GroupIcon + 12) - Pad - 40),
                           static_cast<REAL>(GroupRowH));
            g.DrawString(group.name.c_str(), -1, &groupFont, nameRect, &fmt, &textBrush);

            wchar_t count[16];
            swprintf_s(count, L"%zu", group.windows.size());
            RectF countRect(static_cast<REAL>(w - Pad - 36), static_cast<REAL>(y), 30.0f, static_cast<REAL>(GroupRowH));
            // GdiplusStringFormat's copy ctor is protected, so build a fresh one.
            StringFormat rightFmt;
            rightFmt.SetLineAlignment(StringAlignmentCenter);
            rightFmt.SetAlignment(StringAlignmentFar);
            rightFmt.SetFormatFlags(StringFormatFlagsNoWrap);
            g.DrawString(count, -1, &countFont, countRect, &rightFmt, &dimBrush);

            y += GroupRowH;

            if (m_expanded && groupSelected)
            {
                for (int wi = 0; wi < static_cast<int>(group.windows.size()); ++wi)
                {
                    const WindowInfo& win = group.windows[wi];
                    if (wi == m_selectedWindow)
                    {
                        g.FillRectangle(&windowSelBrush, WindowIndent - 4, y, w - WindowIndent - Pad + 4, WindowRowH - 4);
                    }
                    if (win.icon)
                    {
                        DrawIconEx(dc, WindowIndent, y + (WindowRowH - WindowIcon) / 2, win.icon, WindowIcon, WindowIcon, 0, nullptr, DI_NORMAL);
                    }
                    RectF titleRect(static_cast<REAL>(WindowIndent + WindowIcon + 10),
                                    static_cast<REAL>(y),
                                    static_cast<REAL>(w - (WindowIndent + WindowIcon + 10) - Pad),
                                    static_cast<REAL>(WindowRowH));
                    g.DrawString(win.title.c_str(), -1, &subFont, titleRect, &fmt, &textBrush);
                    y += WindowRowH;
                }
            }
        }
    }

    BitBlt(screenDc, 0, 0, w, h, dc, 0, 0, SRCCOPY);

    SelectObject(dc, oldBmp);
    DeleteObject(bmp);
    DeleteDC(dc);
    ReleaseDC(m_hwnd, screenDc);
}
