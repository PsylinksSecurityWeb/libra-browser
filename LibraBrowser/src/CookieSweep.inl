// Third-party cookie sweeper (PLAN-xsite.md P2 + P3). A tracker cookie set while you read page A is deleted before page B loads, so nothing can link A and B.
// Kept: sites you opened as a page (this session), cookies that were already in the profile at start (your sign-ins), installed apps, a short allow-list (sign-in, CAPTCHA, payments).
// P3: analytics ID cookies (_ga, _fbp, ...) are also removed when the browser closes, so a server-side tracker sees a new ID on every visit.
// Never in private windows or app windows; switched off while any open tab is on shield level Off; setting ck_sweep=0 turns it off.
static const wchar_t* const kCkAllow[] = { L"google.com", L"gstatic.com", L"google.co.in", L"googleapis.com", L"microsoftonline.com", L"live.com", L"microsoft.com", L"apple.com", L"hcaptcha.com", L"cloudflare.com",
    L"razorpay.com", L"paytm.com", L"juspay.in", L"cashfree.com", L"payu.in", L"billdesk.com", L"stripe.com", L"paypal.com", L"npci.org.in" };
struct CkC { std::wstring name, dom, path, pk; };
static std::vector<CkC> CkParse(const std::wstring& s) {   // Network.getAllCookies reply (the engine lists keys alphabetically, "domain" first)
    std::vector<CkC> out; const std::wstring mark = L"{\"domain\":\"";
    auto fld = [](const std::wstring& c, const wchar_t* key) { std::wstring k = L"\"" + std::wstring(key) + L"\":\"", o; size_t p = c.find(k); if (p == std::wstring::npos) return o; p += k.size(); while (p < c.size() && c[p] != L'"') { if (c[p] == L'\\' && p + 1 < c.size()) ++p; o += c[p++]; } return o; };
    for (size_t p = s.find(mark); p != std::wstring::npos;) {
        size_t nx = s.find(mark, p + 1); std::wstring c = s.substr(p, nx == std::wstring::npos ? std::wstring::npos : nx - p); p = nx;
        CkC k; k.name = fld(c, L"name"); k.dom = fld(c, L"domain"); k.path = fld(c, L"path");
        size_t q = c.find(L"\"partitionKey\":"); if (q != std::wstring::npos) { q += 15; if (q < c.size() && c[q] == L'{') { size_t e = c.find(L'}', q); if (e != std::wstring::npos) k.pk = c.substr(q, e - q + 1); } else if (q < c.size() && c[q] == L'"') { size_t e = c.find(L'"', q + 1); if (e != std::wstring::npos) k.pk = c.substr(q, e - q + 1); } }
        out.push_back(k);
    }
    return out;
}
static std::wstring CkDomain(std::wstring d) { for (auto& ch : d) ch = (wchar_t)towlower(ch); while (!d.empty() && d[0] == L'.') d.erase(0, 1); return RegDom(d); }
static bool CkAnalyticsName(const std::wstring& n) {   // advertising / analytics IDs only; nothing a login or a cart needs
    static const wchar_t* exact[] = { L"_ga", L"_gid", L"_fbp", L"_fbc", L"IDE", L"MUID", L"_clck", L"_clsk", L"_uetsid", L"_uetvid", L"_scid", L"_rdt_uuid", L"_pin_unauth", L"_ttp" };
    for (auto e : exact) if (n == e) return true;
    for (const wchar_t* p : { L"_ga_", L"_gat", L"_gcl_", L"_gac_", L"__utm" }) if (n.rfind(p, 0) == 0) return true;
    return false;
}
static void CkLog(const std::wstring& line) { wchar_t tf[MAX_PATH]; if (TestEnv(L"LIBRA_TEST_CKSWEEP", tf, MAX_PATH)) WriteAllRaw(tf, ToUtf8(line) + "\n", true); }   // tests only

