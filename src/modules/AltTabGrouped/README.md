# Alt+Tab Grouped

A PowerToys module that replaces the standard Alt+Tab task switcher with one that
**groups every open window by its application**. Instead of a flat strip of
windows, you get a two-level switcher: a list of apps, and — for the focused app —
its individual windows.

This is a net-new module (PowerToys ships no Alt+Tab module), built to mirror the
conventions of the existing native C++ modules such as `alwaysontop`.

## How it works

```
Runner ──loads──> PowerToys.AltTabGroupedModuleInterface.dll
                        │  enable() → ShellExecute
                        ▼
                  PowerToys.AltTabGrouped.exe
                        │  installs WH_KEYBOARD_LL
                        ▼
            Alt+Tab intercepted → SwitcherWindow overlay
```

- **`AltTabGroupedModuleInterface`** — the runner-facing `PowertoyModuleIface`
  DLL. On `enable()` it launches the worker exe; on `disable()` it signals a
  named terminate event (`ALT_TAB_GROUPED_TERMINATE_EVENT`) and waits for exit.
  Alt+Tab is captured by the worker's own hook, so the module exposes **no**
  centralized hotkey to the runner.

- **`AltTabGrouped` (worker exe)** — installs a low-level keyboard hook and owns
  the switcher. The hook only *decides whether to swallow a key* and posts the
  heavy work to a hidden control window, so it never blocks long enough for
  Windows to drop the hook. Pieces:
  - `WindowEnumerator` — applies the standard "alt-tab test" (visible, titled,
    not cloaked, representative of its owner chain, app-window vs tool-window).
  - `AppGrouping` — resolves each window's app identity (executable path, or the
    AppUserModelID for packaged/UWP apps hosted by `ApplicationFrameHost`),
    clusters windows into `AppGroup`s in MRU order, and resolves a name + icon.
  - `SwitcherWindow` — a top-most GDI+ overlay that renders the app rows and,
    when a group is expanded, its windows.
  - `AltTabGrouped` — the controller / state machine that ties the hook to the
    overlay and activates the chosen window when Alt is released.

## Controls

While **Alt** is held:

| Key                | Action                                            |
|--------------------|---------------------------------------------------|
| `Tab`              | Next **app** group                                |
| `Shift+Tab`        | Previous app group                                |
| `→` / `←`          | Next / previous app group                         |
| `` ` `` (backtick) | Cycle **windows** within the focused app          |
| `↓` / `↑`          | Next / previous window within the focused app     |
| `Enter`            | Activate the current selection immediately        |
| `Esc`              | Cancel without switching                          |
| release `Alt`      | Activate the selected window (or the app's MRU window) |

Tapping `Alt+Tab` and releasing lands on the previous app, matching the standard
switcher's "quick flick" behavior.

## Build

The two projects are registered in `PowerToys.slnx` under
`/modules/AltTabGrouped/` and the interface DLL is registered in the runner's
known-modules list (`src/runner/main.cpp`). Build the `AltTabGrouped` and
`AltTabGroupedModuleInterface` projects (or the whole solution) with Visual
Studio 2022 + the Windows SDK, exactly like any other native PowerToys module.

## Integration status

Implemented and wired:

- [x] Worker exe: keyboard hook, window enumeration, app grouping, two-level
      overlay UI, robust window activation.
- [x] Module interface DLL: enable/disable lifecycle, settings load/save,
      terminate event.
- [x] Registered in `PowerToys.slnx` and the runner's known-modules list.
- [x] Shared terminate event added to `common/interop/shared_constants.h`.

Deliberately left as follow-up (each is its own cross-cutting change in
PowerToys and is documented rather than stubbed):

- [ ] **Settings UI** — a WinUI3 page in `Settings.UI` plus a
      `ModuleType`/GPO entry, to expose options (enable, grouping mode,
      whether to also group across virtual desktops).
- [ ] **Installer** — add both binaries to the WiX/MSI and MSIX manifests.
- [ ] **GPO** — `gpo_policy_enabled_configuration()` currently returns
      *not configured*; add a real policy in `common/utils/gpo.h` if shipping.
- [ ] **Localization** — strings are inline English (`// TODO: localize`).
