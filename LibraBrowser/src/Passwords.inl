// Saved logins + generator glue. Included at the end of BrowserWindow.cpp.
// Rules: a website can only talk about its OWN origin and never receives a saved password.
// Fill/save prompts are Libra's own native popup, so a page cannot click them. Passwords never go in a log or URL.

static std::wstring OriginOf(const std::wstring& u) {   // https://host[:port], or http only for localhost; else empty
    size_t s = u.find(L"://"); if (s == std::wstring::npos || u.size() > 2000) return L"";
    std::wstring sch = u.substr(0, s); for (auto& c : sch) c = (wchar_t)towlower(c);
    size_t e = u.find_first_of(L"/?#", s + 3);
    std::wstring auth = u.substr(s + 3, e == std::wstring::npos ? std::wstring::npos : e - (s + 3));
    if (auth.empty() || auth.find(L'@') != std::wstring::npos) return L"";
    for (auto& c : auth) c = (wchar_t)towlower(c);
    std::wstring host = auth;
    if (host[0] == L'[') { size_t r = host.find(L']'); if (r != std::wstring::npos) host = host.substr(0, r + 1); }
    else { size_t c = host.find(L':'); if (c != std::wstring::npos) host = host.substr(0, c); }
    bool local = host == L"localhost" || host == L"127.0.0.1" || host == L"[::1]";
    if (sch == L"https" || (sch == L"http" && local)) return sch + L"://" + auth;
    return L"";
}
static std::wstring JsLit(const std::wstring& s) {   // JS string literal, every non-alphanumeric as \uXXXX
    std::wstring o = L"\"";
    for (wchar_t c : s) { if (iswalnum(c) && c < 128) o += c; else { wchar_t b[8]; swprintf(b, 8, L"\\u%04x", (unsigned)c); o += b; } }
    return o + L"\"";
}
static void Wipe(std::wstring& s) { if (!s.empty()) SecureZeroMemory(s.data(), s.size() * sizeof(wchar_t)); s.clear(); }
static std::wstring VidLabel(const std::wstring& url, long long size);   // Video.inl
static const wchar_t* kPwClass = L"LibraPw";

void BrowserWindow::PwLoad() {
    if (m_vaultLoaded) return;
    m_vaultLoaded = true;
    VaultLoad(m_vault);
    for (auto& l : Lines(ReadAll(DataDir() + L"\\pwnever.txt"))) if (l.size() < 300) m_pwNever.insert(l);
}

void BrowserWindow::InstallPwScript(Tab* t) {
    if (!t->webView) return;
    std::wstring a = kPwSiteA; size_t at = a.find(L"@PRIV@");
    if (at != std::wstring::npos) a.replace(at, 6, m_incognito ? L"1" : L"0");
    std::wstring js = a + kGenJs + kPwSiteB;
    t->webView->AddScriptToExecuteOnDocumentCreated(js.c_str(), nullptr);
}

void BrowserWindow::OnSitePwMessage(Tab* t, const std::wstring& srcUrl, const std::wstring& msg) {
    if (m_incognito || !t->webView) return;
    DWORD now = GetTickCount();                                   // rate limit: 30 messages / 10 s
    if (now - m_pwRate[0] > 10000) { m_pwRate[0] = now; m_pwRate[1] = 0; }
    if (++m_pwRate[1] > 30) return;
    std::wstring origin = OriginOf(srcUrl); if (origin.empty()) return;
    { wil::unique_cotaskmem_string cur; t->webView->get_Source(&cur);            // the sender must be the page this tab is really showing
      if (!cur.get() || OriginOf(cur.get()) != origin) return; }
    auto v = SplitTab(msg); if (v.empty()) return;
    PwLoad();
    if (v[0] == L"pw.blur") { if (m_pwKind == 2 && m_pwTab == t) HidePw(); return; }
    if (v[0] == L"pw.focus") {
        if (m_certBad.count(HostOf(srcUrl)) || m_activeTab < 0 || m_tabs[m_activeTab].get() != t) return;
        if (m_pwKind == 1 && m_hPw && IsWindowVisible(m_hPw)) return;             // do not cover a save prompt
        m_pwChoice.clear();
        for (auto& e : m_vault) if (e.origin == origin && m_pwChoice.size() < 4) m_pwChoice.push_back(e.created);
        if (!m_pwChoice.empty()) ShowPw(2, t, origin);
        return;
    }
    if (v[0] == L"pw.save" && v.size() == 3) {
        if (m_pwNever.count(origin)) return;
        Cred n; n.origin = origin; n.user = PwDec(ToUtf8(v[1])); n.pass = PwDec(ToUtf8(v[2]));
        if (n.pass.empty() || n.pass.size() > 1024 || n.user.size() > 256) return;
        long long upd = 0;
        for (auto& e : m_vault) if (e.origin == origin && e.user == n.user) { if (e.pass == n.pass) return; upd = e.created; break; }
        Wipe(m_pwPending.pass); m_pwPending = n; m_pwUpdate = upd;
        Wipe(n.pass);
        ShowPw(1, t, origin);
    }
}