bool BrowserWindow::CkOn() const { return !m_incognito && !m_app && m_set.Get(L"ck_sweep", L"1") != L"0"; }
void BrowserWindow::CkSeen(const std::wstring& url) { std::wstring h = HostOf(url); if (!h.empty()) m_ckSeen.insert(RegDom(h)); }
void BrowserWindow::CkInit(ICoreWebView2* wv) {   // the cookies that exist at start are yours (earlier sessions already swept the rest)
    if (m_ckInit || !CkOn() || !wv) return; m_ckInit = true;
    wil::com_ptr<ICoreWebView2> w(wv);
    w->CallDevToolsProtocolMethod(L"Network.getAllCookies", L"{}", Microsoft::WRL::Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>([this](HRESULT, LPCWSTR json) -> HRESULT {
        for (auto& c : CkParse(json ? json : L"")) m_ckLegacy.insert(CkDomain(c.dom));
        CkLog(L"legacy " + std::to_wstring(m_ckLegacy.size())); CkSweepActive(true); return S_OK; }).Get());
}
void BrowserWindow::CkSweep(ICoreWebView2* wv0, const std::wstring& next, bool closing) {
    if (!CkOn() || !wv0 || !m_ckInit) return;
    if (m_ckBusy) { m_ckAgain = true; return; }
    for (auto& t : m_tabs) if (IsHttp(t->url) && ModeFor(HostOf(t->url)) == 2) return;   // the owner switched protection off on an open site: leave cookies alone
    if (!next.empty() && IsHttp(next) && ModeFor(HostOf(next)) == 2) return;
    if (!m_appInstLoaded) AppLoadInstalled();
    std::set<std::wstring> keep = m_ckSeen; keep.insert(m_ckLegacy.begin(), m_ckLegacy.end());
    for (auto& t : m_tabs) { std::wstring h = HostOf(t->url); if (!h.empty()) keep.insert(RegDom(h)); }
    { std::wstring h = HostOf(next); if (!h.empty()) keep.insert(RegDom(h)); }
    for (auto& h : m_appInst) keep.insert(RegDom(h));
    for (auto a : kCkAllow) keep.insert(a);
    m_ckBusy = true; wil::com_ptr<ICoreWebView2> w(wv0);
    w->CallDevToolsProtocolMethod(L"Network.getAllCookies", L"{}", Microsoft::WRL::Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>([this, w, keep, closing](HRESULT, LPCWSTR json) -> HRESULT {
        auto jesc = [](const std::wstring& v) { std::wstring o; for (wchar_t ch : v) { if (ch == L'"' || ch == L'\\') o += L'\\'; o += ch; } return o; };
        std::vector<CkC> all = CkParse(json ? json : L""); int del = 0;
        for (auto& c : all) {
            std::wstring rd = CkDomain(c.dom); bool drop = !keep.count(rd) || (closing && CkAnalyticsName(c.name)); if (!drop) continue;
            std::wstring a = L"{\"name\":\"" + jesc(c.name) + L"\",\"domain\":\"" + jesc(c.dom) + L"\",\"path\":\"" + jesc(c.path) + L"\"" + (c.pk.empty() ? L"" : L",\"partitionKey\":" + c.pk) + L"}";
            ++m_ckPending; ++del; CkLog(L"swept " + rd + L" " + c.name);
            w->CallDevToolsProtocolMethod(L"Network.deleteCookies", a.c_str(), Microsoft::WRL::Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>([this](HRESULT, LPCWSTR) -> HRESULT { if (m_ckPending > 0) --m_ckPending; return S_OK; }).Get());
        }
        CkLog(L"sweep done: " + std::to_wstring(del) + L" of " + std::to_wstring(all.size()));
        m_ckBusy = false; if (m_ckAgain) { m_ckAgain = false; SetTimer(m_hWnd, 2083, 50, nullptr); }
        return S_OK; }).Get());
}
void BrowserWindow::CkSweepActive(bool closing) {
    for (int k = 0; k < (int)m_tabs.size(); ++k) { int i = k == 0 ? std::max(m_activeTab, 0) : k - 1; if (i < (int)m_tabs.size() && m_tabs[i]->webView) { CkSweep(m_tabs[i]->webView.get(), L"", closing); return; } }
}
void BrowserWindow::CkWait(DWORD ms) {   // at close: let the deletes reach the engine before it shuts down
    for (DWORD t0 = GetTickCount(); (m_ckBusy || m_ckPending > 0) && GetTickCount() - t0 < ms;) { MSG m; while (PeekMessage(&m, nullptr, 0, 0, PM_REMOVE)) { if (m.message == WM_QUIT) { PostQuitMessage((int)m.wParam); return; } TranslateMessage(&m); DispatchMessage(&m); } Sleep(5); }
}
