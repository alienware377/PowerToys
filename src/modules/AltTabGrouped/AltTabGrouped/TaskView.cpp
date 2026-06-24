#include "pch.h"
#include "TaskView.h"
#include "WindowEnumerator.h"

#include <windowsx.h>
#include <climits>
#include <cstdlib>
#include <gdiplus.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")

using namespace Gdiplus;

namespace
{
    const wchar_t TaskViewClassName[] = L"PowerToys_AltTabGrouped_TaskView";

    constexpr int Margin = 56;
    constexpr int GridTop = 44;
    constexpr int TileW = 300;
    constexpr int TitleBar = 30;
    constexpr int TileGap = 22;
    constexpr int HeaderH = 38;
    constexpr int HeaderGapAbove = 16;
    constexpr int HeaderGapBelow = 6;
    constexpr int GroupGap = 30;
    constexpr int StripH = 156;
    constexpr int DeskW = 200;
    constexpr int DeskH = 112;
    constexpr int DeskGap = 18;
    constexpr int CloseSize = 22;

    int TileH() { return TitleBar + (TileW * 9) / 16; }

    void FillRound(Graphics& g, Brush& b, int x, int y, int w, int h, int r)
    {
        GraphicsPath path;
        path.AddArc(x, y, r, r, 180, 90);
        path.AddArc(x + w - r, y, r, r, 270, 90);
        path.AddArc(x + w - r, y + h - r, r, r, 0, 90);
        path.AddArc(x, y + h - r, r, r, 90, 90);
        path.CloseFigure();
        g.FillPath(&b, &path);
    }

    void DrawRoundBorder(Graphics& g, Pen& p, int x, int y, int w, int h, int r)
    {
        GraphicsPath path;
        path.AddArc(x, y, r, r, 180, 90);
        path.AddArc(x + w - r, y, r, r, 270, 90);
        path.AddArc(x + w - r, y + h - r, r, r, 0, 90);
        path.AddArc(x, y + h - r, r, r, 90, 90);
        path.CloseFigure();
        g.DrawPath(&p, &path);
    }
}

bool TaskView::Initialize(HINSTANCE hinstance)
{
    m_hinstance = hinstance;

    WNDCLASSEXW wc{ sizeof(wc) };
    wc.lpfnWndProc = WndProcStatic;
    wc.hInstance = hinstance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = TaskViewClassName;
    RegisterClassExW(&wc);

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED,
        TaskViewClassName, L"", WS_POPUP,
        0, 0, 100, 100, nullptr, nullptr, hinstance, this);
    if (!m_hwnd)
    {
        return false;
    }
    // Uniform translucency so the desktop shows faintly behind the grid.
    SetLayeredWindowAttributes(m_hwnd, 0, 238, LWA_ALPHA);
    return true;
}

LRESULT CALLBACK TaskView::WndProcStatic(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    auto* self = (msg == WM_NCCREATE)
                     ? reinterpret_cast<TaskView*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams)
                     : reinterpret_cast<TaskView*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE)
    {
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->WndProc(hwnd, msg, wParam, lParam) : DefWindowProcW(hwnd, msg, wParam, lParam);
}

void TaskView::Toggle()
{
    if (m_visible)
    {
        Close();
    }
    else
    {
        Open();
    }
}

