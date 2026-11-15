// Auto suggest: click an empty text box, see what your saved notes say, click it to type it. Included at the end of BrowserWindow.cpp.
// LOCAL ONLY: no AI, no network, nothing leaves the PC. The page tells us only which box you clicked (SgSite.h). The popup is a Libra window
// the site cannot see or click; the text is typed only after YOU click a row, into the same still-empty box. See PLAN-autosuggest.md.
#include "SgSite.h"

static const wchar_t* kSgClass = L"LibraSg";
static void SgLog(const std::string& s) {   // tests only (needs libra-test.enabled): one line per decision
    wchar_t p[MAX_PATH]; if (!TestEnv(L"LIBRA_TEST_SGLOG", p, MAX_PATH)) return;
    HANDLE h = CreateFileW(p, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr); if (h == INVALID_HANDLE_VALUE) return;
    std::string l = std::to_string(GetTickCount() % 1000000) + " " + s + "\n"; DWORD w; WriteFile(h, l.data(), (DWORD)l.size(), &w, nullptr); CloseHandle(h);
}
static std::string SgClean(const std::wstring& w, size_t n) { std::string s = ToUtf8(w); for (auto& c : s) if (c == '\t' || c == '\r' || c == '\n') c = ' '; if (s.size() > n) s.resize(n); return s; }
bool BrowserWindow::SgOn() const { return m_set.Get(L"autosuggest", L"0") == L"1" && !m_incognito && !m_child; }

void BrowserWindow::InstallSgScript(Tab* t) {
    if (!t->webView) return;
    if (!t->sgScriptId.empty()) { t->webView->RemoveScriptToExecuteOnDocumentCreated(t->sgScriptId.c_str()); t->sgScriptId.clear(); }
    if (!SgOn() || t->incognito) return;
    t->webView->AddScriptToExecuteOnDocumentCreated(kSgSite, Microsoft::WRL::Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
        [this, t](HRESULT, LPCWSTR id) -> HRESULT { if (id && IndexOf(t) >= 0) t->sgScriptId = id; return S_OK; }).Get());
}
void BrowserWindow::SgSetting() {   // the switch changed: scripts on or off for every open tab, popup gone
    HideSg(); for (auto& tb : m_tabs) InstallSgScript(tb.get());
}

const std::vector<SgPair>& BrowserWindow::SgPairs() {   // the notes switched on in the assistant's Memory, decrypted in memory only; rebuilt when the file changes
    WIN32_FILE_ATTRIBUTE_DATA fa{}; ULONGLONG stamp = 0;
    if (GetFileAttributesExW((DataDir() + L"\\memory.dat").c_str(), GetFileExInfoStandard, &fa)) stamp = (((ULONGLONG)fa.ftLastWriteTime.dwHighDateTime << 32) | fa.ftLastWriteTime.dwLowDateTime) ^ ((ULONGLONG)fa.nFileSizeLow << 7) ^ 1;
    if (stamp == m_sgStamp && m_sgLoaded) return m_sgPairs;
    m_sgPairs.clear(); m_sgLoaded = true; m_sgStamp = stamp;
    if (stamp) for (auto& d : AgMemLoad()) if (d.on && m_sgPairs.size() < 3000) { auto p = SgParse(ToUtf8(d.text)); m_sgPairs.insert(m_sgPairs.end(), p.begin(), p.end()); }
    return m_sgPairs;
}

