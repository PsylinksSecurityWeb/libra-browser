// Privacy page + default-browser support. Included at the end of BrowserWindow.cpp.
#include <shobjidl.h>

static long long DirSize(const std::wstring& dir, int depth = 0) {
    long long n = 0; WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
    if (h == INVALID_HANDLE_VALUE) return 0;
    do {
        if (fd.cFileName[0] == L'.' && (!fd.cFileName[1] || (fd.cFileName[1] == L'.' && !fd.cFileName[2]))) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { if (depth < 8) n += DirSize(dir + L"\\" + fd.cFileName, depth + 1); }
        else n += ((long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return n;
}
static std::wstring ProfileDir() { return DataDir() + L"\\UserData\\EBWebView"; }

static long long PrivCacheBytes() {   // what "cached files" means to a person: the web cache and the code cache (GPU and shader caches are not user data)
    const std::wstring b = ProfileDir() + L"\\Default\\"; return DirSize(b + L"Cache") + DirSize(b + L"Code Cache");
}
static long long PrivSiteBytes() {
    const std::wstring b = ProfileDir() + L"\\Default\\"; long long s = 0;
    for (const wchar_t* n : { L"Local Storage", L"IndexedDB", L"Service Worker", L"File System", L"Session Storage" }) s += DirSize(b + n);
    return s;
}
static void RmTree(const std::wstring& d, int depth = 0) {   // delete a folder and everything in it (never follows links)
    WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileExW((d + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, 0);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (fd.cFileName[0] == L'.' && (!fd.cFileName[1] || (fd.cFileName[1] == L'.' && !fd.cFileName[2]))) continue;
            std::wstring p = d + L"\\" + fd.cFileName;
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) { if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) RemoveDirectoryW(p.c_str()); else DeleteFileW(p.c_str()); continue; }
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { if (depth < 10) RmTree(p, depth + 1); } else { SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL); DeleteFileW(p.c_str()); }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(d.c_str());
}
static void PrivWipeCachesAtStart() {   // "cached files" also means the GPU, shader and code caches: the engine locks them while it runs, so they are removed here, before it starts
    static bool once = false; if (once) return; once = true;
    const std::wstring flag = DataDir() + L"\\wipe_cache.flag"; if (GetFileAttributesW(flag.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    const std::wstring p = ProfileDir();
    for (const wchar_t* n : { L"\\GrShaderCache", L"\\ShaderCache", L"\\Default\\GPUCache", L"\\Default\\DawnGraphiteCache", L"\\Default\\DawnWebGPUCache", L"\\Default\\Code Cache", L"\\Default\\Cache" }) RmTree(p + n);
    DeleteFileW(flag.c_str());
}

void BrowserWindow::PrivSendStats(Tab* t) {
    if (!t->webView) return;
    long long site = PrivSiteBytes(), cache = PrivCacheBytes();
    int dns = (int)DohProxy::Get().CacheSize();
    int blk = std::max((int)DohProxy::Get().BlockedHosts().size(), (int)Verdicts::Get().Count());
    int rec = (int)Lines(ReadAll(DataDir() + L"\\sites.txt")).size() + (int)m_closed.size();
    int ndl = 0; for (auto& d : m_dl) if (d.state != 0) ++ndl;
    int agt = (int)Lines(ReadAll(DataDir() + L"\\memory.dat")).size() + (int)Lines(ReadAll(DataDir() + L"\\agent_perms.txt")).size() + (DirSize(DataDir() + L"\\sandbox") > 0 ? 1 : 0);
    std::wstring head = L"{dl:" + std::to_wstring(ndl) + L",agt:" + std::to_wstring(agt) + L",hist:" + std::to_wstring(m_hist.size()) + L",site:" + std::to_wstring(site) + L",cache:" + std::to_wstring(cache) + L",dns:" + std::to_wstring(dns) +
                        L",blk:" + std::to_wstring(blk) + L",vl:" + std::to_wstring(Verdicts::Get().localHits.load()) + L",vc:" + std::to_wstring(Verdicts::Get().cloudAsks.load()) + L",rec:" + std::to_wstring(rec) + L",trk:" + std::to_wstring(m_totalBlocked) + L",ctr:" + std::to_wstring(m_totalBlocked);
    auto send = [this, t](std::wstring js) { if (IndexOf(t) >= 0 && t->webView) t->webView->ExecuteScript(js.c_str(), nullptr); };
    wil::com_ptr<ICoreWebView2_2> w2; wil::com_ptr<ICoreWebView2CookieManager> cm;
    if (SUCCEEDED(t->webView->QueryInterface(IID_PPV_ARGS(&w2))) && SUCCEEDED(w2->get_CookieManager(&cm))) {
        cm->GetCookies(L"", Microsoft::WRL::Callback<ICoreWebView2GetCookiesCompletedHandler>(
            [head, send](HRESULT, ICoreWebView2CookieList* l) -> HRESULT {
                UINT n = 0; std::set<std::wstring> dom;
                if (l) { l->get_Count(&n); for (UINT i = 0; i < n && i < 20000; ++i) { wil::com_ptr<ICoreWebView2Cookie> c; if (SUCCEEDED(l->GetValueAtIndex(i, &c))) { wil::unique_cotaskmem_string d; c->get_Domain(&d); if (d) dom.insert(d.get()); } } }
                send(L"lbStats(" + head + L",cook:" + std::to_wstring(n) + L",cdom:" + std::to_wstring(dom.size()) + L"})");
                return S_OK;
            }).Get());
    } else send(L"lbStats(" + head + L",cook:0,cdom:0})");
}

// ---- clearing: every kind is its own call whose result is read, then a check and a second sweep, then an honest message ----
static void PrivCookieCount(Tab* t, std::function<void(int)> cb) {   // -1 = could not count
    wil::com_ptr<ICoreWebView2_2> w2; wil::com_ptr<ICoreWebView2CookieManager> cm;
    if (!t->webView || FAILED(t->webView->QueryInterface(IID_PPV_ARGS(&w2))) || FAILED(w2->get_CookieManager(&cm)) ||
        FAILED(cm->GetCookies(L"", Microsoft::WRL::Callback<ICoreWebView2GetCookiesCompletedHandler>([cb](HRESULT hr, ICoreWebView2CookieList* l) -> HRESULT { UINT n = 0; if (l) l->get_Count(&n); cb(SUCCEEDED(hr) ? (int)n : -1); return S_OK; }).Get()))) cb(-1);
}
struct PrivRun {
    Tab* t = nullptr; std::set<std::wstring> keys; bool span = false; long long now = 0, since = 0, cacheBefore = 0, cacheAfter = 0, siteBefore = 0, siteAfter = 0; int nh = 0, cookBefore = -1, cookAfter = -1;
    std::vector<std::pair<COREWEBVIEW2_BROWSING_DATA_KINDS, std::wstring>> steps; size_t i = 0; std::vector<std::wstring> failed;
    std::function<void()> engine, sweep, finish;   // they hold only a weak pointer to this run: no cycle
};
static std::wstring PrivHr(HRESULT hr) { wchar_t b[24]; swprintf(b, 24, L" (error 0x%08X)", (unsigned)hr); return b; }

void BrowserWindow::PrivClear(Tab* t, const std::set<std::wstring>& keys, const std::wstring& range) {
    auto run = std::make_shared<PrivRun>(); run->t = t; run->keys = keys; run->now = (long long)time(nullptr);
    const long long span = range == L"hour" ? 3600 : range == L"day" ? 86400 : range == L"week" ? 604800 : range == L"month" ? 2419200 : 0;
    run->span = span > 0; run->since = span ? run->now - span : 0;
    const long long since = run->since;
    if (keys.count(L"hist")) {
        size_t before = m_hist.size();
        m_hist.erase(std::remove_if(m_hist.begin(), m_hist.end(), [&](const HistoryItem& h) { return !span || h.t >= since; }), m_hist.end());
        run->nh = (int)(before - m_hist.size()); SaveHistory();
    }
    if (keys.count(L"dns")) DohProxy::Get().ClearCache();
    if (keys.count(L"rec")) { DeleteFileW((DataDir() + L"\\sites.txt").c_str()); DeleteFileW((DataDir() + L"\\session.txt").c_str()); DeleteFileW((DataDir() + L"\\spons.dat").c_str()); m_sp = SpFeed(); m_closed.clear(); }
    if (keys.count(L"dl")) m_dl.erase(std::remove_if(m_dl.begin(), m_dl.end(), [](const DownloadItem& d) { return d.state != 0; }), m_dl.end());   // the list only, never the files
    if (keys.count(L"agt")) AgWipe();
    if (keys.count(L"ctr")) { m_totalBlocked = 0; m_totalDirty = true; m_set.Set(L"blocked_total", L"0"); m_stat.clear(); m_statLoaded = true; DeleteFileW((DataDir() + L"\\stats.txt").c_str()); m_statDirty = false; for (auto& tb : m_tabs) { tb->blockedReq = 0; tb->letThrough = 0; tb->blockedHosts.clear(); } }
    if (keys.count(L"hist")) run->steps.push_back({ (COREWEBVIEW2_BROWSING_DATA_KINDS)(COREWEBVIEW2_BROWSING_DATA_KINDS_BROWSING_HISTORY | COREWEBVIEW2_BROWSING_DATA_KINDS_DOWNLOAD_HISTORY), L"history" });
    if (keys.count(L"cook")) {
        run->steps.push_back({ COREWEBVIEW2_BROWSING_DATA_KINDS_COOKIES, L"cookies" });
        run->steps.push_back({ (COREWEBVIEW2_BROWSING_DATA_KINDS)(COREWEBVIEW2_BROWSING_DATA_KINDS_PASSWORD_AUTOSAVE | COREWEBVIEW2_BROWSING_DATA_KINDS_GENERAL_AUTOFILL), L"sign-in data" });
    }
    if (keys.count(L"site")) {
        run->steps.push_back({ (COREWEBVIEW2_BROWSING_DATA_KINDS)(COREWEBVIEW2_BROWSING_DATA_KINDS_ALL_DOM_STORAGE | COREWEBVIEW2_BROWSING_DATA_KINDS_SERVICE_WORKERS | COREWEBVIEW2_BROWSING_DATA_KINDS_CACHE_STORAGE), L"site data" });
        run->steps.push_back({ COREWEBVIEW2_BROWSING_DATA_KINDS_SETTINGS, L"site settings" });
    }
    if (keys.count(L"cache")) run->steps.push_back({ COREWEBVIEW2_BROWSING_DATA_KINDS_DISK_CACHE, L"cached files" });
    run->cacheBefore = PrivCacheBytes(); run->siteBefore = PrivSiteBytes();
    std::weak_ptr<PrivRun> w = run;

    run->finish = [this, w]() {   // numbers after, the message, and the page
        auto run = w.lock(); if (!run) return;
        run->cacheAfter = PrivCacheBytes(); run->siteAfter = PrivSiteBytes();
        if (run->keys.count(L"cache") && !run->span) WriteAll(DataDir() + L"\\wipe_cache.flag", "1");   // GPU, shader and code caches go at the next start
        std::wstring msg;
        if (run->failed.empty()) msg = L"<b>Cleared.</b>";
        else { msg = L"<b>Could not clear:</b> "; for (size_t k = 0; k < run->failed.size(); ++k) msg += (k ? L", " : L"") + run->failed[k]; msg += L". Please try again."; }
        std::vector<std::wstring> parts;
        if (run->keys.count(L"hist") && run->nh > 0) parts.push_back(std::to_wstring(run->nh) + L" history entries");
        if (run->keys.count(L"cook") && run->cookBefore > std::max(0, run->cookAfter)) parts.push_back(std::to_wstring(run->cookBefore - std::max(0, run->cookAfter)) + L" cookies");
        if (run->keys.count(L"cache") && run->cacheBefore > run->cacheAfter) parts.push_back(Fmt(run->cacheBefore - run->cacheAfter) + L" of cached files");
        if (run->keys.count(L"site") && run->siteBefore > run->siteAfter + 1024) parts.push_back(Fmt(run->siteBefore - run->siteAfter) + L" of site data");
        if (!parts.empty()) { msg += L" Removed "; for (size_t k = 0; k < parts.size(); ++k) msg += (k ? L", " : L"") + parts[k]; msg += L"."; }
        else if (run->failed.empty()) msg = L"<b>Done.</b> Nothing was left to clear.";
        if (run->cookAfter > 0) msg += L" " + std::to_wstring(run->cookAfter) + L" cookie(s) were set again by pages that are open: close or reload them and clear once more.";
        Tab* t = run->t; if (IndexOf(t) < 0 || !t->webView) return;
        std::wstring plain = msg; for (const wchar_t* tg : { L"<b>", L"</b>" }) { size_t p; while ((p = plain.find(tg)) != std::wstring::npos) plain.erase(p, wcslen(tg)); }
        if (t->url == L"libra://history") { t->webView->ExecuteScript((L"lbDone(" + std::to_wstring(run->since) + L"," + (run->keys.count(L"hist") ? L"1" : L"0") + L",'" + plain + L"')").c_str(), nullptr); return; }   // no page reload: the page removes the rows itself
        if (t->url != L"libra://privacy") return;
        PrivSendStats(t);
        t->webView->ExecuteScript((L"setTimeout(()=>lbCleared({},'" + msg + L"'),900)").c_str(), nullptr);
    };
    run->sweep = [this, w]() {   // check, and sweep again whatever the first calls left (cookies, and storage of the pages that are open)
        auto run = w.lock(); if (!run) return;
        Tab* t = run->t; if (IndexOf(t) < 0 || !t->webView) return;
        if (!run->span && run->keys.count(L"site")) {
            std::set<std::wstring> origins;
            for (auto& tb : m_tabs) { wil::unique_cotaskmem_string s; if (!tb->incognito && tb->webView && SUCCEEDED(tb->webView->get_Source(&s)) && s.get()) { std::wstring o = OriginOf(s.get()); if (!o.empty()) origins.insert(o); } }
            for (auto& o : origins) t->webView->CallDevToolsProtocolMethod(L"Storage.clearDataForOrigin", (L"{\"origin\":" + JsLit(o) + L",\"storageTypes\":\"all\"}").c_str(), nullptr);
        }
        if (run->span || !run->keys.count(L"cook")) { run->finish(); return; }
        PrivCookieCount(t, [this, run](int n) {
            if (n <= 0) { run->cookAfter = n < 0 ? -1 : 0; run->finish(); return; }
            wil::com_ptr<ICoreWebView2_2> w2; wil::com_ptr<ICoreWebView2CookieManager> cm;   // some are left: the cookie manager and the debugger remove every one
            if (run->t->webView && SUCCEEDED(run->t->webView->QueryInterface(IID_PPV_ARGS(&w2))) && SUCCEEDED(w2->get_CookieManager(&cm))) cm->DeleteAllCookies();
            run->t->webView->CallDevToolsProtocolMethod(L"Network.clearBrowserCookies", L"{}", Microsoft::WRL::Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>(
                [run](HRESULT, LPCWSTR) -> HRESULT { PrivCookieCount(run->t, [run](int n2) { run->cookAfter = n2; run->finish(); }); return S_OK; }).Get());
        });
    };
    run->engine = [this, w]() {   // one engine call per kind, in turn
        auto run = w.lock(); if (!run) return;
        if (run->i >= run->steps.size()) { run->sweep(); return; }
        Tab* t = run->t; const auto kind = run->steps[run->i].first; const std::wstring name = run->steps[run->i].second;
        wchar_t fl[64]; wil::com_ptr<ICoreWebView2_13> w13; wil::com_ptr<ICoreWebView2Profile> prof; wil::com_ptr<ICoreWebView2Profile2> p2;
        if (TestEnv(L"LIBRA_TEST_CLEARFAIL", fl, 64) && name == fl) { run->failed.push_back(name + L" (test)"); ++run->i; run->engine(); return; }   // tests only: pretend this kind failed
        if (IndexOf(t) < 0 || !t->webView || FAILED(t->webView->QueryInterface(IID_PPV_ARGS(&w13))) || FAILED(w13->get_Profile(&prof)) || FAILED(prof->QueryInterface(IID_PPV_ARGS(&p2)))) { run->failed.push_back(name); ++run->i; run->engine(); return; }
        auto h = Microsoft::WRL::Callback<ICoreWebView2ClearBrowsingDataCompletedHandler>([run, name](HRESULT hr) -> HRESULT { if (FAILED(hr)) run->failed.push_back(name + PrivHr(hr)); ++run->i; run->engine(); return S_OK; });
        HRESULT r0 = run->span ? p2->ClearBrowsingDataInTimeRange(kind, (double)run->since, (double)(run->now + 3600), h.Get()) : p2->ClearBrowsingData(kind, h.Get());
        if (FAILED(r0)) { run->failed.push_back(name + PrivHr(r0)); ++run->i; run->engine(); }   // the handler will not be called
    };
    if (keys.count(L"cook")) PrivCookieCount(t, [run](int n) { run->cookBefore = n; run->engine(); });
    else run->engine();
}

void BrowserWindow::WipeAll(int idx) {   // everything Libra remembers, all time. Kept: API key, bookmarks, saved logins, settings.
    if (idx < 0 || idx >= (int)m_tabs.size()) return;
    PrivClear(m_tabs[idx].get(), { L"hist", L"cook", L"site", L"cache", L"dns", L"rec", L"ctr", L"dl", L"agt" }, L"all");
}

void BrowserWindow::OnPrivPage(int idx, const std::vector<std::wstring>& v) {
    if (idx < 0 || idx >= (int)m_tabs.size() || (m_tabs[idx]->url != L"libra://privacy" && !(m_tabs[idx]->url == L"libra://history" && v[0] == L"priv.clear"))) return;
    Tab* t = m_tabs[idx].get();
    if (v[0] == L"priv.stats") { PrivSendStats(t); PrivSendReport(t); }
    else if (v[0] == L"priv.clear" && v.size() == 3) {
        std::set<std::wstring> keys; size_t s = 0;
        for (;;) { size_t e = v[1].find(L',', s); std::wstring k = v[1].substr(s, e == std::wstring::npos ? e : e - s);
                   if (k == L"hist" || k == L"cook" || k == L"site" || k == L"cache" || k == L"dns" || k == L"rec" || k == L"ctr" || k == L"dl" || k == L"agt") keys.insert(k);
                   if (e == std::wstring::npos) break; s = e + 1; }
        if (v[2] != L"hour" && v[2] != L"day" && v[2] != L"week" && v[2] != L"month" && v[2] != L"all") return;
        PrivClear(t, keys, v[2]);
    }
}

// ── default browser ──
static bool RegSetStr(const std::wstring& path, const wchar_t* name, const std::wstring& val) {   // HKCU only
    HKEY k; if (RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
    LONG r = RegSetValueExW(k, name, 0, REG_SZ, (const BYTE*)val.c_str(), (DWORD)((val.size() + 1) * sizeof(wchar_t))); RegCloseKey(k); return r == ERROR_SUCCESS;
}
bool RegisterAsBrowser() {   // lists Libra in Windows "Default apps"; Windows itself makes the user confirm the choice
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
    const std::wstring ex = exe, cmd = L"\"" + ex + L"\" \"%1\"", C = L"Software\\Classes\\", S = L"Software\\Clients\\StartMenuInternet\\LibraBrowser";
    bool ok = true;
    for (const wchar_t* id : { L"LibraHTML", L"LibraURL" }) {
        ok &= RegSetStr(C + id, nullptr, id == std::wstring(L"LibraURL") ? L"Libra Browser URL" : L"Libra Browser Document");
        ok &= RegSetStr(C + id + L"\\DefaultIcon", nullptr, ex + L",0");
        ok &= RegSetStr(C + id + L"\\shell\\open\\command", nullptr, cmd);
    }
    ok &= RegSetStr(C + L"LibraURL", L"URL Protocol", L"");
    ok &= RegSetStr(S, nullptr, L"Libra Browser");
    ok &= RegSetStr(S + L"\\Capabilities", L"ApplicationName", L"Libra Browser");
    ok &= RegSetStr(S + L"\\Capabilities", L"ApplicationDescription", L"Secure, private and lightweight browser by Psylinks Security Private Limited");
    for (const wchar_t* p : { L"http", L"https" }) ok &= RegSetStr(S + L"\\Capabilities\\URLAssociations", p, L"LibraURL");
    for (const wchar_t* e : { L".htm", L".html" }) ok &= RegSetStr(S + L"\\Capabilities\\FileAssociations", e, L"LibraHTML");
    ok &= RegSetStr(S + L"\\DefaultIcon", nullptr, ex + L",0");
    ok &= RegSetStr(S + L"\\shell\\open\\command", nullptr, L"\"" + ex + L"\"");
    ok &= RegSetStr(L"Software\\RegisteredApplications", L"LibraBrowser", S + L"\\Capabilities");
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return ok;
}
bool IsDefaultBrowser() {
    wil::com_ptr<IApplicationAssociationRegistration> r;
    if (FAILED(CoCreateInstance(CLSID_ApplicationAssociationRegistration, nullptr, CLSCTX_INPROC, IID_PPV_ARGS(&r)))) return false;
    wil::unique_cotaskmem_string p;
    return SUCCEEDED(r->QueryCurrentDefault(L"https", AT_URLPROTOCOL, AL_EFFECTIVE, &p)) && p && wcscmp(p.get(), L"LibraURL") == 0;
}
void BrowserWindow::SetAsDefault() {
    RegisterAsBrowser();
    ShellExecuteW(nullptr, L"open", L"ms-settings:defaultapps?registeredAppUser=LibraBrowser", nullptr, nullptr, SW_SHOWNORMAL);
}

void BrowserWindow::FirstBlock() {   // once ever: "Wow, you just blocked N ads and M trackers"
    if (m_incognito || m_setup || m_set.On(L"first_block_shown") || m_activeTab < 0 || m_activeTab >= (int)m_tabs.size()) return;
    if (m_pwKind && m_hPw && IsWindowVisible(m_hPw)) return;
    Tab* t = m_tabs[m_activeTab].get();
    if (!IsHttp(t->url) || t->loading || t->blockedReq < 5) return;
    m_fbAds = t->blockedAds; m_fbTrk = t->blockedReq - t->blockedAds;
    ShowPw(3, t, HostOf(t->url));
    if (m_pwKind == 3) m_set.Set(L"first_block_shown", L"1");
}

// ---- Protection report: blocked requests per day and host, kept 90 days, local file only ----
void BrowserWindow::StatLoad() {
    m_statLoaded = true; int cut = StatDay(90);
    for (auto& ln : Lines(ReadAll(DataDir() + L"\\stats.txt"))) {
        std::string u = ToUtf8(ln); int d = 0, n = 0; char host[260] = {};
        if (sscanf_s(u.c_str(), "%d\t%d\t%259s", &d, &n, host, (unsigned)sizeof(host)) == 3 && d >= cut && n > 0) m_stat[{ d, host }] += n;
    }
}
void BrowserWindow::StatSave() {
    m_statDirty = false; int cut = StatDay(90); std::string o;
    for (auto it = m_stat.begin(); it != m_stat.end();) {
        if (it->first.first < cut) { it = m_stat.erase(it); continue; }
        o += std::to_string(it->first.first) + "\t" + std::to_string(it->second) + "\t" + it->first.second + "\n"; ++it;
    }
    WriteAll(DataDir() + L"\\stats.txt", o);
}
void BrowserWindow::PrivSendReport(Tab* t) {
    if (!m_statLoaded) StatLoad();
    int cnt[7][4] = {}; int day[7]; for (int i = 0; i < 7; ++i) day[6 - i] = StatDay(i);
    std::map<std::string, int> tot;
    for (auto& kv : m_stat) {
        const std::string& h = kv.first.second; bool ok = !h.empty();
        for (char c : h) if (!(isalnum((unsigned char)c) || c == '.' || c == '-' || c == '_')) ok = false;
        if (!ok) continue;
        tot[h] += kv.second;
        for (int i = 0; i < 7; ++i) if (day[i] == kv.first.first) cnt[i][DohProxy::Cat(h)] += kv.second;
    }
    std::vector<std::pair<int, std::string>> top; for (auto& kv : tot) top.push_back({ kv.second, kv.first });
    std::sort(top.begin(), top.end(), [](auto& a, auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
    std::wstring js = L"lbReport({days:[";
    for (int i = 0; i < 7; ++i) js += L"[" + std::to_wstring(day[i]) + L"," + std::to_wstring(cnt[i][0]) + L"," + std::to_wstring(cnt[i][1]) + L"," + std::to_wstring(cnt[i][2]) + L"," + std::to_wstring(cnt[i][3]) + L"],";
    js += L"],top:[";
    for (size_t i = 0; i < top.size() && i < 10; ++i) js += L"[\"" + FromUtf8(top[i].second) + L"\"," + std::to_wstring(DohProxy::Cat(top[i].second)) + L"," + std::to_wstring(top[i].first) + L"],";
    js += L"]})";
    if (t->webView) t->webView->ExecuteScript(js.c_str(), nullptr);
}
