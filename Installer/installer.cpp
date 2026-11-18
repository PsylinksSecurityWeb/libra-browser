// Libra Browser Setup: per-user installer (no admin rights needed). Embeds LibraBrowser.exe, shows a progress bar,
// installs to %LOCALAPPDATA%\Programs\Libra Browser, adds Desktop + Start Menu shortcuts, registers the uninstaller and the
// browser capabilities (default-apps list). "Setup.exe /uninstall" removes everything (asks before deleting your data).
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <string>
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "uuid.lib")

#define IDR_PAYLOAD 101
#define WM_PROG (WM_APP + 1)
#define WM_DONE (WM_APP + 2)
static const wchar_t* kName = L"Libra Browser";
static const wchar_t* kCompany = L"Psylinks Security Private Limited";
static const wchar_t* kUninstKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\LibraBrowser";

static HWND g_wnd, g_bar, g_status, g_btn; static HFONT g_fTitle, g_fText, g_fSmall; static std::wstring g_dir;
static std::wstring SpecialDir(int csidl) { wchar_t p[MAX_PATH] = {}; SHGetFolderPathW(nullptr, csidl, nullptr, 0, p); return p; }
static std::wstring InstallDir() { return SpecialDir(CSIDL_LOCAL_APPDATA) + L"\\Programs\\" + kName; }

static bool MakeLink(const std::wstring& lnk, const std::wstring& target) {
    IShellLinkW* sl = nullptr; bool ok = false;
    if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&sl))) {
        sl->SetPath(target.c_str()); sl->SetIconLocation(target.c_str(), 0); sl->SetDescription(L"Libra Browser");
        std::wstring wd = target.substr(0, target.rfind(L'\\')); sl->SetWorkingDirectory(wd.c_str());
        IPersistFile* pf = nullptr;
        if (SUCCEEDED(sl->QueryInterface(IID_IPersistFile, (void**)&pf))) { ok = SUCCEEDED(pf->Save(lnk.c_str(), TRUE)); pf->Release(); }
        sl->Release();
    }
    return ok;
}
static void RegStr(HKEY k, const wchar_t* n, const std::wstring& v) { RegSetValueExW(k, n, 0, REG_SZ, (const BYTE*)v.c_str(), (DWORD)((v.size() + 1) * 2)); }
static void RegDw(HKEY k, const wchar_t* n, DWORD v) { RegSetValueExW(k, n, 0, REG_DWORD, (const BYTE*)&v, 4); }
static void ResetWelcome() {   // forget "welcomed" so the first-run film plays again after a fresh install (other settings stay)
    std::wstring f = SpecialDir(CSIDL_LOCAL_APPDATA) + L"\\LibraBrowser\\settings.ini";
    HANDLE h = CreateFileW(f.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD sz = GetFileSize(h, nullptr), rd = 0; std::string in(sz, '\0'); ReadFile(h, &in[0], sz, &rd, nullptr); in.resize(rd); CloseHandle(h);
    std::string out; size_t p = 0;
    while (p < in.size()) { size_t e = in.find('\n', p); if (e == std::string::npos) e = in.size(); std::string line = in.substr(p, e - p); p = e + 1;
        if (line.compare(0, 9, "welcomed=") != 0) out += line + "\n"; }
    h = CreateFileW(f.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, out.data(), (DWORD)out.size(), &w, nullptr); CloseHandle(h); }
}
static void WipeData() {   // a fresh install starts clean: no history, tiles, passwords, cookies from an earlier copy
    std::wstring d = SpecialDir(CSIDL_LOCAL_APPDATA) + L"\\LibraBrowser"; d.push_back(L'\0');
    SHFILEOPSTRUCTW op{}; op.wFunc = FO_DELETE; op.pFrom = d.c_str(); op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT; SHFileOperationW(&op);
}
static void Progress(int pct, const wchar_t* text) { PostMessageW(g_wnd, WM_PROG, pct, (LPARAM)_wcsdup(text)); Sleep(130); }   // small pause so each step is readable

