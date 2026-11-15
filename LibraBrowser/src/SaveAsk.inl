// Ask-before-download gate. EVERY download (link, right-click Save, video helper, Assistant) goes through SaveGate:
// one Windows "Save as" window at a time, at least 1 s apart, floods dropped. Nothing is fetched (not even a probe) until the user presses Save.

struct SaveReq { std::wstring name, url; Tab* tab = nullptr; std::function<void(std::wstring)> cb; };
static std::deque<SaveReq> g_saveQ; static bool g_saveBusy = false; static DWORD g_saveLast = 0;
static std::vector<std::pair<std::wstring, DWORD>> g_saveRecent;   // url + tick of recent requests (repeat guard)
static std::vector<DWORD> g_saveTicks;                              // burst guard

void BrowserWindow::SaveGate(const std::wstring& name, const std::wstring& url, Tab* tab, std::function<void(std::wstring)> cb) {
    DWORD now = GetTickCount();
    g_saveRecent.erase(std::remove_if(g_saveRecent.begin(), g_saveRecent.end(), [&](auto& r) { return now - r.second > 10000; }), g_saveRecent.end());
    g_saveTicks.erase(std::remove_if(g_saveTicks.begin(), g_saveTicks.end(), [&](DWORD t) { return now - t > 1000; }), g_saveTicks.end());
    for (auto& r : g_saveRecent) if (r.first == url) { cb(L""); return; }          // the same address again within 10 s: ignored
    if (g_saveTicks.size() >= 2 || g_saveQ.size() >= 5) { cb(L""); return; }       // more than 2 requests in one second: the extras are cancelled
    g_saveTicks.push_back(now); g_saveRecent.push_back({ url, now });
    g_saveQ.push_back({ name, url, tab, std::move(cb) });
    PostMessage(m_hWnd, WM_APP + 40, 0, 0);
}

static std::wstring SaveDirOf(const std::wstring& p) { size_t s = p.find_last_of(L"\\/"); return s == std::wstring::npos ? L"" : p.substr(0, s); }

std::wstring BrowserWindow::SaveAskPath(const std::wstring& nameIn) {
    std::wstring name = SafeName(nameIn); if (name.empty()) name = L"download";
    wchar_t th[MAX_PATH];
    if (TestEnv(L"LIBRA_TEST_SAVEPATH", th, MAX_PATH) > 0) {   // tests only: answer the window with a folder (or CANCEL); every window is logged
        std::wstring f = th; WriteAllRaw(f + L"\\savedlg.log", ToUtf8(L"dialog:" + name + L"@" + std::to_wstring(GetTickCount()) + L"\n"), true);
        if (f == L"CANCEL" || !PathIsDirectoryW(f.c_str())) return L"";
        return UniquePath(f + L"\\" + name);
    }
    wil::com_ptr<IFileSaveDialog> d; if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) return L"";
    DWORD o = 0; d->GetOptions(&o); d->SetOptions(o | FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM | FOS_NOREADONLYRETURN);
    d->SetTitle(L"Save as"); d->SetFileName(name.c_str());
    size_t dot = name.find_last_of(L'.'); std::wstring ext = (dot != std::wstring::npos && dot > 0 && name.size() - dot <= 6) ? name.substr(dot + 1) : L"";
    std::wstring spec = ext.empty() ? L"" : L"*." + ext, label = ext.empty() ? L"" : ext + L" file";
    COMDLG_FILTERSPEC fs[2] = { { label.c_str(), spec.c_str() }, { L"All files", L"*.*" } };
    if (!ext.empty()) { d->SetFileTypes(2, fs); d->SetFileTypeIndex(1); d->SetDefaultExtension(ext.c_str()); }
    std::wstring dir = m_set.Get(L"dl_last_dir"); if (dir.empty() || !PathIsDirectoryW(dir.c_str())) dir = DownloadsDir(m_set);
    wil::com_ptr<IShellItem> fld; if (SUCCEEDED(SHCreateItemFromParsingName(dir.c_str(), nullptr, IID_PPV_ARGS(&fld)))) d->SetFolder(fld.get());
    if (FAILED(d->Show(m_hWnd))) return L"";
    wil::com_ptr<IShellItem> r; if (FAILED(d->GetResult(&r)) || !r) return L"";
    wil::unique_cotaskmem_string p; if (FAILED(r->GetDisplayName(SIGDN_FILESYSPATH, &p)) || !p.get()) return L"";
    std::wstring path = p.get();
    if (path.size() < 4 || path.size() > 259 || path.compare(0, 2, L"\\\\") == 0) return L"";   // local drives only
    if (!m_incognito) m_set.Set(L"dl_last_dir", SaveDirOf(path));
    return path;
}

