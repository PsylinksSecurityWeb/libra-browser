// Burn: hamburger menu entry. Asks "Are you sure?", plays a fire animation, closes every tab (and private windows), deletes history, cookies,
// site data, cache, DNS memory, counters, home shortcuts, download list and assistant data (Privacy.inl WipeAll), then quits and starts a fresh Libra.
// Kept on purpose: bookmarks, saved logins, settings, API key. Not offered in private windows (nothing is saved there) or in Child Safety.

static HWND g_burnWnd = nullptr; static DWORD g_burnT0 = 0, g_burnDoneMs = 0; static int g_burnTries = 0;
static bool g_burnWiped = false, g_burnOk = true; static std::wstring g_burnErr;
static const DWORD kBurnMin = 2800, kBurnEnd = 800;   // the fire burns at least this long; then 0.8 s to black and quit

static LRESULT CALLBACK BurnProc(HWND h, UINT m, WPARAM w, LPARAM l) { return DefWindowProcW(h, m, w, l); }   // swallows clicks while burning
static float BurnRnd(int i, int k) { unsigned x = (unsigned)(i * 7919 + k * 104729 + 12345); x ^= x << 13; x ^= x >> 17; x ^= x << 5; x ^= x << 7; return (x & 0xFFFF) / 65535.0f; }