static DWORD WINAPI Worker(LPVOID) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    Progress(2, L"Preparing...");
    bool fresh; { HKEY k0; fresh = RegOpenKeyExW(HKEY_CURRENT_USER, kUninstKey, 0, KEY_READ, &k0) != ERROR_SUCCESS; if (!fresh) RegCloseKey(k0); }
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_PAYLOAD), RT_RCDATA);
    const BYTE* data = (const BYTE*)LockResource(LoadResource(nullptr, r)); DWORD size = SizeofResource(nullptr, r);
    g_dir = InstallDir(); SHCreateDirectoryExW(nullptr, g_dir.c_str(), nullptr);
    std::wstring exe = g_dir + L"\\LibraBrowser.exe", tmp = exe + L".new";
    Progress(5, L"Copying files...");
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) { PostMessageW(g_wnd, WM_DONE, 0, 0); return 1; }
    for (DWORD off = 0; off < size;) {
        DWORD n = min(65536u, size - off), w = 0; WriteFile(h, data + off, n, &w, nullptr); off += n;
        if (((off / 65536) & 3) == 0 || off == size) Progress(5 + (int)(65ull * off / size), L"Copying files...");
    }
    CloseHandle(h);
    for (int tries = 0; !MoveFileExW(tmp.c_str(), exe.c_str(), MOVEFILE_REPLACE_EXISTING); ++tries) {   // old copy still running?
        if (tries > 40) { DeleteFileW(tmp.c_str()); PostMessageW(g_wnd, WM_DONE, 0, 0); return 1; }
        Progress(70, L"Close Libra Browser to continue..."); Sleep(1500);
    }
    wchar_t self[MAX_PATH]; GetModuleFileNameW(nullptr, self, MAX_PATH);
    CopyFileW(self, (g_dir + L"\\Uninstall.exe").c_str(), FALSE);
    Progress(75, L"Creating shortcuts...");
    MakeLink(SpecialDir(CSIDL_DESKTOPDIRECTORY) + L"\\" + kName + L".lnk", exe);
    std::wstring sm = SpecialDir(CSIDL_PROGRAMS); MakeLink(sm + L"\\" + kName + L".lnk", exe);
    Progress(88, L"Registering Libra Browser...");
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kUninstKey, 0, nullptr, 0, KEY_WRITE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        RegStr(k, L"DisplayName", kName); RegStr(k, L"DisplayVersion", L"3.9.4"); RegStr(k, L"Publisher", kCompany);
        RegStr(k, L"InstallLocation", g_dir); RegStr(k, L"DisplayIcon", exe);
        RegStr(k, L"UninstallString", L"\"" + g_dir + L"\\Uninstall.exe\" /uninstall");
        RegStr(k, L"URLInfoAbout", L"https://github.com/PsylinksSecurityWeb/libra-browser");
        RegDw(k, L"NoModify", 1); RegDw(k, L"NoRepair", 1); RegDw(k, L"EstimatedSize", size / 1024 * 2); RegCloseKey(k);
    }
    { STARTUPINFOW si{ sizeof(si) }; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; PROCESS_INFORMATION pi{};
      std::wstring cl = L"\"" + exe + L"\" --register-default";   // makes Libra appear in Windows "Default apps" (does not change your default)
      if (CreateProcessW(nullptr, &cl[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, g_dir.c_str(), &si, &pi)) { WaitForSingleObject(pi.hProcess, 10000); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); } }
    if (fresh) { WipeData(); ResetWelcome(); }   // new install: clean slate, welcome film plays once
    Progress(100, L"Installed.");
    PostMessageW(g_wnd, WM_DONE, 1, 0); CoUninitialize(); return 0;
}