void TaskView::Open()
{
    HMONITOR mon = MonitorFromWindow(GetForegroundWindow(), MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(mon, &mi);
    m_monitor = mi.rcMonitor;

    BuildModel();
    if (m_tiles.empty())
    {
        m_groups.clear();
        return;
    }

    m_scrollY = 0;
    m_selected = 0;
    m_hot = m_hotClose = m_hotDesktop = -1;

    const int mw = m_monitor.right - m_monitor.left;
    const int mh = m_monitor.bottom - m_monitor.top;
    SetWindowPos(m_hwnd, HWND_TOPMOST, m_monitor.left, m_monitor.top, mw, mh, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    Layout();

    ShowWindow(m_hwnd, SW_SHOW);

    // Force foreground so we receive keyboard input.
    DWORD fg = GetWindowThreadProcessId(GetForegroundWindow(), nullptr);
    DWORD me = GetCurrentThreadId();
    AttachThreadInput(me, fg, TRUE);
    SetForegroundWindow(m_hwnd);
    SetFocus(m_hwnd);
    AttachThreadInput(me, fg, FALSE);

    RegisterThumbnails();
    m_visible = true;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TaskView::Close()
{
    if (!m_visible)
    {
        return;
    }
    m_visible = false;
    UnregisterThumbnails();
    ShowWindow(m_hwnd, SW_HIDE);
    m_groups.clear();
    m_tiles.clear();
    m_headers.clear();
    m_desktops.clear();
}

void TaskView::BuildModel()
{
    m_groups = AppGrouping::Group(WindowEnumerator::Enumerate());

    m_tiles.clear();
    m_headers.clear();
    for (int gi = 0; gi < static_cast<int>(m_groups.size()); ++gi)
    {
        Header h{};
        h.name = m_groups[gi].name;
        h.icon = m_groups[gi].icon;
        h.count = m_groups[gi].windows.size();
        m_headers.push_back(h);

        for (const auto& win : m_groups[gi].windows)
        {
            Tile t{};
            t.hwnd = win.hwnd;
            t.title = win.title;
            t.icon = win.icon;
            t.group = gi;
            m_tiles.push_back(t);
        }
    }

    m_desktops.clear();
    int current = 0;
    auto desks = VirtualDesktops::Enumerate(current);
    m_currentDesktop = current;
    for (int i = 0; i < static_cast<int>(desks.size()); ++i)
    {
        m_desktops.push_back(DesktopTile{ desks[i].name, desks[i].isCurrent, false, i, {} });
    }
    m_desktops.push_back(DesktopTile{ L"New desktop", false, true, static_cast<int>(desks.size()), {} });
}

void TaskView::Layout()
{
    const int mw = m_monitor.right - m_monitor.left;
    const int mh = m_monitor.bottom - m_monitor.top;
    const int stripTop = mh - StripH;
    m_gridBottom = stripTop - 14;

    const int contentLeft = Margin;
    const int contentRight = mw - Margin;
    const int contentWidth = contentRight - contentLeft;
    const int cols = (contentWidth + TileGap) / (TileW + TileGap);
    const int columns = cols < 1 ? 1 : cols;
    const int tileH = TileH();

    int y = GridTop;
    size_t tileIdx = 0;
    for (int gi = 0; gi < static_cast<int>(m_groups.size()); ++gi)
    {
        y += HeaderGapAbove;
        m_headers[gi].rect = { contentLeft, y, contentRight, y + HeaderH };
        y += HeaderH + HeaderGapBelow;

        const int n = static_cast<int>(m_groups[gi].windows.size());
        for (int i = 0; i < n; ++i)
        {
            const int col = i % columns;
            const int row = i / columns;
            const int tx = contentLeft + col * (TileW + TileGap);
            const int ty = y + row * (tileH + TileGap);
            m_tiles[tileIdx].rect = { tx, ty, tx + TileW, ty + tileH };
            ++tileIdx;
        }
        const int rows = (n + columns - 1) / columns;
        y += rows * (tileH + TileGap) + GroupGap;
    }
    m_contentHeight = y;

    // Desktop strip (fixed, screen/client coords).
    const int total = static_cast<int>(m_desktops.size());
    const int stripWidth = total * DeskW + (total - 1) * DeskGap;
    int dx = (mw - stripWidth) / 2;
    if (dx < Margin)
    {
        dx = Margin;
    }
    const int dy = stripTop + (StripH - (DeskH + 26)) / 2;
    for (auto& d : m_desktops)
    {
        d.rect = { dx, dy, dx + DeskW, dy + DeskH };
        dx += DeskW + DeskGap;
    }

    ClampScroll();
}

void TaskView::ClampScroll()
{
    const int viewport = m_gridBottom - GridTop;
    const int maxScroll = (m_contentHeight - GridTop) > viewport ? (m_contentHeight - GridTop - viewport) : 0;
    if (m_scrollY < 0)
    {
        m_scrollY = 0;
    }
    if (m_scrollY > maxScroll)
    {
        m_scrollY = maxScroll;
    }
}

void TaskView::RegisterThumbnails()
{
    for (auto& t : m_tiles)
    {
        if (FAILED(DwmRegisterThumbnail(m_hwnd, t.hwnd, &t.thumb)))
        {
            t.thumb = nullptr;
        }
    }
    UpdateThumbnails();
}

void TaskView::UpdateThumbnails()
{
    for (auto& t : m_tiles)
    {
        if (!t.thumb)
        {
            continue;
        }
        const int top = t.rect.top - m_scrollY + TitleBar;
        const int bottom = t.rect.bottom - m_scrollY - 3;
        const int clientTop = t.rect.top - m_scrollY;
        const int clientBottom = t.rect.bottom - m_scrollY;
        const bool visible = clientTop >= GridTop - 1 && clientBottom <= m_gridBottom + 1;

        DWM_THUMBNAIL_PROPERTIES props{};
        props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE | DWM_TNP_OPACITY | DWM_TNP_SOURCECLIENTAREAONLY;
        props.fSourceClientAreaOnly = FALSE;
        props.opacity = 255;
        props.fVisible = visible ? TRUE : FALSE;
        props.rcDestination = { t.rect.left + 3, top, t.rect.right - 3, bottom };
        DwmUpdateThumbnailProperties(t.thumb, &props);
    }
}

void TaskView::UnregisterThumbnails()
{
    for (auto& t : m_tiles)
    {
        if (t.thumb)
        {
            DwmUnregisterThumbnail(t.thumb);
            t.thumb = nullptr;
        }
    }
}

void TaskView::Render()
{
    RECT rc;
    GetClientRect(m_hwnd, &rc);
    const int w = rc.right;
    const int h = rc.bottom;
    if (w <= 0 || h <= 0)
    {
        return;
    }

    HDC screen = GetDC(m_hwnd);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bmp = CreateCompatibleBitmap(screen, w, h);
    HBITMAP old = static_cast<HBITMAP>(SelectObject(dc, bmp));

    {
        Graphics g(dc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

        SolidBrush bg(Color(255, 24, 24, 28));
        g.FillRectangle(&bg, 0, 0, w, h);

        FontFamily fam(L"Segoe UI");
        Font headerFont(&fam, 15, FontStyleBold, UnitPixel);
        Font titleFont(&fam, 12, FontStyleRegular, UnitPixel);
        Font deskFont(&fam, 13, FontStyleRegular, UnitPixel);
        SolidBrush white(Color(255, 240, 240, 240));
        SolidBrush dim(Color(255, 165, 165, 172));
        SolidBrush titleBarBrush(Color(255, 44, 44, 52));
        SolidBrush tileBodyBrush(Color(255, 32, 32, 38));
        SolidBrush accent(Color(255, 0, 120, 215));
        SolidBrush closeHotBrush(Color(255, 200, 60, 60));
        Pen accentPen(Color(255, 0, 120, 215), 2.0f);
        Pen deskPen(Color(255, 90, 90, 100), 1.0f);
        StringFormat sf;
        sf.SetLineAlignment(StringAlignmentCenter);
        sf.SetTrimming(StringTrimmingEllipsisCharacter);
        sf.SetFormatFlags(StringFormatFlagsNoWrap);

        // Clip the grid region so scrolled content doesn't bleed into the strip.
        g.SetClip(Rect(0, GridTop - 2, w, m_gridBottom - GridTop + 4));

        for (int gi = 0; gi < static_cast<int>(m_headers.size()); ++gi)
        {
            const Header& hd = m_headers[gi];
            const int hy = hd.rect.top - m_scrollY;
            if (hy + HeaderH >= GridTop && hy <= m_gridBottom)
            {
                if (hd.icon)
                {
                    DrawIconEx(dc, hd.rect.left, hy + (HeaderH - 24) / 2, hd.icon, 24, 24, 0, nullptr, DI_NORMAL);
                }
                RectF nameRect(static_cast<REAL>(hd.rect.left + 34), static_cast<REAL>(hy), 600.0f, static_cast<REAL>(HeaderH));
                std::wstring label = hd.name + L"   " + std::to_wstring(hd.count);
                g.DrawString(label.c_str(), -1, &headerFont, nameRect, &sf, &white);
            }
        }

        for (int i = 0; i < static_cast<int>(m_tiles.size()); ++i)
        {
            const Tile& t = m_tiles[i];
            const int x = t.rect.left;
            const int y = t.rect.top - m_scrollY;
            const int tw = t.rect.right - t.rect.left;
            const int th = t.rect.bottom - t.rect.top;
            if (y + th < GridTop || y > m_gridBottom)
            {
                continue;
            }

            // Body placeholder (covered by the DWM thumbnail when visible).
            FillRound(g, tileBodyBrush, x, y, tw, th, 8);
            // Title bar.
            FillRound(g, titleBarBrush, x, y, tw, TitleBar, 8);
            g.FillRectangle(&titleBarBrush, x, y + TitleBar - 8, tw, 8);

            if (t.icon)
            {
                DrawIconEx(dc, x + 8, y + (TitleBar - 16) / 2, t.icon, 16, 16, 0, nullptr, DI_NORMAL);
            }
            RectF titleRect(static_cast<REAL>(x + 30), static_cast<REAL>(y), static_cast<REAL>(tw - 30 - CloseSize - 6), static_cast<REAL>(TitleBar));
            g.DrawString(t.title.c_str(), -1, &titleFont, titleRect, &sf, &white);

            // Close button.
            const int cx = x + tw - CloseSize - 4;
            const int cy = y + (TitleBar - CloseSize) / 2;
            if (i == m_hotClose)
            {
                FillRound(g, closeHotBrush, cx, cy, CloseSize, CloseSize, 4);
            }
            Pen xpen(Color(255, 220, 220, 220), 1.6f);
            g.DrawLine(&xpen, cx + 7, cy + 7, cx + CloseSize - 7, cy + CloseSize - 7);
            g.DrawLine(&xpen, cx + CloseSize - 7, cy + 7, cx + 7, cy + CloseSize - 7);

            if (i == m_selected || i == m_hot)
            {
                DrawRoundBorder(g, accentPen, x, y, tw, th, 8);
            }
        }

        g.ResetClip();

        // Desktop strip.
        g.DrawLine(&deskPen, Margin, m_gridBottom + 7, w - Margin, m_gridBottom + 7);
        for (int i = 0; i < static_cast<int>(m_desktops.size()); ++i)
        {
            const DesktopTile& d = m_desktops[i];
            const int x = d.rect.left;
            const int y = d.rect.top;
            SolidBrush deskBg(Color(255, 38, 38, 46));
            FillRound(g, deskBg, x, y, DeskW, DeskH, 8);

            if (d.isNew)
            {
                Pen plus(Color(255, 200, 200, 205), 2.0f);
                g.DrawLine(&plus, x + DeskW / 2 - 12, y + DeskH / 2, x + DeskW / 2 + 12, y + DeskH / 2);
                g.DrawLine(&plus, x + DeskW / 2, y + DeskH / 2 - 12, x + DeskW / 2, y + DeskH / 2 + 12);
            }

            if (d.isCurrent)
            {
                DrawRoundBorder(g, accentPen, x, y, DeskW, DeskH, 8);
            }
            else if (i == m_hotDesktop)
            {
                Pen hot(Color(255, 150, 150, 160), 2.0f);
                DrawRoundBorder(g, hot, x, y, DeskW, DeskH, 8);
            }

            RectF lblRect(static_cast<REAL>(x), static_cast<REAL>(y + DeskH + 3), static_cast<REAL>(DeskW), 22.0f);
            StringFormat center;
            center.SetAlignment(StringAlignmentCenter);
            center.SetTrimming(StringTrimmingEllipsisCharacter);
            center.SetFormatFlags(StringFormatFlagsNoWrap);
            g.DrawString(d.name.c_str(), -1, &deskFont, lblRect, &center, d.isCurrent ? &white : &dim);
        }
    }

    BitBlt(screen, 0, 0, w, h, dc, 0, 0, SRCCOPY);
    SelectObject(dc, old);
    DeleteObject(bmp);
    DeleteDC(dc);
    ReleaseDC(m_hwnd, screen);
}

int TaskView::TileAtPoint(POINT pt) const
{
    if (pt.y < GridTop || pt.y > m_gridBottom)
    {
        return -1;
    }
    for (int i = 0; i < static_cast<int>(m_tiles.size()); ++i)
    {
        RECT r = m_tiles[i].rect;
        r.top -= m_scrollY;
        r.bottom -= m_scrollY;
        if (pt.x >= r.left && pt.x < r.right && pt.y >= r.top && pt.y < r.bottom)
        {
            return i;
        }
    }
    return -1;
}

int TaskView::CloseButtonAtPoint(POINT pt) const
{
    const int i = TileAtPoint(pt);
    if (i < 0)
    {
        return -1;
    }
    RECT r = m_tiles[i].rect;
    const int y = r.top - m_scrollY;
    const int cx = r.right - CloseSize - 4;
    const int cy = y + (TitleBar - CloseSize) / 2;
    if (pt.x >= cx && pt.x < cx + CloseSize && pt.y >= cy && pt.y < cy + CloseSize)
    {
        return i;
    }
    return -1;
}

int TaskView::DesktopAtPoint(POINT pt) const
{
    for (int i = 0; i < static_cast<int>(m_desktops.size()); ++i)
    {
        const RECT& r = m_desktops[i].rect;
        if (pt.x >= r.left && pt.x < r.right && pt.y >= r.top && pt.y < r.bottom + 24)
        {
            return i;
        }
    }
    return -1;
}

void TaskView::MoveSelection(int dx, int dy)
{
    if (m_tiles.empty())
    {
        return;
    }
    if (m_selected < 0)
    {
        m_selected = 0;
    }
    else if (dx != 0)
    {
        m_selected = (m_selected + dx + static_cast<int>(m_tiles.size())) % static_cast<int>(m_tiles.size());
    }
    else if (dy != 0)
    {
        // Geometric move to the nearest tile in the row above/below.
        const RECT cur = m_tiles[m_selected].rect;
        const int curCx = (cur.left + cur.right) / 2;
        const int curCy = (cur.top + cur.bottom) / 2;
        int best = -1;
        int bestScore = INT_MAX;
        for (int i = 0; i < static_cast<int>(m_tiles.size()); ++i)
        {
            if (i == m_selected)
            {
                continue;
            }
            const RECT r = m_tiles[i].rect;
            const int cy = (r.top + r.bottom) / 2;
            if (dy < 0 && cy >= curCy - 4)
            {
                continue;
            }
            if (dy > 0 && cy <= curCy + 4)
            {
                continue;
            }
            const int cx = (r.left + r.right) / 2;
            const int score = abs(cy - curCy) * 4 + abs(cx - curCx);
            if (score < bestScore)
            {
                bestScore = score;
                best = i;
            }
        }
        if (best >= 0)
        {
            m_selected = best;
        }
    }

    // Keep the selection within the scrolled viewport.
    const RECT r = m_tiles[m_selected].rect;
    if (r.top - m_scrollY < GridTop)
    {
        m_scrollY = r.top - GridTop;
    }
    if (r.bottom - m_scrollY > m_gridBottom)
    {
        m_scrollY = r.bottom - m_gridBottom;
    }
    ClampScroll();
    UpdateThumbnails();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void TaskView::ActivateSelection()
{
    if (m_selected >= 0 && m_selected < static_cast<int>(m_tiles.size()))
    {
        CommitWindow(m_tiles[m_selected].hwnd);
    }
}

void TaskView::CommitWindow(HWND hwnd)
{
    Close();
    if (hwnd && IsWindow(hwnd))
    {
        ActivateWindow(hwnd);
    }
}

void TaskView::CloseTileWindow(int tileIndex)
{
    if (tileIndex < 0 || tileIndex >= static_cast<int>(m_tiles.size()))
    {
        return;
    }
    HWND hwnd = m_tiles[tileIndex].hwnd;
    if (m_tiles[tileIndex].thumb)
    {
        DwmUnregisterThumbnail(m_tiles[tileIndex].thumb);
        m_tiles[tileIndex].thumb = nullptr;
    }
    PostMessageW(hwnd, WM_CLOSE, 0, 0);

    // Drop the tile and its (now stale) group, then relayout from a fresh model.
    const int group = m_tiles[tileIndex].group;
    if (group >= 0 && group < static_cast<int>(m_groups.size()))
    {
        auto& windows = m_groups[group].windows;
        for (auto it = windows.begin(); it != windows.end(); ++it)
        {
            if (it->hwnd == hwnd)
            {
                windows.erase(it);
                break;
            }
        }
    }

    UnregisterThumbnails();
    // Rebuild tiles/headers from the trimmed groups (drop emptied groups).
    std::vector<AppGroup> remaining;
    for (auto& gp : m_groups)
    {
        if (!gp.windows.empty())
        {
            remaining.push_back(std::move(gp));
        }
    }
    m_groups = std::move(remaining);

    m_tiles.clear();
    m_headers.clear();
    for (int gi = 0; gi < static_cast<int>(m_groups.size()); ++gi)
    {
        m_headers.push_back(Header{ m_groups[gi].name, m_groups[gi].icon, m_groups[gi].windows.size(), {} });
        for (const auto& win : m_groups[gi].windows)
        {
            Tile t{};
            t.hwnd = win.hwnd;
            t.title = win.title;
            t.icon = win.icon;
            t.group = gi;
            m_tiles.push_back(t);
        }
    }

    if (m_tiles.empty())
    {
        Close();
        return;
    }
    if (m_selected >= static_cast<int>(m_tiles.size()))
    {
        m_selected = static_cast<int>(m_tiles.size()) - 1;
    }
    Layout();
    RegisterThumbnails();
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

LRESULT TaskView::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
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

    case WM_ERASEBKGND:
        return 1;

    case WM_KEYDOWN:
        switch (wParam)
        {
        case VK_ESCAPE:
            Close();
            return 0;
        case VK_RETURN:
            ActivateSelection();
            return 0;
        case VK_LEFT:
            MoveSelection(-1, 0);
            return 0;
        case VK_RIGHT:
            MoveSelection(1, 0);
            return 0;
        case VK_UP:
            MoveSelection(0, -1);
            return 0;
        case VK_DOWN:
            MoveSelection(0, 1);
            return 0;
        case VK_TAB:
            MoveSelection((GetKeyState(VK_SHIFT) & 0x8000) ? -1 : 1, 0);
            return 0;
        default:
            return 0;
        }

    case WM_MOUSEMOVE:
    {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        const int tile = TileAtPoint(pt);
        const int close = CloseButtonAtPoint(pt);
        const int desk = DesktopAtPoint(pt);
        if (tile != m_hot || close != m_hotClose || desk != m_hotDesktop)
        {
            m_hot = tile;
            m_hotClose = close;
            m_hotDesktop = desk;
            if (tile >= 0)
            {
                m_selected = tile;
            }
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP:
    {
        POINT pt{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        const int close = CloseButtonAtPoint(pt);
        if (close >= 0)
        {
            CloseTileWindow(close);
            return 0;
        }
        const int tile = TileAtPoint(pt);
        if (tile >= 0)
        {
            CommitWindow(m_tiles[tile].hwnd);
            return 0;
        }
        const int desk = DesktopAtPoint(pt);
        if (desk >= 0)
        {
            const bool isNew = m_desktops[desk].isNew;
            const int target = m_desktops[desk].index;
            const int current = m_currentDesktop;
            Close();
            if (isNew)
            {
                VirtualDesktops::CreateNew();
            }
            else
            {
                VirtualDesktops::SwitchTo(target, current);
            }
            return 0;
        }
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        const int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        m_scrollY -= (delta / WHEEL_DELTA) * 90;
        ClampScroll();
        UpdateThumbnails();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_KILLFOCUS:
        // Dismiss when focus leaves the overlay (click-away / Alt+Tab out).
        Close();
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void TaskView::ActivateWindow(HWND hwnd)
{
    if (IsIconic(hwnd))
    {
        ShowWindow(hwnd, SW_RESTORE);
    }
    HWND foreground = GetForegroundWindow();
    DWORD fgThread = GetWindowThreadProcessId(foreground, nullptr);
    DWORD me = GetCurrentThreadId();
    DWORD target = GetWindowThreadProcessId(hwnd, nullptr);
    if (fgThread != me)
    {
        AttachThreadInput(me, fgThread, TRUE);
    }
    if (target != me && target != fgThread)
    {
        AttachThreadInput(me, target, TRUE);
    }
    AllowSetForegroundWindow(ASFW_ANY);
    BringWindowToTop(hwnd);
    SetForegroundWindow(hwnd);
    SetFocus(hwnd);
    if (target != me && target != fgThread)
    {
        AttachThreadInput(me, target, FALSE);
    }
    if (fgThread != me)
    {
        AttachThreadInput(me, fgThread, FALSE);
    }
}