static void BurnDraw(HWND h, DWORD ms, int W, int H, HWND main) {
    using namespace Gdiplus;
    Bitmap bmp(W, H, PixelFormat32bppPARGB); Graphics g(&bmp); g.SetSmoothingMode(SmoothingModeAntiAlias);
    const float t = ms / 1000.f;
    const float e = g_burnDoneMs ? std::min(1.f, (float)(ms - g_burnDoneMs) / kBurnEnd) : 0.f;   // 0 -> 1 while the fire dies and the screen goes black
    const float fadeIn = std::min(1.f, ms / 450.f);
    SolidBrush dim(Gdiplus::Color((BYTE)(205 * fadeIn + 50 * e), 14, 8, 8)); g.FillRectangle(&dim, 0, 0, W, H);
    const float reach = std::min(1.f, ms / 1200.f) * (1.f - e);   // the fire climbs, then dies down at the end
    const float k = (1.f - e);
    if (reach > 0.01f) {
        int gh = (int)(H * 0.6f * reach) + 2;   // glow on the ground
        LinearGradientBrush glow(Point(0, H - gh), Point(0, H), Gdiplus::Color(1, 200, 60, 0), Gdiplus::Color((BYTE)(200 * k), 255, 110, 10));
        g.FillRectangle(&glow, 0, H - gh, W, gh);
        auto tongue = [&](float x, float w, float hh, float sway, Gdiplus::Color c) {   // one teardrop flame
            GraphicsPath p;
            p.AddBezier(PointF(x - w, (float)H), PointF(x - w * 0.95f, H - hh * 0.40f), PointF(x + sway * 0.35f - w * 0.30f, H - hh * 0.78f), PointF(x + sway, H - hh));
            p.AddBezier(PointF(x + sway, H - hh), PointF(x + sway * 0.35f + w * 0.30f, H - hh * 0.78f), PointF(x + w * 0.95f, H - hh * 0.40f), PointF(x + w, (float)H));
            p.CloseFigure(); SolidBrush b(c); g.FillPath(&b, &p);
        };
        for (int row = 0; row < 3; ++row) {   // back (tall, deep red) -> front (short, bright)
            const int n = 14 + row * 5; const float rowH = (0.62f - row * 0.15f), rowA = 150.f + row * 45.f;
            for (int i = 0; i < n; ++i) {
                int s = row * 100 + i;
                float x = (i + 0.5f + (BurnRnd(s, 1) - 0.5f) * 0.9f) / n * W, w = (float)W / n * (0.95f + BurnRnd(s, 2) * 0.8f);
                float fl = 0.72f + 0.28f * std::sin(t * (4.f + BurnRnd(s, 3) * 6.f) + BurnRnd(s, 4) * 6.28f) * std::sin(t * 2.3f + i);
                float hh = H * rowH * (0.55f + 0.45f * BurnRnd(s, 5)) * fl * reach;
                float sw = std::sin(t * (1.6f + BurnRnd(s, 6) * 2.f) + BurnRnd(s, 7) * 6.28f) * w * 0.9f;
                BYTE a = (BYTE)(rowA * k);
                tongue(x, w,          hh,          sw,        Gdiplus::Color(a, 200 - row * 15, 30 + row * 10, 8));          // outer: red
                tongue(x, w * 0.74f,  hh * 0.78f,  sw * 0.9f, Gdiplus::Color(a, 255, 115 + row * 15, 10));                    // orange
                tongue(x, w * 0.48f,  hh * 0.55f,  sw * 0.75f, Gdiplus::Color(a, 255, 205, 55));                              // yellow
                if (row > 0) tongue(x, w * 0.24f, hh * 0.30f, sw * 0.6f, Gdiplus::Color(a, 255, 245, 175));                   // hot core
            }
        }
        for (int i = 0; i < 70; ++i) {   // sparks
            float u = std::fmod(t * (0.25f + BurnRnd(i, 11) * 0.5f) + BurnRnd(i, 12), 1.f);
            float x = BurnRnd(i, 13) * W + std::sin(u * 9.f + i) * 30.f, y = H - u * H * (0.3f + 0.65f * reach) * (0.5f + 0.5f * BurnRnd(i, 14)), r = 1.5f + BurnRnd(i, 15) * 2.5f;
            SolidBrush b(Gdiplus::Color((BYTE)((1.f - u) * 240 * k), 255, 190, 70)); g.FillEllipse(&b, x - r, y - r, r * 2, r * 2);
        }
    }
    { FontFamily ff(L"Segoe UI"); Font f(&ff, 26.f, FontStyleBold, UnitPixel); SolidBrush wb(Gdiplus::Color((BYTE)(235 * fadeIn), 255, 255, 255));
      StringFormat sf; sf.SetAlignment(StringAlignmentCenter); sf.SetLineAlignment(StringAlignmentCenter);
      g.DrawString(g_burnDoneMs ? L"All clear. Restarting..." : L"Burning your data...", -1, &f, RectF(0, H * 0.22f, (float)W, 50.f), &sf, &wb); }
    HBITMAP hb = nullptr; bmp.GetHBITMAP(Gdiplus::Color(0, 0, 0, 0), &hb); if (!hb) return;
    HDC sdc = GetDC(nullptr), mdc = CreateCompatibleDC(sdc); HGDIOBJ old = SelectObject(mdc, hb);
    POINT org{ 0, 0 }; ClientToScreen(main, &org);
    POINT dst = org, src{ 0, 0 }; SIZE sz{ W, H }; BLENDFUNCTION bf{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    UpdateLayeredWindow(h, sdc, &dst, &sz, mdc, &src, 0, &bf, ULW_ALPHA);
    SelectObject(mdc, old); DeleteObject(hb); DeleteDC(mdc); ReleaseDC(nullptr, sdc);
}

void BrowserWindow::Burn() {
    if (m_incognito || m_child || m_setup || m_burning || !m_envReady) return;
    bool yes = false; wchar_t tf[8];
    if (TestEnv(L"LIBRA_TEST_BURN", tf, 8) > 0) yes = true;   // tests only
    else {
        TASKDIALOG_BUTTON btns[] = { { 201, L"&Yes, burn" }, { 202, L"Cancel" } };
        TASKDIALOGCONFIG tc{ sizeof tc }; tc.hwndParent = m_hWnd; tc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW; tc.pszWindowTitle = L"Burn";
        tc.pszMainInstruction = L"Are you sure you want to burn everything?";
        tc.pszContent = L"Libra will close all tabs and private windows, permanently delete your browsing history, cookies, site data, cache, home shortcuts, download list, tracking counters, DNS memory and assistant data, then restart.\n\nKept: bookmarks, saved logins, settings and your API key. Downloads in progress will stop.\n\nThis cannot be undone.";
        tc.pButtons = btns; tc.cButtons = 2; tc.nDefaultButton = 202; tc.pszMainIcon = TD_WARNING_ICON;
        int btn = 0; if (SUCCEEDED(TaskDialogIndirect(&tc, &btn, nullptr, nullptr))) yes = btn == 201;
    }
    if (!yes) return;
    m_burning = true; g_burnWiped = false; g_burnOk = true; g_burnErr.clear(); g_burnDoneMs = 0; g_burnTries = 0;
    static bool reg = false; if (!reg) { WNDCLASSEXW wc{ sizeof wc }; wc.lpfnWndProc = BurnProc; wc.hInstance = m_hInst; wc.hCursor = LoadCursor(nullptr, IDC_WAIT); wc.lpszClassName = L"LibraBurn"; RegisterClassExW(&wc); reg = true; }
    RECT cr; GetClientRect(m_hWnd, &cr); POINT org{ 0, 0 }; ClientToScreen(m_hWnd, &org);
    g_burnWnd = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"LibraBurn", L"", WS_POPUP, org.x, org.y, cr.right, cr.bottom, m_hWnd, nullptr, m_hInst, nullptr);
    g_burnT0 = GetTickCount();
    if (g_burnWnd) { BurnDraw(g_burnWnd, 0, cr.right, cr.bottom, m_hWnd); ShowWindow(g_burnWnd, SW_SHOWNOACTIVATE); }
    HidePw();
    // a fresh Privacy tab (hidden under the fire) carries the clearing; every other tab is closed, whatever screen the user was on
    OpenTab(L"libra://privacy"); m_burnTab = m_tabs.back().get();
    for (int i = (int)m_tabs.size() - 2; i >= 0; --i) CloseTab(i);
    DeleteFileW((DataDir() + L"\\tiles.txt").c_str()); DeleteFileW((DataDir() + L"\\pinned.txt").c_str());
    m_wipeDone = [](bool ok, std::wstring err) { g_burnWiped = true; g_burnOk = ok; g_burnErr = err; };
    SetTimer(m_hWnd, 2070, 500, nullptr); SetTimer(m_hWnd, 2071, 33, nullptr);
}

