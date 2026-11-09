#pragma once

#ifdef _WIN32
#include <windows.h>

#ifndef DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2
DECLARE_HANDLE(DPI_AWARENESS_CONTEXT);
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#endif

namespace Utils {

/// Call before any window is created. Stops DWM from soft-scaling a 125% desktop
/// over our integer/nearest present (which looks like pixel crawl on both axes).
inline void enableDpiAwareness()
{
    if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
        using SetCtxFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        if (auto setCtx = reinterpret_cast<SetCtxFn>(
                GetProcAddress(user32, "SetProcessDpiAwarenessContext"))) {
            if (setCtx(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
                return;
        }
    }
    if (HMODULE shcore = LoadLibraryW(L"shcore.dll")) {
        using SetAwFn = HRESULT(WINAPI*)(int);
        if (auto setAw = reinterpret_cast<SetAwFn>(
                GetProcAddress(shcore, "SetProcessDpiAwareness"))) {
            if (SUCCEEDED(setAw(2))) // PROCESS_PER_MONITOR_DPI_AWARE
                return;
        }
    }
    SetProcessDPIAware();
}

} // namespace Utils
#endif
