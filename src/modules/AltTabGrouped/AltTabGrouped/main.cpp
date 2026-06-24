#include "pch.h"

#include <common/utils/logger_helper.h>
#include <common/utils/ProcessWaiter.h>
#include <common/utils/window.h>
#include <common/utils/UnhandledExceptionHandler.h>
#include <common/utils/gpo.h>
#include <common/interop/shared_constants.h>

#include <common/Telemetry/EtwTrace/EtwTrace.h>

#include "AltTabGrouped.h"
#include "ModuleConstants.h"
#include "TrayIcon.h"
#include "trace.h"

#include <memory>
#include <thread>

// Non-localizable
const std::wstring moduleName = L"AltTabGrouped";
const std::wstring internalPath = L"";
const std::wstring instanceMutexName = L"Local\\PowerToys_AltTabGrouped_InstanceMutex";

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE, _In_ PWSTR lpCmdLine, _In_ int)
{
    Shared::Trace::ETWTrace trace;
    trace.UpdateState(true);

    winrt::init_apartment();
    LoggerHelpers::init_logger(moduleName, internalPath, "AltTabGrouped");

    InitUnhandledExceptionHandler();

    auto mutex = CreateMutexW(nullptr, true, instanceMutexName.c_str());
    if (mutex == nullptr)
    {
        Logger::error(L"Failed to create instance mutex.");
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        return 0;
    }

    const DWORD mainThreadId = GetCurrentThreadId();

    // Exit cleanly when the runner exits.
    const std::wstring pid = lpCmdLine ? std::wstring(lpCmdLine) : std::wstring();
    if (!pid.empty())
    {
        ProcessWaiter::OnProcessTerminate(pid, [mainThreadId](int) {
            Logger::trace(L"Runner exited, stopping AltTabGrouped");
            PostThreadMessageW(mainThreadId, WM_QUIT, 0, 0);
        });
    }

    // Exit when the module interface signals a disable.
    HANDLE terminateEvent = CreateEventW(nullptr, false, false, CommonSharedConstants::ALT_TAB_GROUPED_TERMINATE_EVENT);
    std::thread terminateWatcher([terminateEvent, mainThreadId]() {
        if (terminateEvent)
        {
            WaitForSingleObject(terminateEvent, INFINITE);
            PostThreadMessageW(mainThreadId, WM_QUIT, 0, 0);
        }
    });
    terminateWatcher.detach();

    Trace::AltTabGrouped::RegisterProvider();
    Trace::AltTabGrouped::Enable(true);

    ULONG_PTR gdiplusToken = 0;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

    {
        AltTabGrouped app(hInstance, mainThreadId);

        // When launched without a parent PID we are running standalone (not under
        // the PowerToys runner): add a tray icon so the user can quit, since the
        // app otherwise silently owns Alt+Tab.
        std::unique_ptr<TrayIcon> tray;
        if (pid.empty())
        {
            tray = std::make_unique<TrayIcon>();
            tray->Create(hInstance, mainThreadId);
        }

        run_message_loop();
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);

    Trace::AltTabGrouped::Enable(false);
    Trace::AltTabGrouped::UnregisterProvider();
    trace.Flush();
    return 0;
}
