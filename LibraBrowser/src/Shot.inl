// Screenshot (Ctrl+Shift+S or hamburger > Screenshot): a native layered overlay over the page area (websites can never see or draw on it)
// with a 3-way slider Visible | Full page | Selected area. Output = lossless PNG at physical pixels in the Downloads folder.
// Capture uses CDP Page.captureScreenshot (surface only: our overlay is never in the image). Nothing is uploaded, no list is kept.

struct ShotS {
    HWND wnd = nullptr, toast = nullptr, main = nullptr; HINSTANCE inst = nullptr; Tab* tab = nullptr; wil::com_ptr<ICoreWebView2> wv;
    int W = 0, H = 0; float u = 1.f; POINT org{}; int mode = 0; float hi = 0.f; bool busy = false, dragging = false, hasSel = false, dirty = true, test = false;
    int hover = -1, dragKind = 0; RECT sel{}, sel0{}; POINT p0{}; std::wstring busyMsg; DWORD opened = 0;
    std::function<bool()> alive; std::function<void(int)> onMode;
    std::wstring base; std::vector<std::wstring> files; std::wstring toastMsg; bool toastErr = false; int toastHover = -1; DWORD toastT0 = 0;
};
static ShotS g_s;
static int ShPngW(const std::string& b) { return b.size() > 24 ? (int)(((unsigned char)b[16] << 24) | ((unsigned char)b[17] << 16) | ((unsigned char)b[18] << 8) | (unsigned char)b[19]) : 0; }
// CDP clip scale = output pixels per CSS pixel. We ask for physical pixels and CHECK the width of the PNG that comes back; if the engine multiplied by the device scale as well, one correction retry fixes it (tests: tools/tests/iso/shot.cjs).

// ---- layout (all in overlay pixels) ----
static RECT ShBar() { int bw = (int)(388 * g_s.u), bh = (int)(46 * g_s.u), y = (int)(18 * g_s.u); int x = (g_s.W - bw) / 2; return { x, y, x + bw, y + bh }; }
static RECT ShSeg(int i) { RECT b = ShBar(); int pad = (int)(6 * g_s.u), sw = (int)(108 * g_s.u); return { b.left + pad + i * sw, b.top + pad, b.left + pad + (i + 1) * sw, b.bottom - pad }; }
static RECT ShCap() { RECT b = ShBar(); int d = (int)(34 * g_s.u), pad = (int)(6 * g_s.u); return { b.right - pad - d, b.top + (b.bottom - b.top - d) / 2, b.right - pad, b.top + (b.bottom - b.top - d) / 2 + d }; }
static RECT ShBtn(bool save) {   // Save / Cancel next to the selection
    int bw = (int)(88 * g_s.u), bh = (int)(34 * g_s.u), gap = (int)(8 * g_s.u), m = (int)(8 * g_s.u);
    int right = std::min((int)g_s.sel.right, g_s.W - m), y = g_s.sel.bottom + (int)(12 * g_s.u);
    if (y + bh > g_s.H - m) { y = g_s.sel.top - bh - (int)(12 * g_s.u); if (y < m) y = g_s.sel.bottom - bh - m; }
    int x = right - bw - (save ? 0 : bw + gap); if (x < m) x = m + (save ? bw + gap : 0);
    return { x, y, x + bw, y + bh };
}
static bool ShIn(const RECT& r, int x, int y) { return x >= r.left && x < r.right && y >= r.top && y < r.bottom; }
static RECT ShHandle(int k) {   // 0..7 = NW N NE E SE S SW W
    int cx[8] = { g_s.sel.left, (g_s.sel.left + g_s.sel.right) / 2, g_s.sel.right, g_s.sel.right, g_s.sel.right, (g_s.sel.left + g_s.sel.right) / 2, g_s.sel.left, g_s.sel.left };
    int cy[8] = { g_s.sel.top, g_s.sel.top, g_s.sel.top, (g_s.sel.top + g_s.sel.bottom) / 2, g_s.sel.bottom, g_s.sel.bottom, g_s.sel.bottom, (g_s.sel.top + g_s.sel.bottom) / 2 };
    int h = (int)(7 * g_s.u); return { cx[k] - h, cy[k] - h, cx[k] + h, cy[k] + h };
}
static int ShHit(int x, int y) {   // 0-2 segment, 3 capture, 4 save, 5 cancel, 10 inside selection, 11-18 handle, -1 nothing
    if (g_s.busy) return -1;
    if (g_s.hasSel && g_s.mode == 2) {
        if (ShIn(ShBtn(true), x, y)) return 4; if (ShIn(ShBtn(false), x, y)) return 5;
        for (int k = 0; k < 8; ++k) if (ShIn(ShHandle(k), x, y)) return 11 + k;
    }
    for (int i = 0; i < 3; ++i) if (ShIn(ShSeg(i), x, y)) return i;
    if (g_s.mode < 2 && ShIn(ShCap(), x, y)) return 3;
    if (g_s.hasSel && g_s.mode == 2 && ShIn(g_s.sel, x, y)) return 10;
    return -1;
}

