// Web-app detection (PLAN-webapps.md section 8, step D1). After a page loads, ask the engine (DevTools protocol) whether the site is an installable web app.
// No page script, one call per host per run, nothing sent anywhere. Included at the end of BrowserWindow.cpp.
static bool AppHostOk(const std::wstring& url) {   // https, or the local test host
    if (url.rfind(L"https://", 0) == 0) return true;
    return url.rfind(L"http://127.0.0.1", 0) == 0 || url.rfind(L"http://localhost", 0) == 0;
}
static std::wstring AppManifestName(const std::wstring& j) {   // "name" out of the engine's manifest reply (the raw text sits escaped inside "data")
    for (const wchar_t* mk : { L"\\\"name\\\":\\\"", L"\"name\":\"" }) {
        size_t p = j.find(mk); if (p == std::wstring::npos) continue; p += wcslen(mk);
        std::wstring o; while (p < j.size() && j[p] != L'\\' && j[p] != L'"' && o.size() < 40) { if (j[p] >= 32 && j[p] != 0x202E && j[p] != 0x202D) o += j[p]; ++p; }
        if (!o.empty()) return o;
    }
    return L"";
}
bool BrowserWindow::AppOffer() const {
    if (m_incognito || m_child || m_setup || m_app || m_set.Get(L"app_detect", L"1") == L"0") return false;
    if (m_activeTab < 0 || m_activeTab >= (int)m_tabs.size() || TabHid(m_activeTab)) return false;
    const Tab* t = m_tabs[m_activeTab].get(); if (t->incognito || !AppHostOk(t->url)) return false;
    auto it = m_appCache.find(HostOf(t->url)); return it != m_appCache.end() && !it->second.empty() && !m_appInst.count(HostOf(t->url));   // (an installed site is not offered again)
}
std::wstring BrowserWindow::AppOfferName() const { auto it = m_appCache.find(HostOf(m_tabs[m_activeTab]->url)); return it == m_appCache.end() ? L"" : it->second; }
RECT BrowserWindow::AppRect() const {
    if (SiteOn()) return SiteRect((int)SiteList().size() + 1);
    RECT s = StarRect(); int w = MulDiv(30, m_dpi, 96); int n = m_set.On(L"save_video") ? 2 : 1; return { s.left - w * n, s.top, s.left - w * (n - 1), s.bottom };
}
void BrowserWindow::AppRelayout() {   // the address box gives up room for the install icon only while it shows
    bool want = AppOffer() && !SiteOn(); if (want == m_appShown) return; m_appShown = want;
    RECT cr; GetClientRect(m_hWnd, &cr); LayoutToolbar(cr.right); InvalidateRect(m_hWnd, nullptr, FALSE);
}
void BrowserWindow::AppDetectSoon() {   // called when the active tab finished loading
    if (m_incognito || m_child || m_setup || m_app || m_set.Get(L"app_detect", L"1") == L"0") return;
    if (m_activeTab < 0 || m_activeTab >= (int)m_tabs.size() || !AppHostOk(m_tabs[m_activeTab]->url) || m_tabs[m_activeTab]->incognito) return;
    if (!m_appInstLoaded) AppLoadInstalled();
    if (m_appCache.count(HostOf(m_tabs[m_activeTab]->url))) { AppRelayout(); return; }
    SetTimer(m_hWnd, 2080, 2000, nullptr);   // let the site add its manifest first
}
void BrowserWindow::AppDetectRun() {
    if (m_activeTab < 0 || m_activeTab >= (int)m_tabs.size()) return;
    Tab* t = m_tabs[m_activeTab].get(); if (!t->webView || !AppHostOk(t->url) || t->incognito) return;
    const std::wstring host = HostOf(t->url), url = t->url; if (m_appCache.count(host) || m_appBusy) return; m_appBusy = true;
    auto wv = t->webView;
    wv->CallDevToolsProtocolMethod(L"Page.enable", L"{}", nullptr);
    auto done = [this, host](const std::wstring& name) {
        m_appBusy = false; m_appCache[host] = name;
        wchar_t tf[MAX_PATH]; if (TestEnv(L"LIBRA_TEST_APPLOG", tf, MAX_PATH)) WriteAllRaw(tf, (name.empty() ? "no " : "yes ") + ToUtf8(host) + " " + ToUtf8(name) + "\n", true);
        AppRelayout(); InvalidateRect(m_hWnd, nullptr, FALSE);
        wchar_t ti[8]; if (TestEnv(L"LIBRA_TEST_APPINSTALL", ti, 8) > 0 && (!name.empty() || ti[0] == L'm')) SetTimer(m_hWnd, 2081, 400, nullptr);   // test hook: click the install icon for the test
    };
    wv->CallDevToolsProtocolMethod(L"Page.getInstallabilityErrors", L"{}", Microsoft::WRL::Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>(
        [this, wv, host, done](HRESULT hr, LPCWSTR json) -> HRESULT {
            std::wstring s = json ? json : L"";
            if (FAILED(hr) || s.find(L"\"installabilityErrors\":[]") == std::wstring::npos) { done(L""); return S_OK; }   // not installable (or no answer): remember, do not ask again
            wv->CallDevToolsProtocolMethod(L"Page.getAppManifest", L"{}", Microsoft::WRL::Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>(
                [host, done](HRESULT, LPCWSTR mj) -> HRESULT { std::wstring n = AppManifestName(mj ? mj : L""); done(n.empty() ? host : n); return S_OK; }).Get());
            return S_OK;
        }).Get());
}
void BrowserWindow::PaintAppIcon(HDC hdc) {   // sidebar round icon or address-bar glyph (accent colour), only while the page is a detected app
    if (!AppOffer()) return; RECT r = AppRect();
    if (SiteOn()) { RECT cr; GetClientRect(m_hWnd, &cr); if (r.bottom >= cr.bottom) return;
        HPEN pn = CreatePen(PS_SOLID, 2, Color::Accent); HGDIOBJ op = SelectObject(hdc, pn), ob = SelectObject(hdc, GetStockObject(NULL_BRUSH)); Ellipse(hdc, r.left, r.top, r.right, r.bottom); SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(pn); }
    DrawGlyph(hdc, L"", r, MulDiv(15, m_dpi, 96), Color::Accent);
}