void BrowserWindow::SaveNext() {
    if (g_saveBusy || g_saveQ.empty()) return;
    DWORD gap = GetTickCount() - g_saveLast;
    if (g_saveLast && gap < 1000) { SetTimer(m_hWnd, 2060, 1010 - gap, nullptr); return; }   // at least 1 s between two windows
    g_saveBusy = true; SaveReq r = std::move(g_saveQ.front()); g_saveQ.pop_front();
    std::wstring path;
    if (!r.tab || IndexOf(r.tab) >= 0) path = SaveAskPath(r.name);   // (the tab was closed meanwhile: cancelled, no window)
    g_saveLast = GetTickCount(); g_saveBusy = false;
    r.cb(path);
    if (!g_saveQ.empty()) SetTimer(m_hWnd, 2060, 1010, nullptr);
}
bool BrowserWindow::SaveTimer(UINT_PTR id) { if (id != 2060) return false; KillTimer(m_hWnd, 2060); SaveNext(); return true; }

// The old fast-path code of DownloadStarting, now run only AFTER the user chose a path (chosen empty = cancelled).
void BrowserWindow::DlContinue(wil::com_ptr<ICoreWebView2DownloadStartingEventArgs> a, wil::com_ptr<ICoreWebView2Deferral> def, wil::com_ptr<ICoreWebView2DownloadOperation> op,
                               wil::com_ptr<ICoreWebView2> snd, const std::wstring& name, const std::wstring& url, const std::wstring& chosen) {
    if (chosen.empty()) { a->put_Cancel(TRUE); if (def) def->Complete(); return; }
    int port = DohProxy::Get().Port();
    if (!IsHttp(url) || !port) { AdoptDownload(a.get(), op.get(), name, chosen); if (def) def->Complete(); return; }
    wil::unique_cotaskmem_string src; snd->get_Source(&src);
    std::wstring ref = (src.get() && IsHttp(src.get())) ? src.get() : L"";
    std::wstring ua;
    { wil::com_ptr<ICoreWebView2Settings> st; wil::com_ptr<ICoreWebView2Settings2> st2;
      if (SUCCEEDED(snd->get_Settings(&st)) && SUCCEEDED(st->QueryInterface(IID_PPV_ARGS(&st2)))) { wil::unique_cotaskmem_string u; st2->get_UserAgent(&u); if (u.get()) ua = u.get(); } }
    auto job = std::make_shared<DlJob>(); job->url = url; job->port = port;
    m_pending[job.get()] = PendingDl{ a, def, op, name, chosen };
    auto clean = [](std::wstring s) { for (auto& c : s) if (c == L'\r' || c == L'\n') c = L' '; return s; };
    auto begin = [this, job, ref, ua, clean](std::wstring cookie) {
        job->ref = ref;
        job->hdr = (ref.empty() ? L"" : L"Referer: " + clean(ref) + L"\r\n") + (ua.empty() ? L"" : L"User-Agent: " + clean(ua) + L"\r\n") +
                   (cookie.empty() ? L"" : L"Cookie: " + clean(cookie) + L"\r\n");
        HWND hw = m_hWnd; int pr = job->port;
        std::thread([hw, job, pr] {      // never touch WebView2 objects here: only the job, then hand back to the UI thread
            long long total = 0;
            bool ok = dl::Probe(pr, job->url, job->hdr, total) && total >= (4ll << 20);   // small files: not worth splitting
            job->total = total;
            PostMessage(hw, WM_APP + 3, ok ? 1 : 0, (LPARAM) new std::shared_ptr<DlJob>(job));
        }).detach();
    };
    wil::com_ptr<ICoreWebView2_2> w2; wil::com_ptr<ICoreWebView2CookieManager> cm;
    if (SUCCEEDED(snd->QueryInterface(IID_PPV_ARGS(&w2))) && SUCCEEDED(w2->get_CookieManager(&cm)))
        cm->GetCookies(url.c_str(), Microsoft::WRL::Callback<ICoreWebView2GetCookiesCompletedHandler>(
            [begin](HRESULT, ICoreWebView2CookieList* list) -> HRESULT {
                std::wstring c; UINT n = 0;
                if (list) { list->get_Count(&n);
                    for (UINT i = 0; i < n; ++i) { wil::com_ptr<ICoreWebView2Cookie> ck; if (FAILED(list->GetValueAtIndex(i, &ck))) continue;
                        wil::unique_cotaskmem_string nm, vl; ck->get_Name(&nm); ck->get_Value(&vl);
                        if (nm.get() && vl.get()) c += std::wstring(nm.get()) + L"=" + vl.get() + L"; "; } }
                begin(c); return S_OK; }).Get());
    else begin(L"");
}