static bool SgUserIdle() { LASTINPUTINFO li{ sizeof(li) }; return GetLastInputInfo(&li) && (DWORD)(GetTickCount() - li.dwTime) > 2500; }   // no real keyboard or mouse use for 2.5 s: nobody is clicking
static RECT SgWebRect(HWND wnd, Tab* t) {   // the tab's page area in screen pixels
    RECT cb{}; if (t && t->controller) t->controller->get_Bounds(&cb); POINT o = { cb.left, cb.top }; ClientToScreen(wnd, &o);
    return { o.x, o.y, o.x + (cb.right - cb.left), o.y + (cb.bottom - cb.top) };
}
void BrowserWindow::OnSiteSgMessage(Tab* t, const std::wstring& srcUrl, const std::wstring& msg) {
    if (!SgOn()) return;
    auto no = [&](const char* why) { SgLog(std::string("REFUSE ") + why); };   // the reason goes to the test log, like chrome://autofill-internals
    if (!t->webView || t->incognito || m_agRun || m_setup) { no("mode"); return; }
    DWORD now = GetTickCount(); if (now - m_sgRate[0] > 10000) { m_sgRate[0] = now; m_sgRate[1] = 0; } if (++m_sgRate[1] > 60) { no("rate"); return; }   // 60 messages / 10 s
    std::wstring origin = OriginOf(srcUrl); if (origin.empty()) { no("origin"); return; }
    { wil::unique_cotaskmem_string cur; t->webView->get_Source(&cur); if (!cur.get() || OriginOf(cur.get()) != origin) { no("origin"); return; } }   // only the page this tab is really showing
    auto v = SplitTab(msg); if (v.empty()) return;
    if (v[0] == L"sg.hide") { SgLog(m_sgTab == t ? "HIDE" : "HIDE-other"); if (m_sgTab == t) HideSg(); return; }
    if (v[0] != L"sg.focus" || v.size() != 15) { no("format"); return; }
    if (m_certBad.count(HostOf(srcUrl))) { no("certificate"); return; }
    if (m_activeTab < 0 || m_tabs[m_activeTab].get() != t) { no("inactive-tab"); return; }
    wchar_t th[8]; const bool test = TestEnv(L"LIBRA_TEST_SGCLICK", th, 8) > 0;
    if (!test && (GetForegroundWindow() != m_hWnd || IsIconic(m_hWnd))) { no("background-window"); return; }
    if ((!test || TestEnv(L"LIBRA_TEST_SGIDLE", th, 8)) && SgUserIdle()) { no("idle"); return; }   // tests skip this unless they ask for it (they click through the debugger, not the keyboard)
    { std::set<std::wstring> nv; for (auto& l : Lines(ReadAll(DataDir() + L"\\sgnever.txt"))) nv.insert(l); if (nv.count(origin)) { no("never"); return; } }
    SgField f; f.label = SgClean(v[1], 160); f.placeholder = SgClean(v[2], 120); f.name = SgClean(v[3], 80); f.id = SgClean(v[4], 80); f.autocomplete = SgClean(v[5], 40); f.type = SgClean(v[6], 12); f.aria = SgClean(v[7], 120);
    f.textarea = v[8] == L"1"; f.maxlen = std::max(0, std::min(_wtoi(v[9].c_str()), 100000));
    double dpr = _wtof(v[14].c_str()); if (!(dpr >= 0.25 && dpr <= 8)) dpr = 1;
    int rx = _wtoi(v[10].c_str()), ry = _wtoi(v[11].c_str()), rw = _wtoi(v[12].c_str()), rh = _wtoi(v[13].c_str());
    if (rw < 1 || rh < 1 || rw > 20000 || rh > 20000) { no("size"); return; }
    if (SgSensitiveField(f)) { SgLog("REFUSE sensitive " + f.label + "|" + f.name); HideSg(); return; }
    std::vector<SgHit> hits = SgMatch(SgPairs(), f, 3);
    if (hits.empty()) { SgLog("NONE " + f.label + "|" + f.placeholder + "|" + f.name); HideSg(); return; }
    RECT wv = SgWebRect(m_hWnd, t);   // where the page is on screen: the popup may never leave it (no drawing over the toolbar, other popups or prompts)
    RECT box = { wv.left + (LONG)(rx * dpr), wv.top + (LONG)(ry * dpr), wv.left + (LONG)((rx + rw) * dpr), wv.top + (LONG)((ry + rh) * dpr) };
    POINT mid = { (box.left + box.right) / 2, (box.top + box.bottom) / 2 };
    if (!PtInRect(&wv, mid) || (box.right - box.left) > 2 * (wv.right - wv.left) || (box.bottom - box.top) > 2 * (wv.bottom - wv.top)) { no("outside"); HideSg(); return; }
    std::string lg = "SHOW " + f.label + "|" + f.name + " =>"; for (auto& h : hits) lg += " [" + h.label + ": " + h.value + "]"; SgLog(lg);
    m_sgField = f; m_sgHits = hits; m_sgTab = t; m_sgOrigin = origin; m_sgHot = -1; m_sgShown = GetTickCount(); m_sgBox = box; m_sgWv = wv;
    ShowSg();
    if (test) { wchar_t n[8]; TestEnv(L"LIBRA_TEST_SGCLICK", n, 8); m_sgTestClick = _wtoi(n); SetTimer(m_hWnd, 2042, 300, nullptr); }
}

