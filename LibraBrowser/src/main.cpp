#include "pch.h"
#include "BrowserWindow.h"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "uxtheme.lib")

// Enable Visual Styles (rounded scrollbars, etc.)
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

static std::wstring UrlArg(PCWSTR cl) {   // first http(s) address on the command line (Windows passes it when Libra is the default browser)
    if (!cl) return L"";
    std::wstring a(cl); size_t p = a.find(L"http"); if (p == std::wstring::npos) return L"";
    size_t e = (p > 0 && a[p - 1] == L'"') ? a.find(L'"', p) : a.find(L' ', p);
    std::wstring u = a.substr(p, e == std::wstring::npos ? e : e - p);
    return (u.rfind(L"http://", 0) == 0 || u.rfind(L"https://", 0) == 0) && u.size() < 2000 ? u : L"";
}
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR lpCmdLine, int nCmdShow) {
    if (lpCmdLine && wcsstr(lpCmdLine, L"--register-default")) return RegisterAsBrowser() ? 0 : 1;
    // Single-instance: bring existing window to front if already running
    HWND existing = FindWindowW(L"LibraBrowserWnd", nullptr);
    if (existing) {
        ShowWindow(existing, SW_RESTORE);
        SetForegroundWindow(existing);
        std::wstring u = UrlArg(lpCmdLine);
        if (!u.empty()) { COPYDATASTRUCT cd{ 1, (DWORD)((u.size() + 1) * 2), (PVOID)u.c_str() }; SendMessageW(existing, WM_COPYDATA, 0, (LPARAM)&cd); }
        return 0;
    }

    // Per-monitor DPI awareness
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // Init COM for WebView2
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Init Common Controls (needed for subclassing)
    INITCOMMONCONTROLSEX icc{ sizeof(icc), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES };
    InitCommonControlsEx(&icc);

    bool incognito = (lpCmdLine && wcsstr(lpCmdLine, L"--incognito") != nullptr);
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
            { FVIRTKEY,                         VK_F4,  ID_FILE_CLOSE_TAB      }, // Alt+F4 handled by OS
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
