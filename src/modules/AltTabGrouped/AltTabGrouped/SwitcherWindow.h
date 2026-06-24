#pragma once

#include "AppGrouping.h"

#include <Windows.h>
#include <vector>

// The on-screen switcher overlay. It owns a top-most popup window that renders
// the app groups and, for the focused group, its individual windows. The owning
// App drives selection through the public navigation methods while Alt is held
// and reads SelectedTarget() when Alt is released.
class SwitcherWindow
{
public:
    SwitcherWindow();
    ~SwitcherWindow();

    bool Initialize(HINSTANCE hinstance);

    // Populate with a fresh set of groups and show the overlay. initialAdvance
    // moves the selection forward once so a quick Alt+Tab tap lands on the
    // previous app, matching the standard switcher behavior.
    void Show(std::vector<AppGroup> groups, bool initialAdvance);
    void Hide();
    bool Visible() const { return m_visible; }

    // Top-level navigation between app groups.
    void NextGroup();
    void PrevGroup();

    // Second-level navigation between the focused group's windows. Entering
    // window level expands the focused group.
    void NextWindow();
    void PrevWindow();
    void ExpandFocusedGroup();
    void CollapseFocusedGroup();

    // The window that should be activated for the current selection.
    HWND SelectedTarget() const;

private:
    static LRESULT CALLBACK WndProcStatic(HWND, UINT, WPARAM, LPARAM);
    LRESULT WndProc(HWND, UINT, WPARAM, LPARAM);

    void Layout();
    void Render();

    HINSTANCE m_hinstance = nullptr;
    HWND m_hwnd = nullptr;
    bool m_visible = false;

    std::vector<AppGroup> m_groups;
    int m_selectedGroup = 0;
    int m_selectedWindow = 0;
    bool m_expanded = false;

    // Computed layout.
    int m_width = 0;
    int m_height = 0;
};
