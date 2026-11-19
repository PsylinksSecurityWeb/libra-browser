#include <fstream>
// Sites sidebar (Settings > Startup > "Sidebar sites"): a slim strip of favourite sites at the right edge; a click opens the site in a docked panel beside the page, no tab switch.
struct SiteIcon { std::wstring host; std::string bytes; };
static std::wstring HostOfUrl(const std::wstring& u) { size_t a = u.find(L"://"); a = a == std::wstring::npos ? 0 : a + 3; size_t e = u.find(L'/', a); return u.substr(a, e == std::wstring::npos ? e : e - a); }
static std::string SiteFetchBytes(int port, const std::wstring& url, size_t cap = 65536) {   // port 0 = direct (local test host)   // favicon through Libra's own DNS proxy, no cookies, 64 KB cap
    std::string body;
    if (HINTERNET ses = port ? dl::Session(port, 1) : WinHttpOpen(L"Libra", WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0)) {
        WinHttpSetTimeouts(ses, 3000, 3000, 4000, 4000);
        dl::Url u;
        if (dl::Crack(url, u)) if (HINTERNET con = WinHttpConnect(ses, u.host.c_str(), u.port, 0)) {
            if (HINTERNET rq = WinHttpOpenRequest(con, L"GET", u.path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, u.https ? WINHTTP_FLAG_SECURE : 0)) {
                DWORD nc = WINHTTP_DISABLE_COOKIES; WinHttpSetOption(rq, WINHTTP_OPTION_DISABLE_FEATURE, &nc, sizeof nc);
                if (WinHttpSendRequest(rq, L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64)\r\n", (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(rq, nullptr)) {
                    DWORD sc = 0, sl = sizeof sc; WinHttpQueryHeaders(rq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &sc, &sl, nullptr);
                    char buf[2048]; DWORD n = 0;
                    if (sc == 200) while (body.size() < cap && WinHttpReadData(rq, buf, sizeof buf, &n) && n) body.append(buf, n);
                }
                WinHttpCloseHandle(rq);
            }
            WinHttpCloseHandle(con);
        }
        WinHttpCloseHandle(ses);
    }
    return body;
}
static std::wstring SiteFile(const std::wstring& host) {   // icon file name: only plain host characters
    std::wstring h; for (wchar_t c : host) { if (iswalnum(c) || c == L'.' || c == L'-') h += (wchar_t)towlower(c); else return L""; }
    return h.empty() ? L"" : DataDir() + L"\\sf_" + h + L".png";
}
void BrowserWindow::SiteSaveIcon(const std::string& bytes, const std::shared_ptr<void>& bm) {
    std::wstring h = HostOf(m_siteUrl), f = SiteFile(h); if (f.empty() || bytes.empty() || bytes.size() > 65536) return;
    m_siteFav[h] = bm; std::ofstream(f, std::ios::binary).write(bytes.data(), (std::streamsize)bytes.size()); InvalidateRect(m_hWnd, nullptr, FALSE);
}
void BrowserWindow::SiteIconGot(void* p) {
    std::unique_ptr<SiteIcon> m((SiteIcon*)p);
    if (m->bytes.empty() || m_siteFav.count(m->host)) return;   // a real tab icon already won
    if (IStream* ms = SHCreateMemStream((const BYTE*)m->bytes.data(), (UINT)m->bytes.size())) {
        Gdiplus::Bitmap* bm = nullptr;
        { Gdiplus::Bitmap tmp(ms); if (tmp.GetLastStatus() == Gdiplus::Ok && tmp.GetWidth() > 0) bm = tmp.Clone(0, 0, tmp.GetWidth(), tmp.GetHeight(), PixelFormat32bppARGB); }
        ms->Release();
        if (bm) { m_siteFav[m->host].reset(bm, [](void* q) { delete (Gdiplus::Bitmap*)q; }); InvalidateRect(m_hWnd, nullptr, FALSE); }
    }
}
bool BrowserWindow::SiteOn() const { return !m_incognito && !m_child && !m_full && !m_setup && m_set.On(L"site_dock"); }
int BrowserWindow::SiteW() const { return SiteOn() ? MulDiv(48, m_dpi, 96) : 0; }
std::vector<std::wstring> BrowserWindow::SiteList() const {
    std::wstring s = m_set.Get(L"site_list", L"https://mail.google.com, https://web.whatsapp.com"); std::vector<std::wstring> v;
    size_t p = 0;
    while (p < s.size() && v.size() < 24) {
        size_t e = s.find_first_of(L", \r\n;", p); if (e == std::wstring::npos) e = s.size();
        std::wstring u = s.substr(p, e - p); p = e + 1;
        if (u.empty() || u == L"-") continue;
        if (u.find(L"://") == std::wstring::npos) u = L"https://" + u;
        if (IsHttp(u)) v.push_back(u);
    }
    return v;
}
RECT BrowserWindow::SiteRect(int i) const {
    RECT w = WebViewRect(); int d = MulDiv(34, m_dpi, 96), g = MulDiv(8, m_dpi, 96);
    int x = w.right + (SiteW() - d) / 2, y = w.top + g + i * (d + g);
    return { x, y, x + d, y + d };
}
void BrowserWindow::PaintSites(HDC hdc) {
    if (!SiteW()) return;
    RECT w = WebViewRect(), cr; GetClientRect(m_hWnd, &cr); RECT st = { w.right, w.top, cr.right, cr.bottom };
    HBRUSH bg = CreateSolidBrush(RGB(247, 247, 247)); FillRect(hdc, &st, bg); DeleteObject(bg);
    RECT ln = { st.left, st.top, st.left + 1, st.bottom }; HBRUSH lb = CreateSolidBrush(RGB(228, 228, 228)); FillRect(hdc, &ln, lb); DeleteObject(lb);
    static const COLORREF pal[] = { RGB(219, 234, 254), RGB(220, 252, 231), RGB(254, 226, 226), RGB(254, 243, 199), RGB(237, 233, 254), RGB(224, 242, 254) };
    const bool open = m_sitePanel && IndexOf(m_sitePanel) >= 0;
    HFONT f = CreateFontW(-MulDiv(15, m_dpi, 96), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI"), of = (HFONT)SelectObject(hdc, f);
    SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, RGB(40, 40, 40));
    auto v = SiteList();
    for (int i = 0; i < (int)v.size(); ++i) {
        std::wstring h = HostOf(v[i]); if (h.rfind(L"www.", 0) == 0) h = h.substr(4);
        std::wstring lab = h.substr(0, h.find(L'.'));
        if ((lab == L"web" || lab == L"app" || lab == L"m") && h.find(L'.') != std::wstring::npos) { std::wstring r2 = h.substr(h.find(L'.') + 1); lab = r2.substr(0, r2.find(L'.')); }
        size_t hs = 0; for (wchar_t c : h) hs = hs * 31 + c;
        RECT r = SiteRect(i); const bool on = open && m_siteUrl == v[i];
        HBRUSH b = CreateSolidBrush(pal[hs % 6]); HPEN pn = on ? CreatePen(PS_SOLID, 2, Color::Accent) : (HPEN)CreatePen(PS_NULL, 0, 0);
        HGDIOBJ ob = SelectObject(hdc, b), op = SelectObject(hdc, pn);
        Ellipse(hdc, r.left, r.top, r.right, r.bottom);
        SelectObject(hdc, ob); SelectObject(hdc, op); DeleteObject(b); DeleteObject(pn);
        auto fi = m_siteFav.find(HostOf(v[i]));
        if (fi != m_siteFav.end() && fi->second) {
            HBRUSH wb = CreateSolidBrush(RGB(255, 255, 255)); HGDIOBJ o1 = SelectObject(hdc, wb), o2 = SelectObject(hdc, GetStockObject(NULL_PEN)); Ellipse(hdc, r.left + 1, r.top + 1, r.right - 1, r.bottom - 1); SelectObject(hdc, o1); SelectObject(hdc, o2); DeleteObject(wb);
            int s = (r.right - r.left) * 3 / 5; Gdiplus::Graphics g(hdc); g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            g.DrawImage((Gdiplus::Bitmap*)fi->second.get(), r.left + (r.right - r.left - s) / 2, r.top + (r.bottom - r.top - s) / 2, s, s); continue;
        }
        if (m_siteTry.insert(HostOf(v[i])).second) {   // first time this run: saved icon from an earlier visit, else fetch it
            std::wstring f = SiteFile(HostOf(v[i])); std::ifstream in(f, std::ios::binary); std::string b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            if (!b.empty() && b.size() <= 65536) { SiteIconGot(new SiteIcon{ HostOf(v[i]), b }); continue; }
        }
        if (m_siteTry.insert(L"F:" + HostOf(v[i])).second) { HWND hw = m_hWnd; std::wstring url = L"https://" + HostOf(v[i]) + L"/favicon.ico"; int port = DohProxy::Get().Port(); if (port) std::thread([hw, url, port] { PostMessage(hw, WM_APP + 70, 0, (LPARAM) new SiteIcon{ HostOfUrl(url), SiteFetchBytes(port, url) }); }).detach(); }
        wchar_t ch[2] = { lab.empty() ? L'?' : (wchar_t)towupper(lab[0]), 0 };
        DrawTextW(hdc, ch, 1, &r, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
    }
    SelectObject(hdc, of); DeleteObject(f);
    RECT pr = SiteRect((int)v.size());   // "+" : add the site of the open page (or open Settings)
    if (pr.bottom < st.bottom) {
        HPEN pn = CreatePen(PS_SOLID, 1, RGB(170, 170, 170)); HGDIOBJ op = SelectObject(hdc, pn), ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
        Ellipse(hdc, pr.left, pr.top, pr.right, pr.bottom); SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(pn);
        int cx = (pr.left + pr.right) / 2, cy = (pr.top + pr.bottom) / 2, h = MulDiv(6, m_dpi, 96);
        DrawLineAA(hdc, (float)cx - h, (float)cy, (float)cx + h, (float)cy, RGB(110, 110, 110), 1.6f); DrawLineAA(hdc, (float)cx, (float)cy - h, (float)cx, (float)cy + h, RGB(110, 110, 110), 1.6f);
    }
    PaintAppIcon(hdc);
}
bool BrowserWindow::SiteClick(int x, int y) {
    if (!SiteW()) return false;
    POINT pt = { x, y }; auto v = SiteList();
    if (AppOffer()) { RECT ar = AppRect(); if (PtInRect(&ar, pt)) { AppInstallAsk(); return true; } }
    for (int i = 0; i < (int)v.size(); ++i) { RECT r = SiteRect(i); if (PtInRect(&r, pt)) { OpenSite(v[i]); return true; } }
    RECT pr = SiteRect((int)v.size());
    if (PtInRect(&pr, pt)) {
        const std::wstring u = (m_activeTab >= 0 && m_activeTab < (int)m_tabs.size() && !TabHid(m_activeTab)) ? m_tabs[m_activeTab]->url : L"";
        bool have = false; if (IsHttp(u)) for (auto& s : v) if (HostOf(s) == HostOf(u)) have = true;
        if (IsHttp(u) && !have) SiteAdd(u); else ShowInternal(L"libra://settings");   // already there or not a website: Settings has the full list
        return true;
    }
    return false;
}
void BrowserWindow::OpenSite(const std::wstring& u) {
    int pi = m_sitePanel ? IndexOf(m_sitePanel) : -1;
    if (pi >= 0 && m_siteUrl == u) { CloseTab(pi); ResizeWebViews(); InvalidateRect(m_hWnd, nullptr, FALSE); return; }   // same icon again = close
    if (AgDockIdx() >= 0) ToggleAgent();   // one side panel at a time
    pi = m_sitePanel ? IndexOf(m_sitePanel) : -1; m_siteUrl = u;
    if (pi >= 0 && m_tabs[pi]->webView) { m_tabs[pi]->webView->Navigate(u.c_str()); }
    else { size_t n0 = m_tabs.size(); OpenTab(u, false, true); if (m_tabs.size() > n0) m_sitePanel = m_tabs.back().get(); }
    ApplyDock(); InvalidateRect(m_hWnd, nullptr, FALSE);
}
void BrowserWindow::ApplySite() {
    int pi = m_sitePanel ? IndexOf(m_sitePanel) : -1;
    if (pi < 0) { m_sitePanel = nullptr; return; }
    if (!SiteOn()) { CloseTab(pi); return; }
    ResizeWebViews();
    if (pi == m_activeTab) return;
    Tab* a = m_tabs[pi].get(); a->hiddenSince = 0; ResumeTab(a);
    if (a->controller) a->controller->put_IsVisible(TRUE);
}

bool BrowserWindow::TabHid(int i) const { return i >= 0 && i < (int)m_tabs.size() && (m_tabs[i].get() == m_sitePanel || m_tabs[i]->url == L"libra://agent"); }
int BrowserWindow::LastVisTab() const { for (int i = (int)m_tabs.size() - 1; i >= 0; --i) if (!TabHid(i)) return i; return -1; }
void BrowserWindow::SiteSetList(const std::vector<std::wstring>& v) {
    std::wstring s; for (auto& u : v) s += (s.empty() ? L"" : L", ") + u;
    m_set.Set(L"site_list", s.empty() ? L"-" : s);   // "-" = an empty list on purpose (an empty value would bring the defaults back)
    InvalidateRect(m_hWnd, nullptr, FALSE);
}
void BrowserWindow::SiteAdd(const std::wstring& url) {
    if (!IsHttp(url) || SiteList().size() >= 24) return;
    size_t e = url.find(L'/', 8); std::wstring o = url.substr(0, e); auto v = SiteList();
    for (auto& u : v) if (HostOf(u) == HostOf(o)) return;
    v.push_back(o); SiteSetList(v);
}
bool BrowserWindow::SiteRight(int x, int y) {
    if (!SiteW()) return false;
    RECT w = WebViewRect(); if (x < w.right || y < w.top) return false;
    auto v = SiteList(); int hit = -1; POINT pt = { x, y };
    for (int i = 0; i < (int)v.size(); ++i) { RECT r = SiteRect(i); if (PtInRect(&r, pt)) hit = i; }
    HMENU m = CreatePopupMenu();
    if (hit >= 0) AppendMenuW(m, MF_STRING, 1, (L"Remove " + HostOf(v[hit]) + L" from the sidebar").c_str());
    AppendMenuW(m, MF_STRING, 2, L"Hide the sidebar");
    ClientToScreen(m_hWnd, &pt); int c = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hWnd, nullptr); DestroyMenu(m);
    if (c == 1) { int pi = m_sitePanel ? IndexOf(m_sitePanel) : -1; if (pi >= 0 && m_siteUrl == v[hit]) CloseTab(pi); v.erase(v.begin() + hit); SiteSetList(v); ResizeWebViews(); }
    if (c == 2) { m_set.Set(L"site_dock", L"0"); ApplyDock(); ResizeWebViews(); InvalidateRect(m_hWnd, nullptr, FALSE); }   // turn it on again in Settings > Startup
    return true;
}
void BrowserWindow::SwitchEngineTabs(const std::wstring& oldEu) {   // a new default engine: open new-tab pages and result pages of the old engine move to it
    const std::wstring ne = EngineUrl(); if (ne == oldEu) return;
    for (int i = 0; i < (int)m_tabs.size(); ++i) {
        Tab* t = m_tabs[i].get();
        if (t->url == L"libra://home") { RenderTab(i); continue; }
        if (!t->webView || !IsHttp(t->url) || t->url.rfind(oldEu, 0) != 0) continue;
        std::wstring q = t->url.substr(oldEu.size()); q = q.substr(0, q.find(L'&')); if (q.empty()) continue;
        t->webView->Navigate((ne + q).c_str());
    }
}
