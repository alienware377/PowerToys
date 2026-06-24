#pragma once

#include "AppGrouping.h"
#include "VirtualDesktops.h"

#include <Windows.h>
#include <dwmapi.h>

#include <string>
#include <vector>

// A full-screen, Task-View-style overlay (triggered by Win+Tab) that shows live
// DWM thumbnails of every window on the current desktop, clustered by app, plus
// a virtual-desktop strip along the bottom. Unlike the Alt+Tab switcher this is
// a persistent, focusable surface: it opens, takes focus, and stays up until the
// user picks a window, switches desktop, presses Esc, or clicks away.
class TaskView
{
public:
    bool Initialize(HINSTANCE hinstance);
    void Toggle(); // open if hidden, close if shown
    void Close();
    bool Visible() const { return m_visible; }

private:
    struct Tile
    {
        HWND hwnd = nullptr;
        std::wstring title;
        HICON icon = nullptr;
        int group = 0;
        RECT rect{}; // content-space (pre-scroll)
        HTHUMBNAIL thumb = nullptr;
    };
    struct Header
    {
        std::wstring name;
        HICON icon = nullptr;
        size_t count = 0;
        RECT rect{};
    };
    struct DesktopTile
    {
        std::wstring name;
        bool isCurrent = false;
        bool isNew = false;
        int index = 0;
        RECT rect{}; // screen-space (strip is not scrolled)
    };

    static LRESULT CALLBACK WndProcStatic(HWND, UINT, WPARAM, LPARAM);
    LRESULT WndProc(HWND, UINT, WPARAM, LPARAM);

    void Open();
    void BuildModel();
    void Layout();
    void RegisterThumbnails();
    void UpdateThumbnails();
    void UnregisterThumbnails();
    void Render();

    int TileAtPoint(POINT pt) const; // index into m_tiles, or -1
    int CloseButtonAtPoint(POINT pt) const; // index whose [x] is hit, or -1
    int DesktopAtPoint(POINT pt) const; // index into m_desktops, or -1
    void MoveSelection(int dx, int dy);
    void ActivateSelection();
    void CommitWindow(HWND hwnd);
    void CloseTileWindow(int tileIndex);
    void ClampScroll();

    static void ActivateWindow(HWND hwnd);

    HINSTANCE m_hinstance = nullptr;
    HWND m_hwnd = nullptr;
    bool m_visible = false;

    RECT m_monitor{}; // full monitor bounds the overlay covers
    int m_scrollY = 0;
    int m_contentHeight = 0;
    int m_gridBottom = 0; // y where the grid area ends and the desktop strip begins

    std::vector<AppGroup> m_groups;
    std::vector<Tile> m_tiles;
    std::vector<Header> m_headers;
    std::vector<DesktopTile> m_desktops;
    int m_currentDesktop = 0;

    int m_selected = -1;
    int m_hot = -1; // hovered tile
    int m_hotClose = -1; // hovered close button
    int m_hotDesktop = -1;
};