static BOOL CALLBACK BurnClosePriv(HWND h, LPARAM) { wchar_t c[40] = {}; GetClassNameW(h, c, 40); if (wcscmp(c, L"LibraBrowserWndP") == 0) PostMessageW(h, WM_CLOSE, 0, 0); return TRUE; }

bool BrowserWindow::BurnTimer(UINT_PTR id) {
    if (id == 2072) { KillTimer(m_hWnd, 2072); Burn(); return true; }   // test hook only (LIBRA_TEST_OPEN=burn)
    if (id == 2070) {
        KillTimer(m_hWnd, 2070);
        int i = IndexOf(m_burnTab);
        if (i >= 0 && !m_tabs[i]->webView && ++g_burnTries < 40) { SetTimer(m_hWnd, 2070, 250, nullptr); return true; }   // the engine of the new tab is still starting
        if (i >= 0) WipeAll(i); else { g_burnWiped = true; g_burnOk = false; g_burnErr = L"engine not ready"; }
        return true;
    }
    if (id != 2071) return false;
    DWORD ms = GetTickCount() - g_burnT0;
    if (!g_burnDoneMs && !g_burnWiped && ms > 25000) { g_burnWiped = true; g_burnOk = false; g_burnErr = L"it took too long"; }
    if (!g_burnDoneMs && g_burnWiped && !g_burnOk) {   // something could not be cleared: stay open and say so (nothing restarts)
        KillTimer(m_hWnd, 2071); if (g_burnWnd) { DestroyWindow(g_burnWnd); g_burnWnd = nullptr; } m_burning = false;
        MessageBoxW(m_hWnd, (L"Not everything could be cleared: " + g_burnErr + L"\n\nLibra stays open. Please try Burn again.").c_str(), L"Burn", MB_ICONWARNING);
        return true;
    }
    if (!g_burnDoneMs && g_burnWiped && ms >= kBurnMin) g_burnDoneMs = ms;
    RECT cr; GetClientRect(m_hWnd, &cr);
    if (g_burnDoneMs && ms - g_burnDoneMs >= kBurnEnd) {   // black screen: close private windows, start the fresh Libra after this one has quit, quit
        KillTimer(m_hWnd, 2071);
        EnumWindows(BurnClosePriv, 0);
        wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
        SetEnvironmentVariableW(L"LIBRA_TEST_OPEN", nullptr);   // (test hook must not fire again in the new process)
        std::wstring cmd = L"cmd.exe /c ping -n 4 127.0.0.1 >nul & start \"\" \"" + std::wstring(exe) + L"\"";
        STARTUPINFOW si{ sizeof(si) }; PROCESS_INFORMATION pi{};
        if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
        PostMessage(m_hWnd, WM_CLOSE, 0, 0);
        return true;
    }
    if (g_burnWnd) BurnDraw(g_burnWnd, ms, cr.right, cr.bottom, m_hWnd);
    return true;
}
