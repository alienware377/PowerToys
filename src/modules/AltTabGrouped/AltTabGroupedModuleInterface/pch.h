#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Unknwn.h>
#include <Windows.h>
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Data.Json.h>
#include <wil/common.h>
#include <wil/result.h>

// Linker-provided module base, used to obtain this DLL's HINSTANCE.
EXTERN_C IMAGE_DOS_HEADER __ImageBase;