// ---- drawing ----
static void ShPaint(HWND wnd, int W, int H, POINT org, const std::function<void(Gdiplus::Graphics&)>& fn, BYTE alpha = 255, int dx = 0) {
    using namespace Gdiplus;
    Bitmap bmp(W, H, PixelFormat32bppPARGB); Graphics g(&bmp); g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    fn(g);
    HBITMAP hb = nullptr; bmp.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hb); if (!hb) return;
    HDC sdc = GetDC(nullptr), mdc = CreateCompatibleDC(sdc); HGDIOBJ old = SelectObject(mdc, hb);
    POINT dst{ org.x + dx, org.y }, src{ 0, 0 }; SIZE sz{ W, H }; BLENDFUNCTION bf{ AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA };
    UpdateLayeredWindow(wnd, sdc, &dst, &sz, mdc, &src, 0, &bf, ULW_ALPHA);
    SelectObject(mdc, old); DeleteObject(hb); DeleteDC(mdc); ReleaseDC(nullptr, sdc);
}
static void ShRound(Gdiplus::GraphicsPath& p, float x, float y, float w, float h, float r) {
    p.AddArc(x, y, 2 * r, 2 * r, 180, 90); p.AddArc(x + w - 2 * r, y, 2 * r, 2 * r, 270, 90); p.AddArc(x + w - 2 * r, y + h - 2 * r, 2 * r, 2 * r, 0, 90); p.AddArc(x, y + h - 2 * r, 2 * r, 2 * r, 90, 90); p.CloseFigure();
}
static void ShText(Gdiplus::Graphics& g, const wchar_t* s, RECT r, float px, bool bold, Gdiplus::Color c, bool left = false) {
    Gdiplus::FontFamily ff(L"Segoe UI"); Gdiplus::Font f(&ff, px, bold ? Gdiplus::FontStyleBold : Gdiplus::FontStyleRegular, Gdiplus::UnitPixel); Gdiplus::SolidBrush b(c);
    Gdiplus::StringFormat sf; sf.SetAlignment(left ? Gdiplus::StringAlignmentNear : Gdiplus::StringAlignmentCenter); sf.SetLineAlignment(Gdiplus::StringAlignmentCenter); sf.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);
    g.DrawString(s, -1, &f, Gdiplus::RectF((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top)), &sf, &b);
}
static void ShDraw() {
    if (!g_s.wnd) return; g_s.dirty = false;
    ShPaint(g_s.wnd, g_s.W, g_s.H, g_s.org, [](Gdiplus::Graphics& g) {
        using namespace Gdiplus; const float u = g_s.u; const bool area = g_s.mode == 2;
        SolidBrush dim(Gdiplus::Color(area && g_s.hasSel ? 120 : 70, 0, 0, 0)); g.FillRectangle(&dim, 0, 0, g_s.W, g_s.H);   // alpha > 0 everywhere: the overlay owns the mouse
        if (area && g_s.hasSel) {   // the chosen area is cut out (alpha 2: still catches the mouse)
            g.SetCompositingMode(CompositingModeSourceCopy); SolidBrush hole(Gdiplus::Color(2, 0, 0, 0)); g.FillRectangle(&hole, (INT)g_s.sel.left, (INT)g_s.sel.top, (INT)(g_s.sel.right - g_s.sel.left), (INT)(g_s.sel.bottom - g_s.sel.top)); g.SetCompositingMode(CompositingModeSourceOver);
            Pen pw(Gdiplus::Color(255, 255, 255, 255), 2.f * u); g.DrawRectangle(&pw, (REAL)g_s.sel.left, (REAL)g_s.sel.top, (REAL)(g_s.sel.right - g_s.sel.left), (REAL)(g_s.sel.bottom - g_s.sel.top));
            SolidBrush hw(Gdiplus::Color(255, 255, 255, 255)); Pen hp(Gdiplus::Color(255, 17, 17, 17), 1.f);
            for (int k = 0; k < 8; ++k) { RECT h = ShHandle(k); int s = (int)(5 * u); RectF hr((REAL)((h.left + h.right) / 2 - s), (REAL)((h.top + h.bottom) / 2 - s), (REAL)(2 * s), (REAL)(2 * s)); g.FillRectangle(&hw, hr); g.DrawRectangle(&hp, hr); }
            wchar_t dim2[40]; swprintf(dim2, 40, L"%d \x00D7 %d", (int)(g_s.sel.right - g_s.sel.left), (int)(g_s.sel.bottom - g_s.sel.top));
            RECT lr = { g_s.sel.left, std::max(4, (int)(g_s.sel.top - 30 * u)), g_s.sel.left + (int)(96 * u), std::max(4, (int)(g_s.sel.top - 30 * u)) + (int)(24 * u) };
            GraphicsPath lp; ShRound(lp, (REAL)lr.left, (REAL)lr.top, (REAL)(lr.right - lr.left), (REAL)(lr.bottom - lr.top), 12 * u); SolidBrush lb(Gdiplus::Color(235, 17, 17, 17)); g.FillPath(&lb, &lp);
            ShText(g, dim2, lr, 12 * u, false, Gdiplus::Color(255, 255, 255, 255));
            for (int s2 = 0; s2 < 2; ++s2) {   // Cancel (0) then Save (1)
                RECT b = ShBtn(s2 == 1); bool hov = g_s.hover == (s2 ? 4 : 5); GraphicsPath bp; ShRound(bp, (REAL)b.left, (REAL)b.top, (REAL)(b.right - b.left), (REAL)(b.bottom - b.top), 17 * u);
                SolidBrush bb(s2 ? Gdiplus::Color(255, hov ? 60 : 17, hov ? 60 : 17, hov ? 60 : 17) : Gdiplus::Color(255, hov ? 235 : 255, hov ? 235 : 255, hov ? 235 : 255)); g.FillPath(&bb, &bp);
                ShText(g, s2 ? L"Save" : L"Cancel", b, 14 * u, s2 == 1, s2 ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, 17, 17, 17));
            }
        }
        // the bar
        RECT b = ShBar(); GraphicsPath bp; ShRound(bp, (REAL)b.left, (REAL)b.top, (REAL)(b.right - b.left), (REAL)(b.bottom - b.top), (b.bottom - b.top) / 2.f);
        SolidBrush bf(Gdiplus::Color(250, 255, 255, 255)); g.FillPath(&bf, &bp); Pen bo(Gdiplus::Color(255, 224, 224, 224), 1.f); g.DrawPath(&bo, &bp);
        if (g_s.busy) {
            ShText(g, g_s.busyMsg.c_str(), b, 14 * u, false, Gdiplus::Color(255, 17, 17, 17));
            float t = (GetTickCount() % 1200) / 1200.f; REAL lw = (b.right - b.left) * 0.3f; REAL lx = b.left + (b.right - b.left - lw) * t; SolidBrush pb(Gdiplus::Color(255, 17, 17, 17));
            g.FillRectangle(&pb, lx, (REAL)(b.bottom - 5 * u), lw, 2.f * u); return;
        }
        RECT s0 = ShSeg(0), s1 = ShSeg(1); float hx = s0.left + (s1.left - s0.left) * g_s.hi;   // the gliding highlight
        GraphicsPath hp; ShRound(hp, hx, (REAL)s0.top, (REAL)(s0.right - s0.left), (REAL)(s0.bottom - s0.top), (s0.bottom - s0.top) / 2.f); SolidBrush hb(Gdiplus::Color(255, 17, 17, 17)); g.FillPath(&hb, &hp);
        const wchar_t* nm[3] = { L"Visible", L"Full page", L"Selected area" };
        for (int i = 0; i < 3; ++i) { float d = std::fabs(g_s.hi - i); bool on = d < 0.5f; ShText(g, nm[i], ShSeg(i), 13 * u, on, on ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, g_s.hover == i ? 17 : 90, g_s.hover == i ? 17 : 90, g_s.hover == i ? 17 : 90)); }
        if (g_s.mode < 2) {   // shutter button
            RECT c = ShCap(); bool hov = g_s.hover == 3; SolidBrush cb(Gdiplus::Color(255, hov ? 70 : 17, hov ? 70 : 17, hov ? 70 : 17)); g.FillEllipse(&cb, (REAL)c.left, (REAL)c.top, (REAL)(c.right - c.left), (REAL)(c.bottom - c.top));
            float cx = (c.left + c.right) / 2.f, cy = (c.top + c.bottom) / 2.f, r1 = 9.f * u, r2 = 5.f * u; Pen rp(Gdiplus::Color(255, 255, 255, 255), 1.6f * u); g.DrawEllipse(&rp, cx - r1, cy - r1, 2 * r1, 2 * r1);
            SolidBrush wbr(Gdiplus::Color(255, 255, 255, 255)); g.FillEllipse(&wbr, cx - r2, cy - r2, 2 * r2, 2 * r2);
        }
        const wchar_t* hint = g_s.mode == 0 ? L"What you see now. Press Enter or the button." : g_s.mode == 1 ? L"The whole page, top to bottom." : g_s.hasSel ? L"Drag the edges to adjust. Enter saves, Esc cancels." : L"Drag on the page to choose the area.";
        RECT hr = { 0, b.bottom + (int)(8 * u), g_s.W, b.bottom + (int)(34 * u) }; ShText(g, hint, hr, 13 * u, false, Gdiplus::Color(255, 255, 255, 255));
    });
}

