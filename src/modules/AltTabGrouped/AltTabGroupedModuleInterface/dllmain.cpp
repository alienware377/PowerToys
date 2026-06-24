#include "pch.h"

#include <interface/powertoy_module_interface.h>

#include <common/logger/logger.h>
#include <common/SettingsAPI/settings_objects.h>
#include <common/interop/shared_constants.h>
#include <common/utils/gpo.h>
#include <common/utils/winapi_error.h>

#include <AltTabGrouped/ModuleConstants.h>
#include <AltTabGrouped/trace.h>

#include <shellapi.h>

namespace NonLocalizable
{
    const wchar_t ModulePath[] = L"PowerToys.AltTabGrouped.exe";
}

BOOL APIENTRY DllMain(HMODULE, DWORD ul_reason_for_call, LPVOID)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        Trace::AltTabGrouped::RegisterProvider();
        break;
    case DLL_PROCESS_DETACH:
        Trace::AltTabGrouped::UnregisterProvider();
        break;
    }
    return TRUE;
}

// PowerToy module: a grouped Alt+Tab task switcher. The interface DLL owns the
// runner-facing contract and launches/stops the worker executable that installs
// the keyboard hook and renders the switcher.
class AltTabGroupedModuleInterface : public PowertoyModuleIface
{
public:
    AltTabGroupedModuleInterface()
    {
        app_name = L"Alt+Tab Grouped"; // TODO: localize
        app_key = NonLocalizable::ModuleKey;
        m_hTerminateEvent = CreateEventW(nullptr, false, false, CommonSharedConstants::ALT_TAB_GROUPED_TERMINATE_EVENT);
        init_settings();
    }

    virtual PCWSTR get_name() override { return app_name.c_str(); }
    virtual const wchar_t* get_key() override { return app_key.c_str(); }

    virtual powertoys_gpo::gpo_rule_configured_t gpo_policy_enabled_configuration() override
    {
        return powertoys_gpo::gpo_rule_configured_not_configured;
    }

    virtual bool get_config(wchar_t* buffer, int* buffer_size) override
    {
        HINSTANCE hinstance = reinterpret_cast<HINSTANCE>(&__ImageBase);
        PowerToysSettings::Settings settings(hinstance, get_name());
        settings.set_description(L"Replaces Alt+Tab with a switcher that groups every window by its application.");
        settings.set_overview_link(L"https://aka.ms/PowerToysOverview_AltTabGrouped");
        return settings.serialize_to_buffer(buffer, buffer_size);
    }

    virtual void set_config(const wchar_t* config) override
    {
        try
        {
            PowerToysSettings::PowerToyValues values =
                PowerToysSettings::PowerToyValues::from_json_string(config, get_key());
            values.save_to_settings_file();
        }
        catch (std::exception&)
        {
        }
    }

    // Alt+Tab is intercepted by the worker's own low-level keyboard hook, so the
    // module exposes no centralized hotkey to the runner.
    virtual size_t get_hotkeys(Hotkey*, size_t) override { return 0; }
    virtual bool on_hotkey(size_t) override { return false; }

    virtual void enable() override
    {
        Logger::info(L"AltTabGrouped enabling");
        Enable();
    }

    virtual void disable() override
    {
        Logger::info(L"AltTabGrouped disabling");
        Disable(true);
    }

    virtual bool is_enabled() override { return m_enabled; }

    virtual void destroy() override
    {
        Disable(false);
        delete this;
    }

private:
    void Enable()
    {
        m_enabled = true;
        Trace::AltTabGrouped::Enable(true);

        ResetEvent(m_hTerminateEvent);

        const std::wstring args = std::to_wstring(GetCurrentProcessId());
        SHELLEXECUTEINFOW sei{ sizeof(sei) };
        sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
        sei.lpFile = NonLocalizable::ModulePath;
        sei.nShow = SW_SHOWNORMAL;
        sei.lpParameters = args.c_str();
        if (ShellExecuteExW(&sei))
        {
            m_hProcess = sei.hProcess;
        }
        else
        {
            Logger::error(L"Failed to start PowerToys.AltTabGrouped.exe");
            auto message = get_last_error_message(GetLastError());
            if (message.has_value())
            {
                Logger::error(message.value());
            }
        }
    }

    void Disable(bool traceEvent)
    {
        m_enabled = false;
        if (traceEvent)
        {
            Trace::AltTabGrouped::Enable(false);
        }

        SetEvent(m_hTerminateEvent);

        if (m_hProcess)
        {
            WaitForSingleObject(m_hProcess, 1500);
            TerminateProcess(m_hProcess, 0);
            m_hProcess = nullptr;
        }
    }

    void init_settings()
    {
        try
        {
            PowerToysSettings::PowerToyValues::load_from_settings_file(get_key());
        }
        catch (std::exception&)
        {
            Logger::warn(L"An exception occurred while loading the AltTabGrouped settings file");
        }
    }

    std::wstring app_name;
    std::wstring app_key;
    bool m_enabled = false;
    HANDLE m_hProcess = nullptr;
    HANDLE m_hTerminateEvent = nullptr;
};

extern "C" __declspec(dllexport) PowertoyModuleIface* __cdecl powertoy_create()
{
    return new AltTabGroupedModuleInterface();
}
