#include "pch.h"
#include "BrowserWindow.h"
#include "Meter.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "uxtheme.lib")

// Enable Visual Styles (rounded scrollbars, etc.)
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static std::vector<std::wstring> CmdArgs(PCWSTR cl) {   // real command-line words (quotes handled), so a flag word inside a web address is never taken as a flag
    std::vector<std::wstring> out; int n = 0; std::wstring all = L"x " + std::wstring(cl ? cl : L"");
    if (LPWSTR* v = CommandLineToArgvW(all.c_str(), &n)) { for (int i = 1; i < n; ++i) out.push_back(v[i]); LocalFree(v); }
    return out;
}
static bool HasFlag(PCWSTR cl, const wchar_t* flag) { for (auto& a : CmdArgs(cl)) if (a == flag) return true; return false; }
static std::wstring UrlArg(PCWSTR cl) {   // first http(s) address among the arguments (Windows passes it when Libra is the default browser)
    for (auto& u : CmdArgs(cl)) if ((u.rfind(L"http://", 0) == 0 || u.rfind(L"https://", 0) == 0) && u.size() < 2000) return u;
    return L"";
}
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpCmdLine, int nCmdShow) {
    // process hardening: no DLL from the current folder, abort on heap corruption, no DLLs from network shares or low-integrity folders
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS | LOAD_LIBRARY_SEARCH_SYSTEM32);
    HeapSetInformation(nullptr, HeapEnableTerminationOnCorruption, nullptr, 0);
    { PROCESS_MITIGATION_IMAGE_LOAD_POLICY ip{}; ip.NoRemoteImages = 1; ip.NoLowMandatoryLabelImages = 1; ip.PreferSystem32Images = 1; SetProcessMitigationPolicy(ProcessImageLoadPolicy, &ip, sizeof ip); }
    if (HasFlag(lpCmdLine, L"--meter")) return Mt::Run(hInstance);   // Libra Meter: only the meter, no browser engine
    if (HasFlag(lpCmdLine, L"--register-default")) return RegisterAsBrowser() ? 0 : 1;
    // Single-instance: bring existing window to front if already running
    HWND existing = nullptr;   // test hook LIBRA_TEST_MULTI: isolated test run beside a real window; a private window is always its own process
    if (!HasFlag(lpCmdLine, L"--incognito") && !TestEnv(L"LIBRA_TEST_MULTI", nullptr, 0))
        existing = FindWindowW(L"LibraBrowserWnd", nullptr);   // (private windows use class LibraBrowserWndP)
    if (existing) {
        ShowWindow(existing, SW_RESTORE);
        SetForegroundWindow(existing);
        std::wstring u = UrlArg(lpCmdLine);
        if (!u.empty()) { COPYDATASTRUCT cd{ 1, (DWORD)((u.size() + 1) * 2), (PVOID)u.c_str() }; DWORD_PTR res = 0; SendMessageTimeoutW(existing, WM_COPYDATA, 0, (LPARAM)&cd, SMTO_ABORTIFHUNG | SMTO_NORMAL, 4000, &res); }   // a frozen window must not freeze this launcher too
        return 0;
    }

    // Per-monitor DPI awareness
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Init COM for WebView2
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Init Common Controls (needed for subclassing)
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);

    bool incognito = HasFlag(lpCmdLine, L"--incognito");
    DWORD safeN = 0;   // Child Safety record present: a private window is never opened (a normal window opens instead)
    if (incognito && RegGetValueW(HKEY_CURRENT_USER, L"Software\\Psylinks\\Libra", L"Safety", RRF_RT_REG_BINARY, nullptr, nullptr, &safeN) == ERROR_SUCCESS) incognito = false;
    std::wstring startUrl = UrlArg(lpCmdLine);

    BrowserWindow browser(hInstance, incognito);
    if (!browser.Create(nCmdShow)) {
        MessageBoxW(nullptr, L"Failed to create browser window.", L"Libra Browser", MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    if (!startUrl.empty()) browser.OpenTab(startUrl);

    // Standard Win32 message pump with accelerators
    HACCEL hAccel = nullptr;
    {
        // Keyboard shortcuts
        ACCEL accels[] = {
            { FVIRTKEY | FCONTROL,              'T',    ID_FILE_NEW_TAB        },
            { FVIRTKEY | FCONTROL | FSHIFT,     'N',    ID_FILE_NEW_INCOGNITO  },
            { FVIRTKEY | FCONTROL,              'W',    ID_FILE_CLOSE_TAB      },
            { FVIRTKEY,                         VK_F5,  ID_NAV_REFRESH         },
            { FVIRTKEY | FCONTROL,              'R',    ID_NAV_REFRESH         },
            { FVIRTKEY | FALT,                  VK_LEFT,  ID_NAV_BACK          },
            { FVIRTKEY | FALT,                  VK_RIGHT, ID_NAV_FORWARD       },
            { FVIRTKEY,                         VK_F6,  IDC_OMNIBOX            }, // focus omnibox
        };
        hAccel = CreateAcceleratorTableW(accels, ARRAYSIZE(accels));
    }

    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        // Ctrl+L / F6 â†’ focus omnibox (special handling)
        if (msg.message == WM_KEYDOWN &&
            ((msg.wParam == 'L' && GetKeyState(VK_CONTROL) < 0) ||
             msg.wParam == VK_F6)) {
            HWND omni = GetDlgItem(browser.Hwnd(), IDC_OMNIBOX);
            if (omni) { SetFocus(omni); SendMessage(omni, EM_SETSEL, 0, -1); }
            continue;
        }
        if (msg.message == WM_KEYDOWN && browser.HandleKey((UINT)msg.wParam)) continue;
        if (!TranslateAcceleratorW(browser.Hwnd(), hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (hAccel) DestroyAcceleratorTable(hAccel);
    CoUninitialize();
    return (int)msg.wParam;
}