// ---- file + capture ----
static std::wstring ShDir() {
    wchar_t tf[MAX_PATH]; if (TestEnv(L"LIBRA_TEST_SHOTDIR", tf, MAX_PATH) > 0) return tf;
    PWSTR p = nullptr; std::wstring d; if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &p)) && p) d = p; CoTaskMemFree(p);
    if (d.empty() || !PathIsDirectoryW(d.c_str())) { p = nullptr; d.clear(); if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &p)) && p) d = p; CoTaskMemFree(p); }
    return d;
}
static std::string ShB64(const std::wstring& j) {   // "data":"<base64>" from a CDP result
    size_t a = j.find(L"\"data\":\""); if (a == std::wstring::npos) return ""; a += 8; size_t e = j.find(L'"', a); if (e == std::wstring::npos) return "";
    DWORD n = 0; if (!CryptStringToBinaryW(j.c_str() + a, (DWORD)(e - a), CRYPT_STRING_BASE64, nullptr, &n, nullptr, nullptr) || !n) return "";
    std::string out(n, '\0'); if (!CryptStringToBinaryW(j.c_str() + a, (DWORD)(e - a), CRYPT_STRING_BASE64, (BYTE*)out.data(), &n, nullptr, nullptr)) return ""; out.resize(n); return out;
}
static std::wstring ShWrite(const std::string& png, const std::wstring& suffix) {
    std::wstring dir = ShDir(); if (dir.empty() || png.size() < 8 || png.compare(1, 3, "PNG") != 0) return L"";
    std::wstring path = dir + L"\\" + g_s.base + suffix + L".png";
    for (int n = 2; GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES && n < 100; ++n) path = dir + L"\\" + g_s.base + suffix + L" (" + std::to_wstring(n) + L").png";
    std::wstring tmp = path + L".tmp"; HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr); if (f == INVALID_HANDLE_VALUE) return L"";
    bool ok = true; size_t off = 0; while (ok && off < png.size()) { DWORD w = 0, c = (DWORD)std::min<size_t>(png.size() - off, 1 << 24); ok = WriteFile(f, png.data() + off, c, &w, nullptr) && w == c; off += w; }
    CloseHandle(f); if (!ok || !MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING)) { DeleteFileW(tmp.c_str()); return L""; }
    return path;
}
static void ShCdp(const std::wstring& method, const std::wstring& params, std::function<void(HRESULT, std::wstring)> cb) {
    HRESULT r = g_s.wv ? g_s.wv->CallDevToolsProtocolMethod(method.c_str(), params.c_str(), Microsoft::WRL::Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>(
        [cb](HRESULT hr, LPCWSTR res) -> HRESULT { cb(hr, res ? res : L""); return S_OK; }).Get()) : E_FAIL;
    if (FAILED(r)) cb(r, L"");
}
static void ShClose(bool refocus = true) {
    KillTimer(g_s.main, 2050); if (g_s.wnd) { DestroyWindow(g_s.wnd); g_s.wnd = nullptr; } g_s.busy = false; g_s.dragging = false;
    if (refocus && g_s.main) SetForegroundWindow(g_s.main);
}
static LRESULT CALLBACK ShToastProc(HWND h, UINT m, WPARAM w, LPARAM l);
static void ShToast(bool err, const std::wstring& msg) {
    ShClose(); g_s.toastErr = err; g_s.toastMsg = msg; g_s.toastHover = -1; g_s.toastT0 = GetTickCount();
    static bool reg = false; if (!reg) { WNDCLASSEXW wc{ sizeof wc }; wc.lpfnWndProc = ShToastProc; wc.hInstance = g_s.inst; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.lpszClassName = L"LibraShotToast"; RegisterClassExW(&wc); reg = true; }
    if (g_s.toast) { DestroyWindow(g_s.toast); g_s.toast = nullptr; }
    int w = (int)(400 * g_s.u), hh = (int)(60 * g_s.u);
    g_s.toast = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, L"LibraShotToast", L"", WS_POPUP, g_s.org.x + g_s.W - w - (int)(18 * g_s.u), g_s.org.y + g_s.H - hh - (int)(18 * g_s.u), w, hh, g_s.main, nullptr, g_s.inst, nullptr);
    if (!g_s.toast) return; SetTimer(g_s.toast, 1, 16, nullptr); ShowWindow(g_s.toast, SW_SHOWNOACTIVATE);
}
static void ShDone(bool ok, const std::wstring& msg) {
    { wchar_t tf[MAX_PATH]; if (TestEnv(L"LIBRA_TEST_SHOTDIR", tf, MAX_PATH) > 0) WriteAllRaw(std::wstring(tf) + L"\\shot.log", ToUtf8(std::wstring(ok ? L"ok: " : L"FAILED: ") + msg + L"\n"), true); }   // test hook only
    if (ok) ShToast(false, msg); else ShToast(true, msg);
}
static std::wstring ShStamp() { SYSTEMTIME t; GetLocalTime(&t); wchar_t b[64]; swprintf(b, 64, L"Libra-Screenshot-%04d-%02d-%02d-%02d-%02d-%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond); return b; }
static double ShNum(const std::wstring& s, int idx) { size_t p = 0; for (int i = 0; i < idx; ++i) { p = s.find(L',', p); if (p == std::wstring::npos) return 0; ++p; } return _wtof(s.c_str() + p); }
static std::wstring ShVal(const std::wstring& j) { size_t a = j.find(L"\"value\":\""); if (a == std::wstring::npos) return L""; a += 9; size_t e = j.find(L'"', a); return e == std::wstring::npos ? L"" : j.substr(a, e - a); }

// Full page = strips: scroll to each 1024-px strip first (so lazy / virtualised content is really there), capture it, paste it into one tall image (GDI+).
// Fixed and sticky elements are hidden after the first strip (so a header is not repeated) and restored at the end.
struct FullJob { double dpr = 1, scale = 1; int Wp = 0, Hp = 0, partHp = 0, n = 0, part = 0, strip = 0; bool fixed = false, hid = false; std::unique_ptr<Gdiplus::Bitmap> comp; };
static FullJob g_job;
static bool ShPngClsid(CLSID* c) {
    UINT n = 0, sz = 0; Gdiplus::GetImageEncodersSize(&n, &sz); if (!sz) return false; std::vector<char> buf(sz); auto* e = (Gdiplus::ImageCodecInfo*)buf.data(); Gdiplus::GetImageEncoders(n, sz, e);
    for (UINT k = 0; k < n; ++k) if (wcscmp(e[k].MimeType, L"image/png") == 0) { *c = e[k].Clsid; return true; } return false;
}
static void ShRestore() { if (g_job.hid && g_s.wv) { g_job.hid = false; ShCdp(L"Runtime.evaluate", L"{\"expression\":\"(()=>{for(const [e,v,p] of (window.__lbH||[]))e.style.setProperty('visibility',v,p);window.__lbH=[];return 1})()\",\"returnByValue\":true}", [](HRESULT, std::wstring) {}); } }
static void ShDone(bool ok, const std::wstring& msg);
static void ShFullStep() {
    if (!g_s.wnd && !g_s.busy) return;
    if (g_job.part >= g_job.n) { ShRestore(); ShDone(true, g_s.files.size() == 1 ? L"Screenshot saved" : L"Saved " + std::to_wstring(g_s.files.size()) + L" parts (very long page)"); return; }
    int partY0 = g_job.part * g_job.partHp, partH = std::min(g_job.partHp, g_job.Hp - partY0);
    if (!g_job.comp) { g_job.comp.reset(new Gdiplus::Bitmap(g_job.Wp, partH, PixelFormat24bppRGB)); g_job.strip = 0; }
    int yP = partY0 + g_job.strip * 1024;
    if (yP - partY0 >= partH) {   // this part is complete: encode and save it
        CLSID png; IStream* st = nullptr; std::string bytes; bool ok = ShPngClsid(&png) && SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &st)) && g_job.comp->Save(st, &png) == Gdiplus::Ok;
        if (ok) { HGLOBAL hg = nullptr; GetHGlobalFromStream(st, &hg); SIZE_T n = GlobalSize(hg); void* p = GlobalLock(hg); if (p) bytes.assign((const char*)p, n); GlobalUnlock(hg); }
        if (st) st->Release(); g_job.comp.reset();
        std::wstring path = bytes.empty() ? L"" : ShWrite(bytes, g_job.n > 1 ? L"-part" + std::to_wstring(g_job.part + 1) : L"");
        if (path.empty()) { ShRestore(); ShDone(false, L"The page could not be saved."); return; }
        g_s.files.push_back(path); ++g_job.part; ShFullStep(); return;
    }
    int hP = std::min(1024, partY0 + partH - yP); int total = (g_job.Hp + 1023) / 1024, doneN = (partY0 + 1023) / 1024 + g_job.strip;
    g_s.busyMsg = L"Capturing " + std::to_wstring(std::min(total, doneN + 1)) + L" of " + std::to_wstring(total) + L"..."; g_s.dirty = true;
    wchar_t sp[260]; swprintf(sp, 260, L"{\"expression\":\"(async()=>{scrollTo({top:%.2f,behavior:'instant'});await new Promise(r=>setTimeout(r,850));return 1})()\",\"awaitPromise\":true,\"returnByValue\":true}", yP / g_job.dpr);
    ShCdp(L"Runtime.evaluate", sp, [yP, hP](HRESULT, std::wstring) {
        wchar_t ps[300]; swprintf(ps, 300, L"{\"format\":\"png\",\"captureBeyondViewport\":true,\"fromSurface\":true,\"clip\":{\"x\":0,\"y\":%.3f,\"width\":%.3f,\"height\":%.3f,\"scale\":%.5f}}", yP / g_job.dpr, g_job.Wp / g_job.dpr, hP / g_job.dpr, g_job.scale);
        ShCdp(L"Page.captureScreenshot", ps, [yP, hP](HRESULT hr, std::wstring r) {
            std::string b = FAILED(hr) ? "" : ShB64(r); double got = ShPngW(b);
            if (b.empty() || got <= 0) { ShRestore(); ShDone(false, L"The page could not be captured."); return; }
            if (!g_job.fixed && std::fabs(got - g_job.Wp) > 2) { g_job.scale *= g_job.Wp / got; g_job.fixed = true; ShFullStep(); return; }   // engine used another scale: redo this strip once
            g_job.fixed = true;
            IStream* ms = SHCreateMemStream((const BYTE*)b.data(), (UINT)b.size()); std::unique_ptr<Gdiplus::Bitmap> sb(ms ? Gdiplus::Bitmap::FromStream(ms) : nullptr);
            if (!sb || sb->GetLastStatus() != Gdiplus::Ok) { if (ms) ms->Release(); ShRestore(); ShDone(false, L"The page could not be captured."); return; }
            { Gdiplus::Graphics g(g_job.comp.get()); g.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor); g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
              g.DrawImage(sb.get(), Gdiplus::Rect(0, g_job.strip * 1024, std::min((int)sb->GetWidth(), g_job.Wp), (int)sb->GetHeight()), 0, 0, std::min((int)sb->GetWidth(), g_job.Wp), (int)sb->GetHeight(), Gdiplus::UnitPixel); }
            sb.reset(); ms->Release();
            auto next = []() { ++g_job.strip; ShFullStep(); };
            if (!g_job.hid && g_job.part == 0 && g_job.strip == 0) {   // after the first strip: hide fixed/sticky elements so they are not repeated
                g_job.hid = true;
                ShCdp(L"Runtime.evaluate", L"{\"expression\":\"(()=>{window.__lbH=[];for(const e of document.querySelectorAll('*')){const p=getComputedStyle(e).position;if(p==='fixed'||p==='sticky'){window.__lbH.push([e,e.style.getPropertyValue('visibility'),e.style.getPropertyPriority('visibility')]);e.style.setProperty('visibility','hidden','important')}}return window.__lbH.length})()\",\"returnByValue\":true}", [next](HRESULT, std::wstring) { next(); });
            } else next();
        });
    });
}
static void ShFullStart(double wcss, double hcss, double dpr) {
    g_job.comp.reset(); g_job.dpr = 1; g_job.scale = 1; g_job.Wp = g_job.Hp = g_job.partHp = g_job.n = g_job.part = g_job.strip = 0; g_job.fixed = g_job.hid = false;
    FullJob& J = g_job; J.dpr = dpr; J.scale = dpr; J.Wp = (int)std::round(wcss * dpr); J.Hp = (int)std::round(hcss * dpr);
    J.partHp = std::max(1024, ((int)std::min(16000.0, 60e6 / J.Wp) / 1024) * 1024); J.n = std::min(12, (J.Hp + J.partHp - 1) / J.partHp); ShFullStep();
}
static void ShRun() {
    if (g_s.busy || !g_s.wv) return; g_s.busy = true; g_s.base = ShStamp(); g_s.files.clear(); g_s.busyMsg = L"Capturing..."; g_s.dirty = true; ShDraw();
    if (g_s.mode == 0) {
        ShCdp(L"Page.captureScreenshot", L"{\"format\":\"png\",\"fromSurface\":true}", [](HRESULT hr, std::wstring r) {
            std::string b = FAILED(hr) ? "" : ShB64(r); std::wstring p = b.empty() ? L"" : ShWrite(b, L"");
            if (p.empty()) { ShDone(false, L"The page could not be captured or saved."); return; } g_s.files.push_back(p); ShDone(true, L"Screenshot saved"); });
    } else if (g_s.mode == 1) {
        const wchar_t* js = L"(async()=>{const d=document.documentElement,b=document.body;const x=scrollX,y=scrollY;const H=()=>Math.max(d.scrollHeight,b?b.scrollHeight:0);let h=H();"
            L"for(let p=0,i=0;p<h&&i<120;i++){scrollTo({top:p,behavior:'instant'});await new Promise(r=>setTimeout(r,260));p+=innerHeight*0.6;h=H()}"
            L"scrollTo({top:H(),behavior:'instant'});await new Promise(r=>setTimeout(r,900));scrollTo({top:0,behavior:'instant'});await new Promise(r=>setTimeout(r,700));scrollTo({left:x,top:y,behavior:'instant'});await new Promise(r=>setTimeout(r,250));return [Math.max(d.scrollWidth,b?b.scrollWidth:0),H(),devicePixelRatio].join(',')})()";
        ShCdp(L"Runtime.evaluate", std::wstring(L"{\"expression\":\"") + js + L"\",\"awaitPromise\":true,\"returnByValue\":true}", [](HRESULT hr, std::wstring r) {
            std::wstring v = ShVal(r); double w = ShNum(v, 0), h = ShNum(v, 1), dpr = ShNum(v, 2); if (FAILED(hr) || w < 1 || h < 1 || dpr < 0.5) { ShDone(false, L"The page could not be measured."); return; }
            ShFullStart(std::min(w, std::floor(16000.0 / dpr)), h, dpr); });
    } else {
        g_s.wv->ExecuteScript(L"[innerWidth,innerHeight,scrollX,scrollY].join(',')", Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>([](HRESULT hr, LPCWSTR res) -> HRESULT {
            std::wstring v = res ? res : L""; if (!v.empty() && v[0] == L'"') v = v.substr(1); double iw = ShNum(v, 0), ih = ShNum(v, 1), sx = ShNum(v, 2), sy = ShNum(v, 3);
            if (FAILED(hr) || iw < 1 || ih < 1) { ShDone(false, L"The page could not be measured."); return S_OK; }
            double fx = iw / g_s.W, fy = ih / g_s.H; const double selW = g_s.sel.right - g_s.sel.left;
            auto cap = std::make_shared<std::function<void(double, bool)>>();
            *cap = [=](double scale, bool fixed) {
                wchar_t ps[300];
                swprintf(ps, 300, L"{\"format\":\"png\",\"fromSurface\":true,\"captureBeyondViewport\":false,\"clip\":{\"x\":%.3f,\"y\":%.3f,\"width\":%.3f,\"height\":%.3f,\"scale\":%.5f}}",
                         g_s.sel.left * fx + sx, g_s.sel.top * fy + sy, selW * fx, (g_s.sel.bottom - g_s.sel.top) * fy, scale);
                ShCdp(L"Page.captureScreenshot", ps, [=](HRESULT hr2, std::wstring r) {
                    std::string b = FAILED(hr2) ? "" : ShB64(r); double got = ShPngW(b);
                    if (!b.empty() && !fixed && got > 0 && std::fabs(got - selW) > 2) { (*cap)(scale * selW / got, true); return; }   // engine used another scale: redo once
                    std::wstring p = b.empty() ? L"" : ShWrite(b, L"");
                    if (p.empty()) { ShDone(false, L"The area could not be captured or saved."); return; } g_s.files.push_back(p); ShDone(true, L"Screenshot saved"); });
            };
            (*cap)(1.0 / fx, false);
            return S_OK; }).Get());
    }
}