void BrowserWindow::PwCopy(const std::wstring& s) {
    if (s.empty() || s.size() > 1024 || !OpenClipboard(m_hWnd)) return;
    EmptyClipboard();
    size_t bytes = (s.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, bytes)) { memcpy(GlobalLock(h), s.c_str(), bytes); GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); }
    for (const wchar_t* f : { L"ExcludeClipboardContentFromMonitorProcessing", L"CanIncludeInClipboardHistory", L"CanUploadToCloudClipboard" })   // keep it out of Win+V and the cloud clipboard
        if (HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, 4)) SetClipboardData(RegisterClipboardFormatW(f), h);
    CloseClipboard();
    Wipe(m_clipVal); m_clipVal = s;
    SetTimer(m_hWnd, 2003, 30000, nullptr);                       // clear after 30 s, only if it is still our text
}
void BrowserWindow::PwClipClear() {
    KillTimer(m_hWnd, 2003);
    if (m_clipVal.empty()) return;
    if (OpenClipboard(m_hWnd)) {
        bool ours = false;
        if (HANDLE h = GetClipboardData(CF_UNICODETEXT)) { if (auto* p = (const wchar_t*)GlobalLock(h)) { ours = m_clipVal == p; GlobalUnlock(h); } }
        if (ours) EmptyClipboard();
        CloseClipboard();
    }
    Wipe(m_clipVal);
}

void BrowserWindow::OnPwPage(int idx, const std::vector<std::wstring>& v) {
    if (idx < 0 || idx >= (int)m_tabs.size() || m_tabs[idx]->url != L"libra://passwords" || !m_tabs[idx]->webView) return;
    PwLoad();
    const std::wstring& c = v[0];
    long long id = v.size() > 1 ? _wtoi64(v[1].c_str()) : 0;
    auto find = [&](long long k) -> Cred* { for (auto& e : m_vault) if (e.created == k) return &e; return nullptr; };
    if (c == L"pw.clip") { if (v.size() == 2) PwCopy(v[1]); return; }
    Cred* e = find(id); if (!e) return;
    if ((c == L"pw.show" || c == L"pw.copy") && GetTickCount64() > m_pwUntil) { m_pwPend = v; m_pwPendTab = m_tabs[idx].get(); AskWindows(5); return; }   // locked: ask Windows first
    if (c == L"pw.show") { std::wstring js = L"lbShown(\"" + std::to_wstring(id) + L"\"," + JsLit(e->pass) + L")"; m_tabs[idx]->webView->ExecuteScript(js.c_str(), nullptr); Wipe(js); }
    else if (c == L"pw.copy") PwCopy(e->pass);
    else if (c == L"pw.del") { Wipe(e->pass); m_vault.erase(m_vault.begin() + (e - m_vault.data())); VaultSave(m_vault); RenderTab(idx); }
}