void BrowserWindow::ShowSg() {
    if (!m_hSg) {
        WNDCLASSEXW wc{ sizeof(wc) }; wc.style = 0; wc.lpfnWndProc = SgProc; wc.hInstance = m_hInst; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.lpszClassName = kSgClass; RegisterClassExW(&wc);
        m_hSg = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE, kSgClass, L"", WS_POPUP, 0, 0, 10, 10, m_hWnd, nullptr, m_hInst, nullptr);
        SetWindowLongPtrW(m_hSg, GWLP_USERDATA, (LONG_PTR)this);
    }
    const int rowH = MulDiv(46, m_dpi, 96), footH = MulDiv(26, m_dpi, 96), w = std::max(MulDiv(280, m_dpi, 96), std::min((int)(m_sgBox.right - m_sgBox.left), MulDiv(460, m_dpi, 96)));
    const int h = rowH * (int)m_sgHits.size() + footH + MulDiv(8, m_dpi, 96);
    HMONITOR mon = MonitorFromRect(&m_sgBox, MONITOR_DEFAULTTONEAREST); MONITORINFO mi{ sizeof(mi) }; GetMonitorInfoW(mon, &mi);
    RECT area; if (!IntersectRect(&area, &m_sgWv, &mi.rcWork)) return;   // the page area on this monitor: the popup never leaves it
    const int gap = MulDiv(4, m_dpi, 96);
    int x = std::max((int)area.left, std::min((int)m_sgBox.left, (int)area.right - w)), y = m_sgBox.bottom + gap;
    if (y + h > area.bottom) { int ya = m_sgBox.top - h - gap; y = ya >= area.top ? ya : std::max((int)area.top, (int)area.bottom - h); }   // no room below: above the box, else the bottom of the page
    SetWindowPos(m_hSg, HWND_TOPMOST, x, y, w, h, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    SetWindowRgn(m_hSg, CreateRoundRectRgn(0, 0, w + 1, h + 1, MulDiv(16, m_dpi, 96), MulDiv(16, m_dpi, 96)), TRUE);
    SetTimer(m_hWnd, 2041, 500, nullptr); InvalidateRect(m_hSg, nullptr, FALSE);
}
void BrowserWindow::HideSg() {
    KillTimer(m_hWnd, 2041); KillTimer(m_hWnd, 2042);
    if (m_hSg && IsWindowVisible(m_hSg)) ShowWindow(m_hSg, SW_HIDE);
    m_sgTab = nullptr; m_sgHits.clear(); m_sgHot = -1;
}
void BrowserWindow::SgTick() {   // popup follows its tab: gone when the window or tab changes, the page moves on, or after 20 s
    if (!m_sgTab) { KillTimer(m_hWnd, 2041); return; }
    int i = IndexOf(m_sgTab); bool bad = i < 0 || i != m_activeTab || m_agRun || GetTickCount() - m_sgShown > 20000;
    wchar_t th[8]; if (!bad && !TestEnv(L"LIBRA_TEST_SGCLICK", th, 8) && (GetForegroundWindow() != m_hWnd || IsIconic(m_hWnd))) bad = true;
    if (!bad) { RECT wv = SgWebRect(m_hWnd, m_tabs[i].get()); if (wv.left != m_sgWv.left || wv.top != m_sgWv.top || wv.right != m_sgWv.right || wv.bottom != m_sgWv.bottom) bad = true; }   // the window or the page area moved or resized
    if (!bad && m_tabs[i]->webView) { wil::unique_cotaskmem_string cur; m_tabs[i]->webView->get_Source(&cur); if (!cur.get() || OriginOf(cur.get()) != m_sgOrigin) bad = true; }
    if (bad) HideSg();
}
void BrowserWindow::SgClick(int what) {
    if (what == 100) { std::wstring o = m_sgOrigin; HideSg(); if (!o.empty()) WriteAll(DataDir() + L"\\sgnever.txt", ToUtf8(o) + "\n", true); SgLog("NEVER " + ToUtf8(o)); return; }
    if (what < 0 || what >= (int)m_sgHits.size()) return;
    SgInsert(what);
}
void BrowserWindow::SgInsert(int i) {
    std::string val = m_sgHits[i].value; Tab* t = m_sgTab; std::wstring origin = m_sgOrigin; SgField f = m_sgField; HideSg();
    if (!t || IndexOf(t) < 0 || !t->webView || m_activeTab < 0 || m_tabs[m_activeTab].get() != t || !SgOn() || m_agRun) return;
    { wil::unique_cotaskmem_string cur; t->webView->get_Source(&cur); if (!cur.get() || OriginOf(cur.get()) != origin || m_certBad.count(HostOf(cur.get()))) return; }   // exact site, still
    // the same box must still be focused and empty (a page cannot swap the field between the click and the typing)
    static const wchar_t* chk = L"(function(){var e=document.activeElement;if(!e||!e.tagName)return'';function C(s,n){s=String(s==null?'':s).replace(/[\\t\\r\\n]+/g,' ').trim();return s.length>n?s.slice(0,n):s}"
        L"return [e.tagName,(e.type||'text').toLowerCase(),C(e.name,80),C(e.id,80),C(e.placeholder,120),(e.value||'').length,e.maxLength>0?e.maxLength:0,e.readOnly?1:0,e.disabled?1:0].join('\\u0001')})()";
    t->webView->ExecuteScript(chk, Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
        [this, t, val, f](HRESULT, LPCWSTR res) -> HRESULT {
            if (IndexOf(t) < 0 || !res) return S_OK;
            std::string lit = ToUtf8(res), s; size_t p = 0; if (!SpReadStr(lit, p, s) || s.empty()) { SgLog("ABORT no focused box"); return S_OK; }
            std::vector<std::string> a; size_t q = 0; for (;;) { size_t e = s.find('\x01', q); a.push_back(s.substr(q, e == std::string::npos ? e : e - q)); if (e == std::string::npos) break; q = e + 1; }
            if (a.size() != 9) return S_OK;
            std::string want = f.textarea ? "TEXTAREA" : "INPUT";
            if (a[0] != want || a[1] != f.type || a[2] != f.name || a[3] != f.id || a[4] != f.placeholder || a[5] != "0" || a[7] != "0" || a[8] != "0") { SgLog("ABORT field changed"); return S_OK; }
            int ml = atoi(a[6].c_str()); std::string text = val;
            if (!f.textarea) for (auto& c : text) if (c == '\n' || c == '\r') c = ' ';
            if ((ml > 0 && (int)text.size() > ml) || text.empty() || text.size() > 4000) { SgLog("ABORT does not fit"); return S_OK; }
            std::wstring params = L"{\"text\":" + JsLit(FromUtf8(text)) + L"}";
            t->webView->CallDevToolsProtocolMethod(L"Input.insertText", params.c_str(), nullptr);   // real typing events: React, Angular and Google Forms see it
            Wipe(params); SgLog("INSERTED " + text);
            return S_OK;
        }).Get());
}

