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

void BrowserWindow::PrivSendStats(Tab* t) {
    if (!t->webView) return;
    std::wstring base = ProfileDir() + L"\\Default\\";
    long long site = 0, cache = 0;
    for (const wchar_t* n : { L"Local Storage", L"IndexedDB", L"Service Worker", L"File System", L"Session Storage" }) site += DirSize(base + n);
    for (const wchar_t* n : { L"Cache", L"Code Cache", L"GPUCache", L"DawnGraphiteCache", L"DawnWebGPUCache" }) cache += DirSize(base + n);
    cache += DirSize(ProfileDir() + L"\\GrShaderCache") + DirSize(ProfileDir() + L"\\ShaderCache");
    int dns = (int)DohProxy::Get().CacheSize();
    int blk = (int)DohProxy::Get().BlockedHosts().size();
    int rec = (int)Lines(ReadAll(DataDir() + L"\\sites.txt")).size() + (int)m_closed.size();
    std::wstring head = L"{hist:" + std::to_wstring(m_hist.size()) + L",site:" + std::to_wstring(site) + L",cache:" + std::to_wstring(cache) + L",dns:" + std::to_wstring(dns) +
                        L",blk:" + std::to_wstring(blk) + L",rec:" + std::to_wstring(rec) + L",trk:" + std::to_wstring(m_totalBlocked) + L",ctr:" + std::to_wstring(m_totalBlocked);
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

void BrowserWindow::PrivClear(Tab* t, const std::set<std::wstring>& keys, const std::wstring& range) {
    long long now = (long long)time(nullptr), span = range == L"hour" ? 3600 : range == L"day" ? 86400 : range == L"week" ? 604800 : 0, since = span ? now - span : 0;
    int nh = 0;
    if (keys.count(L"hist")) {
        size_t before = m_hist.size();
        m_hist.erase(std::remove_if(m_hist.begin(), m_hist.end(), [&](const HistoryItem& h) { return !span || h.t >= since; }), m_hist.end());
        nh = (int)(before - m_hist.size()); SaveHistory();
    }
    if (keys.count(L"dns")) DohProxy::Get().ClearCache();
    if (keys.count(L"rec")) { DeleteFileW((DataDir() + L"\\sites.txt").c_str()); m_closed.clear(); }
    if (keys.count(L"ctr")) { m_totalBlocked = 0; m_totalDirty = true; m_set.Set(L"blocked_total", L"0"); m_stat.clear(); m_statLoaded = true; DeleteFileW((DataDir() + L"\\stats.txt").c_str()); m_statDirty = false; for (auto& tb : m_tabs) { tb->blockedReq = 0; tb->letThrough = 0; tb->blockedHosts.clear(); } }
    DWORD kinds = 0;
    if (keys.count(L"hist")) kinds |= COREWEBVIEW2_BROWSING_DATA_KINDS_BROWSING_HISTORY;
    if (keys.count(L"cook")) kinds |= COREWEBVIEW2_BROWSING_DATA_KINDS_COOKIES;
    if (keys.count(L"site")) kinds |= COREWEBVIEW2_BROWSING_DATA_KINDS_ALL_DOM_STORAGE | COREWEBVIEW2_BROWSING_DATA_KINDS_SERVICE_WORKERS;
    if (keys.count(L"cache")) kinds |= COREWEBVIEW2_BROWSING_DATA_KINDS_DISK_CACHE;
    auto done = [this, t, nh]() {
        if (IndexOf(t) < 0) return;
        PrivSendStats(t);
        t->webView->ExecuteScript((L"setTimeout(()=>lbCleared({},'<b>Cleared.</b> " + std::to_wstring(nh) + L" history entries removed.'),900)").c_str(), nullptr);
    };
    wil::com_ptr<ICoreWebView2_13> w13; wil::com_ptr<ICoreWebView2Profile> prof; wil::com_ptr<ICoreWebView2Profile2> p2;
    if (kinds && t->webView && SUCCEEDED(t->webView->QueryInterface(IID_PPV_ARGS(&w13))) && SUCCEEDED(w13->get_Profile(&prof)) && SUCCEEDED(prof->QueryInterface(IID_PPV_ARGS(&p2)))) {
        auto h = Microsoft::WRL::Callback<ICoreWebView2ClearBrowsingDataCompletedHandler>([done](HRESULT) -> HRESULT { done(); return S_OK; });
        if (span) p2->ClearBrowsingDataInTimeRange((COREWEBVIEW2_BROWSING_DATA_KINDS)kinds, (double)since, (double)(now + 3600), h.Get());
        else p2->ClearBrowsingData((COREWEBVIEW2_BROWSING_DATA_KINDS)kinds, h.Get());   // all time
    } else done();
}

void BrowserWindow::OnPrivPage(int idx, const std::vector<std::wstring>& v) {
    if (idx < 0 || idx >= (int)m_tabs.size() || m_tabs[idx]->url != L"libra://privacy") return;
    Tab* t = m_tabs[idx].get();
    if (v[0] == L"priv.stats") { PrivSendStats(t); PrivSendReport(t); }
    else if (v[0] == L"priv.clear" && v.size() == 3) {
        std::set<std::wstring> keys; size_t s = 0;
        for (;;) { size_t e = v[1].find(L',', s); std::wstring k = v[1].substr(s, e == std::wstring::npos ? e : e - s);
                   if (k == L"hist" || k == L"cook" || k == L"site" || k == L"cache" || k == L"dns" || k == L"rec" || k == L"ctr") keys.insert(k);
                   if (e == std::wstring::npos) break; s = e + 1; }
        if (v[2] != L"hour" && v[2] != L"day" && v[2] != L"week" && v[2] != L"all") return;
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
