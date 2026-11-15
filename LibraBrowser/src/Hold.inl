// Download quarantine. A finished download that can run programs (IsRiskyFile) is
//  1. scanned/marked by Windows itself (IAttachmentExecute::Save: Mark-of-the-Web + the installed antivirus), then
//  2. HELD: renamed to "<name>.libra-hold" so nothing can run it by double-click, until the user presses Release.
// Libra never runs a file. If Windows Security removes the file, the row says so.

struct HoldRes { std::wstring orig, held, note; int hold = 0; };   // hold: 2 held, 3 removed by Windows Security, 0 could not hold

static const GUID kLibraAttachGuid = { 0x6a1f3c52, 0x9d2e, 0x4b7a, { 0x8e, 0x11, 0x5c, 0x3b, 0x7d, 0x4f, 0x20, 0xa9 } };

void BrowserWindow::HoldBegin(int i) {
    if (i < 0 || i >= (int)m_dl.size()) return;
    DownloadItem& d = m_dl[i]; d.hold = 1; d.orig = d.path;
    std::wstring path = d.path, name = d.name.empty() ? d.path : d.name, url;
    if (d.job) url = d.job->url; else if (d.op) { wil::unique_cotaskmem_string u; d.op->get_Uri(&u); if (u.get()) url = u.get(); }
    { size_t sl = name.find_last_of(L"\\/"); if (sl != std::wstring::npos) name = name.substr(sl + 1); }
    HWND hw = m_hWnd;
    std::thread([hw, path, name, url] {   // the scan can take seconds: never on the UI thread
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        auto* r = new HoldRes(); r->orig = path; r->held = path + L".libra-hold";
        HRESULT hr = E_FAIL;
        { wil::com_ptr<IAttachmentExecute> ae;
          if (SUCCEEDED(CoCreateInstance(CLSID_AttachmentServices, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&ae)))) {
              ae->SetClientGuid(kLibraAttachGuid); ae->SetLocalPath(path.c_str()); ae->SetFileName(name.c_str());
              if (!url.empty() && (url.rfind(L"http://", 0) == 0 || url.rfind(L"https://", 0) == 0)) ae->SetSource(url.c_str());
              hr = ae->Save();   // Mark-of-the-Web + antivirus; may delete or change the file
          } }
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) { r->hold = 3; r->note = L"Windows Security removed this file."; }
        else if (MoveFileExW(path.c_str(), r->held.c_str(), 0) || (DeleteFileW(r->held.c_str()) && MoveFileExW(path.c_str(), r->held.c_str(), 0))) {
            r->hold = 2; r->note = SUCCEEDED(hr) ? L"Checked by Windows Security. It will not run until you release it." : L"Windows Security could not approve it (not scanned). Held.";
        } else { r->hold = 0; r->note = L"Could not hold this file. Do not run it unless you trust the site."; }
        std::wstring orig = r->orig, held = r->held; bool wasHeld = r->hold == 2;
        PostMessage(hw, WM_APP + 41, 0, (LPARAM)r);
        if (wasHeld) for (int k = 0; k < 3; ++k) {   // the engine may re-create an EMPTY file under the old name after we moved the real one: remove such a leftover
            Sleep(2000); WIN32_FILE_ATTRIBUTE_DATA fa{};
            if (GetFileAttributesExW(orig.c_str(), GetFileExInfoStandard, &fa) && fa.nFileSizeLow == 0 && fa.nFileSizeHigh == 0) DeleteFileW(orig.c_str());
        }
        CoUninitialize();
    }).detach();
}

void BrowserWindow::HoldDone(void* p) {
    std::unique_ptr<HoldRes> r((HoldRes*)p);
    for (auto& d : m_dl) if (d.orig == r->orig && d.hold == 1) {
        d.scanNote = r->note;
        if (r->hold == 2) { d.path = r->held; d.hold = 2; }
        else if (r->hold == 3) { d.hold = 3; d.state = 2; d.canRetry = false; d.toast |= 4; }
        else { d.hold = 0; d.toast |= 4; }
        break;
    }
    for (int i = 0; i < (int)m_tabs.size(); ++i) if (m_tabs[i]->url == L"libra://downloads") RenderTab(i);
    SetTimer(m_hWnd, 2005, 250, nullptr);   // let the risky-file card appear for the held file
}