// ---- overlay window ----
static void ShSetMode(int m) { if (m < 0 || m > 2 || m == g_s.mode || g_s.busy) return; g_s.mode = m; if (m != 2) { g_s.hasSel = false; g_s.dragging = false; } if (g_s.onMode) g_s.onMode(m); g_s.dirty = true; }
static void ShNorm(RECT& r) { if (r.left > r.right) std::swap(r.left, r.right); if (r.top > r.bottom) std::swap(r.top, r.bottom); r.left = std::max(0L, r.left); r.top = std::max(0L, r.top); r.right = std::min((LONG)g_s.W, r.right); r.bottom = std::min((LONG)g_s.H, r.bottom); }
static LRESULT CALLBACK ShProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    int x = (short)LOWORD(l), y = (short)HIWORD(l);
    switch (m) {
    case WM_MOUSEMOVE: {
        if (g_s.dragging) {
            RECT r = g_s.sel0; int dx = x - g_s.p0.x, dy = y - g_s.p0.y, k = g_s.dragKind;
            if (k == 1) { r = { std::min(g_s.p0.x, (LONG)x), std::min(g_s.p0.y, (LONG)y), std::max(g_s.p0.x, (LONG)x), std::max(g_s.p0.y, (LONG)y) }; }
            else if (k == 2) { int ww = r.right - r.left, hh = r.bottom - r.top; r.left = std::max(0, std::min((int)g_s.W - ww, (int)(g_s.sel0.left + dx))); r.top = std::max(0, std::min((int)g_s.H - hh, (int)(g_s.sel0.top + dy))); r.right = r.left + ww; r.bottom = r.top + hh; }
            else { int e = k - 11; if (e == 0 || e == 6 || e == 7) r.left += dx; if (e == 2 || e == 3 || e == 4) r.right += dx; if (e == 0 || e == 1 || e == 2) r.top += dy; if (e == 4 || e == 5 || e == 6) r.bottom += dy; }
            ShNorm(r); g_s.sel = r; g_s.hasSel = true; g_s.dirty = true; return 0;
        }
        int hv = ShHit(x, y); if (hv != g_s.hover) { g_s.hover = hv; g_s.dirty = true; }
        TRACKMOUSEEVENT te{ sizeof te, TME_LEAVE, h, 0 }; TrackMouseEvent(&te); return 0; }
    case WM_MOUSELEAVE: g_s.hover = -1; g_s.dirty = true; return 0;
    case WM_LBUTTONDOWN: {
        if (g_s.busy) return 0; int hit = ShHit(x, y); SetFocus(h);
        if (hit >= 0 && hit <= 2) { ShSetMode(hit); return 0; }
        if (hit == 3) { ShRun(); return 0; }
        if (hit == 4) { ShRun(); return 0; }
        if (hit == 5) { g_s.hasSel = false; g_s.dirty = true; return 0; }
        if (g_s.mode == 2) {
            g_s.dragKind = hit == 10 ? 2 : hit >= 11 ? hit : 1; g_s.p0 = { x, y }; g_s.sel0 = g_s.sel; g_s.dragging = true; SetCapture(h);
            if (g_s.dragKind == 1) { g_s.hasSel = false; } g_s.dirty = true;
        }
        return 0; }
    case WM_LBUTTONUP: {
        if (g_s.dragging) { g_s.dragging = false; ReleaseCapture(); if (g_s.hasSel && (g_s.sel.right - g_s.sel.left < 8 || g_s.sel.bottom - g_s.sel.top < 8)) g_s.hasSel = false; g_s.hover = ShHit(x, y); g_s.dirty = true; }
        return 0; }
    case WM_SETCURSOR: {
        POINT p; GetCursorPos(&p); ScreenToClient(h, &p); int hit = ShHit(p.x, p.y); LPCWSTR c = IDC_ARROW;
        if (g_s.busy) c = IDC_WAIT; else if (hit >= 0 && hit <= 5) c = IDC_HAND; else if (hit == 10) c = IDC_SIZEALL;
        else if (hit >= 11) { int e = hit - 11; c = (e == 1 || e == 5) ? IDC_SIZENS : (e == 3 || e == 7) ? IDC_SIZEWE : (e == 0 || e == 4) ? IDC_SIZENWSE : IDC_SIZENESW; }
        else if (g_s.mode == 2) c = IDC_CROSS;
        SetCursor(LoadCursor(nullptr, c)); return TRUE; }
    case WM_MOUSEACTIVATE: return MA_ACTIVATE;
    }
    return DefWindowProcW(h, m, w, l);
}
static LRESULT CALLBACK ShToastProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    int x = (short)LOWORD(l), y = (short)HIWORD(l); int tw = (int)(400 * g_s.u), th = (int)(60 * g_s.u);
    RECT rFolder = { (int)(150 * g_s.u), (int)(14 * g_s.u), (int)(280 * g_s.u), th - (int)(14 * g_s.u) }, rCopy = { (int)(290 * g_s.u), (int)(14 * g_s.u), tw - (int)(14 * g_s.u), th - (int)(14 * g_s.u) };
    auto hit = [&](int px, int py) { if (g_s.toastErr) return -1; return ShIn(rFolder, px, py) ? 0 : ShIn(rCopy, px, py) ? 1 : -1; };
    switch (m) {
    case WM_TIMER: {
        DWORD t = GetTickCount() - g_s.toastT0; float k = std::min(1.f, t / 220.f); k = 1.f - (1.f - k) * (1.f - k);   // slides in from the right
        int off = (int)((1.f - k) * (tw + 30 * g_s.u)); float fade = t > 8000 ? std::max(0.f, 1.f - (t - 8000) / 300.f) : 1.f;
        if (t > 8300) { KillTimer(h, 1); DestroyWindow(h); g_s.toast = nullptr; return 0; }
        POINT base{ g_s.org.x + g_s.W - tw - (int)(18 * g_s.u), g_s.org.y + g_s.H - th - (int)(18 * g_s.u) };
        ShPaint(h, tw, th, base, [&](Gdiplus::Graphics& g) {
            using namespace Gdiplus; const float u = g_s.u; GraphicsPath p; ShRound(p, 0.5f, 0.5f, (REAL)tw - 1, (REAL)th - 1, 14 * u); SolidBrush bg(Gdiplus::Color(252, 255, 255, 255)); g.FillPath(&bg, &p); Pen bo(Gdiplus::Color(255, 224, 224, 224), 1.f); g.DrawPath(&bo, &p);
            RECT tr = { (int)(16 * u), 0, (int)(150 * u), th }; if (g_s.toastErr) tr.right = tw - (int)(16 * u);
            ShText(g, g_s.toastMsg.c_str(), tr, 14 * u, true, g_s.toastErr ? Gdiplus::Color(255, 180, 30, 30) : Gdiplus::Color(255, 17, 17, 17), true);
            if (!g_s.toastErr) for (int i = 0; i < 2; ++i) { RECT b = i ? rCopy : rFolder; bool hv = g_s.toastHover == i; GraphicsPath bp; ShRound(bp, (REAL)b.left, (REAL)b.top, (REAL)(b.right - b.left), (REAL)(b.bottom - b.top), (b.bottom - b.top) / 2.f);
                SolidBrush bb(i ? Gdiplus::Color(255, hv ? 60 : 17, hv ? 60 : 17, hv ? 60 : 17) : Gdiplus::Color(255, hv ? 232 : 243, hv ? 232 : 243, hv ? 232 : 243)); g.FillPath(&bb, &bp);
                ShText(g, i ? L"Copy" : L"Show in folder", b, 12.5f * u, false, i ? Gdiplus::Color(255, 255, 255, 255) : Gdiplus::Color(255, 17, 17, 17)); }
        }, (BYTE)(255 * fade), off);
        return 0; }
    case WM_MOUSEMOVE: { int hv = hit(x, y); if (hv != g_s.toastHover) { g_s.toastHover = hv; } SetCursor(LoadCursor(nullptr, hv >= 0 ? IDC_HAND : IDC_ARROW)); return 0; }
    case WM_LBUTTONDOWN: {
        int hv = hit(x, y);
        if (hv == 0 && !g_s.files.empty()) { std::wstring a = L"/select,\"" + g_s.files[0] + L"\""; ShellExecuteW(nullptr, L"open", L"explorer.exe", a.c_str(), nullptr, SW_SHOWNORMAL); }
        if (hv == 1 && !g_s.files.empty()) {   // put the (first) image on the clipboard
            Gdiplus::Bitmap* bm = Gdiplus::Bitmap::FromFile(g_s.files[0].c_str()); HBITMAP hb = nullptr;
            if (bm && bm->GetLastStatus() == Gdiplus::Ok && bm->GetHBITMAP(Gdiplus::Color(255, 255, 255, 255), &hb) == Gdiplus::Ok && OpenClipboard(h)) { EmptyClipboard(); if (!SetClipboardData(CF_BITMAP, hb)) DeleteObject(hb); CloseClipboard(); g_s.toastMsg = L"Copied"; }
            delete bm;
        }
        if (hv >= 0) g_s.toastT0 = GetTickCount() - 7000; return 0; }
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    }
    return DefWindowProcW(h, m, w, l);
}