// ── native popup: save prompt (kind 1) and fill list (kind 2) ──
RECT BrowserWindow::PwRect(int i) const {
    int k = m_dpi;
    if (i < 10) { int bw = MulDiv(94, k, 96), g = MulDiv(8, k, 96); return { MulDiv(20, k, 96) + i * (bw + g), MulDiv(114, k, 96), MulDiv(20, k, 96) + i * (bw + g) + bw, MulDiv(148, k, 96) }; }
    int r = i - 10; return { MulDiv(10, k, 96), MulDiv(50 + 40 * r, k, 96), MulDiv(330, k, 96), MulDiv(50 + 40 * r + 38, k, 96) };
}
int BrowserWindow::PwHit(POINT pt) const {
    if (m_pwKind == 3 || m_pwKind == 6) return 0;   // whole toast is one button
    if (m_pwKind == 1 || m_pwKind == 5 || m_pwKind == 7) { for (int i = 0; i < (m_pwKind == 1 ? 3 : 2); ++i) { RECT r = PwRect(i); if (PtInRect(&r, pt)) return i; } }
    else if (m_pwKind == 2 || m_pwKind == 4) { for (int i = 0; i < (int)m_pwChoice.size(); ++i) { RECT r = PwRect(10 + i); if (PtInRect(&r, pt)) return 10 + i; } }
    return -1;
}
void BrowserWindow::ShowPw(int kind, Tab* t, const std::wstring& origin) {
    if (GetForegroundWindow() != m_hWnd || IsIconic(m_hWnd)) return;
    if (!m_hPw) {
        WNDCLASSEXW wc{ sizeof(wc) }; wc.style = 0; wc.lpfnWndProc = PwProc; wc.hInstance = m_hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.lpszClassName = kPwClass; RegisterClassExW(&wc);
        m_hPw = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, kPwClass, L"", WS_POPUP, 0, 0, 10, 10, m_hWnd, nullptr, m_hInst, nullptr); HideFromRecall(m_hPw);
        SetWindowLongPtrW(m_hPw, GWLP_USERDATA, (LONG_PTR)this);
    }
    bool toast = kind == 3 || kind == 6;
    bool wasToast = toast && (m_pwKind == 3 || m_pwKind == 6) && IsWindowVisible(m_hPw) && m_pwAnim != 2;
    m_pwKind = kind; m_pwTab = t; m_pwOrigin = origin; m_pwHot = -1; m_pwShown = GetTickCount();
    if (toast) {   // slides in from the right edge, under the toolbar
        int w = MulDiv(kind == 3 ? 330 : 300, m_dpi, 96), ht = MulDiv(kind == 3 ? 88 : 72, m_dpi, 96);
        RECT wr, om; GetWindowRect(m_hWnd, &wr); GetWindowRect(m_hOmnibox, &om);
        m_pwW = w; m_pwH = ht; m_pwSl = wasToast ? 0 : w; m_pwAnim = wasToast ? 0 : 1;
        SetWindowRgn(m_hPw, CreateRoundRectRgn(m_pwSl, 0, w + 1, ht + 1, MulDiv(24, m_dpi, 96), MulDiv(24, m_dpi, 96)), TRUE);
        SetWindowPos(m_hPw, HWND_TOPMOST, wr.right - w - MulDiv(20, m_dpi, 96), om.bottom + MulDiv(12, m_dpi, 96), w, ht, SWP_SHOWWINDOW | SWP_NOACTIVATE);
        SetTimer(m_hWnd, 2002, 1000, nullptr); if (m_pwAnim) SetTimer(m_hWnd, 2012, 15, nullptr);
        InvalidateRect(m_hPw, nullptr, FALSE); return;
    }
    m_pwAnim = 0;
    int w =MulDiv(340, m_dpi, 96), ht = MulDiv(kind == 1 || kind == 3 || kind == 5 || kind == 6 || kind == 7 ? 168 : 62 + 40 * (int)m_pwChoice.size(), m_dpi, 96);
    RECT r; GetWindowRect(m_hOmnibox, &r);
    SetWindowPos(m_hPw, HWND_TOPMOST, r.left, r.bottom + MulDiv(8, m_dpi, 96), w, ht, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    SetWindowRgn(m_hPw, CreateRoundRectRgn(0, 0, w + 1, ht + 1, MulDiv(28, m_dpi, 96), MulDiv(28, m_dpi, 96)), TRUE);
    SetTimer(m_hWnd, 2002, 1000, nullptr);
    InvalidateRect(m_hPw, nullptr, FALSE);
}
void BrowserWindow::HidePw() {
    if ((m_pwKind == 3 || m_pwKind == 6) && m_hPw && IsWindowVisible(m_hPw) && m_pwAnim != 3) {   // slide back out first
        if (m_pwAnim != 2) { m_pwAnim = 2; SetTimer(m_hWnd, 2012, 15, nullptr); }
        return;
    }
    m_pwAnim = 0; KillTimer(m_hWnd, 2012);
    KillTimer(m_hWnd, 2002);
    if (m_hPw && IsWindowVisible(m_hPw)) ShowWindow(m_hPw, SW_HIDE);
    if (m_pwKind == 1) { Wipe(m_pwPending.pass); m_pwUpdate = 0; }
    m_pwKind = 0;
}
void BrowserWindow::PwClick(int what) {
    if (m_pwAnim == 2) return;
    if (m_pwKind == 4) { Tab* t = m_pwTab; HidePw(); if (what >= 10 && t && IndexOf(t) >= 0) StartVideoDownload(t, what - 10); return; }
    if (m_pwKind == 7) { HidePw(); if (what == 1) { for (int di = 0; di < (int)m_dl.size(); ++di) if (m_dl[di].path == m_riskPath && m_dl[di].hold == 2) { wchar_t tp[8]; bool yes = TestEnv(L"LIBRA_TEST_SAVEPATH", tp, 8) > 0 || MessageBoxW(m_hWnd, L"This file can run programs. Libra held it so nothing could start it by accident.\n\nRelease it only if you trust the site it came from.", L"Release this file?", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES; if (yes) HoldRelease(di); } for (int ti = 0; ti < (int)m_tabs.size(); ++ti) if (m_tabs[ti]->url == L"libra://downloads") RenderTab(ti); return; } if (what == 0 && !m_riskPath.empty()) { DeleteFileW(m_riskPath.c_str()); for (auto& d : m_dl) if (d.path == m_riskPath) { d.state = 2; d.canRetry = false; } for (int i = 0; i < (int)m_tabs.size(); ++i) if (m_tabs[i]->url == L"libra://downloads") RenderTab(i); } return; }
    if (m_pwKind == 5) { Tab* ct = m_pwTab; HidePw(); if (what == 0) { int ix = IndexOf(ct); if (ix >= 0 && ix != m_activeTab) CloseTab(ix); } return; }
    if (m_pwKind == 3) { HidePw(); if (what == 0) ToggleShieldPopup(); return; }
    if (m_pwKind == 6) { HidePw(); if (what == 0) ShowInternal(L"libra://downloads", true); return; }
    if (m_pwKind == 1) {
        if (what == 0 && !m_pwPending.pass.empty()) {
            bool done = false;
            if (m_pwUpdate) for (auto& e : m_vault) if (e.created == m_pwUpdate) { e.pass = m_pwPending.pass; e.used = time(nullptr); done = true; }
            if (!done) {
                long long id = (long long)time(nullptr) * 1000; auto taken = [&](long long k) { for (auto& e : m_vault) if (e.created == k) return true; return false; };
                while (taken(id)) ++id;
                Cred n = m_pwPending; n.created = id; n.used = time(nullptr); m_vault.push_back(std::move(n));
            }
            VaultSave(m_vault);
            for (int i = 0; i < (int)m_tabs.size(); ++i) if (m_tabs[i]->url == L"libra://passwords") RenderTab(i);
        } else if (what == 2) {
            m_pwNever.insert(m_pwOrigin); WriteAll(DataDir() + L"\\pwnever.txt", ToUtf8(m_pwOrigin) + "\n", true);
        }
        HidePw();
    } else if (m_pwKind == 2 && what >= 10 && what - 10 < (int)m_pwChoice.size()) {
        long long id = m_pwChoice[what - 10]; Tab* t = m_pwTab; std::wstring origin = m_pwOrigin;
        HidePw();
        if (IndexOf(t) < 0 || !t->webView || m_activeTab < 0 || m_tabs[m_activeTab].get() != t) return;
        wil::unique_cotaskmem_string cur; t->webView->get_Source(&cur);
        if (!cur.get() || OriginOf(cur.get()) != origin || m_certBad.count(HostOf(cur.get()))) return;   // exact site only
        for (auto& e : m_vault) if (e.created == id && e.origin == origin) {
            std::wstring js = L"(function(u,p){function vis(e){var r=e.getBoundingClientRect();return r.width>0&&r.height>0}"
                L"function set(e,v){Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(e,v);e.dispatchEvent(new Event('input',{bubbles:true}));e.dispatchEvent(new Event('change',{bubbles:true}))}"
                L"var ps=[].filter.call(document.querySelectorAll('input[type=password]'),vis);if(!ps.length)return;"
                L"var pw=ps.indexOf(document.activeElement)>=0?document.activeElement:ps[0];"
                L"var us=null;[].forEach.call((pw.form||document).querySelectorAll('input'),function(x){if(vis(x)&&/^(text|email|tel)$/.test(x.type)&&(pw.compareDocumentPosition(x)&2))us=x});"
                L"if(us&&u)set(us,u);set(pw,p)})(" + JsLit(e.user) + L"," + JsLit(e.pass) + L");";
            t->webView->ExecuteScript(js.c_str(), nullptr); Wipe(js);
            e.used = time(nullptr); VaultSave(m_vault);
            break;
        }
    }
}
void BrowserWindow::PaintPw(HDC hdc) {
    RECT rc; GetClientRect(m_hPw, &rc);
    HBRUSH w = CreateSolidBrush(RGB(255, 255, 255)); FillRect(hdc, &rc, w); DeleteObject(w);
    float k = m_dpi / 96.f; int pad = (int)(20 * k); SetBkMode(hdc, TRANSPARENT);
    if (m_pwKind == 3 || m_pwKind == 6) {   // toast: ring icon + two text lines, shifted by the slide offset
        SetViewportOrgEx(hdc, m_pwSl, 0, nullptr);
        auto font = [&](int px, int weight) { return CreateFontW(-MulDiv(px, m_dpi, 96), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI"); };
        auto text = [&](const std::wstring& s, RECT r, HFONT f, COLORREF c) { HFONT o = (HFONT)SelectObject(hdc, f); SetTextColor(hdc, c); DrawTextW(hdc, s.c_str(), -1, &r, DT_LEFT | DT_VCENTER | DT_NOPREFIX | DT_SINGLELINE | DT_END_ELLIPSIS); SelectObject(hdc, o); };
        HPEN bp = CreatePen(PS_SOLID, 1, RGB(225, 225, 225)); HPEN op = (HPEN)SelectObject(hdc, bp); HBRUSH nb = (HBRUSH)GetStockObject(NULL_BRUSH); HBRUSH ob = (HBRUSH)SelectObject(hdc, nb);
        RoundRect(hdc, 0, 0, m_pwW, m_pwH, (int)(24 * k), (int)(24 * k)); SelectObject(hdc, op); DeleteObject(bp);
        int d = (int)(40 * k), cx = (int)(16 * k), cy = (m_pwH - d) / 2;
        HPEN ip = CreatePen(PS_SOLID, std::max(1, (int)(1.6f * k)), RGB(17, 17, 17)); SelectObject(hdc, ip);
        Ellipse(hdc, cx, cy, cx + d, cy + d);
        int mx = cx + d / 2, my = cy + d / 2, u = (int)(8 * k);
        if (m_pwKind == 6 && m_toastT != L"Download complete") { MoveToEx(hdc, mx, my - u, nullptr); LineTo(hdc, mx, my + u); MoveToEx(hdc, mx - u / 2 - 1, my + u / 2, nullptr); LineTo(hdc, mx, my + u); LineTo(hdc, mx + u / 2 + 2, my + u / 2 - 1); }
        else { MoveToEx(hdc, mx - u, my, nullptr); LineTo(hdc, mx - u / 3, my + u * 2 / 3); LineTo(hdc, mx + u, my - u * 2 / 3); }
        SelectObject(hdc, op); DeleteObject(ip); SelectObject(hdc, ob);
        HFONT fa = font(12, FW_NORMAL), fbd = font(15, FW_SEMIBOLD), fl = font(20, FW_LIGHT);
        int tx = cx + d + (int)(14 * k), tr = m_pwW - (int)(16 * k);
        if (m_pwKind == 6) {
            text(m_toastT, { tx, (int)(12 * k), tr, (int)(38 * k) }, fbd, RGB(17, 17, 17));
            text(m_toastN, { tx, (int)(38 * k), tr, (int)(58 * k) }, fa, RGB(138, 138, 138));
        } else {
            text(L"You just blocked", { tx, (int)(10 * k), tr, (int)(30 * k) }, fa, RGB(138, 138, 138));
            text(std::to_wstring(m_fbAds) + L" ads  \x00b7  " + std::to_wstring(m_fbTrk) + L" trackers", { tx, (int)(30 * k), tr, (int)(60 * k) }, fl, RGB(17, 17, 17));
            text(L"Tap for details", { tx, (int)(60 * k), tr, (int)(78 * k) }, fa, RGB(138, 138, 138));
        }
        DeleteObject(fa); DeleteObject(fbd); DeleteObject(fl); SetViewportOrgEx(hdc, 0, 0, nullptr);
        return;
    }
    auto font = [&](int px, int weight) { return CreateFontW(-MulDiv(px, m_dpi, 96), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI"); };
    auto text = [&](const std::wstring& t, RECT r, HFONT f, COLORREF c, UINT fl) { HFONT o = (HFONT)SelectObject(hdc, f); SetTextColor(hdc, c); DrawTextW(hdc, t.c_str(), -1, &r, fl | DT_NOPREFIX | DT_SINGLELINE | DT_END_ELLIPSIS); SelectObject(hdc, o); };
    HFONT fb = font(15, FW_SEMIBOLD), fs = font(12, FW_NORMAL), fn = font(13, FW_NORMAL), fsb = font(13, FW_SEMIBOLD);
    const COLORREF grey = RGB(138, 138, 138), ink = RGB(17, 17, 17);
    std::wstring site = m_pwOrigin; size_t p = site.find(L"://"); if (p != std::wstring::npos && site.compare(0, p, L"https") == 0) site = site.substr(p + 3);
    if (m_pwKind == 1) {
        text(m_pwUpdate ? L"Update saved password?" : L"Save password?", { pad, (int)(14 * k), rc.right - pad, (int)(40 * k) }, fb, ink, DT_LEFT | DT_VCENTER);
        text(site, { pad, (int)(42 * k), rc.right - pad, (int)(60 * k) }, fs, grey, DT_LEFT);
        text(m_pwPending.user.empty() ? L"(no username)" : m_pwPending.user, { pad, (int)(66 * k), rc.right - pad, (int)(88 * k) }, fn, ink, DT_LEFT | DT_VCENTER);
        text(L"Password  \x2022\x2022\x2022\x2022\x2022\x2022\x2022\x2022", { pad, (int)(86 * k), rc.right - pad, (int)(106 * k) }, fs, grey, DT_LEFT);
        const wchar_t* names[3] = { m_pwUpdate ? L"Update" : L"Save", L"Not now", L"Never" };
        for (int i = 0; i < 3; ++i) {
            RECT b = PwRect(i); bool pri = i == 0, hot = m_pwHot == i;
            FillRR(hdc, b, (b.bottom - b.top) / 2, pri ? (hot ? RGB(60, 60, 60) : RGB(17, 17, 17)) : (hot ? RGB(226, 226, 226) : RGB(239, 239, 239)));
            text(names[i], b, fsb, pri ? RGB(255, 255, 255) : ink, DT_CENTER | DT_VCENTER);
        }
    } else if (m_pwKind == 2) {
        text(L"Sign in to " + site, { pad, (int)(14 * k), rc.right - pad, (int)(40 * k) }, fb, ink, DT_LEFT | DT_VCENTER);
        for (int i = 0; i < (int)m_pwChoice.size(); ++i) {
            const Cred* c = nullptr; for (auto& e : m_vault) if (e.created == m_pwChoice[i]) c = &e;
            RECT r = PwRect(10 + i);
            if (m_pwHot == 10 + i) FillRR(hdc, r, (int)(12 * k), RGB(239, 239, 239));
            RECT tr = { r.left + (int)(12 * k), r.top, r.right - (int)(12 * k), r.bottom };
            text(c ? (c->user.empty() ? L"(no username)" : c->user) : L"", tr, fn, ink, DT_LEFT | DT_VCENTER);
        }
    }
    if (m_pwKind == 4 && IndexOf(m_pwTab) >= 0) {   // Save video: one row per video found
        text(L"Save video", { pad, (int)(14 * k), rc.right - pad, (int)(40 * k) }, fb, ink, DT_LEFT | DT_VCENTER);
        for (int i = 0; i < (int)m_pwChoice.size() && i < (int)m_pwTab->vids.size(); ++i) {
            RECT r = PwRect(10 + i);
            if (m_pwHot == 10 + i) FillRR(hdc, r, (int)(12 * k), RGB(239, 239, 239));
            RECT tr = { r.left + (int)(12 * k), r.top, r.right - (int)(12 * k), r.bottom };
            text(VidLabel(m_pwTab->vids[i].first, m_pwTab->vids[i].second), tr, fn, ink, DT_LEFT | DT_VCENTER);
        }
    }
    if (m_pwKind == 7) {   // risky file finished
        HFONT fl = font(22, FW_LIGHT);
        text(L"Held for your safety", { pad, (int)(14 * k), rc.right - pad, (int)(44 * k) }, fl, ink, DT_LEFT | DT_VCENTER);
        text(m_toastN, { pad, (int)(48 * k), rc.right - pad, (int)(72 * k) }, fb, ink, DT_LEFT | DT_END_ELLIPSIS);
        text(m_riskNote.empty() ? L"Release it only if you trust the site." : m_riskNote, { pad, (int)(76 * k), rc.right - pad, (int)(104 * k) }, fs, grey, DT_LEFT | DT_END_ELLIPSIS);
        const wchar_t* names[2] = { L"Delete", L"Release" };
        for (int i = 0; i < 2; ++i) {
            RECT b = PwRect(i); bool pri = i == 0, hot = m_pwHot == i;
            FillRR(hdc, b, (b.bottom - b.top) / 2, pri ? (hot ? RGB(60, 60, 60) : RGB(17, 17, 17)) : (hot ? RGB(226, 226, 226) : RGB(239, 239, 239)));
            text(names[i], b, fsb, pri ? RGB(255, 255, 255) : ink, DT_CENTER | DT_VCENTER);
        }
        DeleteObject(fl);
    }
    if (m_pwKind == 5) {   // unused tab: ask before keeping it open
        HFONT fl = font(22, FW_LIGHT); std::wstring host = m_pwOrigin;
        text(L"Close this tab?", { pad, (int)(14 * k), rc.right - pad, (int)(44 * k) }, fl, ink, DT_LEFT | DT_VCENTER);
        text(host, { pad, (int)(48 * k), rc.right - pad, (int)(72 * k) }, fb, ink, DT_LEFT | DT_END_ELLIPSIS);
        text(L"You have not used it for a while. Closing it frees memory.", { pad, (int)(76 * k), rc.right - pad, (int)(104 * k) }, fs, grey, DT_LEFT);
        const wchar_t* names[2] = { L"Close tab", L"Keep" };
        for (int i = 0; i < 2; ++i) {
            RECT b = PwRect(i); bool pri = i == 0, hot = m_pwHot == i;
            FillRR(hdc, b, (b.bottom - b.top) / 2, pri ? (hot ? RGB(60, 60, 60) : RGB(17, 17, 17)) : (hot ? RGB(226, 226, 226) : RGB(239, 239, 239)));
            text(names[i], b, fsb, pri ? RGB(255, 255, 255) : ink, DT_CENTER | DT_VCENTER);
        }
        DeleteObject(fl);
    }
    if (m_pwKind == 6) {   // download started / finished
        HFONT fl = font(22, FW_LIGHT);
        text(m_toastT, { pad, (int)(14 * k), rc.right - pad, (int)(44 * k) }, fl, ink, DT_LEFT | DT_VCENTER);
        text(m_toastN, { pad, (int)(52 * k), rc.right - pad, (int)(76 * k) }, fb, ink, DT_LEFT);
        const wchar_t* names[2] = { L"Show", L"Close" };
        for (int i = 0; i < 2; ++i) {
            RECT b = PwRect(i); bool pri = i == 0, hot = m_pwHot == i;
            FillRR(hdc, b, (b.bottom - b.top) / 2, pri ? (hot ? RGB(60, 60, 60) : RGB(17, 17, 17)) : (hot ? RGB(226, 226, 226) : RGB(239, 239, 239)));
            text(names[i], b, fsb, pri ? RGB(255, 255, 255) : ink, DT_CENTER | DT_VCENTER);
        }
        DeleteObject(fl);
    }
    if (m_pwKind == 3) {   // first blocked ads: a small celebration
        HFONT fl = font(22, FW_LIGHT);
        text(L"Wow, you just blocked", { pad, (int)(14 * k), rc.right - pad, (int)(40 * k) }, fb, ink, DT_LEFT | DT_VCENTER);
        text(std::to_wstring(m_fbAds) + L" ads  \x00b7  " + std::to_wstring(m_fbTrk) + L" trackers", { pad, (int)(44 * k), rc.right - pad, (int)(80 * k) }, fl, ink, DT_LEFT | DT_VCENTER);
        text(L"and the cookies they would have set", { pad, (int)(82 * k), rc.right - pad, (int)(104 * k) }, fs, grey, DT_LEFT);
        const wchar_t* names[2] = { L"Details", L"Close" };
        for (int i = 0; i < 2; ++i) {
            RECT b = PwRect(i); bool pri = i == 0, hot = m_pwHot == i;
            FillRR(hdc, b, (b.bottom - b.top) / 2, pri ? (hot ? RGB(60, 60, 60) : RGB(17, 17, 17)) : (hot ? RGB(226, 226, 226) : RGB(239, 239, 239)));
            text(names[i], b, fsb, pri ? RGB(255, 255, 255) : ink, DT_CENTER | DT_VCENTER);
        }
        DeleteObject(fl);
    }
    DeleteObject(fb); DeleteObject(fs); DeleteObject(fn); DeleteObject(fsb);
}
LRESULT CALLBACK BrowserWindow::PwProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* bw = reinterpret_cast<BrowserWindow*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (!bw) return DefWindowProcW(h, msg, wp, lp);
    POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom); HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
        bw->PaintPw(mem); BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0;
    }
    case WM_MOUSEMOVE: {
        TRACKMOUSEEVENT te{ sizeof(te), TME_LEAVE, h, 0 }; TrackMouseEvent(&te);
        int hit = bw->PwHit(pt); if (hit != bw->m_pwHot) { bw->m_pwHot = hit; InvalidateRect(h, nullptr, FALSE); }
        return 0;
    }
    case WM_MOUSELEAVE: bw->m_pwHot = -1; InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_LBUTTONDOWN: { int hit = bw->PwHit(pt); if (hit >= 0) bw->PwClick(hit); return 0; }
    case WM_SETCURSOR: { POINT c; GetCursorPos(&c); ScreenToClient(h, &c); SetCursor(LoadCursor(nullptr, bw->PwHit(c) >= 0 ? IDC_HAND : IDC_ARROW)); return TRUE; }
    }
    return DefWindowProcW(h, msg, wp, lp);
}