bool BrowserWindow::HoldRelease(int i) {   // user pressed Release (the caller already asked "are you sure")
    if (i < 0 || i >= (int)m_dl.size() || m_dl[i].hold != 2) return false;
    DownloadItem& d = m_dl[i]; std::wstring target = d.orig;
    if (GetFileAttributesW(target.c_str()) != INVALID_FILE_ATTRIBUTES) target = UniquePath(target);
    if (!MoveFileExW(d.path.c_str(), target.c_str(), 0)) return false;
    d.path = target; d.hold = 0; size_t sl = target.find_last_of(L"\\/"); if (sl != std::wstring::npos) d.name = target.substr(sl + 1);
    return true;
}

// Open a held file inside a throw-away Windows Sandbox (Pro/Enterprise, feature turned on by the user). Nothing is run for the user:
// the VM opens a folder with a READ-ONLY copy of the file, no network, no clipboard, no audio/video/printer/GPU. Closing the window destroys everything.
static std::vector<std::wstring> g_sbxDirs;
void BrowserWindow::HoldSandboxCleanup() { for (auto& d : g_sbxDirs) { std::error_code ec; std::filesystem::remove_all(d, ec); } g_sbxDirs.clear(); }
static std::wstring XmlEsc(std::wstring s) { std::wstring o; for (wchar_t c : s) { if (c == L'&') o += L"&amp;"; else if (c == L'<') o += L"&lt;"; else if (c == L'>') o += L"&gt;"; else o += c; } return o; }
bool BrowserWindow::HoldSandbox(int i) {
    wchar_t tp[MAX_PATH]; const bool test = TestEnv(L"LIBRA_TEST_SAVEPATH", tp, MAX_PATH) > 0;   // tests only: write the config, do not launch
    if (i < 0 || i >= (int)m_dl.size() || m_dl[i].hold != 2) return false;
    std::wstring exe = WinSandboxExe(); if (exe.empty() && !test) return false;
    DownloadItem& d = m_dl[i];
    wchar_t t[MAX_PATH]; if (!GetTempPathW(MAX_PATH, t)) return false;
    std::wstring dir = std::wstring(t) + L"LibraSbx-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount());
    if (!CreateDirectoryW(dir.c_str(), nullptr)) return false; g_sbxDirs.push_back(dir);
    std::wstring name = SafeName(d.orig.substr(d.orig.find_last_of(L"\\/") + 1)); if (name.empty()) name = L"file";
    if (!CopyFileW(d.path.c_str(), (dir + L"\\" + name).c_str(), TRUE)) return false;   // only this one file is shared with the VM
    std::string xml = ToUtf8(L"<Configuration>\n <VGpu>Disable</VGpu>\n <Networking>Disable</Networking>\n <AudioInput>Disable</AudioInput>\n <VideoInput>Disable</VideoInput>\n <PrinterRedirection>Disable</PrinterRedirection>\n <ClipboardRedirection>Disable</ClipboardRedirection>\n <ProtectedClient>Enable</ProtectedClient>\n"
        L" <MappedFolders>\n  <MappedFolder>\n   <HostFolder>" + XmlEsc(dir) + L"</HostFolder>\n   <SandboxFolder>C:\\LibraFile</SandboxFolder>\n   <ReadOnly>true</ReadOnly>\n  </MappedFolder>\n </MappedFolders>\n"
        L" <LogonCommand>\n  <Command>explorer.exe C:\\LibraFile</Command>\n </LogonCommand>\n</Configuration>\n");
    std::wstring wsb = dir + L".wsb"; WriteAllRaw(wsb, xml, false); g_sbxDirs.push_back(wsb);
    if (test) { WriteAllRaw(std::wstring(tp) + L"\\sbx.log", ToUtf8(wsb + L"\n"), true); return true; }
    return (INT_PTR)ShellExecuteW(nullptr, L"open", exe.c_str(), (L"\"" + wsb + L"\"").c_str(), nullptr, SW_SHOW) > 32;
}
