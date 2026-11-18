#pragma once
// Libra Meter: a tiny always-on PC load meter. `LibraBrowser.exe --meter` runs ONLY the meter (no browser engine); the browser just starts/stops it.
// Shared helpers (used by the browser and by the meter); the meter itself is in Meter.cpp.
#include "pch.h"
#include "Data.h"
namespace Mt {
inline bool Test() { return TestEnv(L"LIBRA_TEST_MULTI", nullptr, 0) != 0; }   // test runs use their own window class / Run value, never the owner's
inline const wchar_t* Cls() { return Test() ? L"LibraMeterWndT" : L"LibraMeterWnd"; }
inline const wchar_t* RunName() { return Test() ? L"LibraMeterTest" : L"LibraMeter"; }
inline HWND Find() { return FindWindowW(Cls(), nullptr); }
inline bool RunOn() { return RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", RunName(), RRF_RT_REG_SZ, nullptr, nullptr, nullptr) == ERROR_SUCCESS; }
inline void SetRun(bool on) {   // "Start with Windows": only ever called after the user clicked the switch
    HKEY k; if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return;
    if (on) { wchar_t e[MAX_PATH]; GetModuleFileNameW(nullptr, e, MAX_PATH); std::wstring v = L"\"" + std::wstring(e) + L"\" --meter"; RegSetValueExW(k, RunName(), 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * 2)); }
    else RegDeleteValueW(k, RunName());
    RegCloseKey(k);
}
inline void Launch() {
    if (Find()) return;
    wchar_t e[MAX_PATH]; GetModuleFileNameW(nullptr, e, MAX_PATH); std::wstring cmd = L"\"" + std::wstring(e) + L"\" --meter";
    STARTUPINFOW si{ sizeof(si) }; PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
}
inline void Stop() { HWND w = Find(); if (w) PostMessageW(w, WM_CLOSE, 0, 0); }
int Run(HINSTANCE hi);   // the meter process
}