static LRESULT CALLBACK Proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_CREATE: {
        g_fTitle = CreateFontW(-34, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_fText = CreateFontW(-15, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        g_fSmall = CreateFontW(-12, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
        auto mk = [&](const wchar_t* cls, const wchar_t* txt, DWORD st, int x, int y, int cw, int ch, HFONT f) { HWND c = CreateWindowExW(0, cls, txt, WS_CHILD | WS_VISIBLE | st, x, y, cw, ch, h, nullptr, nullptr, nullptr); SendMessageW(c, WM_SETFONT, (WPARAM)f, TRUE); return c; };
        mk(L"STATIC", kName, 0, 40, 36, 440, 48, g_fTitle);
        mk(L"STATIC", L"Secure, private and lightweight. Version 3.9.4", 0, 42, 86, 440, 22, g_fText);
        g_status = mk(L"STATIC", L"Starting...", 0, 42, 150, 440, 22, g_fText);
        g_bar = CreateWindowExW(0, PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE | PBS_SMOOTH, 42, 182, 436, 8, h, nullptr, nullptr, nullptr);
        SetWindowTheme(g_bar, L"", L""); SendMessageW(g_bar, PBM_SETBARCOLOR, 0, RGB(17, 17, 17)); SendMessageW(g_bar, PBM_SETBKCOLOR, 0, RGB(232, 232, 232));
        g_btn = mk(L"BUTTON", L"Launch Libra Browser", BS_PUSHBUTTON | WS_TABSTOP, 42, 214, 200, 36, g_fText); ShowWindow(g_btn, SW_HIDE);
        mk(L"STATIC", kCompany, 0, 42, 288, 440, 18, g_fSmall);
        CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
        return 0; }
    case WM_CTLCOLORSTATIC: SetBkMode((HDC)w, TRANSPARENT); SetTextColor((HDC)w, (HWND)l == g_status ? RGB(90, 90, 90) : RGB(17, 17, 17)); return (LRESULT)GetStockObject(WHITE_BRUSH);
    case WM_PROG: { SendMessageW(g_bar, PBM_SETPOS, w, 0); wchar_t* t = (wchar_t*)l; SetWindowTextW(g_status, t); free(t); return 0; }
    case WM_DONE:
        if (w) { SetWindowTextW(g_status, L"Libra Browser is installed. A shortcut is on your Desktop and in the Start menu."); ShowWindow(g_btn, SW_SHOW); SetFocus(g_btn); }
        else { SetWindowTextW(g_status, L"Installation failed. Close Libra Browser and run Setup again."); }
        return 0;
    case WM_COMMAND: if ((HWND)l == g_btn) { ShellExecuteW(nullptr, L"open", (g_dir + L"\\LibraBrowser.exe").c_str(), nullptr, g_dir.c_str(), SW_SHOWNORMAL); DestroyWindow(h); } return 0;
    case WM_ERASEBKGND: { RECT r; GetClientRect(h, &r); FillRect((HDC)w, &r, (HBRUSH)GetStockObject(WHITE_BRUSH)); return 1; }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

static void Uninstall() {
    if (MessageBoxW(nullptr, L"Remove Libra Browser from this PC?", kName, MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    std::wstring dir = InstallDir();
    { STARTUPINFOW si{ sizeof(si) }; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; PROCESS_INFORMATION pi{}; wchar_t cl[] = L"taskkill /IM LibraBrowser.exe /F";
      if (CreateProcessW(nullptr, cl, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) { WaitForSingleObject(pi.hProcess, 5000); CloseHandle(pi.hProcess); CloseHandle(pi.hThread); } }
    DeleteFileW((SpecialDir(CSIDL_DESKTOPDIRECTORY) + L"\\" + kName + L".lnk").c_str());
    DeleteFileW((SpecialDir(CSIDL_PROGRAMS) + L"\\" + kName + L".lnk").c_str());
    ResetWelcome();
    SHDeleteKeyW(HKEY_CURRENT_USER, kUninstKey);
    SHDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Clients\\StartMenuInternet\\LibraBrowser");
    SHDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\LibraURL"); SHDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\LibraHTML");
    HKEY k; if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\RegisteredApplications", 0, KEY_SET_VALUE, &k) == ERROR_SUCCESS) { RegDeleteValueW(k, L"LibraBrowser"); RegCloseKey(k); }
    if (MessageBoxW(nullptr, L"Also delete your Libra data (history, bookmarks, saved passwords, settings)?", kName, MB_YESNO | MB_DEFBUTTON2 | MB_ICONWARNING) == IDYES) {
        std::wstring d = SpecialDir(CSIDL_LOCAL_APPDATA) + L"\\LibraBrowser"; d.push_back(L'\0');
        SHFILEOPSTRUCTW op{}; op.wFunc = FO_DELETE; op.pFrom = d.c_str(); op.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT; SHFileOperationW(&op);
    }
    DeleteFileW((dir + L"\\LibraBrowser.exe").c_str());
    std::wstring cmd = L"cmd /c ping 127.0.0.1 -n 3 >nul & rmdir /s /q \"" + dir + L"\"";   // removes Uninstall.exe after this process exits
    STARTUPINFOW si{ sizeof(si) }; si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) { CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
    MessageBoxW(nullptr, L"Libra Browser was removed.", kName, MB_OK | MB_ICONINFORMATION);
}

int WinAuthBlocking(HWND owner, const wchar_t* msg);   // SafetyAuth.cpp: Windows sign-in window
// Child Safety on (registry record present): removing Libra needs the Windows sign-in. No Windows lock set up = nothing to ask.
static bool UninstallAllowed() {
    DWORD n = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Psylinks\\Libra", L"Safety", RRF_RT_REG_BINARY, nullptr, nullptr, &n) != ERROR_SUCCESS) return true;
    HWND o = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"Libra", WS_POPUP | WS_VISIBLE, GetSystemMetrics(SM_CXSCREEN) / 2, GetSystemMetrics(SM_CYSCREEN) / 3, 1, 1, nullptr, nullptr, nullptr, nullptr);
    SetForegroundWindow(o);
    struct A { HWND o; int r; } a{ o, 2 };   // the sign-in runs on a worker thread so this thread keeps pumping messages (the Windows dialog needs its owner window alive)
    HANDLE th = CreateThread(nullptr, 0, [](LPVOID p) -> DWORD { auto* x = (A*)p; x->r = WinAuthBlocking(x->o, L"Remove Libra Browser (Child Safety mode is on)"); return 0; }, &a, 0, nullptr);
    if (th) { while (MsgWaitForMultipleObjects(1, &th, FALSE, INFINITE, QS_ALLINPUT) == WAIT_OBJECT_0 + 1) { MSG m; while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageW(&m); } } CloseHandle(th); }
    int r = a.r;
    if (o) DestroyWindow(o);
    if (r == 2) { MessageBoxW(nullptr, L"Libra was not removed. Child Safety mode is on, so your Windows sign-in is needed.", L"Libra Browser", MB_OK | MB_ICONINFORMATION); return false; }
    return true;
}
int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, LPWSTR cmd, int) { SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);   // a planted DLL next to Setup.exe (Downloads folder) is never loaded
    if (cmd && wcsstr(cmd, L"/uninstall")) { if (UninstallAllowed()) Uninstall(); return 0; }
    INITCOMMONCONTROLSEX ic{ sizeof(ic), ICC_PROGRESS_CLASS }; InitCommonControlsEx(&ic);
    WNDCLASSW wc{}; wc.lpfnWndProc = Proc; wc.hInstance = hi; wc.lpszClassName = L"LibraSetup"; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.hIcon = LoadIcon(hi, MAKEINTRESOURCE(1));
    RegisterClassW(&wc);
    int W = 520, H = 360, x = (GetSystemMetrics(SM_CXSCREEN) - W) / 2, y = (GetSystemMetrics(SM_CYSCREEN) - H) / 2;
    g_wnd = CreateWindowExW(0, wc.lpszClassName, L"Libra Browser Setup", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, x, y, W, H, nullptr, nullptr, hi, nullptr);
    MSG m; while (GetMessageW(&m, nullptr, 0, 0) > 0) { TranslateMessage(&m); DispatchMessageW(&m); }
    return 0;
}