void BrowserWindow::PaintSg(HDC hdc) {
    RECT rc; GetClientRect(m_hSg, &rc); HBRUSH w = CreateSolidBrush(RGB(255, 255, 255)); FillRect(hdc, &rc, w); DeleteObject(w);
    HBRUSH bd = CreateSolidBrush(RGB(222, 222, 222)); HRGN rg = CreateRoundRectRgn(0, 0, rc.right, rc.bottom, MulDiv(16, m_dpi, 96), MulDiv(16, m_dpi, 96)); FrameRgn(hdc, rg, bd, 1, 1); DeleteObject(rg); DeleteObject(bd);
    const int rowH = MulDiv(46, m_dpi, 96), pad = MulDiv(14, m_dpi, 96), top = MulDiv(4, m_dpi, 96); SetBkMode(hdc, TRANSPARENT);
    HFONT big = CreateFontW(-MulDiv(14, m_dpi, 96), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT sm = CreateFontW(-MulDiv(11, m_dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    for (int i = 0; i < (int)m_sgHits.size(); ++i) {
        RECT r = { MulDiv(6, m_dpi, 96), top + i * rowH, rc.right - MulDiv(6, m_dpi, 96), top + (i + 1) * rowH };
        if (m_sgHot == i) { HBRUSH hb = CreateSolidBrush(RGB(243, 243, 243)); HPEN np = CreatePen(PS_NULL, 0, 0); HGDIOBJ ob = SelectObject(hdc, hb), op = SelectObject(hdc, np); RoundRect(hdc, r.left, r.top, r.right, r.bottom, MulDiv(10, m_dpi, 96), MulDiv(10, m_dpi, 96)); SelectObject(hdc, ob); SelectObject(hdc, op); DeleteObject(hb); DeleteObject(np); }
        std::wstring val = FromUtf8(m_sgHits[i].value), lab = FromUtf8(m_sgHits[i].label); for (auto& c : val) if (c == L'\n' || c == L'\r') c = L' ';
        RECT t1 = { pad, r.top + MulDiv(5, m_dpi, 96), rc.right - pad, r.top + MulDiv(26, m_dpi, 96) }, t2 = { pad, r.top + MulDiv(26, m_dpi, 96), rc.right - pad, r.bottom };
        SelectObject(hdc, big); SetTextColor(hdc, RGB(17, 17, 17)); DrawTextW(hdc, val.c_str(), -1, &t1, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(hdc, sm); SetTextColor(hdc, RGB(136, 136, 136)); DrawTextW(hdc, lab.c_str(), -1, &t2, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    }
    RECT ft = { pad, top + (int)m_sgHits.size() * rowH, rc.right - pad, rc.bottom - MulDiv(2, m_dpi, 96) };
    SelectObject(hdc, sm); SetTextColor(hdc, m_sgHot == 100 ? RGB(17, 17, 17) : RGB(150, 150, 150)); DrawTextW(hdc, L"Don't suggest on this site", -1, &ft, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    DeleteObject(big); DeleteObject(sm);
}
int BrowserWindow::SgRowAt(POINT p) const {
    const int rowH = MulDiv(46, m_dpi, 96), top = MulDiv(4, m_dpi, 96), n = (int)m_sgHits.size();
    if (p.y >= top && p.y < top + n * rowH) return (p.y - top) / rowH;
    if (p.y >= top + n * rowH) return 100;
    return -1;
}
LRESULT CALLBACK BrowserWindow::SgProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* bw = reinterpret_cast<BrowserWindow*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (!bw) return DefWindowProcW(h, msg, wp, lp);
    POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom); HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
        bw->PaintSg(mem); BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY); SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0;
    }
    case WM_MOUSEMOVE: { TRACKMOUSEEVENT te{ sizeof(te), TME_LEAVE, h, 0 }; TrackMouseEvent(&te); int hit = bw->SgRowAt(pt); if (hit != bw->m_sgHot) { bw->m_sgHot = hit; InvalidateRect(h, nullptr, FALSE); } return 0; }
    case WM_MOUSELEAVE: bw->m_sgHot = -1; InvalidateRect(h, nullptr, FALSE); return 0;
    case WM_LBUTTONDOWN: { int hit = bw->SgRowAt(pt); if (hit >= 0) bw->SgClick(hit); return 0; }
    case WM_SETCURSOR: { POINT c; GetCursorPos(&c); ScreenToClient(h, &c); SetCursor(LoadCursor(nullptr, bw->SgRowAt(c) >= 0 ? IDC_HAND : IDC_ARROW)); return TRUE; }
    }
    return DefWindowProcW(h, msg, wp, lp);
}