// ---- BrowserWindow glue ----
void BrowserWindow::OpenShot() {
    if (g_s.wnd) { ShClose(); return; }
    if (m_activeTab < 0 || m_activeTab >= (int)m_tabs.size() || m_setup || m_burning) return;
    Tab* t = m_tabs[m_activeTab].get(); RECT rc{}; if (!t->webView || !t->controller || FAILED(t->controller->get_Bounds(&rc))) return;
    if (g_s.toast) { DestroyWindow(g_s.toast); g_s.toast = nullptr; }
    g_s.main = m_hWnd; g_s.inst = m_hInst; g_s.tab = t; g_s.wv = t->webView; g_s.u = GetDpiForWindow(m_hWnd) / 96.f; g_s.W = rc.right - rc.left; g_s.H = rc.bottom - rc.top;
    g_s.org = { rc.left, rc.top }; ClientToScreen(m_hWnd, &g_s.org); if (g_s.W < 200 || g_s.H < 120) return;
    int sm = _wtoi(m_set.Get(L"shot_mode", L"0").c_str()); g_s.mode = sm >= 0 && sm <= 2 ? sm : 0; g_s.hi = (float)g_s.mode; g_s.hasSel = false; g_s.busy = false; g_s.dragging = false; g_s.hover = -1; g_s.test = false;
    bool priv = m_incognito; g_s.onMode = [this, priv](int v) { if (!priv) m_set.Set(L"shot_mode", std::to_wstring(v)); };   // (a private window leaves the settings file alone)
    g_s.alive = [this, t]() { return m_activeTab >= 0 && m_activeTab < (int)m_tabs.size() && m_tabs[m_activeTab].get() == t; };
    static bool reg = false; if (!reg) { WNDCLASSEXW wc{ sizeof wc }; wc.lpfnWndProc = ShProc; wc.hInstance = m_hInst; wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.lpszClassName = L"LibraShot"; RegisterClassExW(&wc); reg = true; }
    g_s.wnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW, L"LibraShot", L"", WS_POPUP, g_s.org.x, g_s.org.y, g_s.W, g_s.H, m_hWnd, nullptr, m_hInst, nullptr);
    if (!g_s.wnd) return; g_s.opened = GetTickCount(); ShDraw(); ShowWindow(g_s.wnd, SW_SHOW); SetForegroundWindow(g_s.wnd); SetFocus(g_s.wnd); SetTimer(m_hWnd, 2050, 16, nullptr);
}
bool BrowserWindow::ShotKey(UINT vk) {
    if (!g_s.wnd || g_s.busy) return false;
    if (vk == VK_ESCAPE) { if (g_s.mode == 2 && g_s.hasSel) { g_s.hasSel = false; g_s.dirty = true; } else ShClose(); return true; }
    if (vk == VK_RETURN) { if (g_s.mode < 2 || g_s.hasSel) ShRun(); return true; }
    if (vk == VK_LEFT) { ShSetMode(g_s.mode - 1); return true; }
    if (vk == VK_RIGHT) { ShSetMode(g_s.mode + 1); return true; }
    return false;
}
bool BrowserWindow::ShotTimer(UINT_PTR id) {
    if (id == 2043) {   // test hook: LIBRA_TEST_SHOT = "<mode>" or "2:x,y,w,h" (overlay pixels); needs libra-test.enabled
        KillTimer(m_hWnd, 2043); wchar_t v[64]; if (TestEnv(L"LIBRA_TEST_SHOT", v, 64) <= 0) return true;
        OpenShot(); if (!g_s.wnd) return true; g_s.test = true; bool only = v[0] >= L'3'; if (only) v[0] -= 3; g_s.mode = v[0] - L'0'; g_s.hi = (float)g_s.mode;   // '3'..'5' = open only (for screenshots of the bar)
        if (g_s.mode == 2) { int x = 0, y = 0, w = 0, h = 0; if (swscanf_s(v + 2, L"%d,%d,%d,%d", &x, &y, &w, &h) == 4) { g_s.sel = { x, y, x + w, y + h }; g_s.hasSel = true; } }
        if (only) { g_s.dirty = true; ShDraw(); return true; }
        ShRun(); return true;
    }
    if (id != 2050) return false;
    if (!g_s.wnd) { KillTimer(m_hWnd, 2050); return true; }
    if (!g_s.busy && !g_s.test) {   // leaving: another tab, a moved window, or focus given to something else closes it
        RECT rc{}; POINT o{ 0, 0 }; bool bad = !g_s.alive || !g_s.alive() || !g_s.tab->controller || FAILED(g_s.tab->controller->get_Bounds(&rc));
        if (!bad) { o = { rc.left, rc.top }; ClientToScreen(m_hWnd, &o); bad = o.x != g_s.org.x || o.y != g_s.org.y || rc.right - rc.left != g_s.W || rc.bottom - rc.top != g_s.H; }
        if (bad || (GetTickCount() - g_s.opened > 600 && GetForegroundWindow() != g_s.wnd)) { ShClose(false); return true; }
    }
    float d = g_s.mode - g_s.hi; if (std::fabs(d) > 0.005f) { g_s.hi += d * 0.3f; if (std::fabs(g_s.mode - g_s.hi) < 0.01f) g_s.hi = (float)g_s.mode; g_s.dirty = true; }
    if (g_s.busy) g_s.dirty = true;
    if (g_s.dirty) ShDraw();
    return true;
}
