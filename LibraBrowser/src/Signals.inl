// Anonymous counts (shown / clicked / blocked) -> one fixed-shape report per day. Included after Sponsor.inl.
// Nothing is sent unless sponsored content AND "anonymous counts" are on, never from private windows or Child Safety.
// Request: POST <feed host>/r, no cookies, no redirects, constant headers, fresh connection, fixed size. Sent at a random moment.

static bool SpPost(int port, bool direct, const std::wstring& url, const std::string& body) {   // true only for HTTP 204
    dl::Url u; if (!dl::Crack(url, u)) return false;
    HINTERNET ses = direct ? WinHttpOpen(L"Libra", WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0) : dl::Session(port, 1); if (!ses) return false;
    WinHttpSetTimeouts(ses, 5000, 5000, 8000, 12000); bool ok = false;
    if (HINTERNET con = WinHttpConnect(ses, u.host.c_str(), u.port, 0)) {
        if (HINTERNET rq = WinHttpOpenRequest(con, L"POST", u.path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, u.https ? WINHTTP_FLAG_SECURE : 0)) {
            DWORD nc = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_REDIRECTS; WinHttpSetOption(rq, WINHTTP_OPTION_DISABLE_FEATURE, &nc, sizeof nc);
            std::wstring h = std::wstring(kSpUa) + L"\r\nContent-Type: application/json";
            if (WinHttpSendRequest(rq, h.c_str(), (DWORD)-1, (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0) && WinHttpReceiveResponse(rq, nullptr)) {
                DWORD st = 0, sl = sizeof st; WinHttpQueryHeaders(rq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &st, &sl, nullptr); ok = st == 204;
            }
            WinHttpCloseHandle(rq);
        }
        WinHttpCloseHandle(con);
    }
    WinHttpCloseHandle(ses); return ok;
}
struct SnResult { bool ok; int bytes; SnLedger sent; };

bool BrowserWindow::SnOn() const { return SponsorOn() && m_set.Get(L"sn_count", L"1") != L"0"; }
static std::vector<std::string> SnActive(const SpFeed& f, long long now) {   // campaigns a report must list (all of them, zeros included)
    std::vector<std::string> v; if (f.disabled || now >= f.expires) return v;
    for (auto& it : f.items) if (it.start <= now && now <= it.end) v.push_back(it.id);
    return v;
}
void BrowserWindow::SnLoad() { SnParse(ReadAll(DataDir() + L"\\spstat.dat"), m_sn, m_snLog); }
void BrowserWindow::SnSave() { if (!m_snDirty || m_incognito) return; m_snDirty = false; CreateDirectoryW(DataDir().c_str(), nullptr); WriteAll(DataDir() + L"\\spstat.dat", SnSerialize(m_sn, m_snLog)); }
void BrowserWindow::SnLogAdd(const char* kind, int bytes, bool ok) {
    if (m_incognito || !SnOn()) return;
    m_snLog.push_back({ (long long)time(nullptr), kind, bytes, ok }); if (m_snLog.size() > 30) m_snLog.erase(m_snLog.begin()); m_snDirty = true;
}
void BrowserWindow::SnBlockedHit(const std::string& host) {   // called for every blocked name: only two totals are kept
    if (!SnOn()) return; SnBlocked(m_sn, DohProxy::Cat(host) == 0, (long long)time(nullptr)); m_snDirty = true;
}
void BrowserWindow::SnEvent(const std::wstring& kind, const std::wstring& idw) {   // from the home page only
    if (!SnOn()) return; std::string id = ToUtf8(idw); bool known = false; for (auto& it : m_sp.items) if (it.id == id) known = true; if (!known) return;
    static std::map<std::string, long long> last; long long now = (long long)time(nullptr), t = (long long)GetTickCount64() / 1000; std::string k = ToUtf8(kind) + id;
    auto f = last.find(k); if (f != last.end() && t - f->second < (kind == L"show" ? 10 : 2)) return; last[k] = t;   // a page can not pump the numbers
    if (kind == L"show") SnShown(m_sn, id, now); else if (kind == L"click") SnClick(m_sn, id); else return;
    m_snDirty = true; SnSave(); SnPush();
}
void BrowserWindow::SnKick(bool force) {
    if (m_snBusy || !SnOn()) return; SpCfg c = SpConfig(); if (c.keys.empty()) return;
    long long now = (long long)time(nullptr); auto act = SnActive(m_sp, now); SnPrune(m_sn, act);
    if (m_sn.since && now - m_sn.since > 3 * 86400) { m_sn = SnLedger(); m_snDirty = true; }   // old counts are dropped, never queued up
    if (!SnHasData(m_sn, act)) return;
    wchar_t tf[8]; bool fast = force || TestEnv(L"LIBRA_TEST_SNNOW", tf, 8);
    long long due = _wtoi64(m_set.Get(L"sn_due", L"0").c_str()), last = _wtoi64(m_set.Get(L"sn_last", L"0").c_str());
    if (!force && due > now) return;   // after a failure: wait (one retry about an hour later), never hammer
    if (!fast) {
        if (last <= now && now - last < 20 * 3600) return;
        if (due == 0) { unsigned r = 0; BCryptGenRandom(nullptr, (PUCHAR)&r, sizeof r, BCRYPT_USE_SYSTEM_PREFERRED_RNG); m_set.Set(L"sn_due", std::to_wstring(now + 600 + r % (6 * 3600))); return; }   // a random moment, not "at start"
        if (now < due) return;
    }
    int port = DohProxy::Get().Port(); if (!port && !c.test) return;
    SnLedger sent; std::string body = SnBuild(m_sn, act, &sent); std::wstring url = FromUtf8(c.base) + L"/r";
    m_snBusy = true; HWND hw = m_hWnd; bool direct = c.test;
    std::thread([hw, port, direct, url, body, sent] { PostMessage(hw, WM_APP + 38, 0, (LPARAM) new SnResult{ SpPost(port, direct, url, body), (int)body.size(), sent }); }).detach();
}
void BrowserWindow::SnGot(void* p) {
    m_snBusy = false; std::unique_ptr<SnResult> r((SnResult*)p); if (!r) return;
    long long now = (long long)time(nullptr); SnLogAdd("report", r->bytes, r->ok);
    if (r->ok) { SnSubtract(m_sn, r->sent); m_set.Set(L"sn_last", std::to_wstring(now)); m_set.Set(L"sn_due", L"0"); }
    else m_set.Set(L"sn_due", std::to_wstring(now + 3600));   // one retry later; counts older than 3 days are dropped
    m_snDirty = true; SnSave(); SnPush();
}
void BrowserWindow::SnClear() { m_sn = SnLedger(); m_snDirty = true; SnSave(); SnPush(); }
void BrowserWindow::SnWipe() { m_sn = SnLedger(); m_snLog.clear(); m_snDirty = false; DeleteFileW((DataDir() + L"\\spstat.dat").c_str()); m_set.Set(L"sn_due", L"0"); }

// Privacy page "What Libra sends": the exact next report (padding shortened for reading) and the log of requests to our servers.
void BrowserWindow::SnPush() {
    for (auto& tb : m_tabs) if (tb->url == L"libra://privacy" && tb->webView) {
        long long now = (long long)time(nullptr); bool cfg = !SpConfig().keys.empty(), on = SnOn();
        std::string body = SnBuild(m_sn, SnActive(m_sp, now), nullptr), shown = body; size_t pp = shown.find(",\"pad\":\"");
        if (pp != std::string::npos) shown = shown.substr(0, pp) + ",\"pad\":\"(filler up to " + std::to_string(body.size()) + " bytes)\"}";
        std::string esc; for (char ch : shown) { if (ch == '"' || ch == '\\') esc += '\\'; esc += ch; }
        std::wstring js = L"lbSn({cfg:" + std::wstring(cfg ? L"1" : L"0") + L",on:" + (on ? L"1" : L"0") + L",busy:" + (m_snBusy ? L"1" : L"0") + L",next:\"" + FromUtf8(esc) + L"\",log:[";
        for (auto& g : m_snLog) js += L"[" + std::to_wstring(g.t) + L",\"" + FromUtf8(g.kind) + L"\"," + std::to_wstring(g.bytes) + L"," + (g.ok ? L"1" : L"0") + L"],";
        js += L"]})"; tb->webView->ExecuteScript(js.c_str(), nullptr);
    }
}
