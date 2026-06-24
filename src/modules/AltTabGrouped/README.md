# Alt+Tab Grouped — grouped Task View

A PowerToys module that replaces the Windows **Task View (Win+Tab)** with one
that **groups every window by its application**. Instead of a flat grid of
windows you get the same full-screen, live-thumbnail Task View look, but the
thumbnails are clustered under per-app headers, with the virtual-desktop strip
still along the bottom.

This is a net-new module (PowerToys ships no Task View module), built to mirror
the conventions of the existing native C++ modules such as `alwaysontop`.

## How it works

```
Runner ──loads──> PowerToys.AltTabGroupedModuleInterface.dll
                        │  enable() → ShellExecute
                        ▼
                  PowerToys.AltTabGrouped.exe
                        │  installs WH_KEYBOARD_LL, intercepts Win+Tab
                        ▼
            Full-screen grouped Task View (DWM thumbnails)
```

- **`AltTabGroupedModuleInterface`** — the runner-facing `PowertoyModuleIface`
  DLL. On `enable()` it launches the worker exe; on `disable()` it signals a
  named terminate event and waits for exit.

- **`AltTabGrouped` (worker exe)** — installs a low-level keyboard hook that
  swallows **Win+Tab** before the shell opens the built-in Task View, then
  toggles the grouped overlay in its place. The hook only decides whether to
  swallow the key and posts the work to a hidden control window. To stop a
  swallowed Win+Tab from popping Start when Win is released, it injects a
  reserved no-op key so Explorer treats the Win press as part of a combo.
  Pieces:
  - `WindowEnumerator` — the standard "alt-tab test" (visible, titled, not
    cloaked, representative of its owner chain, app-window vs tool-window).
  - `AppGrouping` — resolves each window's app identity (executable path, or the
    AppUserModelID for packaged/UWP apps) and clusters windows into `AppGroup`s.
  - `VirtualDesktops` — reads the desktop list/names/current index from the
    registry and switches via the Ctrl+Win+Arrow / Ctrl+Win+D shortcuts (no
    fragile undocumented COM).
  - `TaskView` — the full-screen overlay: live DWM thumbnails laid out under app
    headers, the desktop strip, mouse + keyboard input, dark translucent
    background.
  - `TrayIcon` — a notification-area Exit menu shown only when run standalone.

## Controls

Press **Win+Tab** to open. The overlay takes focus and stays up until you choose
something:

| Input                | Action                                            |
|----------------------|---------------------------------------------------|
| Mouse click a tile   | Switch to that window                             |
| Click a tile's **✕** | Close that window                                 |
| Click a desktop      | Switch to it (or **＋** to create a new one)       |
| Arrow keys / Tab     | Move the selection between thumbnails             |
| Enter                | Switch to the selected window                     |
| Mouse wheel          | Scroll when there are more windows than fit       |
| Esc / click away     | Close without switching                           |
| Win+Tab again        | Toggle closed                                     |

## Standalone build & install

The worker exe runs without the PowerToys runner. Built as a self-contained
`/MT` Release binary (only system DLL dependencies), it can be copied anywhere
and launched directly; with no parent PID it shows a tray icon so you can quit.
Launching with `--selftest` opens the overlay once on startup (used to validate
rendering, since an injected Win+Tab is intentionally ignored by the hook).

## Build

Both projects are registered in `PowerToys.slnx` under `/modules/AltTabGrouped/`
and the interface DLL is in the runner's known-modules list. Build with Visual
Studio 2022 + the Windows SDK like any other native PowerToys module.

## Integration status

Implemented and wired:

- [x] Worker exe: Win+Tab hook, window enumeration, app grouping, full-screen
      grouped Task View with live thumbnails, virtual-desktop strip, mouse +
      keyboard, standalone tray.
- [x] Module interface DLL: enable/disable lifecycle, settings, terminate event.
- [x] Registered in `PowerToys.slnx` and the runner's known-modules list.

Deliberately left as follow-up:

- [ ] **Settings UI** — a WinUI3 page in `Settings.UI` plus a GPO entry.
- [ ] **Installer** — add both binaries to the WiX/MSI and MSIX manifests.
- [ ] **Acrylic background** — the real blurred-wallpaper look (currently a dark
      translucent overlay).
- [ ] **Localization** — strings are inline English.
