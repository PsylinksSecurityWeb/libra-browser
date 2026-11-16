#include "pch.h"
#include <random>
using std::min;
using std::max;
#include <gdiplus.h>
#include <wincrypt.h>
#include "DohProxy.h"
#include "Pages.h"
#include "PwPage.h"
#include "PrivPage.h"
#include "ReaderPage.h"
#include "PdfPage.h"
#include "AgentPage.h"
#include "PwSite.h"
#include "Downloader.h"
#include "WebView2EnvironmentOptions.h"
#include "BrowserWindow.h"
static bool CanUpgradeToHttps(const std::wstring& url);   // Fp.inl
static std::wstring StripTrackers(const std::wstring& url);   // Fp.inl
static int StatDay(int back);
static HRESULT CreateController(ICoreWebView2Environment* env, ICoreWebView2Environment10* e10, ICoreWebView2ControllerOptions* o, HWND h, ICoreWebView2CreateCoreWebView2ControllerCompletedHandler* cb) {   // private tabs MUST use the options overload (in-memory profile)
    if (e10 && o) return e10->CreateCoreWebView2ControllerWithOptions(h, o, cb);
    return env->CreateCoreWebView2Controller(h, cb);
}
static std::wstring RegDom(const std::wstring& host) { size_t d = host.find_last_of(L'.'); if (d == std::wstring::npos || d == 0) return host; size_t p = host.find_last_of(L'.', d - 1); return p == std::wstring::npos ? host : host.substr(p + 1); }
#include "Theme.h"
#include "resource.h"
struct HomeSugMsg { Tab* t; std::wstring q; std::vector<std::wstring> v; };

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "crypt32.lib")

// --- Anti-aliased drawing helpers (GDI+) --------------------------------------
static ULONG_PTR g_gdipToken = 0;

static Gdiplus::Color GC(COLORREF c, BYTE a = 255) {
    return Gdiplus::Color(a, GetRValue(c), GetGValue(c), GetBValue(c));
}

static void RoundPath(Gdiplus::GraphicsPath& p, float x, float y, float w, float h, float r) {
    r = std::min(r, std::min(w, h) / 2.f);
    float d = r * 2.f;
    p.AddArc(x,         y,         d, d, 180, 90);
    p.AddArc(x + w - d, y,         d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0,   90);
    p.AddArc(x,         y + h - d, d, d, 90,  90);
    p.CloseFigure();
}

// Filled (and optionally outlined) rounded rectangle; radius >= half height => pill.
static void FillRR(HDC hdc, const RECT& r, int radius, COLORREF fill, BYTE fillA = 255,
                   COLORREF border = CLR_INVALID, float borderW = 1.f, BYTE borderA = 255) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath path;
    float inset = (border != CLR_INVALID) ? borderW / 2.f : 0.f;
    RoundPath(path, r.left + inset, r.top + inset,
              (r.right - r.left) - 2 * inset - 1, (r.bottom - r.top) - 2 * inset - 1,
              (float)radius - inset);
    if (fillA) { Gdiplus::SolidBrush b(GC(fill, fillA)); g.FillPath(&b, &path); }
    if (border != CLR_INVALID) {
        Gdiplus::Pen pen(GC(border, borderA), borderW);
        g.DrawPath(&pen, &path);
    }
}

static void FillCircle(HDC hdc, int cx, int cy, int radius, COLORREF c, BYTE a = 255) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::SolidBrush b(GC(c, a));
    g.FillEllipse(&b, cx - radius, cy - radius, radius * 2, radius * 2);
}

static void DrawLineAA(HDC hdc, float x1, float y1, float x2, float y2, COLORREF c, float w) {
    Gdiplus::Graphics g(hdc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(GC(c), w);
    pen.SetStartCap(Gdiplus::LineCapRound);
    pen.SetEndCap(Gdiplus::LineCapRound);
    g.DrawLine(&pen, x1, y1, x2, y2);
}

static void PrivWipeCachesAtStart();   // Privacy.inl
static void PrivWipeSessionAtStart(bool keepCookies);   // Privacy.inl
struct MenuIt { const wchar_t* n; const wchar_t* g; const wchar_t* u; };
#include <filesystem>
#include "FlameIcon.h"
static const MenuIt kMenuIts[10] = { { L"History", L"\xE81C", L"libra://history" }, { L"Downloads", L"\xE896", L"libra://downloads" }, { L"Bookmarks", L"\xE734", L"libra://bookmarks" },
    { L"Passwords", L"\xE72E", L"libra://passwords" }, { L"Privacy", L"\xE7B3", L"libra://privacy" }, { L"Tabs", L"\xE7C4", L"libra://tabs" }, { L"Settings", L"\xE713", L"libra://settings" },
    { L"New private window", L"\xE7B3", L"libra://private" }, { L"Burn", L"", L"libra://burn" }, { L"Screenshot", L"ç22", L"libra://shot" } };
// Segoe Fluent Icons (Win11) with Segoe MDL2 Assets fallback (Win10).
static HFONT MakeIconFont(int px) {
    HFONT f = CreateFontW(-px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                          OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          DEFAULT_PITCH, L"Segoe Fluent Icons");
    HDC dc = GetDC(nullptr);
    HFONT old = (HFONT)SelectObject(dc, f);
    wchar_t face[LF_FACESIZE] = {};
    GetTextFaceW(dc, LF_FACESIZE, face);
    SelectObject(dc, old);
    ReleaseDC(nullptr, dc);
    if (wcscmp(face, L"Segoe Fluent Icons") != 0) {
        DeleteObject(f);
        f = CreateFontW(-px, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                        DEFAULT_PITCH, L"Segoe MDL2 Assets");
    }
    return f;
}

static void DrawGlyph(HDC hdc, const wchar_t* glyph, RECT rc, int px, COLORREF col) {
    HFONT f = MakeIconFont(px);
    HFONT old = (HFONT)SelectObject(hdc, f);
    SetTextColor(hdc, col);
    SetBkMode(hdc, TRANSPARENT);
    DrawTextW(hdc, glyph, -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    SelectObject(hdc, old);
    DeleteObject(f);
}

// ─── Omnibox sub-class proc (Enter key + ESC) ───────────────────────────────
static LRESULT CALLBACK OmniboxSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam,
                                             UINT_PTR, DWORD_PTR refData) {
    BrowserWindow* bw = reinterpret_cast<BrowserWindow*>(refData);
    if (msg == WM_KEYDOWN) {
        if ((wParam == VK_DOWN || wParam == VK_UP || wParam == VK_ESCAPE) && bw->SugKey((UINT)wParam)) return 0;
        if (wParam == VK_RETURN) { bw->OnOmniboxReturn(); return 0; }
        if (wParam == VK_ESCAPE) { SetFocus(bw->Hwnd()); return 0; }
    }
    return DefSubclassProc(hWnd, msg, wParam, lParam);
}

// ─── Constructor / Destructor ───────────────────────────────────────────────
// --- Visited sites (start-page shortcuts) ------------------------------------------
static std::wstring ImFound();   // defined in Import.inl
static std::wstring PdfFileUrl(const std::wstring& p) {   // file:///C:/dir/a%20b.pdf
    std::wstring u = L"file:///"; static const char* hx = "0123456789ABCDEF";
    for (unsigned char c : ToUtf8(p)) { if (c == '\\') u += L'/'; else if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || strchr("-_.~/:", c)) u += (wchar_t)c; else { u += L'%'; u += (wchar_t)hx[c >> 4]; u += (wchar_t)hx[c & 15]; } }
    return u;
}
static bool IsPdfPath(const std::wstring& p) { size_t n = p.size(); return n > 4 && _wcsicmp(p.c_str() + n - 4, L".pdf") == 0; }
std::wstring BrowserWindow::SitesPath() {
    wchar_t path[MAX_PATH] = {};
    GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
    return std::wstring(path) + L"\\LibraBrowser\\sites.txt";
}

static bool SafeHost(const std::wstring& h) {
    if (h.empty() || h.size() > 100) return false;
    for (wchar_t c : h)
        if (!((c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9') || c == L'.' || c == L'-')) return false;
    return true;
}
static bool SafeB64(const std::string& s) {
    if (s.size() > 40000) return false;
    for (char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '+' || c == '/' || c == '=')) return false;
    return true;
}

std::wstring BrowserWindow::HostOf(const std::wstring& url) {
    size_t s;
    if (url.rfind(L"https://", 0) == 0) s = 8; else if (url.rfind(L"http://", 0) == 0) s = 7; else return L"";
    size_t e = url.find_first_of(L"/?#:", s);
    std::wstring h = url.substr(s, e == std::wstring::npos ? std::wstring::npos : e - s);
    for (auto& c : h) c = (wchar_t)towlower(c);
    if (h == L"localhost" || !SafeHost(h)) return L"";
    return h;
}

static std::wstring TilesPath() { return DataDir() + L"\\tiles.txt"; }
static void TilesLoad(std::map<std::wstring, std::pair<std::wstring, std::wstring>>& m) {
    m.clear();
    for (auto& l : Lines(ReadAll(TilesPath()))) {
        size_t a = l.find(L'\t'), b = a == std::wstring::npos ? a : l.find(L'\t', a + 1);
        if (b == std::wstring::npos) continue;
        std::wstring h = l.substr(0, a), n = l.substr(a + 1, b - a - 1), u = l.substr(b + 1);
        if (SafeHost(h) && IsHttp(u) && n.size() <= 40 && m.size() < 40) m[h] = { n, u };
    }
}
static void TilesSave(const std::map<std::wstring, std::pair<std::wstring, std::wstring>>& m) {
    std::string out; for (auto& kv : m) out += ToUtf8(kv.first + L"\t" + kv.second.first + L"\t" + kv.second.second) + "\n";
    CreateDirectoryW(DataDir().c_str(), nullptr); WriteAll(TilesPath(), out);
}
void BrowserWindow::LoadSites() {
    TilesLoad(m_tileMeta);
    m_sites.clear();
    std::string data = ReadAll(SitesPath());   // encrypted file (old plain files still read)
    if (data.empty() || data.size() > (1u << 20)) return;
    std::istringstream in(data);
    std::string line;
    while (std::getline(in, line) && m_sites.size() < 5) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t tab = line.find('\t');
        std::wstring host = FromUtf8(line.substr(0, tab));
        std::string icon = tab == std::string::npos ? "" : line.substr(tab + 1);
        if (SafeHost(host) && SafeB64(icon)) m_sites.emplace_back(host, icon);
    }
}

void BrowserWindow::SaveSites() {
    std::wstring path = SitesPath();
    CreateDirectoryW(path.substr(0, path.rfind(L'\\')).c_str(), nullptr);
    std::string out;
    for (auto& s : m_sites) out += ToUtf8(s.first) + "\t" + s.second + "\n";
    WriteAll(path, out);
}

void BrowserWindow::RecordVisit(const std::wstring& host, const std::string& icon) {
    if (host.empty()) return;
    std::string keep = icon;
    auto it = std::find_if(m_sites.begin(), m_sites.end(), [&](auto& s) { return s.first == host; });
    if (it != m_sites.end()) {
        if (it == m_sites.begin() && (icon.empty() || icon == it->second)) return;   // nothing new
        if (keep.empty()) keep = it->second;
        m_sites.erase(it);
    }
    m_sites.insert(m_sites.begin(), { host, keep });
    if (m_sites.size() > 5) m_sites.resize(5);
    SaveSites();
}

// NavigateToString pages report themselves as data:text/html or about:blank
static bool IsInternalUri(const wchar_t* u) {
    return u && (wcsncmp(u, L"data:text/html", 14) == 0 || wcscmp(u, L"about:blank") == 0);
}

BrowserWindow::BrowserWindow(HINSTANCE hInst, bool incognito)
    : m_hInst(hInst), m_incognito(incognito) {
    m_adBlocker.LoadDefaults();   // loaded but unused unless the user switches "Local ad blocker" on
    LoadSites();
    LoadData();
}

BrowserWindow::~BrowserWindow() {
    if (m_fontUI)      DeleteObject(m_fontUI);
    if (m_fontSmall)   DeleteObject(m_fontSmall);
    if (m_brToolbar)   DeleteObject(m_brToolbar);
    if (m_brTabActive) DeleteObject(m_brTabActive);
    if (m_brTabInact)  DeleteObject(m_brTabInact);
    if (m_brSurface)   DeleteObject(m_brSurface);
    if (m_penBorder)   DeleteObject(m_penBorder);
    if (m_penAccent)   DeleteObject(m_penAccent);
    if (m_brOmni)      DeleteObject(m_brOmni);
}

// ─── Window Registration & Creation ─────────────────────────────────────────
void BrowserWindow::RegisterClass() {
    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = m_hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(Color::ToolbarBg);
    wc.lpszClassName = m_incognito ? L"LibraBrowserWndP" : L"LibraBrowserWnd";   // private windows never receive links meant for the normal window
    wc.hIcon         = LoadIcon(m_hInst, MAKEINTRESOURCE(IDI_LIBRA));
    wc.hIconSm       = LoadIcon(m_hInst, MAKEINTRESOURCE(IDI_LIBRA));
    RegisterClassExW(&wc);
}

bool BrowserWindow::Create(int nCmdShow) {
    if (!g_gdipToken) {
        Gdiplus::GdiplusStartupInput gin;
        Gdiplus::GdiplusStartup(&g_gdipToken, &gin, nullptr);
    }
    RegisterClass();

    // DPI-aware sizing
    HDC hdc = GetDC(nullptr);
    m_dpi   = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(nullptr, hdc);

    int cxScreen = GetSystemMetrics(SM_CXSCREEN);
    int cyScreen = GetSystemMetrics(SM_CYSCREEN);
    int w = MulDiv(1100, m_dpi, 96);
    int h = MulDiv(720,  m_dpi, 96);
    int x = (cxScreen - w) / 2;
    int y = (cyScreen - h) / 2;

    m_hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        m_incognito ? L"LibraBrowserWndP" : L"LibraBrowserWnd",
        m_incognito ? L"Libra - Incognito" : L"Libra Browser",
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        x, y, w, h,
        nullptr, nullptr, m_hInst, this
    );
    if (!m_hWnd) return false;

    // Enable DWM rounded corner on Win11
    DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_ROUND;
    DwmSetWindowAttribute(m_hWnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));

    // Dark title bar that blends into the tab strip (Win11; ignored on older builds)
    BOOL dark = m_incognito ? TRUE : FALSE;   // bright mode everywhere, except private windows
    DwmSetWindowAttribute(m_hWnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
    COLORREF cap = m_incognito ? Color::IncognitoChrome : Color::Surface;
    DwmSetWindowAttribute(m_hWnd, 35 /*DWMWA_CAPTION_COLOR*/, &cap, sizeof(cap));
    COLORREF bc = m_incognito ? Color::DarkBorder : Color::Border;
    DwmSetWindowAttribute(m_hWnd, 34 /*DWMWA_BORDER_COLOR*/, &bc, sizeof(bc));
    // Hide the caption icon + page title text (kept for taskbar / Alt-Tab only)
    WTA_OPTIONS wta = { WTNCA_NODRAWCAPTION | WTNCA_NODRAWICON, WTNCA_NODRAWCAPTION | WTNCA_NODRAWICON };
    SetWindowThemeAttribute(m_hWnd, WTA_NONCLIENT, &wta, sizeof(wta));

    CreateControls();
    m_set.Load();   // first run: no toolbar or tab strip may ever flash before the intro animation
    if (!m_incognito && !m_set.On(L"welcomed")) { m_setup = true; for (HWND w : { m_hOmnibox, m_hBtnBack, m_hBtnFwd, m_hBtnRefresh, m_hBtnShield, m_hBtnAgent, m_hBtnShot, m_hBtnMenu }) ShowWindow(w, SW_HIDE); }
    ShowWindow(m_hWnd, nCmdShow);
    UpdateWindow(m_hWnd);

    // Create WebView2 environment (async – tab opens once env is ready)
    std::wstring userDataFolder = [this]() -> std::wstring {
        wchar_t path[MAX_PATH];
        GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH);
        if (m_incognito) {   // private window: its own throw-away profile + DNS cache in TEMP, removed when it closes (and swept at the next private start)
            std::wstring tmp = std::wstring(path) + L"\\LibraBrowser\\Private-" + std::to_wstring(GetCurrentProcessId()); std::error_code ec; std::filesystem::remove_all(tmp, ec);
            std::filesystem::create_directories(tmp, ec); m_privDir = tmp; return tmp + L"\\UserData";
        }
        return std::wstring(path) + L"\\LibraBrowser\\UserData";
    }();

    // All traffic goes through the local DoH proxy (names resolved ONLY by dns.psylinks-beta.com).
    // localhost / LAN addresses bypass it, so local servers are never blocked or slowed.
    DohProxy::Get().onLetThrough = [this](const std::string& h) {
        for (char c : h) if (!isalnum((unsigned char)c) && c != '.' && c != '-' && c != '_') return;
        PostMessage(m_hWnd, WM_APP + 5, 0, (LPARAM) new std::string(h));
    };
    DohProxy::Get().onBlockedHit = [this](const std::string& h) {   // (proxy thread) -> UI thread
        for (char c : h) if (!isalnum((unsigned char)c) && c != '.' && c != '-' && c != '_') return;
        PostMessage(m_hWnd, WM_APP + 4, 0, (LPARAM) new std::string(h));
    };
    DohProxy::Get().onBlocked = [this](const std::string& h) {   // tell open pages to drop that host's frames at once
        for (char c : h) if (!isalnum((unsigned char)c) && c != '.' && c != '-') return;
        PostMessage(m_hWnd, WM_APP + 1, 0, (LPARAM) new std::string(h));
    };
    int proxyPort = DohProxy::Get().Start(L"dns.psylinks-beta.com", L"/dns-query", (m_incognito && !m_privDir.empty() ? m_privDir : DataDir()) + L"\\dns.cache");
    if (proxyPort) {   // warm the cache for sites you actually use
        std::vector<std::string> warm; std::set<std::string> seen;
        {   // the search engine you chose, plus the hosts its result page always pulls in
            std::wstring eh = HostOf(EngineUrl()); std::string e8 = ToUtf8(eh);
            if (!e8.empty()) { warm.push_back(e8); seen.insert(e8); }
            if (e8.find("google.") != std::string::npos)
                for (const char* h : { "www.gstatic.com", "ssl.gstatic.com", "apis.google.com", "fonts.gstatic.com", "lh3.googleusercontent.com" })
                    if (seen.insert(h).second) warm.push_back(h);
        }
        auto add = [&](const std::wstring& u) { std::wstring h = HostOf(u); std::string a = ToUtf8(h); if (!a.empty() && warm.size() < 50 && seen.insert(a).second) warm.push_back(a); };
        for (int i = (int)m_hist.size() - 1; i >= 0 && warm.size() < 40; --i) add(m_hist[i].url);
        for (auto& b : m_bm) add(b.url);
        DohProxy::Get().Prewarm(warm);
    }
    if (!proxyPort) {
        MessageBoxW(m_hWnd, L"Could not start the DNS-over-HTTPS proxy. Browsing is disabled.", L"Libra", MB_ICONERROR);
        return false;
    }
    auto envOpts = Microsoft::WRL::Make<CoreWebView2EnvironmentOptions>();
    std::wstring args = L"--proxy-server=http://127.0.0.1:" + std::to_wstring(proxyPort) +
                        L" --proxy-bypass-list=localhost;127.*;[::1];10.*;192.168.*;<local>"
                        L" --no-pings --disable-component-update --disable-domain-reliability"
                        L" --disable-breakpad --disable-background-networking --disable-sync --disable-client-side-phishing-detection"
                        L" --disable-quic --disable-field-trial-config --autoplay-policy=document-user-activation-required"
                        L" --force-webrtc-ip-handling-policy=default_public_interface_only --accept-lang=en-US"
                        + std::wstring(m_set.Get(L"block_3p", L"0") == L"1" ? L" --test-third-party-cookie-phaseout" : L"") +
                        L" --disable-features=Translate,OptimizationHints,MediaRouter,AutofillServerCommunication,DialMediaRouteProvider,BrowsingTopics,PrivacySandboxAdsAPIs,AttributionReportingCrossAppWeb,InterestFeedContentSuggestions";
    {   // older / small PCs: fewer processes, smaller caches
        MEMORYSTATUSEX ms{ sizeof(ms) }; GlobalMemoryStatusEx(&ms); SYSTEM_INFO si; GetSystemInfo(&si);
        std::wstring pm = m_set.Get(L"perf", L"auto");
        m_lowEnd = pm == L"light" || (pm == L"auto" && (ms.ullTotalPhys <= (4ull << 30) || si.dwNumberOfProcessors <= 2));
        if (m_lowEnd) args += L" --enable-low-end-device-mode";   // (a shared-renderer limit stopped sleeping tabs from freeing memory)
    }
    envOpts->put_AdditionalBrowserArguments(args.c_str());

    if (!m_incognito && m_set.On(L"clear_start")) PrivWipeSessionAtStart(!m_set.Get(L"keep_signin", L"").empty());   // fresh start: cookies, site data, caches, Alt-Svc/HSTS/favicon state (engine not running yet)
    PrivWipeCachesAtStart();   // caches the owner asked to clear (GPU, shader, code) are removed before the engine opens them
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, userDataFolder.c_str(), envOpts.Get(),
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT hr, ICoreWebView2Environment* env) -> HRESULT {
                if (SUCCEEDED(hr)) {
                    m_env = env;
                    m_envReady = true;
                    StartupTabs();
                }
                return S_OK;
            }).Get()
    );

    SetTimer(m_hWnd, TIMER_PROGRESS, 16, nullptr); // ~60fps progress anim
    return true;
}

// ─── Controls: Omnibox + Icon Buttons ───────────────────────────────────────
void BrowserWindow::CreateControls() {
    // Create GDI resources
    m_fontUI = CreateFontW(-MulDiv(13, m_dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                           CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    m_fontSmall = CreateFontW(-MulDiv(11, m_dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");

    m_brToolbar   = CreateSolidBrush(m_incognito ? Color::IncognitoTabBg : Color::ToolbarBg);
    m_brTabActive = CreateSolidBrush(m_incognito ? Color::DarkTabActive   : Color::TabActive);
    m_brTabInact  = CreateSolidBrush(m_incognito ? Color::IncognitoTabBg  : Color::TabInactive);
    m_brSurface   = CreateSolidBrush(m_incognito ? Color::DarkSurface     : Color::Surface);
    m_penBorder   = CreatePen(PS_SOLID, 1, Color::Border);
    m_penAccent   = CreatePen(PS_SOLID, 2, Color::Accent);
    m_brOmni      = CreateSolidBrush(m_incognito ? Color::DarkOmnibox : Color::OmniboxBg);

    int btnSz = MulDiv(Layout::ButtonSize,     m_dpi, 96);
    RECT cr; GetClientRect(m_hWnd, &cr);
    int W = cr.right;

    // Omnibox (single-line edit, sits inside a painted pill; positioned in LayoutToolbar)
    m_hOmnibox = CreateWindowExW(
        0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_LEFT,
        0, 0, 100, 20,
        m_hWnd, (HMENU)IDC_OMNIBOX, m_hInst, nullptr
    );
    SendMessage(m_hOmnibox, WM_SETFONT, (WPARAM)m_fontUI, TRUE); SendMessageW(m_hOmnibox, EM_SETCUEBANNER, TRUE, (LPARAM)L"Search or type a URL");
    // Add WS_EX_CLIENTEDGE equivalent via owner-draw: handled in WM_CTLCOLOREDIT
    SetWindowSubclass(m_hOmnibox, OmniboxSubclassProc, 0, (DWORD_PTR)this);

    // Nav buttons – simple BUTTON controls; we owner-draw them in WM_DRAWITEM
    auto makeBtn = [&](HWND& out, int x, int y, int id) {
        out = CreateWindowExW(0, L"BUTTON", L"",
            WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
            x, y, btnSz, btnSz,
            m_hWnd, (HMENU)(UINT_PTR)id, m_hInst, nullptr);
    };
    makeBtn(m_hBtnBack,    0, 0, IDC_BTN_BACK);
    makeBtn(m_hBtnFwd,     0, 0, IDC_BTN_FORWARD);
    makeBtn(m_hBtnRefresh, 0, 0, IDC_BTN_REFRESH);
    makeBtn(m_hBtnMenu,    0, 0, IDC_BTN_MENU);
    makeBtn(m_hBtnShield,  0, 0, IDC_BTN_SHIELD);
    makeBtn(m_hBtnAgent,   0, 0, IDC_BTN_AGENT);
    makeBtn(m_hBtnShot,    0, 0, IDC_BTN_SHOT);
    LayoutToolbar(W);
}

// Rounded address-bar pill, in client coordinates.
RECT BrowserWindow::OmniPillRect() const {
    RECT cr; GetClientRect(m_hWnd, &cr);
    int tabH  = MulDiv(Layout::TabBarHeight,   m_dpi, 96);
    int toolH = MulDiv(Layout::ToolbarHeight,  m_dpi, 96);
    int btnSz = MulDiv(Layout::ButtonSize,     m_dpi, 96);
    int pad   = MulDiv(Layout::ToolbarPadding, m_dpi, 96);
    int omniH = MulDiv(Layout::OmniboxHeight,  m_dpi, 96);
    int gap   = MulDiv(2, m_dpi, 96);
    int left  = pad + (btnSz + gap) * 3 + MulDiv(8, m_dpi, 96);
    int right = cr.right - pad - (AgBtnOn() ? btnSz * 4 + gap * 3 : btnSz * 3 + gap * 2) - MulDiv(10, m_dpi, 96);
    int top   = tabH + (toolH - omniH) / 2;
    return { left, top, std::max(left + 50, (int)right), top + omniH };
}

void BrowserWindow::LayoutToolbar(int cx) {
    int tabH  = MulDiv(Layout::TabBarHeight,   m_dpi, 96);
    int toolH = MulDiv(Layout::ToolbarHeight,  m_dpi, 96);
    int btnSz = MulDiv(Layout::ButtonSize,     m_dpi, 96);
    int pad   = MulDiv(Layout::ToolbarPadding, m_dpi, 96);
    int gap   = MulDiv(2, m_dpi, 96);
    int btnY  = tabH + (toolH - btnSz) / 2;
    SetWindowPos(m_hBtnBack,    nullptr, pad,                    btnY, btnSz, btnSz, SWP_NOZORDER);
    SetWindowPos(m_hBtnFwd,     nullptr, pad + (btnSz + gap),    btnY, btnSz, btnSz, SWP_NOZORDER);
    SetWindowPos(m_hBtnRefresh, nullptr, pad + (btnSz + gap) * 2, btnY, btnSz, btnSz, SWP_NOZORDER);
    SetWindowPos(m_hBtnMenu,    nullptr, cx - pad - btnSz,       btnY, btnSz, btnSz, SWP_NOZORDER);
    SetWindowPos(m_hBtnShield,  nullptr, cx - pad - btnSz * 2 - gap, btnY, btnSz, btnSz, SWP_NOZORDER);
    SetWindowPos(m_hBtnAgent,   nullptr, cx - pad - btnSz * 3 - gap * 2, btnY, btnSz, btnSz, SWP_NOZORDER);
    if (!AgBtnOn()) ShowWindow(m_hBtnAgent, SW_HIDE);
    { int nr = AgBtnOn() ? 3 : 2; SetWindowPos(m_hBtnShot, nullptr, cx - pad - btnSz * (nr + 1) - gap * nr, btnY, btnSz, btnSz, SWP_NOZORDER); }   // camera sits left of the assistant button

    RECT pill = OmniPillRect();
    int editH = MulDiv(20, m_dpi, 96);
    int inL   = MulDiv(42, m_dpi, 96);   // room for the search glyph
    int inR   = MulDiv(44 + (m_set.On(L"save_video") ? 30 : 0), m_dpi, 96);   // room for the star (and the quiet save-video icon)
    SetWindowPos(m_hOmnibox, nullptr, pill.left + inL,
                 pill.top + ((pill.bottom - pill.top) - editH) / 2,
                 std::max(20, (int)(pill.right - pill.left - inL - inR)), editH, SWP_NOZORDER);
}

// Owner-draw: round hover/press plate + vector icon glyph.
void BrowserWindow::DrawNavButton(const DRAWITEMSTRUCT* dis) {
    HDC hdc = dis->hDC;
    RECT rc = dis->rcItem;
    HBRUSH bg = CreateSolidBrush(m_incognito ? Color::IncognitoTabBg : Color::ToolbarBg);
    FillRect(hdc, &rc, bg);
    DeleteObject(bg);

    bool pressed = (dis->itemState & ODS_SELECTED) != 0;
    bool hover   = (dis->hwndItem == m_hoverBtn);
    int  cx = (rc.left + rc.right) / 2, cy = (rc.top + rc.bottom) / 2;
    int  rad = std::min(rc.right - rc.left, rc.bottom - rc.top) / 2 - MulDiv(2, m_dpi, 96);
    if (pressed)      FillCircle(hdc, cx, cy, rad, Color::ButtonPress);
    else if (hover)   FillCircle(hdc, cx, cy, rad, Color::ButtonHover);
    if (dis->CtlID == IDC_BTN_SHIELD) { DrawShieldIcon(hdc, rc, hover || pressed); return; }
    if (dis->CtlID == IDC_BTN_AGENT) { DrawAgentIcon(hdc, rc, AgDockIdx() >= 0); return; }
    if (dis->CtlID == IDC_BTN_SHOT) { DrawShotIcon(hdc, rc); return; }

    const wchar_t* glyph = L"";
    switch (dis->CtlID) {
    case IDC_BTN_BACK:    glyph = L""; break;
    case IDC_BTN_FORWARD: glyph = L""; break;
    case IDC_BTN_MENU:    glyph = L""; break;
    case IDC_BTN_REFRESH: {
        bool loading = m_activeTab >= 0 && m_activeTab < (int)m_tabs.size() && m_tabs[m_activeTab]->loading;
        glyph = loading ? L"" : L"";
        break;
    }
    }
    COLORREF col = m_incognito ? Color::IncognitoText : ((hover || pressed) ? Color::TextPrimary : Color::TextSecondary);
    DrawGlyph(hdc, glyph, rc, MulDiv(16, m_dpi, 96), col);
}

// ─── WebView2 Tab Initialization ─────────────────────────────────────────────
void BrowserWindow::InitWebView2(int tabIndex) {
    if (!m_envReady || tabIndex < 0 || tabIndex >= (int)m_tabs.size()) return;
    Tab* tab = m_tabs[tabIndex].get();

    // Create controller options for in-private mode
    wil::com_ptr<ICoreWebView2ControllerOptions> opts;
    wil::com_ptr<ICoreWebView2Environment10>     env10;
    bool usePrivate = tab->incognito || m_incognito;

    if (SUCCEEDED(m_env->QueryInterface(IID_PPV_ARGS(&env10)))) {
        env10->CreateCoreWebView2ControllerOptions(&opts);
        if (opts && usePrivate) {
            wil::com_ptr<ICoreWebView2ControllerOptions> opts2;
            if (SUCCEEDED(opts->QueryInterface(IID_PPV_ARGS(&opts2))))
                opts2->put_IsInPrivateModeEnabled(TRUE);
        }
    }

    RECT wvRect = WebViewRect();
    // Hide if not active tab
    bool isActive = (tabIndex == m_activeTab);

    CreateController(m_env.get(), env10.get(), opts.get(),
        m_hWnd,
        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this, tab, wvRect](HRESULT hr, ICoreWebView2Controller* ctrl) -> HRESULT {
                if (FAILED(hr)) return hr;
                int tabIndex = IndexOf(tab);
                if (tabIndex < 0) { if (ctrl) ctrl->Close(); return hr; }   // tab was closed while the engine started
                Tab* t = tab;
                t->controller = ctrl;
                ctrl->get_CoreWebView2(&t->webView);
                {
                    wil::com_ptr<ICoreWebView2Controller2> c2;
                    if (SUCCEEDED(ctrl->QueryInterface(IID_PPV_ARGS(&c2))))
                    {
                        BYTE v = (t->incognito || m_incognito) ? 242 : 255;
                        c2->put_DefaultBackgroundColor({ 255, v, v, v });
                    }
                }

                {   // pages follow "light" even if Windows is in dark mode
                    wil::com_ptr<ICoreWebView2_13> w13;
                    wil::com_ptr<ICoreWebView2Profile> prof;
                    if (SUCCEEDED(t->webView->QueryInterface(IID_PPV_ARGS(&w13))) && SUCCEEDED(w13->get_Profile(&prof)))
                        {
                        prof->put_PreferredColorScheme(COREWEBVIEW2_PREFERRED_COLOR_SCHEME_LIGHT);
                        wil::com_ptr<ICoreWebView2Profile2> p2;
                        std::wstring keepS = m_set.Get(L"keep_signin", L"");
                        if (m_set.On(L"clear_start") && !m_cleared && !m_incognito && !keepS.empty()) {   // the files were wiped before the engine started, except cookies: delete every cookie of a site not on the "stay signed in" list
                            m_cleared = true; wil::com_ptr<ICoreWebView2> wvk = t->webView;
                            std::set<std::wstring> keep; { size_t i = 0; while (i <= keepS.size()) { size_t e = keepS.find_first_of(L", ;", i); if (e == std::wstring::npos) e = keepS.size(); std::wstring k = keepS.substr(i, e - i); for (auto& c : k) c = (wchar_t)towlower(c); while (!k.empty() && k[0] == L'.') k.erase(0, 1); if (!k.empty()) keep.insert(RegDom(k)); i = e + 1; } }
                            // CDP (not the CookieManager): it also lists partitioned third-party cookies
                            auto cdpSweep = [wvk, keep]() { wvk->CallDevToolsProtocolMethod(L"Network.getAllCookies", L"{}", Microsoft::WRL::Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>([wvk, keep](HRESULT, LPCWSTR json) -> HRESULT {
                                std::wstring s = json ? json : L""; const std::wstring mark = L"{\"domain\":\"";   // the engine lists keys alphabetically
                                { wchar_t tf[MAX_PATH]; if (TestEnv(L"LIBRA_TEST_CKLOG", tf, MAX_PATH)) WriteAllRaw(tf, "getAll len=" + std::to_string(s.size()) + " head=" + ToUtf8(s.substr(0, 200)) + "\n", true); }
                                auto fld = [](const std::wstring& c, const wchar_t* key) { std::wstring k = L"\"" + std::wstring(key) + L"\":\"", o; size_t p = c.find(k); if (p == std::wstring::npos) return o; p += k.size(); while (p < c.size() && c[p] != L'"') { if (c[p] == L'\\' && p + 1 < c.size()) ++p; o += c[p++]; } return o; };
                                auto jesc = [](const std::wstring& v) { std::wstring o; for (wchar_t ch : v) { if (ch == L'"' || ch == L'\\') o += L'\\'; o += ch; } return o; };
                                for (size_t p = s.find(mark); p != std::wstring::npos;) {
                                    size_t nx = s.find(mark, p + 1); std::wstring c = s.substr(p, nx == std::wstring::npos ? std::wstring::npos : nx - p); p = nx;
                                    std::wstring name = fld(c, L"name"), dom = fld(c, L"domain"), path = fld(c, L"path"), d2 = dom; for (auto& ch : d2) ch = (wchar_t)towlower(ch); while (!d2.empty() && d2[0] == L'.') d2.erase(0, 1);
                                    if (keep.count(RegDom(d2))) continue;
                                    std::wstring pk; size_t q = c.find(L"\"partitionKey\":"); if (q != std::wstring::npos) { q += 15; if (q < c.size() && c[q] == L'{') { size_t e = c.find(L'}', q); if (e != std::wstring::npos) pk = c.substr(q, e - q + 1); } else if (q < c.size() && c[q] == L'"') { size_t e = c.find(L'"', q + 1); if (e != std::wstring::npos) pk = c.substr(q, e - q + 1); } }
                                    std::wstring a = L"{\"name\":\"" + jesc(name) + L"\",\"domain\":\"" + jesc(dom) + L"\",\"path\":\"" + jesc(path) + L"\"" + (pk.empty() ? L"" : L",\"partitionKey\":" + pk) + L"}";
                                    wvk->CallDevToolsProtocolMethod(L"Network.deleteCookies", a.c_str(), Microsoft::WRL::Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>([a](HRESULT hr, LPCWSTR r) -> HRESULT { wchar_t tf[MAX_PATH]; if (TestEnv(L"LIBRA_TEST_CKLOG", tf, MAX_PATH)) WriteAllRaw(tf, ToUtf8(a + L" -> " + std::to_wstring((unsigned)hr) + L" " + (r ? r : L"")) + "\n", true); return S_OK; }).Get());
                                }
                                return S_OK; }).Get()); };
                            wil::com_ptr<ICoreWebView2_2> w2; wil::com_ptr<ICoreWebView2CookieManager> cm;
                            if (SUCCEEDED(wvk->QueryInterface(IID_PPV_ARGS(&w2))) && SUCCEEDED(w2->get_CookieManager(&cm)))
                                cm->GetCookies(L"", Microsoft::WRL::Callback<ICoreWebView2GetCookiesCompletedHandler>([cm, keep, cdpSweep](HRESULT, ICoreWebView2CookieList* l) -> HRESULT {
                                    UINT n = 0; if (l) l->get_Count(&n);
                                    for (UINT i = 0; i < n; ++i) { wil::com_ptr<ICoreWebView2Cookie> c; if (FAILED(l->GetValueAtIndex(i, &c))) continue; wil::unique_cotaskmem_string d; c->get_Domain(&d); std::wstring dom = d ? d.get() : L""; for (auto& ch : dom) ch = (wchar_t)towlower(ch); while (!dom.empty() && dom[0] == L'.') dom.erase(0, 1); if (!keep.count(RegDom(dom))) cm->DeleteCookie(c.get()); }
                                    cdpSweep();   // then the partitioned third-party cookies the manager cannot see
                                    return S_OK; }).Get());
                            else cdpSweep();
                        }
                    }
                }
                // ─ Settings ─────────────────────────────────────────────
                wil::com_ptr<ICoreWebView2Settings> settings;
                t->webView->get_Settings(&settings);
                settings->put_IsStatusBarEnabled(FALSE);
                settings->put_AreHostObjectsAllowed(FALSE);          // pages never get native objects
                { wil::com_ptr<ICoreWebView2Settings8> s8;
                  if (SUCCEEDED(settings->QueryInterface(IID_PPV_ARGS(&s8)))) s8->put_IsReputationCheckingRequired(m_set.On(L"smartscreen_off") ? FALSE : TRUE); }
                settings->put_AreDefaultContextMenusEnabled(TRUE);
                settings->put_IsZoomControlEnabled(TRUE);
                settings->put_AreDevToolsEnabled(m_child ? FALSE : TRUE);   // Child Safety: no developer tools

                // ─ Bounds ───────────────────────────────────────────────
                ctrl->put_Bounds(wvRect);
                ctrl->put_IsVisible(tabIndex == m_activeTab ? TRUE : FALSE);
                if (m_agDock) PostMessage(m_hWnd, WM_APP + 32, 0, 0);   // agent sidebar: lay out once the view exists

                if (!m_set.On(L"welcomed")) {   // first-run hero: files in the data folder, served from a virtual host (no network)
                    std::wstring hd = DataDir() + L"\\hero"; CreateDirectoryW(hd.c_str(), nullptr);
                    auto put = [&](const wchar_t* n, const unsigned char* p, size_t len) { std::wstring f = hd + L"\\" + n; HANDLE h = CreateFileW(f.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr); if (h != INVALID_HANDLE_VALUE) { DWORD w; WriteFile(h, p, (DWORD)len, &w, nullptr); CloseHandle(h); } };
                    put(L"op.mp4", kHeroWebm, sizeof kHeroWebm); put(L"text.png", kHeroText, sizeof kHeroText); put(L"op2.mp4", kHero2Webm, sizeof kHero2Webm); put(L"op3.mp4", kHero3Webm, sizeof kHero3Webm);
                    wil::com_ptr<ICoreWebView2_3> w3; if (SUCCEEDED(t->webView->QueryInterface(IID_PPV_ARGS(&w3)))) w3->SetVirtualHostNameToFolderMapping(L"libra-hero.local", hd.c_str(), COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_ALLOW);
                }
                if (t->url == L"libra://pdfsign") PdfMapTools(t);   // the signer page (and only it) can load the local PDF tools
                // ─ Ad Blocker: intercept all network requests ────────────
                {   // bait scripts that only exist to show ads (ads.js, pagead.js...) are matched by file name: only those requests reach the handler below
                static const wchar_t* bait[] = { L"*/ads.js*", L"*/pagead.js*", L"*/adsbygoogle.js*", L"*/show_ads.js*", L"*/show_ads_impl.js*", L"*/adsense.js*" };
                if (m_adBlock || m_imagesOff) t->webView->AddWebResourceRequestedFilter(L"*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);   // otherwise every request would pay a UI-thread round trip for nothing
                else for (auto b : bait) t->webView->AddWebResourceRequestedFilter(b, COREWEBVIEW2_WEB_RESOURCE_CONTEXT_SCRIPT);
                for (auto& f : AdRuleFilters()) t->webView->AddWebResourceRequestedFilter(f.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);   // path rules (AdRules.h): only matching addresses reach the handler
                t->webView->add_WebResourceRequested(
                    Microsoft::WRL::Callback<ICoreWebView2WebResourceRequestedEventHandler>(
                        [this](ICoreWebView2* wv, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                            wil::com_ptr<ICoreWebView2WebResourceRequest> req;
                            args->get_Request(&req);
                            wil::unique_cotaskmem_string uri;
                            req->get_Uri(&uri);

                            COREWEBVIEW2_WEB_RESOURCE_CONTEXT ctx;
                            args->get_ResourceContext(&ctx);

                            if (ctx == COREWEBVIEW2_WEB_RESOURCE_CONTEXT_SCRIPT && uri.get()) {   // bait script by file name: blocked in Max only
                                std::wstring u = uri.get(); size_t q = u.find_first_of(L"?#"); if (q != std::wstring::npos) u.resize(q);
                                size_t sl = u.find_last_of(L'/'); std::wstring fn = sl == std::wstring::npos ? u : u.substr(sl + 1);
                                for (auto& ch : fn) ch = (wchar_t)towlower(ch);
                                static const wchar_t* names[] = { L"ads.js", L"pagead.js", L"adsbygoogle.js", L"show_ads.js", L"show_ads_impl.js", L"adsense.js" };
                                bool hit = false; for (auto n : names) if (fn == n) hit = true;
                                wil::unique_cotaskmem_string src; wv->get_Source(&src);
                                if (hit && src.get() && IsHttp(src.get()) && ModeFor(HostOf(src.get())) == 0) {
                                    wil::com_ptr<ICoreWebView2WebResourceResponse> resp;
                                    m_env->CreateWebResourceResponse(nullptr, 404, L"Blocked", L"", &resp);
                                    args->put_Response(resp.get());
                                    return S_OK;
                                }
                            }
                            if (uri.get() && (ctx == COREWEBVIEW2_WEB_RESOURCE_CONTEXT_IMAGE || ctx == COREWEBVIEW2_WEB_RESOURCE_CONTEXT_MEDIA || ctx == COREWEBVIEW2_WEB_RESOURCE_CONTEXT_FETCH || ctx == COREWEBVIEW2_WEB_RESOURCE_CONTEXT_XML_HTTP_REQUEST || ctx == COREWEBVIEW2_WEB_RESOURCE_CONTEXT_OTHER)) {   // ads on the site's own server, refused by their address
                                std::wstring rh, rp; AdSplitUrl(uri.get(), rh, rp);
                                if (!rh.empty() && AdPathHit(rh, rp)) {
                                    wil::unique_cotaskmem_string src; wv->get_Source(&src);
                                    if (src.get() && IsHttp(src.get()) && ModeFor(HostOf(src.get())) == 0) {
                                        wil::com_ptr<ICoreWebView2WebResourceResponse> resp;
                                        m_env->CreateWebResourceResponse(nullptr, 404, L"Blocked", L"", &resp);
                                        args->put_Response(resp.get());
                                        OnBlockedHit(ToUtf8(rh));
                                        AdTestLog(uri.get());   // tests only: every refused address
                                        if (m_activeTab >= 0 && m_activeTab < (int)m_tabs.size() && IsHttp(m_tabs[m_activeTab]->url) && !DohProxy::AdLike(ToUtf8(rh))) ++m_tabs[m_activeTab]->blockedAds;   // counted as an ad, not a tracker
                                        return S_OK;
                                    }
                                }
                            }
                            // Image blocking
                            if (m_imagesOff && ctx == COREWEBVIEW2_WEB_RESOURCE_CONTEXT_IMAGE) {
                                wil::com_ptr<ICoreWebView2WebResourceResponse> resp;
                                m_env->CreateWebResourceResponse(nullptr, 200, L"OK", L"", &resp);
                                args->put_Response(resp.get());
                                return S_OK;
                            }

                            // Ad/tracker blocking
                            if (m_adBlock && m_adBlocker.ShouldBlock(uri.get())) {
                                wil::com_ptr<ICoreWebView2WebResourceResponse> resp;
                                m_env->CreateWebResourceResponse(nullptr, 403, L"Blocked", L"", &resp);
                                args->put_Response(resp.get());
                            }
                            return S_OK;
                        }).Get(), &t->tokResReq);
                }

                // ─ Navigation started ────────────────────────────────────
                t->webView->add_NavigationStarting(
                    Microsoft::WRL::Callback<ICoreWebView2NavigationStartingEventHandler>(
                        [this, t](ICoreWebView2* wv, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
                            int tabIndex = IndexOf(t); if (tabIndex < 0) return S_OK;
                            Tab* t2 = t;
                            { UINT64 nid = 0; args->get_NavigationId(&nid); t2->navId = nid; BOOL ui = TRUE; args->get_IsUserInitiated(&ui); BOOL rd = FALSE; args->get_IsRedirected(&rd); if (!rd) { t2->navUser = !!ui; t2->navTick = GetTickCount(); } }
                            t2->loading  = true;
                            t2->progress = 0.1f;
                            wil::unique_cotaskmem_string uri;
                            args->get_Uri(&uri);
                            if (uri.get()) {   // engine-internal and local-file pages (edge://settings, edge://flags, file:, view-source:, devtools:) are never opened as a page
                                std::wstring sch(uri.get()); sch = sch.substr(0, sch.find(L':'));
                                for (auto& ch : sch) ch = (wchar_t)towlower(ch);
                                static const wchar_t* bad[] = { L"file", L"ftp", L"edge", L"chrome", L"devtools", L"view-source", L"chrome-extension", L"chrome-untrusted", L"edge-extension", L"libra" };   // a website can never "navigate" a tab to one of our own pages
                                std::wstring lw(uri.get()); for (auto& ch : lw) ch = (wchar_t)towlower(ch);
                                if (sch == L"file" && m_pdfAllow.count(lw)) sch = L"";   // a downloaded PDF the user chose to read: the engine's own PDF viewer shows it
                                for (auto b : bad) if (sch == b) { args->put_Cancel(TRUE); t2->loading = false; return S_OK; }
                            }
                            if (uri.get() && m_set.Get(L"https_upgrade", L"1") != L"0" && CanUpgradeToHttps(uri.get())) {   // http -> https first
                                std::wstring host = HostOf(uri.get()); DWORD now = GetTickCount();
                                if (!m_httpOk.count(host) && !m_httpOk.count(RegDom(host))) {
                                    if (t2->upHost == host && now - t2->upTick < 4000) m_httpOk.insert(host);   // https sent us straight back to http: stop trying
                                    else { t2->upHost = host; t2->upTick = now; t2->upUrl = L"https://" + std::wstring(uri.get()).substr(7); args->put_Cancel(TRUE); PostMessage(m_hWnd, WM_APP + 7, 0, (LPARAM)t2); return S_OK; }
                                }
                            }
                            if (uri.get() && IsHttp(uri.get()) && m_set.Get(L"fingerprint", L"1") != L"0" && ModeFor(HostOf(uri.get())) != 2) {   // tracking parameters never leave the PC
                                std::wstring cl = StripTrackers(uri.get());
                                if (!cl.empty()) { t2->upUrl = cl; args->put_Cancel(TRUE); PostMessage(m_hWnd, WM_APP + 7, 0, (LPARAM)t2); return S_OK; }
                            }
                            if (uri.get() && IsHttp(uri.get()) && IsHttp(t2->url.c_str()) && RegDom(HostOf(uri.get())) != RegDom(HostOf(t2->url)) && ModeFor(HostOf(t2->url)) != 2 && m_set.Get(L"fingerprint", L"1") != L"0") wv->ExecuteScript(L"try{window.name=''}catch(e){}", nullptr);   // window.name must not follow you to another site
                            /* only real web addresses ever become the tab address: otherwise a page could pose as libra://agent and receive its data */ if (!IsInternalUri(uri.get()) && IsHttp(uri.get())) { if (HostOf(uri.get()) != HostOf(t2->url)) t2->fav.reset(); t2->prevUrl = t2->url; t2->dlNav = false; t2->url = uri.get(); }
                            if (!IsInternalUri(uri.get())) { t2->vids.clear(); t2->blockedReq = 0; t2->blockedAds = 0; t2->letThrough = 0; t2->blockedHosts.clear(); if (tabIndex == m_activeTab) DohProxy::Get().SetNonce(GetTickCount() * 2654435761u); }   // new page: counters restart, a new set of ads gets through
                            if (tabIndex == m_activeTab) {
                                UpdateOmniboxText(t2->url);
                                m_progressVisible = true;
                                InvalidateRect(m_hWnd, nullptr, FALSE);
                            }
                            return S_OK;
                        }).Get(), &t->tokNavStart);

                // ─ Navigation complete ───────────────────────────────────
                t->webView->add_NavigationCompleted(
                    Microsoft::WRL::Callback<ICoreWebView2NavigationCompletedEventHandler>(
                        [this, t](ICoreWebView2* wv, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                            int tabIndex = IndexOf(t); if (tabIndex < 0) return S_OK;
                            Tab* t2 = t;
                            UINT64 cid = 0; args->get_NavigationId(&cid);
                            if (cid != t2->navId) return S_OK;   // an older, superseded navigation
                            t2->loading  = false;
                            t2->progress = 1.0f;
                            if (!m_incognito && !m_setup && !m_set.On(L"first_block_shown")) SetTimer(m_hWnd, 2004, 2800, nullptr);   // maybe celebrate the first blocked ads
                            BOOL canBack = FALSE, canFwd = FALSE;
                            wv->get_CanGoBack(&canBack);
                            wv->get_CanGoForward(&canFwd);
                            t2->canBack = !!canBack;
                            t2->canFwd  = !!canFwd;
                            wil::unique_cotaskmem_string uri;
                            wv->get_Source(&uri);
                            BOOL navOk = FALSE; args->get_IsSuccess(&navOk);
                            bool internal = navOk && IsInternalUri(uri.get());   // a failed load never leaves an internal page
                            if (!internal && navOk) t2->url = uri.get();
                            if (!navOk && !internal && IsHttp(t2->url)) {
                                COREWEBVIEW2_WEB_ERROR_STATUS es; args->get_WebErrorStatus(&es);
                                int st = (int)es;
                                int hs = -1; { wil::com_ptr<ICoreWebView2NavigationCompletedEventArgs2> a2; if (SUCCEEDED(args->QueryInterface(IID_PPV_ARGS(&a2)))) a2->get_HttpStatusCode(&hs); }
                                if (es == COREWEBVIEW2_WEB_ERROR_STATUS_UNKNOWN && hs >= 400 && hs < 1000 && !DohProxy::Get().IsBlocked(ToUtf8(HostOf(t2->url))) && !DohProxy::Get().DnsFailed(ToUtf8(HostOf(t2->url)))) { t2->url = uri.get(); st = -1; }   // a real page with an HTTP error (Google's CAPTCHA is a 429): show it, it is not "unreachable"
                                if (st >= 0 && m_certBad.erase(HostOf(t2->url))) st = 2;                // we refused its certificate
                                if (st >= 0 && st != (int)COREWEBVIEW2_WEB_ERROR_STATUS_OPERATION_CANCELED)
                                    PostMessage(m_hWnd, WM_APP + 2, (WPARAM)st, (LPARAM)t2);   // show after this event returns
                            }
                            if (navOk && !internal && !(t2->incognito || m_incognito)) {
                                wil::unique_cotaskmem_string ttl; wv->get_DocumentTitle(&ttl);
                                RecordHistory(t2->url, ttl.get() ? ttl.get() : L"");
                            }
                            if (tabIndex == m_activeTab) {
                                UpdateOmniboxText(t2->url);
                                UpdateNavButtons();
                                InvalidateRect(m_hWnd, nullptr, FALSE);
                            }
                            return S_OK;
                        }).Get(), &t->tokNavComplete);

                // ─ Title change ──────────────────────────────────────────
                t->webView->add_DocumentTitleChanged(
                    Microsoft::WRL::Callback<ICoreWebView2DocumentTitleChangedEventHandler>(
                        [this, t](ICoreWebView2* wv, IUnknown*) -> HRESULT {
                            int tabIndex = IndexOf(t); if (tabIndex < 0) return S_OK;
                            Tab* t2 = t;
                            wil::unique_cotaskmem_string title;
                            wv->get_DocumentTitle(&title);
                            t2->title = title.get();
                            if (tabIndex == m_activeTab) {
                                std::wstring winTitle = t2->title + L" — Libra Browser";
                                SetWindowTextW(m_hWnd, winTitle.c_str());
                            }
                            InvalidateRect(m_hWnd, nullptr, FALSE);
                            if (t2->url.rfind(L"libra://", 0) != 0) RefreshTabsPage();   // (not for internal pages: that would loop)
                            return S_OK;
                        }).Get(), &t->tokDocTitleChange);

                // ─ New window (open in new tab) ──────────────────────────
                t->webView->add_NewWindowRequested(
                    Microsoft::WRL::Callback<ICoreWebView2NewWindowRequestedEventHandler>(
                        [this](ICoreWebView2* wv, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
                            args->put_Handled(TRUE);
                            BOOL userClick = FALSE; args->get_IsUserInitiated(&userClick);
                            if (!userClick) return S_OK;   // popup blocker: pages cannot open windows without a real click/tap
                            wil::unique_cotaskmem_string uri;
                            args->get_Uri(&uri);
                            if (!uri.get() || !IsHttp(uri.get())) return S_OK;   // a website may open websites only, never libra:// pages, file:, data: or javascript:
                            OpenTab(uri.get());
                            return S_OK;
                        }).Get(), &t->tokNewWindow);
                AgTrackFrames(t);

                {
                    EventRegistrationToken tkf;
                    t->webView->add_ContainsFullScreenElementChanged(
                        Microsoft::WRL::Callback<ICoreWebView2ContainsFullScreenElementChangedEventHandler>(
                            [this, t](ICoreWebView2* wv, IUnknown*) -> HRESULT {
                                BOOL f = FALSE; wv->get_ContainsFullScreenElement(&f);
                                if (IndexOf(t) >= 0) SetFullscreen(!!f);
                                return S_OK;
                            }).Get(), &tkf);
                }

                {
                    EventRegistrationToken tkp;
                    t->webView->add_PermissionRequested(
                        Microsoft::WRL::Callback<ICoreWebView2PermissionRequestedEventHandler>(
                            [this](ICoreWebView2*, ICoreWebView2PermissionRequestedEventArgs* a) -> HRESULT {
                                COREWEBVIEW2_PERMISSION_KIND k; a->get_PermissionKind(&k);
                                if (k == COREWEBVIEW2_PERMISSION_KIND_OTHER_SENSORS) {   // motion sensors: blocked, except on sites whose shield is Off (government, banking and tool sites need them)
                                    wil::unique_cotaskmem_string su; a->get_Uri(&su);
                                    bool off = su.get() && wcsncmp(su.get(), L"https://", 8) == 0 && ModeFor(HostOf(su.get())) == 2;
                                    a->put_State(off ? COREWEBVIEW2_PERMISSION_STATE_ALLOW : COREWEBVIEW2_PERMISSION_STATE_DENY);
                                    wil::com_ptr<ICoreWebView2PermissionRequestedEventArgs3> s3;
                                    if (off && SUCCEEDED(a->QueryInterface(IID_PPV_ARGS(&s3)))) s3->put_SavesInProfile(FALSE);
                                    return S_OK;
                                }
                                if (m_set.On(L"perm_allow")) {                                                                                     // opt-in only, and only on https sites
                                    wil::unique_cotaskmem_string u; a->get_Uri(&u);
                                    if (u.get() && wcsncmp(u.get(), L"https://", 8) == 0) {
                                        a->put_State(COREWEBVIEW2_PERMISSION_STATE_ALLOW);
                                        wil::com_ptr<ICoreWebView2PermissionRequestedEventArgs3> a3;
                                        if (SUCCEEDED(a->QueryInterface(IID_PPV_ARGS(&a3)))) a3->put_SavesInProfile(FALSE);   // never remembered
                                    }
                                }
                                return S_OK;                                                                                                          // (ask mode: WebView2's own prompt)
                            }).Get(), &tkp);
                }

                // ─ libra:// page messages, downloads, shortcuts ─────────────
                {
                    Tab* tp = t; EventRegistrationToken tk;
                    t->webView->add_WebMessageReceived(
                        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                            [this, tp](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* a) -> HRESULT {
                                int idx = -1;
                                for (int i = 0; i < (int)m_tabs.size(); ++i) if (m_tabs[i].get() == tp) idx = i;
                                if (idx < 0) return S_OK;
                                wil::unique_cotaskmem_string src; a->get_Source(&src);
                                wil::unique_cotaskmem_string msg;
                                if (FAILED(a->TryGetWebMessageAsString(&msg)) || !msg.get()) return S_OK;
                                if (src.get() && IsHttp(src.get())) {   // a real website: only the narrow pw.* channel, about its own origin
                                    std::wstring m = msg.get();
                                    if (m.size() < 3000 && m.rfind(L"pw.", 0) == 0) OnSitePwMessage(tp, src.get(), m);
                                    else if (m.size() < 3000 && m.rfind(L"sg.", 0) == 0) OnSiteSgMessage(tp, src.get(), m);
                                    return S_OK;
                                }
                                if (tp->url.rfind(L"libra://", 0) != 0) return S_OK;   // internal pages only
                                OnPageMessage(idx, msg.get());
                                return S_OK;
                            }).Get(), &tk);
                    ctrl->add_AcceleratorKeyPressed(
                        Microsoft::WRL::Callback<ICoreWebView2AcceleratorKeyPressedEventHandler>(
                            [this](ICoreWebView2Controller*, ICoreWebView2AcceleratorKeyPressedEventArgs* a) -> HRESULT {
                                COREWEBVIEW2_KEY_EVENT_KIND k; a->get_KeyEventKind(&k);
                                if (k != COREWEBVIEW2_KEY_EVENT_KIND_KEY_DOWN) return S_OK;
                                UINT vk = 0; a->get_VirtualKey(&vk);
                                if (HandleKey(vk)) a->put_Handled(TRUE);
                                return S_OK;
                            }).Get(), &tk);
                    AttachDownloads(t->webView.get());
                }

                {
                    wil::com_ptr<ICoreWebView2_14> w14; EventRegistrationToken tkc;
                    if (SUCCEEDED(t->webView->QueryInterface(IID_PPV_ARGS(&w14))))
                        w14->add_ServerCertificateErrorDetected(
                            Microsoft::WRL::Callback<ICoreWebView2ServerCertificateErrorDetectedEventHandler>(
                                [this](ICoreWebView2*, ICoreWebView2ServerCertificateErrorDetectedEventArgs* a) -> HRESULT {
                                    wil::unique_cotaskmem_string u; a->get_RequestUri(&u);
                                    std::wstring h = HostOf(u.get());
                                    bool allow = m_certAllow.count(h) > 0;
                                    if (!allow) m_certBad.insert(h);
                                    a->put_Action(allow ? COREWEBVIEW2_SERVER_CERTIFICATE_ERROR_ACTION_ALWAYS_ALLOW
                                                        : COREWEBVIEW2_SERVER_CERTIFICATE_ERROR_ACTION_CANCEL);
                                    return S_OK;
                                }).Get(), &tkc);
                }

                InstallPwScript(t); InstallSgScript(t); AttachVideoSniff(t); AttachContextSave(t);   // password generator card + save/fill triggers
                InstallAdScript(t);   // remove empty ad boxes (skipped on sites where blocking is off)

                // ─ Favicons for the start-page shortcuts (never for private tabs) ─
                {
                    wil::com_ptr<ICoreWebView2_15> wv15;
                    if (SUCCEEDED(t->webView->QueryInterface(IID_PPV_ARGS(&wv15)))) {
                        wv15->add_FaviconChanged(
                            Microsoft::WRL::Callback<ICoreWebView2FaviconChangedEventHandler>(
                                [this, t](ICoreWebView2* wv, IUnknown*) -> HRESULT {
                                    if (IndexOf(t) < 0) return S_OK;
                                    wil::unique_cotaskmem_string src;
                                    wv->get_Source(&src);
                                    std::wstring host = HostOf(src.get());
                                    if (host.empty()) return S_OK;
                                    wil::com_ptr<ICoreWebView2_15> w15;
                                    if (FAILED(wv->QueryInterface(IID_PPV_ARGS(&w15)))) return S_OK;
                                    w15->GetFavicon(COREWEBVIEW2_FAVICON_IMAGE_FORMAT_PNG,
                                        Microsoft::WRL::Callback<ICoreWebView2GetFaviconCompletedHandler>(
                                            [this, host, t](HRESULT hr, IStream* st) -> HRESULT {
                                                if (FAILED(hr) || !st) return S_OK;
                                                std::string bytes; char buf[4096]; ULONG n = 0;
                                                while (SUCCEEDED(st->Read(buf, sizeof(buf), &n)) && n > 0) {
                                                    bytes.append(buf, n);
                                                    if (bytes.size() > 24000) return S_OK;   // keep the list small
                                                }
                                                if (bytes.empty()) return S_OK;
                                                if (IndexOf(t) >= 0) {   // real site icon on the tab
                                                    if (IStream* ms = SHCreateMemStream((const BYTE*)bytes.data(), (UINT)bytes.size())) {
                                                        Gdiplus::Bitmap* bm = nullptr;
                                                        { Gdiplus::Bitmap tmp(ms); if (tmp.GetLastStatus() == Gdiplus::Ok) bm = tmp.Clone(0, 0, tmp.GetWidth(), tmp.GetHeight(), PixelFormat32bppARGB); }
                                                        ms->Release();
                                                        if (bm) { t->fav.reset(bm, [](void* p) { delete (Gdiplus::Bitmap*)p; }); InvalidateRect(m_hWnd, nullptr, FALSE); }
                                                    }
                                                }
                                                if (IndexOf(t) < 0 || t->incognito || m_incognito) return S_OK;   // private: never saved
                                                DWORD len = 0;
                                                CryptBinaryToStringA((const BYTE*)bytes.data(), (DWORD)bytes.size(),
                                                    CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &len);
                                                std::string b64(len, 0);
                                                if (!CryptBinaryToStringA((const BYTE*)bytes.data(), (DWORD)bytes.size(),
                                                        CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, b64.data(), &len)) return S_OK;
                                                b64.resize(len);
                                                return S_OK;
                                            }).Get());
                                    return S_OK;
                                }).Get(), &t->tokFavIcon);
                    }
                }

                // ─ Inject cosmetic ad hiding CSS (only when the local blocker is switched on) ─
                if (m_adBlock) {
                    t->webView->AddScriptToExecuteOnDocumentCreated(
                        LR"js(
(function(){
  var style = document.createElement('style');
  style.textContent =
    '.ad,.ads,.ad-unit,.adsbygoogle,.advert,.advertisement,'+
    '[class*="banner-ad"],[id*="google_ads"],[id*="div-gpt-ad"],'+
    'iframe[src*="doubleclick"],iframe[src*="googlesyndication"]'+
    '{display:none!important;visibility:hidden!important}';
  document.head && document.head.appendChild(style);
})();
)js", nullptr);
                }

                // ─ First navigation ─────────────────────────────────────
                if (t->url.empty()) t->url = L"libra://home";
                if (t->url.rfind(L"libra://", 0) == 0) t->webView->NavigateToString(PageHtml(t->url).c_str());
                else                                   t->webView->Navigate(t->url.c_str());

                return S_OK;
            }).Get()
    );
}

// ─── Homepage HTML ───────────────────────────────────────────────────────────
std::wstring BrowserWindow::GetHomepageHtml() {
    // Monochrome, flat, thin lines. Incognito inverts the variables.
    std::wstring html = LR"html(<!DOCTYPE html>
<html lang="en"><head><meta charset="UTF-8">
<meta http-equiv="Content-Security-Policy" content="default-src 'none'; img-src data:; style-src 'unsafe-inline'; script-src 'unsafe-inline'; form-action https:; base-uri 'none'">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>New Tab</title>@PRE@
<style>
  :root{@VARS@}
  *{margin:0;padding:0;box-sizing:border-box}
  html,body{height:100%}
  body{font-family:'Segoe UI Variable Text','Segoe UI',system-ui,sans-serif;background:var(--bg);color:var(--fg);
    display:flex;flex-direction:column;align-items:center;justify-content:center;user-select:none}
  .logo{font:800 60px/1 'Segoe UI Variable Display','Segoe UI',sans-serif;letter-spacing:-.02em;margin:0 0 34px;animation:up .8s cubic-bezier(.2,.7,.2,1) both}
  @keyframes up{from{opacity:0;transform:translate3d(0,18px,0)}to{opacity:1;transform:none}}
  .sw{position:relative;width:min(580px,88vw)}.sg{position:absolute;top:100%;left:0;right:0;margin-top:8px;background:#fff;border:1px solid #e6e6e6;border-radius:18px;padding:6px;box-shadow:none;z-index:5}.sg[hidden]{display:none}
  .sg div{padding:9px 16px;border-radius:12px;font-size:14px;cursor:pointer}.sg div:hover,.sg div.on{background:#f3f3f3}
  .tw{position:relative}.dots{position:absolute;top:2px;right:2px;z-index:2;width:28px;height:28px;border-radius:50%;border:0;background:transparent;color:#777;font-size:18px;line-height:1;cursor:pointer;opacity:0;transition:opacity .15s,background .15s}
  .tw:hover .dots,.dots:focus{opacity:1}.dots:hover{background:#e8e8e8}
  .tm{position:absolute;top:30px;right:0;z-index:7;background:#fff;border:1px solid #e6e6e6;border-radius:14px;padding:5px;min-width:160px;box-shadow:none}
  .tm button{display:block;width:100%;text-align:left;padding:9px 12px;border:0;background:none;border-radius:10px;font:inherit;font-size:13px;cursor:pointer;color:var(--fg)}.tm button:hover{background:#f3f3f3}.tm .del{color:#d6141c}
  .dlg{position:fixed;inset:0;background:rgba(255,255,255,.7);display:flex;align-items:center;justify-content:center;z-index:20}.dlg[hidden]{display:none}
  .dlg .box{width:min(380px,90vw);background:#fff;border:1px solid #e6e6e6;border-radius:20px;padding:24px;box-shadow:none}
  .dlg h3{font:800 20px 'Segoe UI Variable Display','Segoe UI',sans-serif;margin-bottom:14px}.dlg label{display:block;font-size:12px;color:var(--mute);margin:12px 0 5px}
  .dlg input{width:100%;border:1px solid #e6e6e6;border-radius:12px;padding:10px 12px;font:inherit;outline:none}.dlg input:focus{border-color:#111}
  .dlg .bt{display:flex;justify-content:flex-end;gap:10px;margin-top:20px}.dlg .bt button{border:0;border-radius:999px;padding:9px 20px;font:inherit;cursor:pointer;background:#efefef}.dlg .bt .pri{background:#111;color:#fff}
  .prot{margin-top:38px;font-size:13px;color:var(--mute)}.prot b{color:#d6141c;font-weight:700}
  .search{display:flex;align-items:center;gap:12px;width:min(580px,88vw);height:52px;padding:0 22px;background:#fff;border-radius:999px;
    border:1px solid #e6e6e6;transition:box-shadow .15s,border-color .15s}
  .search:focus-within{border-color:#111}
  .search svg{flex-shrink:0;color:var(--mute)}
  .search input{flex:1;border:none;outline:none;background:transparent;color:var(--fg);font:inherit;font-size:15px}
  .search input::placeholder{color:var(--mute)}
  .sites{display:flex;gap:16px;margin-top:46px;justify-content:center;flex-wrap:wrap}
  .site{width:100px;display:flex;flex-direction:column;align-items:center;gap:10px;padding:18px 6px 14px;text-decoration:none;color:var(--fg);
    background:transparent;border-radius:18px;transition:transform .15s,background .15s}
  .site:hover{transform:translateY(-3px);background:#f3f3f3}
  .ico{width:34px;height:34px;display:grid;place-items:center;font-size:14px;font-weight:600;background:#f0f0f0;border-radius:50%}
  .ico img{width:22px;height:22px;object-fit:contain}
  .site span{font-size:12px;max-width:88px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
  .tw.sp .site{border:1px dashed #d4d4d4}.spl{font-style:normal;font-size:10px;color:var(--mute);letter-spacing:.04em}.spx{position:absolute;top:2px;right:2px;z-index:2;width:24px;height:24px;border-radius:50%;border:0;background:transparent;color:#777;font-size:16px;cursor:pointer;opacity:0;transition:opacity .15s}.tw.sp:hover .spx{opacity:1}.spx:hover{background:#e8e8e8}
  .spbg{position:fixed;inset:0;z-index:-1;background-size:cover;background-position:center}.spbg::after{content:"";position:absolute;inset:0;background:rgba(255,255,255,.62)}
  .spill{position:fixed;left:18px;bottom:18px;display:flex;gap:10px;align-items:center;background:#fff;border:1px solid #e6e6e6;border-radius:99px;padding:6px 8px 6px 14px;font-size:12px}.spill a{color:#111;font-weight:600}.spill button{border:0;background:#f3f3f3;border-radius:99px;padding:4px 10px;font:inherit;cursor:pointer}
  .tip{display:flex;align-items:center;gap:6px;margin-top:20px;font-size:13px;color:var(--mute);animation:up .8s .6s cubic-bezier(.2,.7,.2,1) both}.tip button{border:0;background:none;color:#777;font-size:18px;line-height:1;cursor:pointer;border-radius:50%;width:24px;height:24px}.tip button:hover{background:#eee}
  .spoff{position:fixed;right:18px;bottom:18px;font-size:11px;color:var(--mute)}
</style></head>
<body>@SPBG@
<div class="logo">LIBRA<sup style="font:700 11px 'Segoe UI',sans-serif;letter-spacing:.1em;border:1px solid #111;border-radius:99px;padding:2px 8px;margin-left:10px;vertical-align:34px;display:inline-block">BETA</sup></div>
<div class="sw"><form class="search" action="https://www.google.com/search" method="GET" target="_self">
  <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round">
    <circle cx="11" cy="11" r="7"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>
  <input name="q" placeholder="Search or type a URL" autofocus autocomplete="off" spellcheck="false">
</form>
<div class="sg" id="sg" hidden></div></div>
<script>
(function(){const q=document.querySelector('input[name=q]'),sg=document.getElementById('sg');let t,sel=-1,typed='';
function show(a,v){if(q.value.trim()!==v)return;sg.textContent='';sel=-1;a.forEach(s=>{const d=document.createElement('div');d.textContent=s;d.onmousedown=e=>{e.preventDefault();q.value=s;q.form.submit()};sg.appendChild(d)});sg.hidden=!a.length}
window.__sug=show;
q.addEventListener('input',()=>{clearTimeout(t);typed=q.value;const v=q.value.trim();if(v.length<2){sg.hidden=true;return}t=setTimeout(()=>chrome.webview.postMessage('home.sug\t'+v),180)});
q.addEventListener('keydown',e=>{const k=[...sg.children];if(sg.hidden||!k.length)return;
 if(e.key==='ArrowDown'||e.key==='ArrowUp'){e.preventDefault();sel=(sel+(e.key==='ArrowDown'?1:-1)+k.length+1)%(k.length+1);k.forEach((x,i)=>x.classList.toggle('on',i===sel));q.value=sel<k.length&&sel>=0?k[sel].textContent:typed}
 else if(e.key==='Escape')sg.hidden=true});
q.addEventListener('blur',()=>setTimeout(()=>sg.hidden=true,120))})();
</script>
@SITES@
</body></html>)html";

    {   // search form uses the chosen engine
        std::wstring eu = EngineUrl(); size_t q = eu.find(L'?'), eq = eu.find(L'=', q == std::wstring::npos ? 0 : q);
        std::wstring action = q == std::wstring::npos ? eu : eu.substr(0, q);
        std::wstring param  = (q == std::wstring::npos || eq == std::wstring::npos) ? L"q" : eu.substr(q + 1, eq - q - 1);
        size_t p = html.find(L"https://www.google.com/search"); if (p != std::wstring::npos) html.replace(p, 29, Esc(action));
        p = html.find(L"name=\"q\""); if (p != std::wstring::npos) html.replace(p, 8, L"name=\"" + Esc(param) + L"\"");
    }
    {   // open the search engine's connection while you look at the page
        std::wstring eu = EngineUrl(); size_t e = eu.find(L'/', 8);
        std::wstring origin = Esc(e == std::wstring::npos ? eu : eu.substr(0, e));
        size_t p = html.find(L"@PRE@");
        if (p != std::wstring::npos) html.replace(p, 5, L"");   // (no preconnect: opening a tab must not contact the search engine)
    }
    auto replaceAll = [&](const std::wstring& key, const std::wstring& val) {
        size_t p = html.find(key);
        if (p != std::wstring::npos) html.replace(p, key.size(), val);
    };

    replaceAll(L"@VARS@", m_incognito
        ? L"--bg:#1c1c20;--fg:#f2f2f2;--mute:#9a9aa2"
        : L"--bg:#fff;--fg:#111;--mute:#8a8a8a");

    std::wstring sites;   // only sponsored tiles (if any) go in this row; Libra no longer makes shortcuts by itself
    { std::wstring spT, spB; SponsorHtml(spT, spB); if (!spT.empty()) { if (sites.size() > 6 && sites.compare(sites.size() - 6, 6, L"</div>") == 0 && sites.rfind(L"<div class=\"sites\">", 0) == 0) sites.insert(sites.size() - 6, spT); else sites = L"<div class=\"sites\">" + spT + L"</div>"; } replaceAll(L"@SPBG@", spB + UpHtml()); }
    if (!m_incognito && m_totalBlocked > 0) sites += L"<div class=\"prot\"><b>" + std::to_wstring(m_totalBlocked) + L"</b> ads and trackers blocked so far</div>";
    if (m_incognito) sites += L"<style>body,html{background:#1c1c20!important;color:#f2f2f2}.logo,.logo sup{color:#f2f2f2!important;border-color:#f2f2f2!important}.search{background:#2a2a30!important;border-color:#3a3a42!important;color:#f2f2f2!important}.search input{background:transparent!important;color:#f2f2f2!important}.pv{max-width:520px;margin:34px auto 0;text-align:center;color:#9a9aa2;font:13px/1.6 'Segoe UI',sans-serif}.pv b{display:block;color:#f2f2f2;font-size:16px;margin-bottom:6px}</style>"
        L"<div class=pv><b>You are in a private window</b>No history, cookies or site data are saved. They are deleted when you close this window. Your downloads and bookmarks are kept, and sites, your employer or your network can still see what you do.</div>";
    if (!m_incognito && !m_setup && m_set.On(L"welcomed") && !m_set.On(L"first_hint"))   // one quiet tip on the first new-tab pages, gone for good after the first dismissal, search or tile click
        sites = L"<div class=\"tip\" id=\"tip\"><span>Tip: the shield shows what was blocked on a page. The sparkle button opens the Assistant.</span><button id=\"tipx\" aria-label=\"Dismiss\">&#215;</button></div><script>(function(){const t=document.getElementById('tip'),h=()=>chrome.webview.postMessage('home.hint\\t1');document.getElementById('tipx').onclick=()=>{h();t.remove()};document.querySelector('form.search').addEventListener('submit',h);document.addEventListener('click',e=>{if(e.target.closest&&e.target.closest('a.site'))h()})})()</script>" + sites;
    replaceAll(L"@SITES@", sites);
    return html;
}

// ─── Tab Management ───────────────────────────────────────────────────────────
void BrowserWindow::OpenTab(const std::wstring& url, bool incognito) {
    if (incognito && m_child) return;   // Child Safety: no private tabs
    auto tab       = std::make_unique<Tab>();
    tab->url       = url;
    tab->incognito = incognito || m_incognito;
    tab->title     = url.empty() ? L"New Tab" : url;
    int idx        = static_cast<int>(m_tabs.size());
    m_tabs.push_back(std::move(tab));
    m_activeTab = idx;

    // Hide all other tab's webviews
    for (int i = 0; i < idx; ++i)
        if (m_tabs[i]->controller) {
            m_tabs[i]->controller->put_IsVisible(FALSE);
            if (!m_tabs[i]->hiddenSince) m_tabs[i]->hiddenSince = GetTickCount();
        }

    if (m_envReady) InitWebView2(idx);
    InvalidateRect(m_hWnd, nullptr, FALSE);
    RefreshTabsPage();
}

int BrowserWindow::IndexOf(const Tab* t) const {
    for (int i = 0; i < (int)m_tabs.size(); ++i) if (m_tabs[i].get() == t) return i;
    return -1;
}

void BrowserWindow::CloseTab(int index) {
    if (index < 0 || index >= (int)m_tabs.size()) return;
    // Unregister events & close controller
    Tab* t = m_tabs[index].get();
    if (t->url == L"libra://agent") { AgStop(nullptr); m_agDock = false; }   // however the assistant is closed (button, tab X, shortcut), everything it was doing stops
    if (!t->incognito && !m_incognito && IsHttp(t->url)) { m_closed.push_back(t->url); if (m_closed.size() > 20) m_closed.erase(m_closed.begin()); }
    if (t->webView) {
        t->webView->remove_NavigationStarting(t->tokNavStart);
        t->webView->remove_NavigationCompleted(t->tokNavComplete);
        t->webView->remove_DocumentTitleChanged(t->tokDocTitleChange);
        t->webView->remove_WebResourceRequested(t->tokResReq);
        t->webView->remove_NewWindowRequested(t->tokNewWindow);
    }
    if (t->controller) t->controller->Close();

    bool wasPinned = t->pinned;
    m_tabs.erase(m_tabs.begin() + index);
    if (wasPinned) SavePinned();

    if (m_tabs.empty()) {
        PostMessage(m_hWnd, WM_CLOSE, 0, 0);
        return;
    }
    m_activeTab = std::min(m_activeTab, (int)m_tabs.size() - 1);
    SwitchTab(m_activeTab);
    RefreshTabsPage();
}

static void ResumeTab(Tab* t) {
    if (!t->suspended || !t->webView) return;
    wil::com_ptr<ICoreWebView2_3> w3; if (SUCCEEDED(t->webView->QueryInterface(IID_PPV_ARGS(&w3)))) w3->Resume();
    t->suspended = false;
}

void BrowserWindow::SwitchTab(int index) {
    if (index < 0 || index >= (int)m_tabs.size()) return;
    // Hide old
    if (m_activeTab >= 0 && m_activeTab < (int)m_tabs.size() && m_activeTab != index)
        if (m_tabs[m_activeTab]->controller) {
            m_tabs[m_activeTab]->controller->put_IsVisible(FALSE);
            m_tabs[m_activeTab]->hiddenSince = GetTickCount();
        }
    m_activeTab = index;
    Tab* t = m_tabs[index].get();
    ResumeTab(t); t->hiddenSince = 0; t->idleAsked = false;
    if (t->controller) {
        t->controller->put_IsVisible(TRUE);
        t->controller->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
    }
    UpdateOmniboxText(t->url);
    UpdateNavButtons();
    std::wstring title = t->title + L" — Libra Browser";
    SetWindowTextW(m_hWnd, title.c_str());
    m_progressVisible = t->loading;
    m_displayProgress = t->progress;
    if (m_agDock) ApplyDock();
    InvalidateRect(m_hWnd, nullptr, FALSE);
}

// ─── Navigation ──────────────────────────────────────────────────────────────
void BrowserWindow::NavigateTo(const std::wstring& input) {
    m_typedTick = GetTickCount();
    if (m_activeTab < 0 || m_activeTab >= (int)m_tabs.size()) return;
    Tab* t = m_tabs[m_activeTab].get();
    if (!t->webView) return;
    std::wstring url = SmartUrl(input);
    t->url = url;
    if (url.rfind(L"libra://", 0) == 0) { RenderTab(m_activeTab); UpdateOmniboxText(url); }
    else t->webView->Navigate(url.c_str());
}

std::wstring BrowserWindow::EngineUrl() const {
    std::wstring n = m_set.Get(L"engine", L"Google");
    if (n == L"Custom") { std::wstring u = m_set.Get(L"engine_url"); if (u.rfind(L"https://", 0) == 0) return u; }
    for (auto& e : kEngines) if (n == e.name) return e.url;
    return kEngines[0].url;
}

std::wstring BrowserWindow::SmartUrl(const std::wstring& in0) const {
    std::wstring in = in0;
    while (!in.empty() && iswspace(in.back())) in.pop_back();
    while (!in.empty() && iswspace(in.front())) in.erase(in.begin());
    if (in.empty()) return L"libra://home";
    if (in == L"about:blank") return in;
    if (in.rfind(L"libra://", 0) == 0) {
        for (const wchar_t* pg : { L"libra://home", L"libra://tabs", L"libra://settings", L"libra://history", L"libra://bookmarks", L"libra://downloads", L"libra://passwords", L"libra://privacy", L"libra://agent", L"libra://welcome" })
            if (in == pg) return in;
        return L"libra://home";
    }
    if (IsHttp(in)) return in;
    bool nospace = in.find(L' ') == std::wstring::npos;
    if (nospace && in.find(L"://") == std::wstring::npos) {
        std::wstring host = in.substr(0, in.find_first_of(L"/:?#"));
        bool ip = !host.empty() && std::count(host.begin(), host.end(), L'.') == 3 &&
                  std::all_of(host.begin(), host.end(), [](wchar_t c) { return iswdigit(c) || c == L'.'; });
        if (host == L"localhost" || ip) return L"http://" + in;       // local servers: plain http
        if (in.find(L'.') != std::wstring::npos) return L"https://" + in;
    }
    return EngineUrl() + UrlEnc(in);                                  // anything else is a search
}

void BrowserWindow::OnOmniboxReturn() {
    if (m_hSug && IsWindowVisible(m_hSug) && m_sugSel >= 0 && m_sugSel < (int)m_sugItems.size()) {
        std::wstring u = m_sugItems[m_sugSel].first; HideSug(); NavigateTo(u); SetFocus(m_hWnd); return;
    }
    HideSug();
    int len = GetWindowTextLengthW(m_hOmnibox);
    std::wstring txt(len + 1, L'\0');
    GetWindowTextW(m_hOmnibox, txt.data(), len + 1);
    txt.resize(len);
    NavigateTo(txt);
    SetFocus(m_hWnd);
}

void BrowserWindow::UpdateOmniboxText(const std::wstring& url) {
    if (m_agDock) PostMessage(m_hWnd, WM_APP + 32, 0, 0);   // the address changed: show or hide the docked Assistant
    // Don't show internal scheme
    std::wstring u = url;
    if (u == L"libra://error" && m_activeTab >= 0 && m_activeTab < (int)m_tabs.size()) u = m_tabs[m_activeTab]->errUrl;
    bool internal = (u == L"libra://home" || u == L"about:blank" || u.empty());
    std::wstring display = internal ? L"" : u;
    m_settingText = true;
    SetWindowTextW(m_hOmnibox, display.c_str());
    m_settingText = false;
    SendMessage(m_hOmnibox, EM_SETSEL, 0, -1); // select all when switching tabs
    UpdateBypass(); if (m_hBtnShield) InvalidateRect(m_hBtnShield, nullptr, FALSE);
}

void BrowserWindow::UpdateNavButtons() {
    if (m_activeTab < 0 || m_activeTab >= (int)m_tabs.size()) return;
}

void BrowserWindow::SetAdBlockEnabled(bool enabled) {
    m_adBlock = enabled;
}

// ─── Layout ───────────────────────────────────────────────────────────────────
RECT BrowserWindow::WebViewRect() const {
    RECT cr; GetClientRect(m_hWnd, &cr);
    int tabH  = MulDiv(Layout::TabBarHeight,  m_dpi, 96);
    int toolH = MulDiv(Layout::ToolbarHeight, m_dpi, 96);
    int top   = (m_full || m_setup) ? 0 : tabH + toolH;   // first-run setup: no address bar at all
    return { 0, top, cr.right, cr.bottom };
}

void BrowserWindow::ResizeWebViews() {
    RECT wvRect = WebViewRect(), pan = wvRect;
    const int ai = AgDockIdx(); const bool dock = ai >= 0 && ai != m_activeTab && AgAllowedHere();   // agent sidebar: right strip, other tabs shrink
    if (dock) { int w = std::min<int>(MulDiv(400, m_dpi, 96), (wvRect.right - wvRect.left) / 2); wvRect.right -= w; pan.left = pan.right - w; }
    for (int i = 0; i < (int)m_tabs.size(); ++i)
        if (m_tabs[i]->controller)
            m_tabs[i]->controller->put_Bounds(dock && i == ai ? pan : wvRect);
}

RECT BrowserWindow::TabRect(int index) const {
    RECT cr; GetClientRect(m_hWnd, &cr);
    int n = (int)m_tabs.size();
    if (n == 0 || index < 0 || index >= n) return {};
    int tabH   = MulDiv(Layout::TabBarHeight, m_dpi, 96);
    int margin = MulDiv(Layout::TabMargin, m_dpi, 96);
    int pw     = MulDiv(46, m_dpi, 96);                      // pinned tabs: icon only
    int np = 0; for (auto& t : m_tabs) if (t->pinned) ++np;
    int avail  = cr.right - tabH - margin - np * pw;         // tabH = room for the + button
    int tw     = (n > np) ? std::clamp(avail / (n - np), MulDiv(Layout::TabMinWidth, m_dpi, 96), MulDiv(Layout::TabMaxWidth, m_dpi, 96)) : 0;
    int x = margin;
    for (int i = 0; i < index; ++i) x += m_tabs[i]->pinned ? pw : tw;
    return { x, 0, x + (m_tabs[index]->pinned ? pw : tw), tabH };
}

RECT BrowserWindow::TabCloseRect(int index) const {
    if (index >= 0 && index < (int)m_tabs.size() && m_tabs[index]->pinned) return { 0, 0, 0, 0 };   // pinned: no close button
    RECT r   = TabRect(index);
    int  sz  = MulDiv(Layout::TabCloseSize, m_dpi, 96);
    int  pad = MulDiv(6, m_dpi, 96);
    int cx   = r.right  - pad - sz;
    int cy   = r.top    + (r.bottom - r.top - sz) / 2;
    return { cx, cy, cx + sz, cy + sz };
}

int BrowserWindow::TabIndexAt(int x, int y) const {
    int tabH = MulDiv(Layout::TabBarHeight, m_dpi, 96);
    if (y < 0 || y >= tabH) return -1;
    for (int i = 0; i < (int)m_tabs.size(); ++i) {
        RECT r = TabRect(i);
        if (x >= r.left && x < r.right) return i;
    }
    return -1;
}

bool BrowserWindow::TabCloseHit(int tabIndex, int x, int y) const {
    RECT r = TabCloseRect(tabIndex);
    return (x >= r.left && x < r.right && y >= r.top && y < r.bottom);
}

// ─── Painting ────────────────────────────────────────────────────────────────
void BrowserWindow::OnPaint(HDC hdc, RECT cr) {
    if (m_setup) { HBRUSH w = CreateSolidBrush(RGB(255, 255, 255)); FillRect(hdc, &cr, w); DeleteObject(w); return; }   // first run: blank until the film starts, no tab strip / toolbar painted
    int tabH  = MulDiv(Layout::TabBarHeight,  m_dpi, 96);
    int toolH = MulDiv(Layout::ToolbarHeight, m_dpi, 96);

    RECT tabBarRect  = { 0, 0,       cr.right, tabH };
    RECT toolbarRect = { 0, tabH,    cr.right, tabH + toolH };

    PaintTabBar (hdc, tabBarRect);
    PaintToolbar(hdc, toolbarRect);
    if (m_progressVisible)
        PaintProgressBar(hdc, toolbarRect);
}

void BrowserWindow::PaintTabBar(HDC hdc, const RECT& r) {
    const bool inc = m_incognito;
    HBRUSH bgBr = CreateSolidBrush(inc ? Color::IncognitoChrome : Color::Surface);
    FillRect(hdc, &r, bgBr);
    DeleteObject(bgBr);

    int n     = (int)m_tabs.size();
    int pad   = MulDiv(Layout::TabPaddingH, m_dpi, 96);
    int inset = MulDiv(Layout::TabInset,    m_dpi, 96);
    int gap   = MulDiv(Layout::TabGap,      m_dpi, 96);
    int rad   = MulDiv(Layout::TabRadius,   m_dpi, 96);

    for (int i = 0; i < n; ++i) {
        RECT tr     = TabRect(i);
        bool active = (i == m_activeTab);
        bool hover  = (i == m_hoveredTab);
        bool incog  = m_tabs[i]->incognito;

        RECT pill = { tr.left + gap / 2, tr.top + inset, tr.right - gap / 2, tr.bottom - inset };
        if (active)
            FillRR(hdc, pill, rad, incog ? Color::DarkOmnibox : Color::OmniboxBg, 255,
                   incog ? Color::DarkBorder : Color::OmniboxBorder, 1.f);
        else if (hover)
            FillRR(hdc, pill, rad, incog ? Color::DarkTab : Color::TabHover);

        if (m_tabs[i]->shown > 0.005f) {   // the tab itself fills with a soft gradient while its page loads
            Gdiplus::Graphics g(hdc); g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            Gdiplus::GraphicsPath path; RoundPath(path, (float)pill.left, (float)pill.top, (float)(pill.right - pill.left - 1), (float)(pill.bottom - pill.top - 1), (float)rad);
            g.SetClip(&path);
            Gdiplus::LinearGradientBrush br(Gdiplus::Point(pill.left, 0), Gdiplus::Point(pill.right, 0), Gdiplus::Color(150, 208, 222, 255), Gdiplus::Color(150, 232, 210, 255));
            g.FillRectangle(&br, pill.left, pill.top, (int)((pill.right - pill.left) * m_tabs[i]->shown), pill.bottom - pill.top);
        }
        // Favicon placeholder: coloured disc with the title's first letter
        int fav  = MulDiv(18, m_dpi, 96);
        bool compact = (pill.right - pill.left) < MulDiv(112, m_dpi, 96);   // many tabs: icon only (a cut-off title like "N..." says nothing)
        int favX = (m_tabs[i]->pinned || (compact && !active && !hover)) ? pill.left + ((pill.right - pill.left) - fav) / 2 : pill.left + pad;
        int favY = pill.top + ((pill.bottom - pill.top) - fav) / 2;
        const std::wstring& title = m_tabs[i]->title;
        wchar_t letter[2] = { L'L', 0 };
        if (!title.empty() && title.rfind(L"libra://", 0) != 0 && title != L"New Tab") {
            wchar_t c0 = title[0];
            letter[0] = (c0 >= L'a' && c0 <= L'z') ? (wchar_t)(c0 - 32) : c0;
        }
        if (m_tabs[i]->fav) {
            Gdiplus::Graphics gi(hdc); gi.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            gi.DrawImage((Gdiplus::Bitmap*)m_tabs[i]->fav.get(), favX, favY, fav, fav);
        } else {   // outlined disc, no colour
            RECT disc = { favX, favY, favX + fav, favY + fav };
            FillRR(hdc, disc, fav / 2, incog ? Color::DarkOmnibox : Color::OmniboxBg, 255,
                   incog ? Color::DarkBorder : Color::OmniboxBorder, 1.f);
        }
        if (m_tabs[i]->loading) {          // where the spinner used to be: a cross that stops loading
            int cx = favX + fav / 2, cy = favY + fav / 2; float h2 = fav * 0.2f;
            FillCircle(hdc, cx, cy, fav / 2 + 1, RGB(255, 255, 255), 235);
            DrawLineAA(hdc, cx - h2, cy - h2, cx + h2, cy + h2, Color::TextPrimary, 1.6f);
            DrawLineAA(hdc, cx + h2, cy - h2, cx - h2, cy + h2, Color::TextPrimary, 1.6f);
        }
        if (!m_tabs[i]->fav && !m_tabs[i]->loading) {
            RECT lr = { favX, favY, favX + fav, favY + fav };
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, incog ? Color::IncognitoText : Color::TextPrimary);
            HFONT old = (HFONT)SelectObject(hdc, m_fontSmall);
            DrawTextW(hdc, letter, 1, &lr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            SelectObject(hdc, old);
        }

        if (!m_tabs[i]->pinned && !compact) {
        RECT textRect = pill;
        textRect.left  = favX + fav + MulDiv(8, m_dpi, 96);
        textRect.right = pill.right - MulDiv(30, m_dpi, 96);
        COLORREF textCol = active ? (incog ? Color::IncognitoText : Color::TextPrimary)
                                  : Color::TextSecondary;
        SetTextColor(hdc, textCol);
        SetBkMode(hdc, TRANSPARENT);
        HFONT oldF = (HFONT)SelectObject(hdc, m_fontUI);
        DrawTextW(hdc, title.c_str(), -1, &textRect,
                  DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
        SelectObject(hdc, oldF);
        }

        // Close button: soft circle + thin cross
        if ((active || hover) && !m_tabs[i]->pinned) {
            RECT cr2 = TabCloseRect(i);
            cr2.left -= gap / 2; cr2.right -= gap / 2;
            int cx = (cr2.left + cr2.right) / 2, cy = (cr2.top + cr2.bottom) / 2;
            if (hover) FillCircle(hdc, cx, cy, (cr2.right - cr2.left) / 2, incog ? Color::DarkTab : Color::ButtonHover);
            float h2 = MulDiv(4, m_dpi, 96) * 1.f;
            float w  = MulDiv(3, m_dpi, 192) * 1.f + 0.6f;
            DrawLineAA(hdc, cx - h2, cy - h2, cx + h2, cy + h2, incog ? Color::IncognitoText : Color::TextSecondary, w);
            DrawLineAA(hdc, cx + h2, cy - h2, cx - h2, cy + h2, incog ? Color::IncognitoText : Color::TextSecondary, w);
        }
    }

    // New-tab (+) button: round plate
    int tabH  = r.bottom - r.top;
    int newX  = (n > 0) ? TabRect(n - 1).right : MulDiv(Layout::TabMargin, m_dpi, 96);
    int btnSz = MulDiv(30, m_dpi, 96);
    int bY    = (tabH - btnSz) / 2;
    int pcx   = newX + MulDiv(6, m_dpi, 96) + btnSz / 2;
    int pcy   = bY + btnSz / 2;
    { RECT nb = { pcx - btnSz / 2, pcy - btnSz / 2, pcx + btnSz / 2, pcy + btnSz / 2 };
      FillRR(hdc, nb, btnSz / 2, inc ? Color::DarkOmnibox : Color::OmniboxBg, 255, inc ? Color::DarkBorder : Color::OmniboxBorder, 1.f); }
    float ph = MulDiv(6, m_dpi, 96) * 1.f;
    float pw = MulDiv(3, m_dpi, 192) * 1.f + 0.8f;
    DrawLineAA(hdc, pcx - ph, (float)pcy, pcx + ph, (float)pcy, inc ? Color::IncognitoText : Color::TextPrimary, pw);
    DrawLineAA(hdc, (float)pcx, pcy - ph, (float)pcx, pcy + ph, inc ? Color::IncognitoText : Color::TextPrimary, pw);
}

void BrowserWindow::PaintToolbar(HDC hdc, const RECT& r) {
    const bool inc = m_incognito;
    FillRect(hdc, &r, m_brToolbar);

    // Hairline under the toolbar
    HPEN bord = CreatePen(PS_SOLID, 1, inc ? Color::DarkBorder : Color::Border);
    HPEN old  = (HPEN)SelectObject(hdc, bord);
    MoveToEx(hdc, r.left, r.bottom - 1, nullptr);
    LineTo  (hdc, r.right, r.bottom - 1);
    SelectObject(hdc, old); DeleteObject(bord);

    // Address-bar pill
    RECT pill = OmniPillRect();
    int  rad  = (pill.bottom - pill.top) / 2;
    bool focus = (GetFocus() == m_hOmnibox);
    COLORREF fill = inc ? Color::DarkOmnibox : Color::OmniboxBg;
    if (focus)
        FillRR(hdc, pill, rad, fill, 255, inc ? Color::IncognitoText : Color::OmniboxFocus, 2.f);
    else
        FillRR(hdc, pill, rad, fill, 255, inc ? Color::DarkBorder : Color::OmniboxBorder, 1.f);

    // Search / lock glyph on the left of the pill
    bool secure = m_activeTab >= 0 && m_activeTab < (int)m_tabs.size() &&
                  m_tabs[m_activeTab]->url.rfind(L"https://", 0) == 0;
    RECT gr = { pill.left + MulDiv(8, m_dpi, 96), pill.top,
                pill.left + MulDiv(40, m_dpi, 96), pill.bottom };
    DrawGlyph(hdc, secure ? L"" : L"", gr, MulDiv(14, m_dpi, 96),
              inc ? Color::IncognitoText : Color::TextSecondary);
    if (m_activeTab >= 0 && m_activeTab < (int)m_tabs.size() && IsHttp(m_tabs[m_activeTab]->url)) {
        if (m_set.On(L"save_video") && !m_tabs[m_activeTab]->vids.empty()) DrawGlyph(hdc, L"\uE896", VidRect(), MulDiv(15, m_dpi, 96), Color::TextPrimary);
        RECT sr = StarRect();
        DrawGlyph(hdc, IsBookmarked(m_tabs[m_activeTab]->url) ? L"\uE735" : L"\uE734", sr, MulDiv(15, m_dpi, 96), Color::TextPrimary);
    }
    // (nav buttons are owner-drawn in DrawNavButton)
}

void BrowserWindow::PaintProgressBar(HDC hdc, const RECT& toolbarRect) {
    if (m_activeTab >= 0 && m_activeTab < (int)m_tabs.size() && m_tabs[m_activeTab]->url.rfind(L"libra://", 0) == 0) return;   // no loading bar on our own pages
    if (!m_progressVisible || m_displayProgress <= 0.f) return;
    int barH = MulDiv(3, m_dpi, 96);
    int barY = toolbarRect.bottom - barH;
    int barW = (int)(toolbarRect.right * m_displayProgress);
    Gdiplus::Graphics g(hdc);
    Gdiplus::LinearGradientBrush br(Gdiplus::Point(0, 0), Gdiplus::Point(480, 0), Gdiplus::Color(17, 17, 17), Gdiplus::Color(17, 17, 17));
    Gdiplus::Color cols[5] = { Gdiplus::Color(17, 17, 17), Gdiplus::Color(124, 92, 255), Gdiplus::Color(56, 189, 248), Gdiplus::Color(244, 114, 182), Gdiplus::Color(17, 17, 17) };
    Gdiplus::REAL pos[5] = { 0.f, .3f, .55f, .8f, 1.f };
    br.SetInterpolationColors(cols, pos, 5);
    br.SetWrapMode(Gdiplus::WrapModeTile);
    br.TranslateTransform((float)((GetTickCount() / 3) % 480), 0.f);   // slides while loading
    g.FillRectangle(&br, 0, barY, barW, barH);
}

// ─── Window Proc ─────────────────────────────────────────────────────────────
LRESULT CALLBACK BrowserWindow::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    BrowserWindow* bw = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        bw = reinterpret_cast<BrowserWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(bw));
        bw->m_hWnd = hWnd;
    } else {
        bw = reinterpret_cast<BrowserWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
    }
    if (!bw) return DefWindowProcW(hWnd, msg, wParam, lParam);

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc     = BeginPaint(hWnd, &ps);
        // Double-buffer to eliminate flicker
        RECT cr; GetClientRect(hWnd, &cr);
        int tabH  = MulDiv(Layout::TabBarHeight,  bw->m_dpi, 96);
        int toolH = MulDiv(Layout::ToolbarHeight, bw->m_dpi, 96);
        int uiH   = tabH + toolH;
        HDC memDC   = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, cr.right, uiH);
        HBITMAP old = (HBITMAP)SelectObject(memDC, bmp);
        bw->OnPaint(memDC, cr);
        BitBlt(hdc, 0, 0, cr.right, uiH, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, old);
        DeleteObject(bmp);
        DeleteDC(memDC);
        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_SIZE:
        bw->OnSize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_COMMAND:
        bw->OnCommand(wParam, lParam);
        return 0;
    case WM_LBUTTONDOWN:
        bw->OnLButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONDBLCLK: {
        // Double-click on tab bar: open new tab
        int y = GET_Y_LPARAM(lParam);
        int tabH2 = MulDiv(Layout::TabBarHeight, bw->m_dpi, 96);
        if (y < tabH2) bw->OpenTab(L"");
        return 0;
    }
    case WM_MOUSEMOVE: {
        int x = GET_X_LPARAM(lParam); int y = GET_Y_LPARAM(lParam);
        int prev = bw->m_hoveredTab;
        bw->m_hoveredTab = bw->TabIndexAt(x, y);
        if (bw->m_hoveredTab != prev) InvalidateRect(hWnd, nullptr, FALSE);
        // Track mouse leave
        TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hWnd, 0 };
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        bw->m_hoveredTab = -1;
        InvalidateRect(hWnd, nullptr, FALSE);
        return 0;
    case WM_RBUTTONDOWN: {
        int ti = bw->TabIndexAt(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        ClientToScreen(hWnd, &pt);
        if (ti >= 0) bw->ShowTabMenu(ti, pt); else bw->ShowContextMenu(pt.x, pt.y);
        return 0;
    }
    case WM_TIMER:
        bw->OnTimer((UINT_PTR)wParam);
        return 0;
    case WM_APP + 5: {
        std::unique_ptr<std::string> h((std::string*)lParam);
        bw->OnLetThrough(*h);
        return 0;
    }
    case WM_APP + 4: {
        std::unique_ptr<std::string> h((std::string*)lParam);
        bw->OnBlockedHit(*h);
        return 0;
    }
    case WM_APP + 40: bw->SaveNext(); return 0;
    case WM_APP + 41: bw->HoldDone((void*)lParam); return 0;   // quarantine scan finished   // a download is waiting for its Save window
    case WM_APP + 3: {
        std::unique_ptr<std::shared_ptr<DlJob>> jp((std::shared_ptr<DlJob>*)lParam);
        bw->OnProbeDone(*jp, wParam != 0);
        return 0;
    }
    case WM_APP + 6: {
        std::unique_ptr<std::shared_ptr<DlJob>> jp((std::shared_ptr<DlJob>*)lParam);
        bw->OnVideoProbe(*jp, wParam != 0);
        return 0;
    }
    case WM_APP + 30: bw->OnWinAuth((int)wParam, (int)lParam); return 0;   // Windows sign-in answered
    case WM_APP + 32: bw->ApplyDock(); return 0;
    case WM_APP + 33: bw->ToggleAgent(); return 0;
    case WM_APP + 34: bw->AgFileChosen((void*)lParam); return 0;
    case WM_APP + 36: bw->OnVoiceMsg((void*)lParam); return 0;
    case WM_APP + 37: bw->SponsorGot((void*)lParam); return 0;
    case WM_APP + 38: bw->SnGot((void*)lParam); return 0;
    case WM_APP + 39: bw->UpGot((void*)lParam); return 0;
    case WM_APP + 42: bw->UpReady((void*)lParam); return 0;
    case WM_APP + 43: bw->PdfToolsGot((void*)lParam); return 0;
    case WM_APP + 35: { std::unique_ptr<std::wstring> s((std::wstring*)lParam); bw->AgKeyChecked(*s); return 0; }
    case WM_APP + 31: { std::unique_ptr<std::wstring> s((std::wstring*)lParam); bw->AgentOnLlm((unsigned)wParam, *s); return 0; }   // agent: model reply
    case WM_APP + 20: {   // web suggestions arrived (ignored when the text changed since)
        std::unique_ptr<std::vector<std::wstring>> v((std::vector<std::wstring>*)lParam);
        if ((unsigned)wParam != bw->m_sugGen || GetFocus() != bw->m_hOmnibox) return 0;
        std::wstring eu = bw->EngineUrl(); std::set<std::wstring> have;
        auto low = [](std::wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; };
        for (auto& i : bw->m_sugItems) have.insert(low(i.second));
        for (auto& s : *v) if (bw->m_sugItems.size() < 8 && have.insert(low(s)).second) bw->m_sugItems.push_back({ eu + UrlEnc(s), s });
        { auto& it = bw->m_sugItems; size_t fw = 0; while (fw < it.size() && it[fw].first.rfind(eu, 0) != 0) ++fw;   // what you typed goes first among the searches
          for (size_t j = fw + 1; j < it.size(); ++j) if (low(it[j].second) == bw->m_sugQ) { std::rotate(it.begin() + fw, it.begin() + j, it.begin() + j + 1); break; } }
        if (!bw->m_sugItems.empty()) bw->ShowSug();
        return 0;
    }
    case WM_APP + 21: {   // suggestions for the new-tab page search box
        std::unique_ptr<HomeSugMsg> m((HomeSugMsg*)lParam);
        if (bw->IndexOf(m->t) < 0 || !m->t->webView || m->t->url.rfind(L"libra://", 0) != 0) return 0;
        auto js = [](const std::wstring& s) { std::wstring o = L"\""; for (wchar_t c : s) { if (c == L'"' || c == L'\\') { o += L'\\'; o += c; } else if (c >= 32) o += c; } return o + L"\""; };
        std::wstring arr = L"["; for (size_t i = 0; i < m->v.size(); ++i) arr += (i ? L"," : L"") + js(m->v[i]);
        m->t->webView->ExecuteScript((L"window.__sug&&window.__sug(" + arr + L"]," + js(m->q) + L")").c_str(), nullptr);
        return 0;
    }
    case WM_APP + 7:   // https-upgrade: navigate to the secure address
        for (auto& tb : bw->m_tabs) if ((LPARAM)tb.get() == lParam && tb->webView && !tb->upUrl.empty()) { std::wstring u = tb->upUrl; tb->upUrl.clear(); tb->webView->Navigate(u.c_str()); }
        return 0;
    case WM_APP + 2:
        for (auto& tb : bw->m_tabs)
            if ((LPARAM)tb.get() == lParam) {
                if (wParam == 9 || wParam == 10) { bw->m_errPend.push_back({ tb.get(), tb->navId, (int)wParam }); SetTimer(hWnd, 2030, 2500, nullptr); }   // "aborted": login redirects often abort and retry
                else bw->ShowError(tb.get(), (int)wParam);
                break; }
        return 0;
    case WM_APP + 1: {
        std::unique_ptr<std::string> h((std::string*)lParam);
        bw->SweepBlocked(*h);
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        // Omnibox text sits on the painted pill: same fill, light text (cached brush, no leak)
        HDC editDC = (HDC)wParam;
        SetBkColor(editDC, bw->m_incognito ? Color::DarkOmnibox : Color::OmniboxBg);
        SetTextColor(editDC, bw->m_incognito ? Color::DarkText : Color::TextPrimary);
        return (LRESULT)bw->m_brOmni;
    }
    case WM_MEASUREITEM: {
        auto* mi = reinterpret_cast<MEASUREITEMSTRUCT*>(lParam);
        if (mi->CtlType != ODT_MENU) break;
        mi->itemWidth = MulDiv(200, bw->m_dpi, 96); mi->itemHeight = MulDiv(mi->itemData ? 38 : 11, bw->m_dpi, 96);
        return TRUE;
    }
    case WM_DRAWITEM: {
        auto* di = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
        if (di->CtlType == ODT_MENU) {
            HDC hdc = di->hDC; RECT r = di->rcItem; int dpi = bw->m_dpi;
            FillRect(hdc, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
            const MenuIt* it = (const MenuIt*)di->itemData;
            if (!it) { RECT ln = { r.left + MulDiv(12, dpi, 96), r.top + (r.bottom - r.top) / 2, r.right - MulDiv(12, dpi, 96), r.top + (r.bottom - r.top) / 2 + 1 }; HBRUSH b = CreateSolidBrush(RGB(236, 236, 236)); FillRect(hdc, &ln, b); DeleteObject(b); return TRUE; }
            if (di->itemState & ODS_SELECTED) { RECT hr = { r.left + MulDiv(6, dpi, 96), r.top + 2, r.right - MulDiv(6, dpi, 96), r.bottom - 2 }; HBRUSH b = CreateSolidBrush(RGB(243, 243, 243)); HPEN p = CreatePen(PS_NULL, 0, 0); HGDIOBJ ob = SelectObject(hdc, b), op = SelectObject(hdc, p); RoundRect(hdc, hr.left, hr.top, hr.right, hr.bottom, MulDiv(12, dpi, 96), MulDiv(12, dpi, 96)); SelectObject(hdc, ob); SelectObject(hdc, op); DeleteObject(b); DeleteObject(p); }
            SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, RGB(17, 17, 17));
            HFONT icf = MakeIconFont(MulDiv(16, dpi, 96)); HGDIOBJ of = SelectObject(hdc, icf);
            RECT ir = { r.left + MulDiv(18, dpi, 96), r.top, r.left + MulDiv(42, dpi, 96), r.bottom };
            if (wcscmp(it->u, L"libra://burn") == 0) {   // owner-supplied flame icon (FlameIcon.h)
                static Gdiplus::Bitmap* flame = nullptr;
                if (!flame) { IStream* ms = SHCreateMemStream(kFlamePng, (UINT)sizeof(kFlamePng)); if (ms) { flame = Gdiplus::Bitmap::FromStream(ms); ms->Release(); } }
                if (flame && flame->GetLastStatus() == Gdiplus::Ok) {
                    Gdiplus::Graphics g(hdc); g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
                    int sz = MulDiv(18, dpi, 96); g.DrawImage(flame, (INT)((ir.left + ir.right) / 2 - sz / 2), (INT)((ir.top + ir.bottom) / 2 - sz / 2), sz, sz);
                }
            } else DrawTextW(hdc, it->g, -1, &ir, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, bw->m_fontUI);
            RECT tr = { r.left + MulDiv(54, dpi, 96), r.top, r.right - MulDiv(12, dpi, 96), r.bottom }; DrawTextW(hdc, it->n, -1, &tr, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, of); DeleteObject(icf);
            return TRUE;
        }
        bw->DrawNavButton(di);
        return TRUE;
    }
    case WM_ERASEBKGND:
        return 1; // prevent background erase flicker (we paint everything)
    case WM_COPYDATA: {   // another Libra launch (a clicked link) hands us its URL
        auto* cd = reinterpret_cast<COPYDATASTRUCT*>(lParam);
        if (cd && cd->dwData == 1 && cd->cbData >= 2 && cd->cbData < 4100) {
            std::wstring u((const wchar_t*)cd->lpData, cd->cbData / 2); while (!u.empty() && !u.back()) u.pop_back();
            if (IsHttp(u)) { if (IsIconic(hWnd)) ShowWindow(hWnd, SW_RESTORE); bw->OpenTab(u); SetForegroundWindow(hWnd); }
        }
        return TRUE;
    }
    case WM_CLOSE:
        bw->OnClose();
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hWnd, msg, wParam, lParam);
}

void BrowserWindow::OnSize(int cx, int cy) {
    if (!m_hOmnibox) return;

    LayoutToolbar(cx);
    ResizeWebViews();
    InvalidateRect(m_hWnd, nullptr, FALSE);
}

void BrowserWindow::OnLButtonDown(int x, int y) {
    int tabH = MulDiv(Layout::TabBarHeight, m_dpi, 96);
    if (y >= tabH) {   // toolbar: bookmark star
        RECT sr = StarRect(); POINT pt = { x, y };
        if (m_activeTab >= 0 && m_set.On(L"save_video") && !m_tabs[m_activeTab]->vids.empty()) { RECT vr = VidRect(); if (PtInRect(&vr, pt)) { ShowVideoPopup(); return; } }
        if (PtInRect(&sr, pt)) ToggleBookmark();
        return;
    }

    int idx = TabIndexAt(x, y);
    if (idx >= 0) {
        if (m_tabs[idx]->loading) {       // click the cross on a loading tab to stop it
            RECT f = FavRect(idx); InflateRect(&f, MulDiv(4, m_dpi, 96), MulDiv(4, m_dpi, 96)); POINT pt = { x, y };
            if (PtInRect(&f, pt)) { if (m_tabs[idx]->webView) m_tabs[idx]->webView->Stop(); return; }
        }
        if (TabCloseHit(idx, x, y))
            CloseTab(idx);
        else
            SwitchTab(idx);
        return;
    }

    // New-tab (+) button?
    int n    = (int)m_tabs.size();
    int newX = (n > 0) ? TabRect(n - 1).right : 0;
    int btnSz = MulDiv(28, m_dpi, 96);
    if (x >= newX && x <= newX + MulDiv(40, m_dpi, 96) && y < tabH)
        OpenTab(L"");
}

void BrowserWindow::OnCommand(WPARAM wParam, LPARAM lParam) {
    int id = LOWORD(wParam);
    if (id == IDC_OMNIBOX && HIWORD(wParam) == EN_CHANGE) { UpdateSuggestions(); return; }
    if (id == IDC_OMNIBOX && HIWORD(wParam) == EN_KILLFOCUS) HideSug();
    if (id == IDC_OMNIBOX && (HIWORD(wParam) == EN_SETFOCUS || HIWORD(wParam) == EN_KILLFOCUS)) {
        InvalidateRect(m_hWnd, nullptr, FALSE);   // repaint the focus ring on the pill
        return;
    }
    switch (id) {
    case ID_FILE_NEW_TAB:       OpenTab(L""); break;
    case ID_FILE_NEW_INCOGNITO: NewPrivateWindow(); break;
    case ID_FILE_CLOSE_TAB:     if (m_activeTab >= 0) CloseTab(m_activeTab); break;
    case ID_FILE_EXIT:          PostMessage(m_hWnd, WM_CLOSE, 0, 0); break;
    case ID_NAV_BACK:
        if (m_activeTab >= 0 && m_tabs[m_activeTab]->webView)
            m_tabs[m_activeTab]->webView->GoBack(); break;
    case ID_NAV_FORWARD:
        if (m_activeTab >= 0 && m_tabs[m_activeTab]->webView)
            m_tabs[m_activeTab]->webView->GoForward(); break;
    case ID_NAV_REFRESH:
        if (m_activeTab >= 0 && m_tabs[m_activeTab]->webView) {
            if (m_tabs[m_activeTab]->loading)
                m_tabs[m_activeTab]->webView->Stop();
            else
                m_tabs[m_activeTab]->webView->Reload();
        }
        break;
    case ID_NAV_HOME:          NavigateTo(L"libra://home"); break;
    case ID_TAB_PIN:           TogglePin(m_activeTab);   break;
    case ID_VIEW_FULLSCREEN:   SetFullscreen(!m_full);   break;
    case ID_PAGE_HISTORY:      ShowInternal(L"libra://history");   break;
    case ID_PAGE_DOWNLOADS:    ShowInternal(L"libra://downloads"); break;
    case ID_PAGE_BOOKMARKS:    ShowInternal(L"libra://bookmarks"); break;
    case ID_PAGE_SETTINGS:     ShowInternal(L"libra://settings");  break;
    case ID_PAGE_PASSWORDS:    ShowInternal(L"libra://passwords"); break;
    case ID_PAGE_PRIVACY:      ShowInternal(L"libra://privacy");   break;
    case ID_SETTINGS_ADBLOCK:  m_adBlock   = !m_adBlock;   break;
    case ID_SETTINGS_IMAGES:   m_imagesOff = !m_imagesOff; break;
    case IDC_BTN_BACK:         PostMessage(m_hWnd, WM_COMMAND, ID_NAV_BACK,    0); break;
    case IDC_BTN_FORWARD:      PostMessage(m_hWnd, WM_COMMAND, ID_NAV_FORWARD, 0); break;
    case IDC_BTN_REFRESH:      PostMessage(m_hWnd, WM_COMMAND, ID_NAV_REFRESH, 0); break;
    case IDC_BTN_MENU: {
        HMENU m = CreatePopupMenu();
        static HBRUSH white = CreateSolidBrush(RGB(255, 255, 255));
        MENUINFO mi{ sizeof(mi), MIM_BACKGROUND | MIM_STYLE, MNS_NOCHECK, 0, white, 0, 0 }; SetMenuInfo(m, &mi);
        if (!m_child) { AppendMenuW(m, MF_OWNERDRAW, 9107, (LPCWSTR)&kMenuIts[7]); AppendMenuW(m, MF_OWNERDRAW, 9109, (LPCWSTR)&kMenuIts[9]); if (!m_incognito) AppendMenuW(m, MF_OWNERDRAW, 9108, (LPCWSTR)&kMenuIts[8]); AppendMenuW(m, MF_OWNERDRAW | MF_SEPARATOR, 0, nullptr); }
        for (int k = 0; k < 7; ++k) {
            if (k == 4 || k == 5) continue;   // Privacy and Tabs live elsewhere
            if (k == 6) AppendMenuW(m, MF_OWNERDRAW | MF_SEPARATOR, 0, nullptr);
            AppendMenuW(m, MF_OWNERDRAW, 9100 + k, (LPCWSTR)&kMenuIts[k]);
        }
        RECT br; GetWindowRect(m_hBtnMenu, &br);
        int pick = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTALIGN | TPM_TOPALIGN, br.right, br.bottom + 4, 0, m_hWnd, nullptr);
        DestroyMenu(m);
        if (pick == 9107) NewPrivateWindow();
        else if (pick == 9108) Burn();
        else if (pick == 9109) OpenShot();
        else if (pick >= 9100 && pick < 9107) { if (std::wstring(kMenuIts[pick - 9100].u) == L"libra://agent") ToggleAgent(); else ShowInternal(kMenuIts[pick - 9100].u); }
        break; }
    case IDC_BTN_SHIELD:       ToggleShieldPopup(); break;
    case IDC_BTN_AGENT:        ToggleAgent(); break;
    case IDC_BTN_SHOT:         OpenShot(); break;
    }
}

void BrowserWindow::ShowContextMenu(int screenX, int screenY) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, ID_FILE_NEW_TAB,       L"New Tab\tCtrl+T");
    AppendMenuW(hMenu, MF_STRING, ID_FILE_NEW_INCOGNITO, L"New private window\tCtrl+Shift+N");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_NAV_HOME,           L"Home");
    AppendMenuW(hMenu, MF_STRING, ID_PAGE_HISTORY,       L"History\tCtrl+H");
    AppendMenuW(hMenu, MF_STRING, ID_PAGE_DOWNLOADS,     L"Downloads\tCtrl+J");
    AppendMenuW(hMenu, MF_STRING, ID_PAGE_BOOKMARKS,     L"Bookmarks\tCtrl+Shift+O");
    AppendMenuW(hMenu, MF_STRING, ID_PAGE_PASSWORDS,     L"Passwords");
    AppendMenuW(hMenu, MF_STRING, ID_PAGE_PRIVACY,       L"Privacy");
    AppendMenuW(hMenu, MF_STRING, ID_PAGE_SETTINGS,      L"Settings");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING | (m_adBlock   ? MF_CHECKED : 0), ID_SETTINGS_ADBLOCK, L"Local Ad Blocker (off by default - cloud ad blocking already runs)");
    AppendMenuW(hMenu, MF_STRING | (m_imagesOff ? MF_CHECKED : 0), ID_SETTINGS_IMAGES,  L"Block Images (Data Saver)");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, ID_FILE_CLOSE_TAB,     L"Close Tab\tCtrl+W");
    AppendMenuW(hMenu, MF_STRING, ID_FILE_EXIT,          L"Exit");
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, screenX, screenY, 0, m_hWnd, nullptr);
    DestroyMenu(hMenu);
}

void BrowserWindow::OnTimer(UINT_PTR id) {
    if (BurnTimer(id) || ShotTimer(id) || SaveTimer(id)) return;
    if (id == 2002) {   // password popup: close when the user leaves the tab/window or after 30 s
        bool away = GetForegroundWindow() != m_hWnd || GetTickCount() - m_pwShown > (m_pwKind == 3 ? 6000u : m_pwKind == 6 ? 4500u : 30000u) ||
                    (m_pwKind == 2 && (m_activeTab < 0 || m_tabs[m_activeTab].get() != m_pwTab));
        if (away) HidePw();
        return;
    }
    if (id == 2003) { PwClipClear(); return; }
    if (id == 2004) { KillTimer(m_hWnd, 2004); FirstBlock(); return; }
    if (id == 2005) { DlPush(); return; }
    if (id == 2031) {   // a new local day: every tab gets the new identity script
        if (m_fpDay && StatDay(0) != m_fpDay) for (auto& tb : m_tabs) InstallFpScript(tb.get());
        return;
    }
    if (id == 2030) {   // delayed "aborted" errors: show only if nothing newer started or loaded
        KillTimer(m_hWnd, 2030); auto pend = m_errPend; m_errPend.clear();
        for (auto& e : pend) { if (IndexOf(e.t) < 0 || e.t->navId != e.nav || e.t->loading) continue; ShowError(e.t, e.st); }
        return;
    }
    if (id == 2012) {   // toast slide in / out
        if (!m_hPw || !(m_pwKind == 3 || m_pwKind == 6)) { KillTimer(m_hWnd, 2012); return; }
        if (m_pwAnim == 1) { m_pwSl -= std::max(2, m_pwSl / 5); if (m_pwSl <= 0) { m_pwSl = 0; m_pwAnim = 0; KillTimer(m_hWnd, 2012); } }
        else if (m_pwAnim == 2) { m_pwSl += std::max(4, (m_pwW - m_pwSl) / 4); if (m_pwSl >= m_pwW) { m_pwAnim = 3; HidePw(); return; } }
        else { KillTimer(m_hWnd, 2012); return; }
        SetWindowRgn(m_hPw, CreateRoundRectRgn(m_pwSl, 0, m_pwW + 1, m_pwH + 1, MulDiv(24, m_dpi, 96), MulDiv(24, m_dpi, 96)), TRUE);
        InvalidateRect(m_hPw, nullptr, FALSE); return;
    }
    if (id == 2006) { ApplyMemoryLevels(); return; }
    if (id == 2041) { SgTick(); return; }
    if (id == 2042) { KillTimer(m_hWnd, 2042); SgClick(m_sgTestClick); return; }
    if (id == 2073) { KillTimer(m_hWnd, 2073); OpenReader(); return; }   // test hook only (LIBRA_TEST_READER)
    if (id == 2040) { wchar_t sn[8]; SetTimer(m_hWnd, 2040, TestEnv(L"LIBRA_TEST_SNNOW", sn, 8) ? 3000 : 30 * 60 * 1000, nullptr); SponsorKick(); SnKick(); SnSave(); UpKick(); return; }   // every 30 min: both checks return at once unless due
    if (id == 2010) { KillTimer(m_hWnd, 2010); FetchWebSug(m_sugQ); return; }
    if (id == 2020) { KillTimer(m_hWnd, 2020); AgGoNow(); return; }
    if (id == 2021) { KillTimer(m_hWnd, 2021); AgClickNow(); return; }
    if (id == 2022) { KillTimer(m_hWnd, 2022); AgClickCheck(); return; }
    if (id != TIMER_PROGRESS) return;

    // Hover tracking for the owner-draw toolbar buttons
    {
        POINT pt; GetCursorPos(&pt);
        HWND under = nullptr;
        for (HWND b : { m_hBtnBack, m_hBtnFwd, m_hBtnRefresh, m_hBtnShield, m_hBtnAgent, m_hBtnShot, m_hBtnMenu }) {
            RECT br; GetWindowRect(b, &br);
            if (PtInRect(&br, pt)) { under = b; break; }
        }
        if (under != m_hoverBtn) {
            HWND old = m_hoverBtn;
            m_hoverBtn = under;
            if (old)   InvalidateRect(old,   nullptr, FALSE);
            if (under) InvalidateRect(under, nullptr, FALSE);
        }
    }

    {   // per-tab load fill: eases toward ~92% while loading, snaps to full and fades when done
        bool anim = false;
        for (auto& tb : m_tabs) {
            if (tb->loading && tb->url.rfind(L"libra://", 0) != 0) { tb->shown += (0.92f - tb->shown) * 0.035f; if (tb->shown < 0.04f) tb->shown = 0.04f; anim = true; }
            else if (tb->shown > 0.f && tb->url.rfind(L"libra://", 0) != 0) { tb->shown += (1.f - tb->shown) * 0.35f; if (tb->shown > 0.99f) tb->shown = 0.f; anim = true; }
        }
        if (anim) { RECT c0; GetClientRect(m_hWnd, &c0); RECT tr0 = { 0, 0, c0.right, MulDiv(Layout::TabBarHeight, m_dpi, 96) }; InvalidateRect(m_hWnd, &tr0, FALSE); }
    }
    {   // every ~2 s: put long-hidden tabs to sleep (memory + CPU), never ones that are loading or playing sound
        static int tick = 0;
        if (++tick % 125 == 0) {
            { static int c2 = 0; if (++c2 % 15 == 0) { if (m_totalDirty) { m_set.Set(L"blocked_total", std::to_wstring(m_totalBlocked)); m_totalDirty = false; } if (m_statDirty) StatSave(); } }   // saved every ~30 s
            DWORD now = GetTickCount(), limit = m_lowEnd ? 6000 : 12000; if (m_tabs.size() > 8) limit /= 2;   // many tabs: sleep background tabs quickly
            if (!m_incognito && !m_setup && m_set.Get(L"idle_prompt", L"1") != L"0" && GetForegroundWindow() == m_hWnd && !(m_hPw && IsWindowVisible(m_hPw))) {   // ask once about a tab nobody has used for a long time
                DWORD idleMs = (DWORD)(_wtof(m_set.Get(L"idle_min", L"20").c_str()) * 60000.0);
                for (auto& tb : m_tabs) {
                    Tab* t = tb.get(); BOOL audio = FALSE; wil::com_ptr<ICoreWebView2_8> w8;
                    if (!t->hiddenSince || t->idleAsked || t->pinned || t->loading || !t->webView || !IsHttp(t->url) || now - t->hiddenSince < idleMs) continue;
                    if (SUCCEEDED(t->webView->QueryInterface(IID_PPV_ARGS(&w8)))) w8->get_IsDocumentPlayingAudio(&audio);
                    if (audio) continue;
                    t->idleAsked = true; ShowPw(5, t, HostOf(t->url)); break;
                }
            }
            for (auto& tb : m_tabs) {
                Tab* t = tb.get();
                if (!t->hiddenSince || t->suspended || !t->webView || t->loading || now - t->hiddenSince < limit) continue;
                wil::com_ptr<ICoreWebView2_3> w3; wil::com_ptr<ICoreWebView2_8> w8; BOOL audio = FALSE;
                if (FAILED(t->webView->QueryInterface(IID_PPV_ARGS(&w3)))) continue;
                if (SUCCEEDED(t->webView->QueryInterface(IID_PPV_ARGS(&w8)))) w8->get_IsDocumentPlayingAudio(&audio);
                if (audio) continue;
                w3->TrySuspend(Microsoft::WRL::Callback<ICoreWebView2TrySuspendCompletedHandler>(
                    [this, t](HRESULT, BOOL ok) -> HRESULT {
                        if (IndexOf(t) < 0 || !ok) return S_OK;
                        if (t->hiddenSince) t->suspended = true; else { t->suspended = true; ResumeTab(t); }   // shown again meanwhile: wake it
                        return S_OK;
                    }).Get());
            }
        }
    }
    if (!m_progressVisible) { m_displayProgress = 0.f; return; }

    // Animate progress bar toward the tab's actual progress
    float target = (m_activeTab >= 0 && m_activeTab < (int)m_tabs.size())
                   ? m_tabs[m_activeTab]->progress : 0.f;
    float loading = (m_activeTab >= 0 && m_activeTab < (int)m_tabs.size())
                    ? (m_tabs[m_activeTab]->loading ? 1.f : 0.f) : 0.f;

    if (!m_tabs.empty() && m_activeTab >= 0 && !m_tabs[m_activeTab]->loading && m_displayProgress >= 0.98f) {
        m_progressVisible = false;
        m_displayProgress = 0.f;
    } else {
        // Ease toward target
        float diff = target - m_displayProgress;
        m_displayProgress += diff * 0.08f;
        if (m_displayProgress > 0.95f && loading) m_displayProgress = 0.95f; // stall near end
    }

    // Only invalidate the toolbar strip to avoid repainting WebView area
    RECT cr; GetClientRect(m_hWnd, &cr);
    int tabH  = MulDiv(Layout::TabBarHeight,  m_dpi, 96);
    int toolH = MulDiv(Layout::ToolbarHeight, m_dpi, 96);
    RECT toolRect = { 0, tabH, cr.right, tabH + toolH };
    InvalidateRect(m_hWnd, &toolRect, FALSE);
}

void BrowserWindow::OnClose() {
    HoldSandboxCleanup(); SnSave();
    Verdicts::Get().Save();
    { wchar_t tf[MAX_PATH]; if (TestEnv(L"LIBRA_TEST_VLOG", tf, MAX_PATH)) { std::string o = "local=" + std::to_string(Verdicts::Get().localHits.load()) + " cloud=" + std::to_string(Verdicts::Get().cloudAsks.load()) + " table=" + std::to_string(Verdicts::Get().Count()) + "\n"; WriteAllRaw(tf, o, true); } }   // test hook only
    PwClipClear();
    if (m_statDirty) StatSave();
    if (m_totalDirty && !m_incognito) { m_set.Set(L"blocked_total", std::to_wstring(m_totalBlocked)); m_totalDirty = false; }
    if (!m_incognito) {
        SavePinned();
        std::string o; int n = 0;
        for (auto& tb : m_tabs) if (!tb->incognito && !tb->pinned && IsHttp(tb->url) && n++ < 20) o += ToUtf8(Flat(tb->url)) + "\n";
        WriteAll(DataDir() + L"\\session.txt", o);
    }
    for (auto& tab : m_tabs) {
        if (tab->webView) {
            tab->webView->remove_NavigationStarting(tab->tokNavStart);
            tab->webView->remove_NavigationCompleted(tab->tokNavComplete);
            tab->webView->remove_DocumentTitleChanged(tab->tokDocTitleChange);
            tab->webView->remove_WebResourceRequested(tab->tokResReq);
            tab->webView->remove_NewWindowRequested(tab->tokNewWindow);
        }
        if (tab->controller) tab->controller->Close();
    }
    m_tabs.clear();
    if (m_incognito && !m_privDir.empty()) {   // wipe the private profile once the engine has let go (hidden helper, waits a few seconds)
        std::wstring cmd = L"cmd.exe /c ping -n 8 127.0.0.1 >nul & rd /s /q \"" + m_privDir + L"\"";
        STARTUPINFOW si{ sizeof(si) }; PROCESS_INFORMATION pi{};
        if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
    }
    DestroyWindow(m_hWnd);
}

void BrowserWindow::NewPrivateWindow() {   // like other browsers: a separate dark window, own profile, nothing saved
    if (m_child) return;   // Child Safety: no private windows
    if (m_incognito) { OpenTab(L""); return; }   // already private: just a new tab in it
    wchar_t exe[MAX_PATH]; GetModuleFileNameW(nullptr, exe, MAX_PATH);
    std::wstring cmd = L"\"" + std::wstring(exe) + L"\" --incognito";
    STARTUPINFOW si{ sizeof(si) }; PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
}


// === Phase 1: data, internal pages, downloads, shortcuts ==============================
void BrowserWindow::LoadData() {
    m_set.Load();
    { std::wstring en = m_set.Get(L"engine"); if (en == L"Phind" || en == L"Claude" || en == L"Copilot" || en == L"You.com" || en == L"Qwant" || en == L"ChatGPT" || en == L"Grok") m_set.Set(L"engine", L"Google"); }   // removed engines (shut down / need sign-in)
    m_adBlock = m_set.On(L"local_block"); ChildLoad();
    m_imagesOff = m_set.On(L"block_images");
    m_totalBlocked = (unsigned long long)_wtoi64(m_set.Get(L"blocked_total", L"0").c_str());
    for (auto& l : Lines(ReadAll(DataDir() + L"\\sitemodes.txt"))) { auto f = SplitTab(l); if (f.size() == 2 && f[0].size() < 120) { int m = _wtoi(f[1].c_str()); if (m >= 0 && m <= 2) m_modes[f[0]] = m; } }
    for (auto& l : Lines(ReadAll(DataDir() + L"\\keepid.txt"))) if (!l.empty() && l.size() < 120) m_keepId.insert(l);
    for (auto& l : Lines(ReadAll(DataDir() + L"\\history.txt"))) {
        auto f = SplitTab(l);
        if (f.size() >= 2) m_hist.push_back({ _wtoi64(f[0].c_str()), f[1], f.size() > 2 ? f[2] : L"" });
    }
    if (m_hist.size() > 5000) m_hist.erase(m_hist.begin(), m_hist.end() - 5000);
    for (auto& l : Lines(ReadAll(DataDir() + L"\\bookmarks.txt"))) {
        auto f = SplitTab(l);
        if (!f.empty() && IsHttp(f[0])) m_bm.push_back({ f[0], f.size() > 1 ? f[1] : L"" });
    }
}

void BrowserWindow::SaveHistory() {
    std::string o;
    for (auto& h : m_hist) o += ToUtf8(std::to_wstring(h.t) + L"\t" + Flat(h.url) + L"\t" + Flat(h.title)) + "\n";
    WriteAll(DataDir() + L"\\history.txt", o);
}
void BrowserWindow::SaveBookmarks() {
    std::string o;
    for (auto& b : m_bm) o += ToUtf8(Flat(b.url) + L"\t" + Flat(b.title)) + "\n";
    WriteAll(DataDir() + L"\\bookmarks.txt", o);
}

void BrowserWindow::RecordHistory(const std::wstring& url, const std::wstring& title) {
    if (!IsHttp(url)) return;
    if (!m_hist.empty() && m_hist.back().url == url) { m_hist.back().title = Flat(title); return; }   // reload: no duplicate
    m_hist.push_back({ (long long)time(nullptr), url, Flat(title) });
    if (m_hist.size() > 6000) { m_hist.erase(m_hist.begin(), m_hist.begin() + 1000); SaveHistory(); return; }
    WriteAll(DataDir() + L"\\history.txt", ToUtf8(std::to_wstring(m_hist.back().t) + L"\t" + Flat(url) + L"\t" + Flat(title)) + "\n", true);
}

bool BrowserWindow::IsBookmarked(const std::wstring& url) const {
    return std::any_of(m_bm.begin(), m_bm.end(), [&](auto& b) { return b.url == url; });
}
void BrowserWindow::ToggleBookmark() {
    if (m_activeTab < 0 || m_activeTab >= (int)m_tabs.size()) return;
    Tab* t = m_tabs[m_activeTab].get();
    if (!IsHttp(t->url)) return;
    auto it = std::find_if(m_bm.begin(), m_bm.end(), [&](auto& b) { return b.url == t->url; });
    if (it != m_bm.end()) m_bm.erase(it); else m_bm.push_back({ t->url, t->title });
    SaveBookmarks();
    InvalidateRect(m_hWnd, nullptr, FALSE);
    for (int i = 0; i < (int)m_tabs.size(); ++i) if (m_tabs[i]->url == L"libra://bookmarks") RenderTab(i);
}
RECT BrowserWindow::StarRect() const {
    RECT p = OmniPillRect();
    return { p.right - MulDiv(38, m_dpi, 96), p.top, p.right - MulDiv(8, m_dpi, 96), p.bottom };
}

std::wstring BrowserWindow::PageHtml(const std::wstring& url) {
    bool g = m_incognito;
    if (url == L"libra://tabs") {
        std::vector<TabInfo> v;
        for (int i = 0; i < (int)m_tabs.size(); ++i) {
            auto& t = m_tabs[i];
            v.push_back({ t->title, t->url == L"libra://error" ? t->errUrl : t->url, t->pinned, t->loading, i == m_activeTab });
        }
        return TabsPage(v, g);
    }
    if (url == L"libra://settings")  { long long lo = DohProxy::Get().LastOk(), n = (long long)time(nullptr); return SettingsPage(m_set, g, lo ? (n - lo < 900 ? L"Online" : L"Idle") : L"Ready", (long long)m_totalBlocked); }
    if (url == L"libra://history")   return HistoryPage(m_hist, g);
    if (url == L"libra://passwords") { PwLoad(); return PasswordsPage(m_vault, g); }
    if (url == L"libra://bookmarks") return BookmarksPage(m_bm, g);
    if (url == L"libra://downloads") { SyncJobs(); return DownloadsPage(m_dl, g); }
    if (url == L"libra://welcome")   return WelcomePage(g, IsDefaultBrowser(), m_set.Get(L"setup_step") == L"default", ImFound());
    if (url == L"libra://reader")    return ReaderPage(m_readerRaw, g);
    if (url == L"libra://pdfsign")   return PdfSignPage(g);
    if (url == L"libra://privacy")   return PrivacyPage(g, IsDefaultBrowser(), m_set);
    if (url == L"libra://agent")     return AgentPage(g);
    return GetHomepageHtml();
}
void BrowserWindow::RenderTab(int idx) {
    if (idx < 0 || idx >= (int)m_tabs.size()) return;
    Tab* t = m_tabs[idx].get();
    if (t->webView && t->url == L"libra://error") { t->webView->NavigateToString(ErrorPage(t->errKind, HostOf(t->errUrl), t->errUrl, m_incognito).c_str()); return; }
    if (t->webView && t->url.rfind(L"libra://", 0) == 0) {
        bool same = t->renderedUrl == t->url; t->renderedUrl = t->url;
        auto show = [this, t](int y, std::wstring tab) {
            std::wstring h = PageHtml(t->url);
            for (auto& c : tab) if (!iswalnum(c) && c != L' ') c = L'_';
            if (!tab.empty()) { size_t p0 = h.find(L"<script>"); if (p0 != std::wstring::npos) h.insert(p0, L"<script>window.__tab0=\"" + tab + L"\"</script>"); }
            if (y > 0) { std::wstring js = L"<script>scrollTo(0," + std::to_wstring(y) + L")</script>"; size_t p = h.rfind(L"</body>"); if (p != std::wstring::npos) h.insert(p, js); else h += js; }
            t->webView->NavigateToString(h.c_str());
        };
        if (!same) { show(0, L""); return; }
        t->webView->ExecuteScript(L"Math.round(window.scrollY||0)+'|'+(window.__tab||'')", Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [this, t, show](HRESULT, LPCWSTR r) -> HRESULT { if (IndexOf(t) >= 0 && t->webView) { std::wstring v = r ? r : L""; v.erase(std::remove(v.begin(), v.end(), L'"'), v.end()); size_t bar = v.find(L'|'); show(_wtoi(v.c_str()), bar == std::wstring::npos ? L"" : v.substr(bar + 1)); } return S_OK; }).Get());
    }
}
void BrowserWindow::ShowInternal(const std::wstring& url, bool activate) {
    for (int i = 0; i < (int)m_tabs.size(); ++i)
        if (m_tabs[i]->url == url) { RenderTab(i); if (activate) SwitchTab(i); return; }
    int prev = m_activeTab;
    OpenTab(url);
    if (!activate && prev >= 0) SwitchTab(prev);
}

static void CALLBACK LbMenuEvt(HWINEVENTHOOK, DWORD, HWND h, LONG, LONG, DWORD, DWORD) {   // menus without the system drop shadow (it does not match our flat look)
    char cls[16] = {}; if (!h || !GetClassNameA(h, cls, 16) || strcmp(cls, "#32768") != 0) return;
    LONG_PTR st = GetClassLongPtr(h, GCL_STYLE); if (st & CS_DROPSHADOW) { SetClassLongPtr(h, GCL_STYLE, st & ~(LONG_PTR)CS_DROPSHADOW); ShowWindow(h, SW_HIDE); ShowWindow(h, SW_SHOWNA); }
}
void BrowserWindow::StartupTabs() {
    static HWINEVENTHOOK s_mh = SetWinEventHook(EVENT_SYSTEM_MENUPOPUPSTART, EVENT_SYSTEM_MENUPOPUPSTART, nullptr, LbMenuEvt, GetCurrentProcessId(), 0, WINEVENT_OUTOFCONTEXT); (void)s_mh;
    { std::random_device rd; m_fpSeed = rd(); }   // new fingerprint noise every launch
    { wchar_t rv[8]; if (TestEnv(L"LIBRA_TEST_READER", rv, 8)) SetTimer(m_hWnd, 2073, 12000, nullptr); }
    SetTimer(m_hWnd, 2031, 300000, nullptr);   // new day -> new fingerprint identity
    SetTimer(m_hWnd, 2006, 4000, nullptr);   // background tabs keep a small memory footprint
    UpLoad(); SponsorLoad(); { wchar_t sf[8]; SetTimer(m_hWnd, 2040, TestEnv(L"LIBRA_TEST_SPFEED", sf, 8) ? 1500 : 30000, nullptr); }   // sponsored feed: cached copy now, a daily fetch soon after start
    int np = 0;
    if (!m_incognito)
        for (auto& u : Lines(ReadAll(DataDir() + L"\\pinned.txt")))
            if (IsHttp(u) && np < 12) { OpenTab(u); m_tabs.back()->pinned = true; ++np; }
    if (!m_set.On(L"welcomed")) { m_setup = true; for (HWND w : { m_hOmnibox, m_hBtnBack, m_hBtnFwd, m_hBtnRefresh, m_hBtnShield, m_hBtnAgent, m_hBtnShot, m_hBtnMenu }) ShowWindow(w, SW_HIDE); OpenTab(L"libra://welcome"); ResizeWebViews(); return; }
    std::wstring st = m_set.Get(L"startup", L"home"), hp = m_set.Get(L"homepage");
    if (st == L"url" && IsHttp(hp)) { OpenTab(hp); return; }
    if (st == L"last" && !m_incognito) {
        int n = 0;
        for (auto& u : Lines(ReadAll(DataDir() + L"\\session.txt"))) if (IsHttp(u) && n++ < 20) OpenTab(u);
        if (n) { m_activeTab = np; return; }
    }
    { wchar_t to[32]; if (TestEnv(L"LIBRA_TEST_OPEN", to, 32) && wcsncmp(to, L"settings", 8) == 0) { OpenTab(L"libra://settings"); return; }
      if (TestEnv(L"LIBRA_TEST_OPEN", to, 32) && wcscmp(to, L"history") == 0) OpenTab(L"libra://history");
      if (TestEnv(L"LIBRA_TEST_OPEN", to, 32) && wcscmp(to, L"privacy") == 0) OpenTab(L"libra://privacy");
      if (TestEnv(L"LIBRA_TEST_OPEN", to, 32) && wcscmp(to, L"bookmarks") == 0) OpenTab(L"libra://bookmarks");
      if (TestEnv(L"LIBRA_TEST_OPEN", to, 32) && wcscmp(to, L"pdfsign") == 0) OpenTab(L"libra://pdfsign");
      if (TestEnv(L"LIBRA_TEST_OPEN", to, 32) && wcscmp(to, L"downloads") == 0) OpenTab(L"libra://downloads");
      if (TestEnv(L"LIBRA_TEST_OPEN", to, 32) && wcscmp(to, L"passwords") == 0) OpenTab(L"libra://passwords");
      if (TestEnv(L"LIBRA_TEST_OPEN", to, 32) && wcscmp(to, L"burn") == 0) SetTimer(m_hWnd, 2072, 7000, nullptr);
      { wchar_t sv[64]; if (TestEnv(L"LIBRA_TEST_SHOT", sv, 64) > 0) SetTimer(m_hWnd, 2043, 11000, nullptr); } }   // test hook only
    OpenTab(L"libra://home");
}

bool BrowserWindow::HandleKey(UINT vk) {
    if (ShotKey(vk)) return true;   // the screenshot bar owns Esc / Enter / arrows while it is open
    if (m_setup) return false;   // no shortcuts until the first-run choice is made
    if (vk == VK_F11) { SetFullscreen(!m_full); return true; }
    if (vk == VK_F9) { OpenReader(); return true; }   // reader mode
    if (vk == VK_ESCAPE && m_full) { SetFullscreen(false); return true; }
    if (!(GetKeyState(VK_CONTROL) < 0 || (GetAsyncKeyState(VK_CONTROL) & 0x8000)) || GetKeyState(VK_MENU) < 0) return false;   // either view of the keyboard: the engine hands keys over from its own input thread   // async: the engine delivers keys from its own input thread, GetKeyState is stale there
    bool shift = GetKeyState(VK_SHIFT) < 0 || (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    switch (vk) {
    case 'T':
        if (!shift) OpenTab(L"");
        else if (!m_closed.empty()) { std::wstring u = m_closed.back(); m_closed.pop_back(); OpenTab(u); }
        return true;
    case 'W': if (m_activeTab >= 0) CloseTab(m_activeTab); return true;
    case 'N': if (!shift) return false; NewPrivateWindow(); return true;
    case 'S': if (!shift) return false; OpenShot(); return true;
    case 'F': if (!shift) return false; ShowInternal(L"libra://pdfsign"); return true;   // Ctrl+F find, Ctrl+Shift+F sign a PDF   // fill and sign a PDF
    case 'H': ShowInternal(L"libra://history");   return true;
    case 'J': ShowInternal(L"libra://downloads"); return true;
    case 'D': ToggleBookmark(); return true;
    case 'A': if (!shift) return false; ShowInternal(L"libra://tabs"); return true;
    case 'P': if (!shift) { if (m_activeTab < 0 || !m_tabs[m_activeTab]->webView) return false; wil::com_ptr<ICoreWebView2_16> w16; if (SUCCEEDED(m_tabs[m_activeTab]->webView->QueryInterface(IID_PPV_ARGS(&w16)))) { w16->ShowPrintUI(COREWEBVIEW2_PRINT_DIALOG_KIND_BROWSER); return true; } return false; } TogglePin(m_activeTab); return true;   // Ctrl+P = the engine's print preview, asked for directly; Ctrl+Shift+P = pin
    case 'O': if (!shift) return false; ShowInternal(L"libra://bookmarks"); return true;
    case VK_SPACE: if (!shift) return false; ToggleAgent(); return true;   // agent sidebar
    case 'L': SetFocus(m_hOmnibox); SendMessage(m_hOmnibox, EM_SETSEL, 0, -1); return true;
    case VK_TAB: {
        int n = (int)m_tabs.size();
        if (n > 1) SwitchTab((m_activeTab + (shift ? n - 1 : 1)) % n);
        return true;
    }
    }
    return false;
}

void BrowserWindow::ClearData() {
    m_hist.clear(); SaveHistory();
    if (m_activeTab < 0 || m_activeTab >= (int)m_tabs.size() || !m_tabs[m_activeTab]->webView) return;
    wil::com_ptr<ICoreWebView2_13> w13; wil::com_ptr<ICoreWebView2Profile> prof; wil::com_ptr<ICoreWebView2Profile2> p2;
    if (SUCCEEDED(m_tabs[m_activeTab]->webView->QueryInterface(IID_PPV_ARGS(&w13))) && SUCCEEDED(w13->get_Profile(&prof)) &&
        SUCCEEDED(prof->QueryInterface(IID_PPV_ARGS(&p2))))
        p2->ClearBrowsingDataAll(Microsoft::WRL::Callback<ICoreWebView2ClearBrowsingDataCompletedHandler>(
            [](HRESULT) -> HRESULT { return S_OK; }).Get());
}

static bool RiskyFile(const std::wstring& p) {   // never auto-run things that execute code
    size_t d = p.find_last_of(L'.'); if (d == std::wstring::npos) return false;
    std::wstring e = p.substr(d + 1); for (auto& c : e) c = (wchar_t)towlower(c);
    for (const wchar_t* x : { L"exe", L"msi", L"bat", L"cmd", L"ps1", L"vbs", L"vbe", L"js", L"jse", L"wsf", L"scr", L"lnk", L"com", L"jar", L"hta", L"reg", L"cpl", L"dll", L"msix", L"appx", L"url", L"pif", L"application", L"msc", L"inf", L"chm", L"wsh", L"msp", L"mst", L"xll", L"docm", L"xlsm", L"pptm", L"iso", L"img", L"vhd", L"vhdx", L"psm1", L"gadget", L"appref-ms", L"settingcontent-ms", L"library-ms", L"search-ms" })
        if (e == x) return true;
    return false;
}
static std::wstring SafeName(std::wstring n) {   // a name can never carry a path, a stream (:), or a Windows device name
    for (auto& c : n) if (c < 32 || wcschr(L"<>:\"/\\|?*", c)) c = L'_';
    while (!n.empty() && (n.back() == L'.' || n.back() == L' ')) n.pop_back();
    if (n.size() > 150) n.resize(150);
    if (n.empty()) return L"download";
    std::wstring stem = n.substr(0, n.find(L'.')); for (auto& c : stem) c = (wchar_t)towupper(c);
    for (const wchar_t* d : { L"CON", L"PRN", L"AUX", L"NUL", L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7", L"COM8", L"COM9", L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9" })
        if (stem == d) return L"_" + n;
    return n;
}
static std::wstring UniquePath(const std::wstring& path) {
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return path;
    size_t dot = path.find_last_of(L'.'), sl = path.find_last_of(L"\\/");
    if (dot == std::wstring::npos || (sl != std::wstring::npos && dot < sl)) dot = path.size();
    for (int n = 1; n < 1000; ++n) {
        std::wstring q = path.substr(0, dot) + L" (" + std::to_wstring(n) + L")" + path.substr(dot);
        if (GetFileAttributesW(q.c_str()) == INVALID_FILE_ATTRIBUTES) return q;
    }
    return path;
}

void BrowserWindow::SyncDownload(ICoreWebView2DownloadOperation* op, bool render) {
    for (auto& d : m_dl) {
        if (d.op.get() != op) continue;
        INT64 got = 0, total = 0; op->get_BytesReceived(&got); op->get_TotalBytesToReceive(&total);
        d.got = got; d.total = total;
        wil::unique_cotaskmem_string rp; op->get_ResultFilePath(&rp);
        if (rp.get() && *rp.get()) { d.path = rp.get(); size_t sl = d.path.find_last_of(L"\\/"); d.name = sl == std::wstring::npos ? d.path : d.path.substr(sl + 1); }
        COREWEBVIEW2_DOWNLOAD_STATE st; op->get_State(&st);
        d.state = st == COREWEBVIEW2_DOWNLOAD_STATE_COMPLETED ? 1 : st == COREWEBVIEW2_DOWNLOAD_STATE_INTERRUPTED ? 2 : 0;
        if (d.state == 0 && total > 0 && got >= total) d.state = 1;
        break;
    }
    if (render) for (int i = 0; i < (int)m_tabs.size(); ++i) if (m_tabs[i]->url == L"libra://downloads") RenderTab(i);
}

// Normal (single-stream) WebView2 download: used for blob:/data: URLs, small files, servers without ranges.
void BrowserWindow::AdoptDownload(ICoreWebView2DownloadStartingEventArgs* a, ICoreWebView2DownloadOperation* op, const std::wstring& nameIn, const std::wstring& chosen) {
    std::wstring name = SafeName(nameIn);
    { a->put_ResultFilePath(chosen.c_str()); a->put_Handled(TRUE); }   // always the path the user chose in the Save window
    DownloadItem it; it.op = op; m_dl.push_back(it);
    SetTimer(m_hWnd, 2005, 250, nullptr);   // live numbers without re-rendering the page
    EventRegistrationToken t1, t2;
    op->add_BytesReceivedChanged(Microsoft::WRL::Callback<ICoreWebView2BytesReceivedChangedEventHandler>(
        [this, op](ICoreWebView2DownloadOperation*, IUnknown*) -> HRESULT { SyncDownload(op, false); return S_OK; }).Get(), &t1);
    op->add_StateChanged(Microsoft::WRL::Callback<ICoreWebView2StateChangedEventHandler>(
        [this, op](ICoreWebView2DownloadOperation*, IUnknown*) -> HRESULT { SyncDownload(op, true); return S_OK; }).Get(), &t2);
    SyncDownload(op, false);
    ShowInternal(L"libra://downloads", false);   // background tab shows progress
}

void BrowserWindow::AttachDownloads(ICoreWebView2* wv) {
    wil::com_ptr<ICoreWebView2_4> w4;
    if (FAILED(wv->QueryInterface(IID_PPV_ARGS(&w4)))) return;
    EventRegistrationToken tok;
    w4->add_DownloadStarting(Microsoft::WRL::Callback<ICoreWebView2DownloadStartingEventHandler>(
        [this](ICoreWebView2* sender, ICoreWebView2DownloadStartingEventArgs* a) -> HRESULT {
            wil::com_ptr<ICoreWebView2DownloadOperation> op; a->get_DownloadOperation(&op);
            if (!op) return S_OK;
            wil::unique_cotaskmem_string rp; a->get_ResultFilePath(&rp);
            std::wstring name = rp.get() ? rp.get() : L"";
            size_t sl = name.find_last_of(L"\\/"); if (sl != std::wstring::npos) name = name.substr(sl + 1);
            wil::unique_cotaskmem_string uri; op->get_Uri(&uri);
            {   // which tab started this download? (compare COM identity, not interface pointers)
                wil::com_ptr<IUnknown> su; sender->QueryInterface(IID_PPV_ARGS(&su));
                for (auto& tb : m_tabs) {
                    if (!tb->webView) continue;
                    wil::com_ptr<IUnknown> tu; tb->webView->QueryInterface(IID_PPV_ARGS(&tu));
                    if (tu.get() != su.get()) continue;
                    tb->dlTick = GetTickCount();
                    // drive-by guard: a download after a script-started navigation (nothing typed) must be backed by a real click on the page
                    { std::wstring du = uri.get() ? uri.get() : L"";
                      if (!tb->allowUri.empty() && tb->allowUri == du) tb->allowUri.clear();
                      else if (IsHttp(du) && !tb->navUser && GetTickCount() - tb->navTick < 10000 && GetTickCount() - m_typedTick > 10000) {
                        a->put_Cancel(TRUE);
                        Tab* tp = tb.get();
                        tb->webView->ExecuteScript(L"!!(navigator.userActivation&&navigator.userActivation.isActive)",
                            Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>([this, tp, du](HRESULT, LPCWSTR r) -> HRESULT {
                                if (IndexOf(tp) >= 0 && r && wcscmp(r, L"true") == 0) { tp->allowUri = du; tp->dlTick = GetTickCount(); tp->webView->Navigate(du.c_str()); }
                                return S_OK; }).Get());
                        return S_OK;
                      } }
                    if (tb->url == L"libra://error" && GetTickCount() - tb->errTick < 8000) {   // error page won the race: put the old page back
                        wil::unique_cotaskmem_string cur; tb->webView->get_Source(&cur);
                        std::wstring back = (cur.get() && IsHttp(cur.get())) ? std::wstring(cur.get()) : tb->prevUrl;
                        tb->dlTick = 0; tb->url = back;
                        if (IsHttp(back)) tb->webView->Navigate(back.c_str()); else RenderTab(IndexOf(tb.get()));
                    }
                }
            }
            // ask first: hold the download (deferral) until the user has chosen a place; nothing is fetched meanwhile
            wil::com_ptr<ICoreWebView2Deferral> def; a->GetDeferral(&def);
            wil::com_ptr<ICoreWebView2DownloadStartingEventArgs> args(a); wil::com_ptr<ICoreWebView2> snd(sender); std::wstring u0 = uri.get() ? uri.get() : L"";
            Tab* from = nullptr; { wil::com_ptr<IUnknown> su2; sender->QueryInterface(IID_PPV_ARGS(&su2));
                for (auto& tb : m_tabs) { if (!tb->webView) continue; wil::com_ptr<IUnknown> tu2; tb->webView->QueryInterface(IID_PPV_ARGS(&tu2)); if (tu2.get() == su2.get()) { from = tb.get(); break; } } }
            SaveGate(name, u0, from, [this, args, def, op, snd, name, u0](std::wstring chosen) { DlContinue(args, def, op, snd, name, u0, chosen); });
            return S_OK;
        }).Get(), &tok);
}

void BrowserWindow::OnProbeDone(std::shared_ptr<DlJob> job, bool ok) {
    if (ok) {   // a server must not be able to make us reserve more disk than exists
        ULARGE_INTEGER fb{}; std::wstring dir = DownloadsDir(m_set); { auto pp = m_pending.find(job.get()); if (pp != m_pending.end() && !pp->second.path.empty()) { size_t sl2 = pp->second.path.find_last_of(L"\/"); if (sl2 != std::wstring::npos) dir = pp->second.path.substr(0, sl2); } }
        if (GetDiskFreeSpaceExW(dir.c_str(), &fb, nullptr, nullptr) && (ULONGLONG)job->total + (64ull << 20) > fb.QuadPart) ok = false;
    }
    auto it = m_pending.find(job.get());
    if (it == m_pending.end()) return;
    PendingDl p = it->second; m_pending.erase(it);
    if (ok) {
        std::wstring path = p.path;   // the place the user chose
        job->path = path; job->part = path + L".libra-part";
        p.args->put_Cancel(TRUE);                                  // WebView2 stands down; our downloader takes over
        DownloadItem d; d.job = job; d.path = path; d.total = job->total;
        size_t sl = path.find_last_of(L"\\/"); d.name = sl == std::wstring::npos ? path : path.substr(sl + 1);
        m_dl.push_back(d);
        int conns = job->total > (40ll << 20) ? 8 : 4;
        dl::Start(job, conns);
        SetTimer(m_hWnd, 2005, 250, nullptr);
        ShowInternal(L"libra://downloads", false);
    } else {
        AdoptDownload(p.args.get(), p.op.get(), p.name, p.path);   // plain download
    }
    p.def->Complete();
}

void BrowserWindow::SyncJobs() {
    for (auto& d : m_dl) if (d.job) { d.got = d.job->got; d.total = d.job->total; d.state = d.job->state; d.canRetry = d.state == 2 && d.job->resumable && !d.job->cancel; }
}

void BrowserWindow::OnPageMessage(int idx, const std::wstring& msg) {
    auto v = SplitTab(msg);
    if (v.empty()) return;
    const std::wstring c = v[0], a = v.size() > 1 ? v[1] : L"", val = v.size() > 2 ? v[2] : L"";
    int i = _wtoi(a.c_str());

    if (c.rfind(L"pw.", 0) == 0) { OnPwPage(idx, v); return; }
    if (c.rfind(L"priv.", 0) == 0) { OnPrivPage(idx, v); return; }
    if (c.rfind(L"agent.", 0) == 0) { OnAgentPage(idx, v); return; }
    if (c == L"home.del") {
        m_sites.erase(std::remove_if(m_sites.begin(), m_sites.end(), [&](auto& s) { return s.first == a; }), m_sites.end());
        m_tileMeta.erase(a); SaveSites(); TilesSave(m_tileMeta); RenderTab(idx); return;
    }
    if (c == L"home.edit" && v.size() >= 4) {
        auto clean = [](std::wstring s) { std::wstring o; for (wchar_t ch : s) if (ch >= 32 && ch != 127) o += ch; while (!o.empty() && iswspace(o.back())) o.pop_back(); size_t k = 0; while (k < o.size() && iswspace(o[k])) ++k; return o.substr(k); };
        std::wstring name = clean(v[2]), url = clean(v[3]);
        if (name.size() > 40) name.resize(40);
        if (url.find(L"://") == std::wstring::npos) url = L"https://" + url;
        bool known = std::any_of(m_sites.begin(), m_sites.end(), [&](auto& s) { return s.first == a; });
        bool urlOk = false;
        if (url.size() <= 500 && IsHttp(url)) {   // host must be a real hostname, optional numeric port
            size_t p0 = url.find(L"://") + 3, e0 = url.find_first_of(L"/?#", p0);
            std::wstring hp = url.substr(p0, e0 == std::wstring::npos ? std::wstring::npos : e0 - p0); size_t col = hp.find(L':');
            bool portOk = col == std::wstring::npos || (hp.size() > col + 1 && std::all_of(hp.begin() + col + 1, hp.end(), [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; }));
            urlOk = portOk && SafeHost(hp.substr(0, col));
        }
        if (known && urlOk) { m_tileMeta[a] = { name, url }; TilesSave(m_tileMeta); }
        RenderTab(idx); return;
    }
    if (c == L"home.hint") { m_set.Set(L"first_hint", L"1"); return; }
    if (c == L"home.sug") { if (a.size() < 200) HomeSug(idx, a); return; }
    if (c == L"home.spshow" || c == L"home.spclick") { if (idx == m_activeTab && m_tabs[idx]->url == L"libra://home" && !IsIconic(m_hWnd)) SnEvent(c == L"home.spshow" ? L"show" : L"click", a); return; }   // only the visible home page counts
    if (c.rfind(L"pdf.", 0) == 0) { if (m_tabs[idx]->url == L"libra://pdfsign") OnPdf(idx, v); return; }
    if (c.rfind(L"bm.bk", 0) == 0) { if (m_tabs[idx]->url == L"libra://bookmarks") OnBackup(idx, v); return; }
    if (c.rfind(L"bm.imp", 0) == 0) { if (m_tabs[idx]->url == L"libra://bookmarks") OnImport(idx, c, a); return; }
    if (c == L"home.update") { if (m_tabs[idx]->url == L"libra://home") UpAct(a); return; }
    if (c == L"home.sphide") { SponsorHide(a); RenderTab(idx); return; }
    if (c == L"home.spoff") return;   // no global off switch any more
    if (c == L"default.set") { SetAsDefault(); return; }
    if (c == L"onb.imp") { if (m_setup && m_tabs[idx]->url == L"libra://welcome" && a != L"file") OnboardImport(idx, a); return; }
    if (c == L"default.chk") { if (m_setup && m_tabs[idx]->url == L"libra://welcome" && m_tabs[idx]->webView) m_tabs[idx]->webView->ExecuteScript(IsDefaultBrowser() ? L"window.lbDef&&lbDef(true)" : L"window.lbDef&&lbDef(false)", nullptr); return; }
    if (c == L"family.toggle") { AskWindows(m_child ? 2 : 1); return; }
    if (c == L"family.unlock") { if (m_child && idx == m_activeTab) { m_authHost = HostOf(m_tabs[idx]->errUrl); AskWindows(4); } return; }
    if (c == L"set") {
        static const wchar_t* keys[] = { L"engine", L"engine_url", L"startup", L"homepage", L"dl_ask", L"dl_dir", L"clear_start", L"local_block", L"block_images", L"perm_allow", L"smartscreen_off", L"perf", L"block_default", L"save_video", L"https_upgrade", L"fingerprint", L"block_3p", L"fp_a11y", L"tz_utc", L"keep_signin", L"agent_on", L"agent_btn", L"agent_opt", L"agent_vauto", L"sponsored", L"sn_count", L"autosuggest" };
        if (std::none_of(std::begin(keys), std::end(keys), [&](const wchar_t* k) { return a == k; })) return;
        if (val.size() > 500) return;
        if (a == L"engine") {
            bool known = val == L"Custom" || std::any_of(std::begin(kEngines), std::end(kEngines), [&](auto& e) { return val == e.name; });
            if (!known) return;
        }
        if (a == L"engine_url" && !val.empty() && val.rfind(L"https://", 0) != 0) return;
        if (a == L"homepage" && !val.empty() && !IsHttp(val)) return;
        if (a == L"startup" && val != L"home" && val != L"last" && val != L"url") return;
        if (a == L"perf" && val != L"auto" && val != L"light" && val != L"normal") return;
        if (a == L"block_default" && val != L"0" && val != L"1" && val != L"2") return;
        if (m_child && a == L"block_default" && val == L"2") return;   // Child Safety: protection cannot be switched off
        if (a == L"dl_dir" && !val.empty()) { DWORD at = GetFileAttributesW(val.c_str()); if (at == INVALID_FILE_ATTRIBUTES || !(at & FILE_ATTRIBUTE_DIRECTORY)) return; }
        m_set.Set(a, val);
        if (a == L"fingerprint" || a == L"fp_a11y" || a == L"tz_utc") for (auto& tb : m_tabs) InstallFpScript(tb.get());
        if (a == L"agent_on" && val != L"1") { AgStop(L"Agent is off."); if (AgDockIdx() >= 0) ToggleAgent(); }   // off = stopped and closed
        if (a == L"agent_btn") { if (m_hBtnAgent && !m_setup && !m_full) ShowWindow(m_hBtnAgent, AgBtnOn() ? SW_SHOW : SW_HIDE); RECT cr1; GetClientRect(m_hWnd, &cr1); LayoutToolbar(cr1.right); InvalidateRect(m_hWnd, nullptr, FALSE); }
        if (a == L"save_video") { RECT cr0; GetClientRect(m_hWnd, &cr0); LayoutToolbar(cr0.right); }
        if (a == L"save_video") for (auto& tb : m_tabs) { if (val == L"1") AttachVideoSniff(tb.get()); else DetachVideoSniff(tb.get()); }
        if (a == L"sponsored") { if (val == L"1") { SponsorLoad(); SponsorKick(); } else { m_sp = SpFeed(); } }
        if (a == L"autosuggest") SgSetting();
        if (a == L"local_block") m_adBlock = (val == L"1");
        if (a == L"block_images") m_imagesOff = (val == L"1");
        if (a == L"block_default") { UpdateBypass(); for (auto& tb : m_tabs) InstallAdScript(tb.get()); }
        return;
    }
    if (c == L"engine") {
        bool known = std::any_of(std::begin(kEngines), std::end(kEngines), [&](auto& e) { return a == e.name; });
        if (!known) return;
        m_set.Set(L"engine", a); m_set.Set(L"setup_step", L"default");   // a relaunch after an early close resumes at the default-browser step
        // the page now plays the "all set / built by" moment, then asks us to move on
        return;
    }
    if (c == L"welcome.done") {
        m_set.Set(L"setup_step", L"");
        m_set.Set(L"welcomed", L"1");   // setup is finished only here: closing earlier shows the whole setup again
        { std::wstring hd = DataDir() + L"\\hero"; DeleteFileW((hd + L"\\op.mp4").c_str()); DeleteFileW((hd + L"\\text.png").c_str()); DeleteFileW((hd + L"\\op2.mp4").c_str()); DeleteFileW((hd + L"\\op3.mp4").c_str()); RemoveDirectoryW(hd.c_str()); }
        if (m_setup) { m_setup = false; for (HWND w : { m_hOmnibox, m_hBtnBack, m_hBtnFwd, m_hBtnRefresh, m_hBtnShield, m_hBtnAgent, m_hBtnShot, m_hBtnMenu }) ShowWindow(w, SW_SHOW); if (!AgBtnOn()) ShowWindow(m_hBtnAgent, SW_HIDE); ResizeWebViews(); InvalidateRect(m_hWnd, nullptr, FALSE); }   // setup done: the toolbar appears
        if (m_tabs[idx]->url == L"libra://welcome") { m_tabs[idx]->url = L"libra://home"; RenderTab(idx); UpdateOmniboxText(L"libra://home"); }
        return;
    }
    if (c == L"nav") {
        if (a == L"agent") { ToggleAgent(); return; }
        for (const wchar_t* pg : { L"home", L"tabs", L"settings", L"history", L"bookmarks", L"downloads", L"passwords", L"privacy" })
            if (a == pg) { ShowInternal(L"libra://" + a); break; }
        return;
    }
    if (c == L"http.go") {   // the owner of this page chose to continue without https (this site, this session)
        Tab* t = m_tabs[idx].get();
        if (t->url != L"libra://error" || t->errKind != L"https" || !t->webView || t->errUrl.rfind(L"https://", 0) != 0) return;
        std::wstring u = L"http://" + t->errUrl.substr(8); m_httpOk.insert(HostOf(u)); m_httpOk.insert(RegDom(HostOf(u)));
        t->url = u; t->webView->Navigate(u.c_str()); UpdateOmniboxText(u);
        return;
    }
    if (c == L"retry" || c == L"proceed") {
        Tab* t = m_tabs[idx].get();
        if (t->url != L"libra://error" || !IsHttp(t->errUrl) || !t->webView) return;
        if (c == L"proceed") { if (t->errKind != L"cert") return; m_certAllow.insert(HostOf(t->errUrl)); }
        t->url = t->errUrl; t->webView->Navigate(t->url.c_str()); UpdateOmniboxText(t->url);
        return;
    }
    if (c == L"back") {
        Tab* t = m_tabs[idx].get(); BOOL cb = FALSE;
        if (t->webView) t->webView->get_CanGoBack(&cb);
        if (cb) t->webView->GoBack(); else { t->url = L"libra://home"; RenderTab(idx); UpdateOmniboxText(t->url); }
        return;
    }
    if (c == L"tab.go")    { SwitchTab(i); return; }
    if (c == L"tab.close") { CloseTab(i);  return; }
    if (c == L"tab.pin")   { TogglePin(i); return; }
    if (c == L"tabs.dups" || c == L"tabs.others") {
        Tab* self = m_tabs[idx].get();
        for (;;) {
            int victim = -1; std::set<std::wstring> seen;
            for (int k = 0; k < (int)m_tabs.size() && victim < 0; ++k) {
                Tab* tb = m_tabs[k].get(); if (tb == self) continue;
                if (c == L"tabs.others") { if (!tb->pinned) victim = k; }
                else { std::wstring u = tb->url == L"libra://error" ? tb->errUrl : tb->url;
                       for (int j = 0; j < k; ++j) if (m_tabs[j]->url == u && !u.empty()) { victim = k; break; } }
            }
            if (victim < 0) break;
            CloseTab(victim);
        }
        return;
    }
    if (c == L"newtab")  { OpenTab(L"");       return; }
    if (c == L"private") { NewPrivateWindow(); return; }
    if (c == L"clear") { WipeAll(idx); return; }
    if (c == L"hist.clear") { WipeAll(idx); return; }   // clearing history clears cookies, site data, cache and the rest too
    else if (c == L"hist.del") { if (i >= 0 && i < (int)m_hist.size()) { m_hist.erase(m_hist.begin() + i); SaveHistory(); } }
    else if (c == L"bm.del")   { if (i >= 0 && i < (int)m_bm.size())   { m_bm.erase(m_bm.begin() + i); SaveBookmarks(); InvalidateRect(m_hWnd, nullptr, FALSE); } }
    else if (c == L"dl.cancel") { if (i >= 0 && i < (int)m_dl.size()) { if (m_dl[i].op) m_dl[i].op->Cancel(); if (m_dl[i].job) m_dl[i].job->cancel = true; } return; }
    else if (c == L"dl.retry") {   // continue from where the connection broke
        if (i >= 0 && i < (int)m_dl.size() && m_dl[i].job && m_dl[i].canRetry) {
            auto j = m_dl[i].job; j->resumable = false; j->state = 0; m_dl[i].state = 0; m_dl[i].lastTick = 0; m_dl[i].speed = 0;
            dl::Start(j, j->conns, true); SetTimer(m_hWnd, 2005, 250, nullptr);
        }
    }
    else if (c == L"dl.remove") { if (i >= 0 && i < (int)m_dl.size() && m_dl[i].state != 0) { if (m_dl[i].job && m_dl[i].job->resumable) DeleteFileW(m_dl[i].job->part.c_str()); m_dl.erase(m_dl.begin() + i); } }
    else if (c == L"dl.release") {   // let a held file go: asks first (tests skip the question)
        if (i >= 0 && i < (int)m_dl.size() && m_dl[i].hold == 2) {
            wchar_t tp[8]; bool yes = TestEnv(L"LIBRA_TEST_SAVEPATH", tp, 8) > 0 ||
                MessageBoxW(m_hWnd, L"This file can run programs. Libra held it so nothing could start it by accident.\n\nRelease it only if you trust the site it came from.", L"Release this file?", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2) == IDYES;
            if (yes) HoldRelease(i);
        }
    }
    else if (c == L"dl.sandbox") { HoldSandbox(i); }
    else if (c == L"dl.delhold") { if (i >= 0 && i < (int)m_dl.size() && m_dl[i].hold == 2) { DeleteFileW(m_dl[i].path.c_str()); m_dl[i].state = 2; m_dl[i].canRetry = false; m_dl[i].hold = 0; } }
    else if (c == L"dl.open" || c == L"dl.show") {
        if (i < 0 || i >= (int)m_dl.size() || m_dl[i].path.empty()) return;
        const std::wstring& p = m_dl[i].path;
        if (c == L"dl.open" && m_dl[i].hold) return;   // a held file is never opened by Libra
        if (c == L"dl.open" && IsPdfPath(p) && m_dl[i].state == 1 && GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES) {   // read PDFs in Libra's own PDF reader, not an outside app
            std::wstring u = PdfFileUrl(p), lw = u; for (auto& ch : lw) ch = (wchar_t)towlower(ch); m_pdfAllow.insert(lw); OpenTab(u); return; }
        if (c == L"dl.open" && !RiskyFile(p)) ShellExecuteW(nullptr, L"open", p.c_str(), nullptr, nullptr, SW_SHOW);
        else ShellExecuteW(nullptr, L"open", L"explorer.exe", (L"/select,\"" + p + L"\"").c_str(), nullptr, SW_SHOW);
        return;
    }
    else if (c != L"dl.refresh") return;
    RenderTab(idx);
}


void BrowserWindow::ShowError(Tab* t, int status) {
    if (t->webView && (status == 9 || status == 10 || status == 14)) {   // "aborted": a raw video/audio file is aborted and re-requested by the media player
        wil::unique_cotaskmem_string src; t->webView->get_Source(&src);
        if (src.get() && t->url == src.get()) return;                     // ...and has really loaded. Other failures always get our error page.
    }
    if (t->dlTick && GetTickCount() - t->dlTick < 6000) {   // not a failure: the link turned out to be a file download
        t->dlTick = 0;
        wil::unique_cotaskmem_string cur; if (t->webView) t->webView->get_Source(&cur);   // the page that is still on screen
        if (cur.get() && IsHttp(cur.get())) t->url = cur.get(); else if (!t->prevUrl.empty()) t->url = t->prevUrl;
        if (IndexOf(t) == m_activeTab) UpdateOmniboxText(t->url);
        return;
    }
    std::wstring host = HostOf(t->url);
    std::string h8 = ToUtf8(host);
    std::wstring kind = L"conn";
    if (status >= 1 && status <= 5) kind = L"cert";
    else if (status == 7) kind = L"timeout";
    else if (status == 11) kind = L"offline";
    else if (status == 13) kind = L"dns";
    if (!t->upHost.empty() && t->upHost == host && GetTickCount() - t->upTick < 20000 && kind != L"dns") kind = L"https";   // the secure version did not load
    if (kind == L"conn" && !host.empty() && host.find_first_not_of(L"0123456789.") == std::wstring::npos && std::count(host.begin(), host.end(), L'.') == 3 && (m_child || ModeFor(host) == 0)) kind = L"ip";   // number address refused by the proxy
    else if (m_child && !h8.empty() && DohProxy::Get().FamilyWasBlocked(h8)) kind = L"family";
    else if (kind == L"conn" && !h8.empty() && (DohProxy::Get().DnsFailed(h8) || DohProxy::Get().IsBlocked(h8))) kind = L"dns";   // NXDOMAIN or server unreachable
    t->errUrl = t->url; t->errKind = kind; t->url = L"libra://error"; t->errTick = GetTickCount();
    if (t->webView) t->webView->NavigateToString(ErrorPage(kind, host, t->errUrl, m_incognito).c_str());
    for (int i = 0; i < (int)m_tabs.size(); ++i) if (m_tabs[i].get() == t && i == m_activeTab) UpdateOmniboxText(t->url);
}

void BrowserWindow::SweepBlocked(const std::string& host) {
    std::wstring js = L"window.__lb&&__lb('" + FromUtf8(host) + L"')";
    for (auto& t : m_tabs) if (t->webView) t->webView->ExecuteScript(js.c_str(), nullptr);
}


void BrowserWindow::SavePinned() {
    if (m_incognito) return;
    std::string o;
    for (auto& t : m_tabs) if (t->pinned && !t->incognito && IsHttp(t->url)) o += ToUtf8(Flat(t->url)) + "\n";
    WriteAll(DataDir() + L"\\pinned.txt", o);
}
void BrowserWindow::TogglePin(int idx) {
    if (idx < 0 || idx >= (int)m_tabs.size()) return;
    m_tabs[idx]->pinned = !m_tabs[idx]->pinned;   // shortcut: pinned tabs keep their position instead of moving to the front
    SavePinned(); InvalidateRect(m_hWnd, nullptr, FALSE); RefreshTabsPage();
}
void BrowserWindow::ShowTabMenu(int idx, POINT pt) {
    if (idx < 0 || idx >= (int)m_tabs.size()) return;
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, 1, m_tabs[idx]->pinned ? L"Unpin tab" : L"Pin tab");
    AppendMenuW(m, MF_STRING, 2, L"Duplicate tab");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, 3, L"Close other tabs");
    AppendMenuW(m, MF_STRING, 4, L"Close tab");
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hWnd, nullptr);
    DestroyMenu(m);
    Tab* keep = m_tabs[idx].get();
    if (cmd == 1) TogglePin(IndexOf(keep));
    else if (cmd == 2) OpenTab(keep->url == L"libra://error" ? keep->errUrl : keep->url, keep->incognito);
    else if (cmd == 3) {
        for (;;) {
            int other = -1;
            for (int i = 0; i < (int)m_tabs.size(); ++i) if (m_tabs[i].get() != keep && !m_tabs[i]->pinned) { other = i; break; }
            if (other < 0) break;
            CloseTab(other);
        }
    }
    else if (cmd == 4) CloseTab(IndexOf(keep));
}

// ---- F11 / page fullscreen -------------------------------------------------------------
void BrowserWindow::SetFullscreen(bool on) {
    if (on == m_full) return;
    HideSug();
    if (on) {
        m_style = GetWindowLong(m_hWnd, GWL_STYLE);
        GetWindowPlacement(m_hWnd, &m_wp);
        MONITORINFO mi{ sizeof(mi) };
        GetMonitorInfo(MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST), &mi);
        SetWindowLong(m_hWnd, GWL_STYLE, m_style & ~WS_OVERLAPPEDWINDOW);
        SetWindowPos(m_hWnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right - mi.rcMonitor.left,
                     mi.rcMonitor.bottom - mi.rcMonitor.top, SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    } else {
        SetWindowLong(m_hWnd, GWL_STYLE, m_style);
        SetWindowPlacement(m_hWnd, &m_wp);
        SetWindowPos(m_hWnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
    m_full = on;
    for (HWND h : { m_hOmnibox, m_hBtnBack, m_hBtnFwd, m_hBtnRefresh, m_hBtnShield, m_hBtnAgent, m_hBtnShot, m_hBtnMenu }) ShowWindow(h, on ? SW_HIDE : SW_SHOW);
    if (!on && !AgBtnOn()) ShowWindow(m_hBtnAgent, SW_HIDE);
    ResizeWebViews();
    InvalidateRect(m_hWnd, nullptr, FALSE);
}

// ---- address-bar suggestions (local only: bookmarks, then history) ----------------------
bool BrowserWindow::SugKey(UINT vk) {
    if (!m_hSug || !IsWindowVisible(m_hSug) || m_sugItems.empty()) return false;
    int n = (int)m_sugItems.size();
    if (vk == VK_DOWN)   { m_sugSel = (m_sugSel + 1) % n;                  InvalidateRect(m_hSug, nullptr, FALSE); return true; }
    if (vk == VK_UP)     { m_sugSel = m_sugSel <= 0 ? n - 1 : m_sugSel - 1; InvalidateRect(m_hSug, nullptr, FALSE); return true; }
    if (vk == VK_ESCAPE) { HideSug(); return true; }
    return false;
}
void BrowserWindow::HideSug() { m_sugSel = -1; if (m_hSug) ShowWindow(m_hSug, SW_HIDE); }

void BrowserWindow::UpdateSuggestions() {
    if (m_settingText || m_full || GetFocus() != m_hOmnibox) { HideSug(); return; }
    int len = GetWindowTextLengthW(m_hOmnibox);
    ++m_sugGen; KillTimer(m_hWnd, 2010);
    if (len < 2) { HideSug(); return; }                       // shortcut: waits for 2 characters
    std::wstring q(len + 1, L'\0'); GetWindowTextW(m_hOmnibox, q.data(), len + 1); q.resize(len);
    for (auto& c : q) c = (wchar_t)towlower(c);
    m_sugItems.clear(); std::set<std::wstring> seen;
    auto add = [&](const std::wstring& u, const std::wstring& t) {
        if (m_sugItems.size() >= 6 || !IsHttp(u)) return;
        std::wstring hay = u + L" " + t; for (auto& c : hay) c = (wchar_t)towlower(c);
        if (hay.find(q) != std::wstring::npos && seen.insert(u).second) m_sugItems.push_back({ u, t });
    };
    for (auto& b : m_bm) add(b.url, b.title);
    for (int i = (int)m_hist.size() - 1; i >= 0; --i) add(m_hist[i].url, m_hist[i].title);
    m_sugQ = q; if (WebSugOn(q)) SetTimer(m_hWnd, 2010, 180, nullptr);   // web suggestions after a short pause
    if (m_sugItems.empty()) { HideSug(); return; }
    m_sugSel = -1; ShowSug();
}

// ---- web suggestions: the chosen engine's own suggest service, through our DoH proxy, no cookies ----
static const wchar_t* SugEndpoint(const std::wstring& e) {
    if (e == L"Google")     return L"https://suggestqueries.google.com/complete/search?client=firefox&q=";
    if (e == L"DuckDuckGo") return L"https://duckduckgo.com/ac/?type=list&q=";
    if (e == L"Bing")       return L"https://api.bing.com/qsonhs.aspx?type=json&q=";
    if (e == L"Brave" || e == L"Brave Ask") return L"https://search.brave.com/api/suggest?q=";
    if (e == L"Startpage")  return L"https://www.startpage.com/osuggestions?q=";
    if (e == L"Ecosia")     return L"https://ac.ecosia.org/autocomplete?type=list&q=";
    if (e == L"Perplexity") return L"https://duckduckgo.com/ac/?type=list&q=";   // no public service of its own: DuckDuckGo's (privacy-friendly)
    return nullptr;   // custom engine: local suggestions only
}
static std::string SugReadStr(const std::string& b, size_t& p) {   // p is just after the opening quote; returns the text, p ends after the closing quote
    std::string s;
    while (p < b.size() && b[p] != '"') {
        if (b[p] == '\\' && p + 1 < b.size()) {
            ++p;
            if (b[p] == 'u' && p + 4 < b.size()) {
                unsigned cp = (unsigned)strtoul(b.substr(p + 1, 4).c_str(), nullptr, 16); p += 4;
                if (cp < 0x80) s += (char)cp; else if (cp < 0x800) { s += (char)(0xC0 | cp >> 6); s += (char)(0x80 | (cp & 63)); }
                else { s += (char)(0xE0 | cp >> 12); s += (char)(0x80 | (cp >> 6 & 63)); s += (char)(0x80 | (cp & 63)); }
            } else s += b[p];
        } else s += b[p];
        ++p;
    }
    ++p; return s;
}
static std::vector<std::wstring> SugFetch(int port, const std::wstring& url) {
    std::string body;
    if (HINTERNET ses = dl::Session(port, 1)) {
        WinHttpSetTimeouts(ses, 2000, 2000, 2500, 2500);
        dl::Url u;
        if (dl::Crack(url, u)) if (HINTERNET con = WinHttpConnect(ses, u.host.c_str(), u.port, 0)) {
            if (HINTERNET rq = WinHttpOpenRequest(con, L"GET", u.path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, u.https ? WINHTTP_FLAG_SECURE : 0)) {
                DWORD nc = WINHTTP_DISABLE_COOKIES; WinHttpSetOption(rq, WINHTTP_OPTION_DISABLE_FEATURE, &nc, sizeof nc);
                static const wchar_t* hdr = L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Safari/537.36\r\nAccept: application/json, */*";
                if (WinHttpSendRequest(rq, hdr, (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(rq, nullptr)) {
                    char buf[2048]; DWORD n = 0;
                    while (body.size() < 16384 && WinHttpReadData(rq, buf, sizeof buf, &n) && n) body.append(buf, n);
                }
                WinHttpCloseHandle(rq);
            }
            WinHttpCloseHandle(con);
        }
        WinHttpCloseHandle(ses);
    }
    std::vector<std::wstring> out; size_t p = 0;
    auto add = [&](const std::string& s) { if (!s.empty() && s.size() < 120 && out.size() < 6) out.push_back(FromUtf8(s)); };
    while (p < body.size() && isspace((unsigned char)body[p])) ++p;
    if (p < body.size() && body[p] == '{') {   // Bing: {"AS":{"Results":[{"Suggests":[{"Txt":"apple"},...
        for (size_t k = body.find("\"Txt\":\""); k != std::string::npos && out.size() < 6; k = body.find("\"Txt\":\"", k)) { k += 7; add(SugReadStr(body, k)); }
    } else {                                    // OpenSearch: ["query",["a","b",...]]
        p = body.find('['); if (p != std::string::npos) p = body.find('[', p + 1);
        if (p != std::string::npos) for (++p; p < body.size() && out.size() < 6;) {
            while (p < body.size() && body[p] != '"' && body[p] != ']') ++p;
            if (p >= body.size() || body[p] == ']') break;
            ++p; add(SugReadStr(body, p));
        }
    }
    return out;
}
bool BrowserWindow::WebSugOn(const std::wstring& q) const {
    if (m_incognito || m_setup || q.size() > 100 || m_set.Get(L"web_suggest", L"1") == L"0") return false;
    if (!SugEndpoint(m_set.Get(L"engine", L"Google"))) return false;
    if (q.find(L"://") != std::wstring::npos || q.rfind(L"libra:", 0) == 0 || q.rfind(L"javascript:", 0) == 0) return false;
    if (q.find(L' ') == std::wstring::npos && q.find(L'.') != std::wstring::npos) return false;   // looks like an address, not a search
    return true;
}
void BrowserWindow::FetchWebSug(const std::wstring& q) {
    const wchar_t* ep = SugEndpoint(m_set.Get(L"engine", L"Google")); int port = DohProxy::Get().Port();
    if (!ep || !port || !WebSugOn(q)) return;
    unsigned gen = ++m_sugGen; HWND hw = m_hWnd; std::wstring url = std::wstring(ep) + UrlEnc(q);
    std::thread([hw, gen, url, port] { PostMessage(hw, WM_APP + 20, gen, (LPARAM) new std::vector<std::wstring>(SugFetch(port, url))); }).detach();
}
void BrowserWindow::HomeSug(int idx, const std::wstring& q) {   // suggestions for the search box on the new-tab page
    if (idx < 0 || idx >= (int)m_tabs.size()) return;
    const wchar_t* ep = SugEndpoint(m_set.Get(L"engine", L"Google")); int port = DohProxy::Get().Port();
    std::wstring ql = q; for (auto& c : ql) c = (wchar_t)towlower(c);
    if (!ep || !port || !WebSugOn(ql)) return;
    HWND hw = m_hWnd; Tab* t = m_tabs[idx].get(); std::wstring url = std::wstring(ep) + UrlEnc(q);
    std::thread([hw, t, q, url, port] { PostMessage(hw, WM_APP + 21, 0, (LPARAM) new HomeSugMsg{ t, q, SugFetch(port, url) }); }).detach();
}

void BrowserWindow::ShowSug() {
    if (!m_hSug) {
        WNDCLASSEXW wc{ sizeof(wc) }; wc.style = 0; wc.lpfnWndProc = SugProc; wc.hInstance = m_hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.lpszClassName = L"LibraSuggest";
        RegisterClassExW(&wc);
        m_hSug = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST, L"LibraSuggest", L"", WS_POPUP,
                                 0, 0, 10, 10, m_hWnd, nullptr, m_hInst, nullptr);
        SetWindowLongPtrW(m_hSug, GWLP_USERDATA, (LONG_PTR)this);
    }
    RECT pill = OmniPillRect();
    POINT p = { pill.left, pill.bottom + MulDiv(6, m_dpi, 96) }; ClientToScreen(m_hWnd, &p);
    int w = pill.right - pill.left, rowH = MulDiv(42, m_dpi, 96), h = (int)m_sugItems.size() * rowH + MulDiv(12, m_dpi, 96);
    SetWindowPos(m_hSug, HWND_TOPMOST, p.x, p.y, w, h, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetWindowRgn(m_hSug, CreateRoundRectRgn(0, 0, w + 1, h + 1, MulDiv(20, m_dpi, 96), MulDiv(20, m_dpi, 96)), TRUE);
    InvalidateRect(m_hSug, nullptr, FALSE);
}

void BrowserWindow::PaintSug(HDC hdc) {
    RECT rc; GetClientRect(m_hSug, &rc);
    HBRUSH w = CreateSolidBrush(RGB(255, 255, 255)); FillRect(hdc, &rc, w); DeleteObject(w);
    int rowH = MulDiv(42, m_dpi, 96), padY = MulDiv(6, m_dpi, 96), side = MulDiv(8, m_dpi, 96);
    SetBkMode(hdc, TRANSPARENT);
    { HPEN pn = CreatePen(PS_SOLID, 1, RGB(226, 226, 226)); HGDIOBJ op = SelectObject(hdc, pn), ob = SelectObject(hdc, GetStockObject(NULL_BRUSH)); int rr = MulDiv(20, m_dpi, 96); RoundRect(hdc, 0, 0, rc.right, rc.bottom, rr, rr); SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(pn); }   // thin border instead of a drop shadow
    wchar_t qb[300] = L""; GetWindowTextW(m_hOmnibox, qb, 300); std::wstring typed = qb, typedL = typed; for (auto& c : typedL) c = (wchar_t)towlower(c);
    static HFONT bold = nullptr; if (!bold) { LOGFONTW lf{}; GetObjectW(m_fontUI, sizeof lf, &lf); lf.lfWeight = FW_SEMIBOLD; bold = CreateFontIndirectW(&lf); }
    for (int i = 0; i < (int)m_sugItems.size(); ++i) {
        RECT row = { side, padY + i * rowH, rc.right - side, padY + (i + 1) * rowH };
        if (i == m_sugSel) FillRR(hdc, row, MulDiv(12, m_dpi, 96), RGB(240, 240, 240));
        std::wstring url = m_sugItems[i].first; bool search = url.rfind(EngineUrl(), 0) == 0;
        if (!search) { size_t sl = url.find(L"://"); if (sl != std::wstring::npos) url = url.substr(sl + 3); if (!url.empty() && url.back() == L'/') url.pop_back(); }
        const std::wstring& title = m_sugItems[i].second.empty() ? url : m_sugItems[i].second;
        int cx = row.left + MulDiv(24, m_dpi, 96), cy = (row.top + row.bottom) / 2, r7 = MulDiv(6, m_dpi, 96);   // small line icon: magnifier for searches, globe for pages
        { HPEN pn = CreatePen(PS_SOLID, 1, RGB(150, 150, 150)); HGDIOBJ op = SelectObject(hdc, pn), ob = SelectObject(hdc, GetStockObject(NULL_BRUSH));
          if (search) { Ellipse(hdc, cx - r7, cy - r7, cx + r7 - 2, cy + r7 - 2); MoveToEx(hdc, cx + r7 - 4, cy + r7 - 4, nullptr); LineTo(hdc, cx + r7 + 1, cy + r7 + 1); }
          else { Ellipse(hdc, cx - r7 - 1, cy - r7 - 1, cx + r7 + 1, cy + r7 + 1); MoveToEx(hdc, cx - r7 - 1, cy, nullptr); LineTo(hdc, cx + r7 + 1, cy); MoveToEx(hdc, cx, cy - r7 - 1, nullptr); LineTo(hdc, cx, cy + r7 + 1); }
          SelectObject(hdc, op); SelectObject(hdc, ob); DeleteObject(pn); }
        RECT t1 = { row.left + MulDiv(46, m_dpi, 96), row.top, row.right - MulDiv(16, m_dpi, 96), row.bottom };
        HFONT old = (HFONT)SelectObject(hdc, m_fontUI); UINT fl = DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX;
        if (search) {   // what you typed in normal weight, the completion in bold
            std::wstring tl = title; for (auto& c : tl) c = (wchar_t)towlower(c);
            if (!typedL.empty() && tl.rfind(typedL, 0) == 0 && title.size() > typed.size()) {
                SIZE sz{}; std::wstring head = title.substr(0, typed.size()); SetTextColor(hdc, RGB(90, 90, 90)); GetTextExtentPoint32W(hdc, head.c_str(), (int)head.size(), &sz);
                DrawTextW(hdc, head.c_str(), -1, &t1, fl); RECT t2 = t1; t2.left += sz.cx; SelectObject(hdc, bold); SetTextColor(hdc, RGB(17, 17, 17)); std::wstring tail = title.substr(typed.size()); DrawTextW(hdc, tail.c_str(), -1, &t2, fl);
            } else { SetTextColor(hdc, RGB(17, 17, 17)); DrawTextW(hdc, title.c_str(), -1, &t1, fl); }
        } else {   // page: title, then the address in grey on the same line
            SetTextColor(hdc, RGB(17, 17, 17)); SIZE sz{}; GetTextExtentPoint32W(hdc, title.c_str(), (int)title.size(), &sz); DrawTextW(hdc, title.c_str(), -1, &t1, fl);
            RECT t2 = t1; t2.left += sz.cx + MulDiv(12, m_dpi, 96); if (t2.left < t2.right - MulDiv(40, m_dpi, 96) && title != url) { SetTextColor(hdc, RGB(138, 138, 138)); DrawTextW(hdc, url.c_str(), -1, &t2, fl); }
        }
        SelectObject(hdc, old);
    }
}

LRESULT CALLBACK BrowserWindow::SugProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* bw = reinterpret_cast<BrowserWindow*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (!bw) return DefWindowProcW(h, msg, wp, lp);
    int rowH = MulDiv(42, bw->m_dpi, 96), padY = MulDiv(6, bw->m_dpi, 96);
    switch (msg) {
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;       // keep keyboard focus in the address bar
    case WM_ERASEBKGND:    return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom); HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
        bw->PaintSug(mem); BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0;
    }
    case WM_MOUSEMOVE: {
        int i = (GET_Y_LPARAM(lp) - padY) / rowH;
        if (i >= 0 && i < (int)bw->m_sugItems.size() && i != bw->m_sugSel) { bw->m_sugSel = i; InvalidateRect(h, nullptr, FALSE); }
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int i = (GET_Y_LPARAM(lp) - padY) / rowH;
        if (i >= 0 && i < (int)bw->m_sugItems.size()) { std::wstring u = bw->m_sugItems[i].first; bw->HideSug(); bw->NavigateTo(u); SetFocus(bw->m_hWnd); }
        return 0;
    }
    }
    return DefWindowProcW(h, msg, wp, lp);
}

void BrowserWindow::RefreshTabsPage() {
    for (int i = 0; i < (int)m_tabs.size(); ++i) if (m_tabs[i]->url == L"libra://tabs") RenderTab(i);
}
RECT BrowserWindow::FavRect(int i) const {
    RECT tr = TabRect(i);
    int gap = MulDiv(Layout::TabGap, m_dpi, 96), inset = MulDiv(Layout::TabInset, m_dpi, 96), pad = MulDiv(Layout::TabPaddingH, m_dpi, 96), fav = MulDiv(18, m_dpi, 96);
    RECT pill = { tr.left + gap / 2, tr.top + inset, tr.right - gap / 2, tr.bottom - inset };
    int x = (i >= 0 && i < (int)m_tabs.size() && m_tabs[i]->pinned) ? pill.left + ((pill.right - pill.left) - fav) / 2 : pill.left + pad;
    int y = pill.top + ((pill.bottom - pill.top) - fav) / 2;
    return { x, y, x + fav, y + fav };
}

void BrowserWindow::InstallAdScript(Tab* t) {
    if (!t->webView) return;
    InstallFpScript(t);
    std::wstring allow = L"[", ball = L"[", maxl = L"[";
    for (auto& kv : m_modes) (kv.second == 2 ? allow : kv.second == 1 ? ball : maxl) += L"\"" + Esc(kv.first) + L"\",";
    allow += L"]"; ball += L"]"; maxl += L"]";
    std::wstring defs = std::to_wstring(_wtoi(m_set.Get(L"block_default", L"0").c_str()));
                    std::wstring list = L"[";
                    int n = 0;
                    for (auto& h : DohProxy::Get().BlockedHosts()) {
                        bool ok = !h.empty(); for (char c : h) if (!isalnum((unsigned char)c) && c != '.' && c != '-') ok = false;
                        if (ok && n++ < 3000) list += L"\"" + FromUtf8(h) + L"\",";
                    }
                    list += L"]";
                    std::wstring js = LR"js((function(){
var AL=)js" + allow + LR"js(;var BL=)js" + ball + LR"js(;var ML=)js" + maxl + LR"js(;var DEF=)js" + defs + LR"js(;
var th=location.hostname;try{if(window.top!==window&&location.ancestorOrigins&&location.ancestorOrigins.length)th=new URL(location.ancestorOrigins[location.ancestorOrigins.length-1]).hostname}catch(e){}
function mt(L){for(var i=0;i<L.length;i++)if(th===L[i]||th.endsWith('.'+L[i]))return true;return false}
var MODE=mt(ML)?0:mt(BL)?1:mt(AL)?2:DEF;if(MODE===2)return;var BAL=(MODE===1);
if(MODE===0){try{var cs=new CSSStyleSheet();cs.replaceSync('ins.adsbygoogle,.adsbygoogle,[id^="google_ads_iframe"],[id^="div-gpt-ad"],[data-ad-slot],[data-google-query-id],iframe[src*="doubleclick.net"],iframe[src*="googlesyndication.com"],iframe[src*="adservice.google"],.adsbox,.ad-box,.ad-banner,.ad_banner,.banner_ad,.bannerAd,.ad-unit,.ad_unit,.ad-slot,.ad-placeholder,.ad-container,.ad-wrapper,.advertisement,.text-ad,.textAd,.text_ad,.text_ads,.pub_300x250,.pub_300x250m,.pub_728x90,#ad-banner,#ad_banner,#adbanner,#ad-slot{display:none!important}');document.adoptedStyleSheets=[].concat(document.adoptedStyleSheets||[],[cs])}catch(e){}}   /* cosmetic ad boxes: Max only, our own short list of common ad container names; Balanced keeps the ads it lets through */
(function(){function ua(){return navigator.userActivation&&navigator.userActivation.isActive}
function ok(el){return ua()||(BAL&&el.muted)}
var P=HTMLMediaElement.prototype,op=P.play;
P.play=function(){if(!this.__lbOk&&!ok(this)){try{this.pause()}catch(e){}return Promise.reject(new DOMException('Autoplay blocked by Libra','NotAllowedError'))}this.__lbOk=true;return op.apply(this,arguments)};
document.addEventListener('play',function(e){var el=e.target;if(!el||!el.pause||el.__lbOk)return;if(ok(el)){el.__lbOk=true;return}el.pause()},true);
document.addEventListener('DOMContentLoaded',function(){[].forEach.call(document.querySelectorAll('video[autoplay],audio[autoplay]'),function(v){v.removeAttribute('autoplay');v.preload='none'})});})();
var B=new Set()js" + list + LR"js();
var PR=)js" + AdRulesJson() + LR"js(;
function pbad(u){if(!u)return false;try{var a=new URL(u,location.href),p=a.pathname.toLowerCase(),h=a.hostname.toLowerCase();for(var i=0;i<PR.length;i++){var r=PR[i];if(r[0]&&h!==r[0]&&!h.endsWith('.'+r[0]))continue;var q=r[1],e=q.charAt(q.length-1)==='$';if(e)q=q.slice(0,-1);if(e?p.endsWith(q):p.indexOf(q)>=0)return true}}catch(x){}return false}
function bad(u){try{var h=new URL(u,location.href).hostname,p=h.split('.');if(B.has(h))return true;for(var i=1;i<p.length-1;i++)if(B.has(p.slice(i).join('.')))return true}catch(e){}return false}
var MEDIA='img,video,canvas,svg,picture,object,embed,audio,input,button,textarea,select';
var LAB=/^(advertisement|advertise|advertorial|sponsored|promoted|ad|ads)$/i;
var TOK=/^(ad|ads|adv|advert|advertisement|advertising|adslot|adunit|adbox|adwrap|adfree|adsense|dfp|gpt|sponsor|sponsored|promoted|taboola|outbrain|mgid|colombia|appgutter|banner|adtext)$/;
function vis(e){var r=e.getBoundingClientRect();if(!r.width||!r.height)return false;var c=getComputedStyle(e);return c.display!=='none'&&c.visibility!=='hidden'&&c.opacity!=='0'}
// an ad frame is "empty" when it is hidden, points at a blocked host, or is a blank frame with nothing inside
function emptyFrame(f){if(!vis(f))return true;var s=f.getAttribute('src');if(s&&s!=='about:blank')return bad(s);try{var d=f.contentDocument;if(d&&d.body)return !d.body.innerText.trim()&&!d.body.querySelector(MEDIA)}catch(e){}return false}
// does e show anything real? ("Advertisement" captions do not count)
function bgi(e){try{return getComputedStyle(e).backgroundImage!=='none'}catch(x){return false}}
function content(e){if(bgi(e))return true;for(var c=e.firstElementChild;c;c=c.nextElementSibling)if(vis(c)&&bgi(c))return true;var t=(e.innerText||'').trim();if(t){var q=t.split(/\n+/);for(var i=0;i<q.length;i++){var w=q[i].trim();if(w&&!LAB.test(w))return true}}
 var m=e.querySelectorAll(MEDIA);for(var j=0;j<m.length;j++){var x=m[j];if(vis(x)){var r=x.getBoundingClientRect();if(r.width>8&&r.height>8)return true}}
 var f=e.querySelectorAll('iframe');for(var k=0;k<f.length;k++)if(!emptyFrame(f[k]))return true;return false}
// does the element's id/class name look like an ad slot?  (camelCase and separators are split into words)
function sig(e){var s=(e.id||'')+' '+((typeof e.className==='string')?e.className:'');if(!s.trim())return false;
 var t=s.replace(/([a-z])([A-Z])/g,'$1 $2').toLowerCase().split(/[^a-z0-9]+/);for(var i=0;i<t.length;i++)if(TOK.test(t[i])||/^ads?\d/.test(t[i]))return true;return false}
function hide(e){e.classList.add('lb-c')}
// after an ad is gone, also collapse parents that are left holding nothing (up to 3 levels, never the page skeleton)
function up(p){for(var i=0;i<3&&p&&p!==document.body&&p!==document.documentElement&&!/^(MAIN|ARTICLE|NAV|HEADER|FOOTER|FORM)$/.test(p.tagName);i++){if(content(p)||p.querySelector('input,textarea,select,form'))break;hide(p);p=p.parentElement}}
function kill(n){if(BAL||!n||n.nodeType!==1)return;var t=n.tagName;
 if(t==='OBJECT'||t==='EMBED'){var ty=(n.getAttribute('type')||'').toLowerCase();if(ty.indexOf('shockwave')>=0||pbad(n.getAttribute('data')||n.getAttribute('src'))){var q0=n.parentElement;n.remove();up(q0);return}}   // Flash and ad-path objects leave only an empty box
 if((t==='IMG'||t==='SOURCE'||t==='VIDEO'||t==='AUDIO')&&pbad(n.getAttribute('src'))){var q1=n.parentElement;n.remove();up(q1);return}
 if((t==='IFRAME'||t==='SCRIPT'||t==='IMG'||t==='EMBED')&&n.getAttribute('src')&&bad(n.getAttribute('src'))){var p=n.parentElement;n.remove();up(p);return}
 if(n.querySelectorAll)n.querySelectorAll('iframe[src],script[src],img[src],object,embed,source[src],video[src],audio[src]').forEach(kill)}
if(!BAL)addEventListener('error',function(e){var n=e.target;if(n&&n.tagName==='IMG'&&pbad(n.currentSrc||n.src)){var q=n.parentElement;n.remove();up(q)}},true);   // an image the engine refused: take the broken picture and its empty box away
// look for slots that stayed empty, collapse them; give the space back if real content arrives later
function sweep(){
 document.querySelectorAll('.lb-c').forEach(function(e){if(content(e))e.classList.remove('lb-c')});
 var els=document.querySelectorAll('div,section,aside,ins,span,p,a,figure,li,center'),n=0;
 for(var i=0;i<els.length&&n<4000;i++){var e=els[i];n++;if(e.classList.contains('lb-c'))continue;
  if((e.id||e.className)&&sig(e)){if(!content(e)&&!e.querySelector('input,textarea,select,form')){hide(e);up(e.parentElement)}}
  else if(e.children.length===0){var tx=(e.textContent||'').trim();if(tx.length<16&&LAB.test(tx)){var q=e.parentElement;if(q&&q!==document.body&&!content(q)){hide(q);up(q.parentElement)}}}}}
window.__lb=function(h){B.add(h);kill(document.documentElement);if(window.top===window)later(400)};
var st=document.createElement('style');
st.textContent='ins.adsbygoogle,[id^="div-gpt-ad"],[id^="google_ads_"],[class*="adsbygoogle"],[data-google-query-id],[data-ad-slot],[data-adunit],[class~="ad-slot"],[class~="adslot"],[class~="ad-container"],[class~="ad-wrapper"],[class~="advert"],[class~="advertisement"],[class~="adbox"],[id^="taboola-"],[id^="outbrain"],[class*="taboola"],iframe[src*="doubleclick.net"],iframe[src*="googlesyndication"],iframe[id^="aswift"]{display:none!important}.lb-c{height:0!important;min-height:0!important;max-height:0!important;margin-top:0!important;margin-bottom:0!important;padding-top:0!important;padding-bottom:0!important;border:0!important;overflow:hidden!important}';
(function addStyle(){var r=document.head||document.documentElement;if(!r){setTimeout(addStyle,5);return}r.appendChild(st)})();   // the page root does not exist yet when this runs
var tm=0,t0=Date.now();function later(ms){clearTimeout(tm);tm=setTimeout(sweep,ms||1200)}
new MutationObserver(function(m){m.forEach(function(x){if(x.type==='attributes')kill(x.target);else x.addedNodes.forEach(kill)});if(window.top===window&&Date.now()-t0<40000)later(1500)}).observe(document,{childList:true,subtree:true,attributes:true,attributeFilter:['src']});
if(window.top===window){[1200,3000,6000,10000,16000,24000].forEach(function(ms){setTimeout(sweep,ms)});addEventListener('load',function(){later(300)})}
})();)js";
    if (!t->scriptId.empty()) t->webView->RemoveScriptToExecuteOnDocumentCreated(t->scriptId.c_str());
    t->webView->AddScriptToExecuteOnDocumentCreated(js.c_str(),
        Microsoft::WRL::Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
            [this, t](HRESULT hr, LPCWSTR id) -> HRESULT { if (IndexOf(t) >= 0 && SUCCEEDED(hr) && id) t->scriptId = id; return S_OK; }).Get());
}

// ======================= Shield: blocked counter + 3 levels (Off / Balanced / Max) =======================
static std::wstring Num(unsigned long long n) {
    std::wstring d = std::to_wstring(n), o;
    for (size_t i = 0; i < d.size(); ++i) { if (i && (d.size() - i) % 3 == 0) o += L','; o += d[i]; }
    return o;
}

// "news.example.co.in" -> "example.co.in", "www.bbc.com" -> "bbc.com"
std::wstring BrowserWindow::SiteOf(const std::wstring& host) {
    std::vector<std::wstring> p; size_t s0 = 0;
    for (;;) { size_t e = host.find(L'.', s0); p.push_back(host.substr(s0, e == std::wstring::npos ? e : e - s0)); if (e == std::wstring::npos) break; s0 = e + 1; }
    int n = (int)p.size();
    if (n <= 2 || iswdigit(host[0])) return host.rfind(L"www.", 0) == 0 ? host.substr(4) : host;
    bool second = p[n - 1].size() == 2 && (p[n - 2] == L"co" || p[n - 2] == L"com" || p[n - 2] == L"org" || p[n - 2] == L"net" || p[n - 2] == L"gov" || p[n - 2] == L"ac" || p[n - 2] == L"edu" || p[n - 2] == L"nic");
    int from = n - (second ? 3 : 2); std::wstring r;
    for (int i = from; i < n; ++i) r += (i > from ? L"." : L"") + p[i];
    return r;
}
static bool SiteMatches(const std::wstring& host, const std::wstring& k) {
    return host == k || (host.size() > k.size() && host.compare(host.size() - k.size() - 1, std::wstring::npos, L"." + k) == 0);
}
int BrowserWindow::ModeFor(const std::wstring& host) const {
    int r = ModeForRaw(host); return (m_child && r == 2) ? 1 : r;   // Child Safety: Off is not available
}
int BrowserWindow::ModeForRaw(const std::wstring& host) const {
    int best = -1; size_t bl = 0;
    for (auto& kv : m_modes) if (SiteMatches(host, kv.first) && kv.first.size() >= bl) { best = kv.second; bl = kv.first.size(); }
    if (best >= 0) return best;
    int d = _wtoi(m_set.Get(L"block_default", L"0").c_str());
    return (d < 0 || d > 2) ? 0 : d;
}
bool BrowserWindow::ShieldCtx(std::wstring& host, int& mode, int& req, int& nh, int& lt) const {
    host.clear(); mode = 0; req = 0; nh = 0; lt = 0;
    if (m_activeTab < 0 || m_activeTab >= (int)m_tabs.size()) return false;
    const Tab* t = m_tabs[m_activeTab].get();
    std::wstring u = t->url == L"libra://error" ? t->errUrl : t->url;
    host = HostOf(u);
    req = t->blockedReq; nh = (int)t->blockedHosts.size(); lt = t->letThrough;
    if (host.empty() || !IsHttp(u)) return false;
    mode = ModeFor(host);
    return true;
}
void BrowserWindow::SaveAllow() {
    std::string o; for (auto& kv : m_modes) o += ToUtf8(kv.first + L"\t" + std::to_wstring(kv.second)) + "\n";
    WriteAll(DataDir() + L"\\sitemodes.txt", o);
}
void BrowserWindow::UpdateBypass() {   // the page on screen decides how the proxy treats DNS
    std::wstring h; int m, r, n, l;
    DohProxy::Get().SetMode(ShieldCtx(h, m, r, n, l) ? m : 0);
    Verdicts::Get().SetPrivate(m_incognito || (m_activeTab >= 0 && m_activeTab < (int)m_tabs.size() && m_tabs[m_activeTab]->incognito));   // private pages teach the local block memory nothing
}
static int StatDay(int back) {   // local date yyyymmdd, "back" days ago
    time_t t = time(nullptr) - (time_t)back * 86400; tm l; localtime_s(&l, &t); return (l.tm_year + 1900) * 10000 + (l.tm_mon + 1) * 100 + l.tm_mday;
}
void BrowserWindow::OnBlockedHit(const std::string& host) {
    ++m_totalBlocked; m_totalDirty = true; SnBlockedHit(host);
    if (!m_incognito) { if (!m_statLoaded) StatLoad(); ++m_stat[{ StatDay(0), host }]; m_statDirty = true; }
    if (m_activeTab >= 0 && m_activeTab < (int)m_tabs.size()) {
        Tab* t = m_tabs[m_activeTab].get();
        if (IsHttp(t->url)) { ++t->blockedReq; if (DohProxy::AdLike(host)) ++t->blockedAds; if (t->blockedHosts.size() < 500 || t->blockedHosts.count(host)) ++t->blockedHosts[host]; }
    }
    if (m_hBtnShield) InvalidateRect(m_hBtnShield, nullptr, FALSE);
    if (m_hShield && IsWindowVisible(m_hShield)) {
        std::wstring h; int m, r, n, l; int ht = MulDiv(ShieldCtx(h, m, r, n, l) ? ShieldSelY() + 166 : 196, m_dpi, 96);
        RECT wr; GetWindowRect(m_hShield, &wr);
        if (wr.bottom - wr.top != ht) { int w = wr.right - wr.left; SetWindowPos(m_hShield, nullptr, 0, 0, w, ht, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            SetWindowRgn(m_hShield, CreateRoundRectRgn(0, 0, w + 1, ht + 1, MulDiv(30, m_dpi, 96), MulDiv(30, m_dpi, 96)), TRUE); }
        InvalidateRect(m_hShield, nullptr, FALSE);
    }
}
void BrowserWindow::OnLetThrough(const std::string&) {
    if (m_activeTab >= 0 && m_activeTab < (int)m_tabs.size() && IsHttp(m_tabs[m_activeTab]->url)) ++m_tabs[m_activeTab]->letThrough;
    if (m_hShield && IsWindowVisible(m_hShield)) InvalidateRect(m_hShield, nullptr, FALSE);
}
void BrowserWindow::SetSiteMode(int mode) {
    std::wstring h; int cur, r, n, l;
    if (!ShieldCtx(h, cur, r, n, l) || mode < 0 || mode > 2 || (m_child && mode == 2)) return;
    if (mode == cur) { HideShield(); return; }
    for (auto it = m_modes.begin(); it != m_modes.end();) it = SiteMatches(h, it->first) ? m_modes.erase(it) : std::next(it);
    m_modes[SiteOf(h)] = mode;
    SaveAllow(); UpdateBypass();
    for (auto& tb : m_tabs) InstallAdScript(tb.get());              // the page script learns the new level
    Tab* t = m_tabs[m_activeTab].get();
    if (t->webView) t->webView->Reload();                            // load the page again the new way
    HideShield(); if (m_hBtnShield) InvalidateRect(m_hBtnShield, nullptr, FALSE);
}

void BrowserWindow::DrawShieldIcon(HDC hdc, RECT rc, bool hot) {
    std::wstring h; int mode, req, nh, lt; bool http = ShieldCtx(h, mode, req, nh, lt);
    bool off = http && mode == 2, bal = http && mode == 1;
    float k = m_dpi / 96.f; float cx = (rc.left + rc.right) / 2.f, cy = (rc.top + rc.bottom) / 2.f + 1 * k;
    COLORREF col = (!http || off) ? Color::TextSecondary : Color::TextPrimary;
    Gdiplus::Graphics g(hdc); g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath p;
    p.AddLine(cx, cy - 9 * k, cx + 8 * k, cy - 6 * k);
    p.AddBezier(cx + 8 * k, cy - 6 * k, cx + 8 * k, cy + 2 * k, cx + 4 * k, cy + 7 * k, cx, cy + 9 * k);
    p.AddBezier(cx, cy + 9 * k, cx - 4 * k, cy + 7 * k, cx - 8 * k, cy + 2 * k, cx - 8 * k, cy - 6 * k);
    p.CloseFigure();
    if (bal) {   // Balanced: left half filled
        Gdiplus::SolidBrush fb(GC(col)); g.SetClip(Gdiplus::RectF(cx - 10 * k, cy - 11 * k, 10 * k, 22 * k)); g.FillPath(&fb, &p); g.ResetClip();
    }
    Gdiplus::Pen pen(GC(col), 1.6f * k); pen.SetLineJoin(Gdiplus::LineJoinRound);
    g.DrawPath(&pen, &p);
    if (off) { Gdiplus::Pen sl(GC(col), 1.6f * k); sl.SetStartCap(Gdiplus::LineCapRound); sl.SetEndCap(Gdiplus::LineCapRound); g.DrawLine(&sl, cx - 9 * k, cy + 9 * k, cx + 9 * k, cy - 10 * k); }
    else if (http && !bal && req == 0) { Gdiplus::Pen ck(GC(col), 1.5f * k); ck.SetStartCap(Gdiplus::LineCapRound); ck.SetEndCap(Gdiplus::LineCapRound); g.DrawLine(&ck, cx - 3.5f * k, cy + 0.5f * k, cx - 1 * k, cy + 3.2f * k); g.DrawLine(&ck, cx - 1 * k, cy + 3.2f * k, cx + 3.8f * k, cy - 2.6f * k); }
    if (http && !off && req > 0) {   // count badge
        float r = 8 * k, bx = cx + 8 * k, by = cy - 8 * k;
        Gdiplus::SolidBrush bb(GC(Color::TextPrimary)); g.FillEllipse(&bb, bx - r, by - r, 2 * r, 2 * r);
        std::wstring t = req > 99 ? L"99+" : std::to_wstring(req);
        HFONT f = CreateFontW(-MulDiv(req > 99 ? 8 : 10, m_dpi, 96), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HFONT old = (HFONT)SelectObject(hdc, f); SetBkMode(hdc, TRANSPARENT); SetTextColor(hdc, RGB(255, 255, 255));
        RECT tr = { (int)(bx - r - 2 * k), (int)(by - r), (int)(bx + r + 2 * k), (int)(by + r) };
        DrawTextW(hdc, t.c_str(), -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        SelectObject(hdc, old); DeleteObject(f);
    }
}

int BrowserWindow::ShieldRows() const {
    std::wstring h; int m, r, n, l;
    if (!ShieldCtx(h, m, r, n, l) || m == 2 || r == 0) return 0;
    return (int)std::min<size_t>(5, m_tabs[m_activeTab]->blockedHosts.size());
}
int BrowserWindow::ShieldSelY() const { int r = ShieldRows(); return r ? 198 + r * 22 + 14 : 166; }

RECT BrowserWindow::ShieldSegRect(int i) const {   // 0 Off, 1 Balanced, 2 Max (popup client coords)
    int pad = MulDiv(22, m_dpi, 96), pw = MulDiv(320, m_dpi, 96), y = MulDiv(ShieldSelY(), m_dpi, 96), h = MulDiv(34, m_dpi, 96), in = MulDiv(3, m_dpi, 96);
    int tw = pw - 2 * pad, sw = tw / 3;
    return { pad + i * sw + in, y + in, pad + (i + 1) * sw - in, y + h - in };
}

RECT BrowserWindow::ShieldKeepRect() const {   // "keep my identity" row (popup client coords)
    int pad = MulDiv(22, m_dpi, 96), pw = MulDiv(320, m_dpi, 96), y = MulDiv(ShieldSelY() + 104, m_dpi, 96);
    return { pad, y, pw - pad, y + MulDiv(30, m_dpi, 96) };
}
bool BrowserWindow::KeepIdHere() { std::wstring h; int m, r, n, l; return ShieldCtx(h, m, r, n, l) && m_keepId.count(SiteOf(h)); }
void BrowserWindow::ToggleKeepId() {
    std::wstring h; int m, r, n, l; if (!ShieldCtx(h, m, r, n, l)) return;
    std::wstring s = SiteOf(h); if (!m_keepId.erase(s)) m_keepId.insert(s);
    std::string o; for (auto& k : m_keepId) o += ToUtf8(k) + "\n"; WriteAll(DataDir() + L"\\keepid.txt", o);
    for (auto& tb : m_tabs) InstallFpScript(tb.get());
    if (m_activeTab >= 0 && m_tabs[m_activeTab]->webView) m_tabs[m_activeTab]->webView->Reload();
    HideShield();
}

void BrowserWindow::PaintShield(HDC hdc) {
    std::wstring host; int mode, req, nh, lt; bool http = ShieldCtx(host, mode, req, nh, lt);
    RECT rc; GetClientRect(m_hShield, &rc);
    HBRUSH w = CreateSolidBrush(RGB(255, 255, 255)); FillRect(hdc, &rc, w); DeleteObject(w);
    float k = m_dpi / 96.f; int pad = (int)(22 * k);
    SetBkMode(hdc, TRANSPARENT);
    auto font = [&](int px, int weight, const wchar_t* face) { return CreateFontW(-MulDiv(px, m_dpi, 96), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face); };
    auto text = [&](const std::wstring& t, RECT r, HFONT f, COLORREF c, UINT fl) { HFONT o = (HFONT)SelectObject(hdc, f); SetTextColor(hdc, c); DrawTextW(hdc, t.c_str(), -1, &r, fl | DT_NOPREFIX); SelectObject(hdc, o); };
    HFONT fb = font(15, FW_SEMIBOLD, L"Segoe UI"), fs = font(12, FW_NORMAL, L"Segoe UI"), fbig = font(44, FW_LIGHT, L"Segoe UI Light"), fl = font(13, FW_NORMAL, L"Segoe UI"), fsb = font(12, FW_SEMIBOLD, L"Segoe UI");
    const COLORREF grey = RGB(138, 138, 138), ink = RGB(17, 17, 17);
    bool off = http && mode == 2;
    std::wstring title = http ? (host.rfind(L"www.", 0) == 0 ? host.substr(4) : host) : L"Libra pages";
    RECT th = { pad, (int)(16 * k), rc.right - pad - (int)(100 * k), (int)(42 * k) };
    text(title, th, fb, ink, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (http) {
        RECT chip = { rc.right - pad - (int)(92 * k), (int)(18 * k), rc.right - pad, (int)(40 * k) };
        FillRR(hdc, chip, (int)(11 * k), off ? RGB(236, 236, 236) : RGB(17, 17, 17));
        text(mode == 2 ? L"Off" : mode == 1 ? L"Balanced" : L"Max", chip, fs, off ? grey : RGB(255, 255, 255), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    int rows = ShieldRows(); int sy = (int)(ShieldSelY() * k);
    static const COLORREF catCol[4] = { RGB(229, 72, 77), RGB(245, 165, 36), RGB(62, 123, 250), RGB(142, 78, 198) };
    static const wchar_t* catName[4] = { L"Ads", L"Trackers", L"Analytics", L"Social" };
    if (rows) {   // rich view: ring + legend, then top blocked servers
        const Tab* tb = m_tabs[m_activeTab].get(); int cnt[4] = { 0, 0, 0, 0 };
        for (auto& kv : tb->blockedHosts) cnt[DohProxy::Cat(kv.first)] += kv.second;
        int tot = cnt[0] + cnt[1] + cnt[2] + cnt[3]; if (!tot) tot = 1;
        float d = 92 * k, rx = pad + 4 * k, ry = 52 * k; float sw = 9 * k;
        { Gdiplus::Graphics g(hdc); g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
          Gdiplus::RectF ar(rx + sw / 2, ry + sw / 2, d - sw, d - sw);
          Gdiplus::Pen track(GC(RGB(236, 236, 236)), sw); g.DrawEllipse(&track, ar);
          float a = -90; int nz = (cnt[0] > 0) + (cnt[1] > 0) + (cnt[2] > 0) + (cnt[3] > 0);
          for (int i = 0; i < 4; ++i) if (cnt[i]) { float sweep = 360.f * cnt[i] / tot, gap = nz > 1 ? 3.f : 0.f;
              Gdiplus::Pen p(GC(catCol[i]), sw); p.SetStartCap(Gdiplus::LineCapFlat); p.SetEndCap(Gdiplus::LineCapFlat);
              if (sweep > gap + 0.5f) g.DrawArc(&p, ar, a + gap / 2, sweep - gap); a += sweep; } }
        RECT nr = { (int)rx, (int)ry, (int)(rx + d), (int)(ry + d) };
        HFONT fr2 = font(24, FW_SEMIBOLD, L"Segoe UI"); text(Num(req), nr, fr2, off ? grey : ink, DT_CENTER | DT_VCENTER | DT_SINGLELINE); DeleteObject(fr2);
        int ly = (int)(60 * k), lx = (int)(rx + d + 22 * k);
        for (int i = 0; i < 4; ++i) {
            RECT row = { lx, ly + (int)(i * 22 * k), rc.right - pad, ly + (int)((i + 1) * 22 * k) };
            FillRR(hdc, { row.left, row.top + (int)(7 * k), row.left + (int)(8 * k), row.top + (int)(15 * k) }, (int)(4 * k), catCol[i]);
            RECT lt2 = { row.left + (int)(16 * k), row.top, row.right - (int)(36 * k), row.bottom };
            text(catName[i], lt2, fs, cnt[i] ? ink : grey, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            text(Num(cnt[i]), { lt2.right, row.top, row.right, row.bottom }, fsb, cnt[i] ? ink : grey, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
        RECT subr = { pad, (int)(152 * k), rc.right - pad, (int)(172 * k) };
        text(mode == 1 ? (std::to_wstring(lt) + L" ads let through \x00b7 " + std::to_wstring(nh) + L" servers blocked") : (L"from " + std::to_wstring(nh) + L" servers (estimated types)"), subr, fs, grey, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
        text(L"Top blocked", { pad, (int)(178 * k), rc.right - pad, (int)(196 * k) }, fsb, ink, DT_LEFT | DT_SINGLELINE);
        std::vector<std::pair<int, std::string>> top; for (auto& kv : tb->blockedHosts) top.push_back({ kv.second, kv.first });
        std::sort(top.begin(), top.end(), [](auto& a, auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
        int mx = top[0].first;
        for (int i = 0; i < rows; ++i) {
            int y0 = (int)((198 + i * 22) * k); int c = DohProxy::Cat(top[i].second);
            std::wstring hn = FromUtf8(top[i].second); if (hn.rfind(L"www.", 0) == 0) hn = hn.substr(4);
            int bw2 = std::max(3, (int)(40 * k * top[i].first / mx));
            FillRR(hdc, { pad, y0 + (int)(8 * k), pad + (int)(6 * k), y0 + (int)(14 * k) }, (int)(3 * k), catCol[c]);
            text(hn, { pad + (int)(14 * k), y0, rc.right - pad - (int)(84 * k), y0 + (int)(22 * k) }, fs, ink, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            RECT bar = { rc.right - pad - (int)(80 * k), y0 + (int)(9 * k), rc.right - pad - (int)(80 * k) + bw2, y0 + (int)(13 * k) };
            FillRR(hdc, bar, (int)(2 * k), catCol[c]);
            text(Num(top[i].first), { rc.right - pad - (int)(36 * k), y0, rc.right - pad, y0 + (int)(22 * k) }, fs, grey, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
        }
    }
    RECT nr = { pad, (int)(48 * k), rc.right - pad, (int)(108 * k) };
    if (!rows) text(Num(http ? req : m_totalBlocked), nr, fbig, off ? grey : ink, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    RECT lr = { pad, (int)(108 * k), rc.right - pad, (int)(128 * k) };
    if (!rows) text(http ? L"blocked on this page" : L"blocked in total", lr, fl, ink, DT_LEFT | DT_SINGLELINE);
    RECT sr = { pad, (int)(128 * k), rc.right - pad, (int)(148 * k) };
    std::wstring sub = !http ? L"Cloud ad blocking is on for every site." : off ? L"Blocking is off for this site." :
        mode == 1 ? (std::to_wstring(lt) + L" ads let through, " + std::to_wstring(nh) + L" servers blocked") :
        req == 0 ? L"Nothing blocked yet on this page." : L"from " + std::to_wstring(nh) + L" ad and tracker servers";
    if (!rows) text(sub, sr, fs, grey, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (http) {   // the three levels
        int tw = rc.right - 2 * pad, ty = sy, th2 = (int)(34 * k);
        RECT track = { pad, ty, pad + tw, ty + th2 };
        FillRR(hdc, track, th2 / 2, RGB(236, 236, 236));
        int sel = mode == 2 ? 0 : mode == 1 ? 1 : 2; const wchar_t* names[3] = { L"Off", L"Balanced", L"Max" };
        for (int i = 0; i < 3; ++i) {
            RECT sg = ShieldSegRect(i);
            if (i == sel) FillRR(hdc, sg, (sg.bottom - sg.top) / 2, RGB(17, 17, 17));
            text(names[i], sg, fsb, i == sel ? RGB(255, 255, 255) : RGB(90, 90, 90), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        RECT hr = { pad, sy + (int)(44 * k), rc.right - pad, sy + (int)(96 * k) };
        text(mode == 2 ? L"Loads the normal way, with no ad blocking. Use this if a site breaks."
           : mode == 1 ? L"Cloud ad blocking stops trackers. About half of the ads load, a different half each visit."
           : L"Cloud ad blocking stops all ads and trackers. Turn it down if a site does not load properly.", hr, fs, grey, DT_LEFT | DT_WORDBREAK);
    }
    if (http && mode != 2) {   // keep the same identity on this site (the daily change is off for it)
        RECT kr = ShieldKeepRect(); bool on = KeepIdHere();
        text(L"Keep my identity on this site", { kr.left, kr.top, kr.right - (int)(48 * k), kr.bottom }, fs, ink, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        RECT sw = { kr.right - (int)(40 * k), kr.top + (int)(6 * k), kr.right, kr.top + (int)(24 * k) };
        FillRR(hdc, sw, (sw.bottom - sw.top) / 2, on ? RGB(17, 17, 17) : RGB(214, 214, 214));
        int d = sw.bottom - sw.top - (int)(4 * k); int x0 = on ? sw.right - d - (int)(2 * k) : sw.left + (int)(2 * k);
        FillRR(hdc, { x0, sw.top + (int)(2 * k), x0 + d, sw.top + (int)(2 * k) + d }, d / 2, RGB(255, 255, 255));
    }
    RECT fr = { pad, rc.bottom - (int)(34 * k), rc.right - pad, rc.bottom - (int)(12 * k) };
    text(L"Cloud ad blocking  \x00b7  blocked in total: " + Num(m_totalBlocked), fr, fs, grey, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    DeleteObject(fb); DeleteObject(fs); DeleteObject(fbig); DeleteObject(fl); DeleteObject(fsb);
}

void BrowserWindow::HideShield() {
    if (m_hShield && IsWindowVisible(m_hShield)) { ShowWindow(m_hShield, SW_HIDE); m_shieldHiddenTick = GetTickCount(); }
}

void BrowserWindow::ToggleShieldPopup() {
    if (m_hShield && IsWindowVisible(m_hShield)) { HideShield(); return; }
    if (GetTickCount() - m_shieldHiddenTick < 250) return;           // the click that closed it must not reopen it
    if (!m_hShield) {
        WNDCLASSEXW wc{ sizeof(wc) }; wc.style = 0; wc.lpfnWndProc = ShieldProc; wc.hInstance = m_hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW); wc.lpszClassName = L"LibraShield";
        RegisterClassExW(&wc);
        m_hShield = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, L"LibraShield", L"", WS_POPUP, 0, 0, 10, 10, m_hWnd, nullptr, m_hInst, nullptr);
        SetWindowLongPtrW(m_hShield, GWLP_USERDATA, (LONG_PTR)this);
    }
    std::wstring h; int m, r, n, l; bool http = ShieldCtx(h, m, r, n, l);
    int w = MulDiv(320, m_dpi, 96), ht = MulDiv(http ? ShieldSelY() + 166 : 196, m_dpi, 96);
    RECT br; GetWindowRect(m_hBtnShield, &br);
    SetWindowPos(m_hShield, HWND_TOPMOST, br.right - w, br.bottom + MulDiv(6, m_dpi, 96), w, ht, SWP_SHOWWINDOW);
    SetWindowRgn(m_hShield, CreateRoundRectRgn(0, 0, w + 1, ht + 1, MulDiv(30, m_dpi, 96), MulDiv(30, m_dpi, 96)), TRUE);
    SetForegroundWindow(m_hShield);
    InvalidateRect(m_hShield, nullptr, FALSE);
}

LRESULT CALLBACK BrowserWindow::ShieldProc(HWND h, UINT msg, WPARAM wp, LPARAM lp) {
    auto* bw = reinterpret_cast<BrowserWindow*>(GetWindowLongPtrW(h, GWLP_USERDATA));
    if (!bw) return DefWindowProcW(h, msg, wp, lp);
    auto segAt = [&](POINT pt) { std::wstring hs; int m, r, n, l; if (!bw->ShieldCtx(hs, m, r, n, l)) return -1; for (int i = 0; i < 3; ++i) { RECT sg = bw->ShieldSegRect(i); if (PtInRect(&sg, pt)) return i; } return -1; };
    switch (msg) {
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps); RECT rc; GetClientRect(h, &rc);
        HDC mem = CreateCompatibleDC(dc); HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom); HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
        bw->PaintShield(mem); BitBlt(dc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old); DeleteObject(bmp); DeleteDC(mem); EndPaint(h, &ps); return 0;
    }
    case WM_LBUTTONDOWN: {
        POINT cp = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) }; RECT kr = bw->ShieldKeepRect();
        if (PtInRect(&kr, cp)) { std::wstring hs; int m2, r2, n2, l2; if (bw->ShieldCtx(hs, m2, r2, n2, l2) && m2 != 2) { bw->ToggleKeepId(); return 0; } }
        int i = segAt(cp);
        if (i >= 0) bw->SetSiteMode(i == 0 ? 2 : i == 1 ? 1 : 0);
        return 0;
    }
    case WM_SETCURSOR: {
        POINT pt; GetCursorPos(&pt); ScreenToClient(h, &pt);
        RECT kr = bw->ShieldKeepRect(); SetCursor(LoadCursor(nullptr, (segAt(pt) >= 0 || PtInRect(&kr, pt)) ? IDC_HAND : IDC_ARROW)); return TRUE;
    }
    case WM_ACTIVATE: if (LOWORD(wp) == WA_INACTIVE) bw->HideShield(); return 0;
    case WM_KEYDOWN: if (wp == VK_ESCAPE) bw->HideShield(); return 0;
    }
    return DefWindowProcW(h, msg, wp, lp);
}

#include "Safety.inl"
#include "Passwords.inl"
#include "Privacy.inl"
#include "Video.inl"
#include "Fp.inl"
#include "Agent.inl"
#include "Sponsor.inl"
#include "Signals.inl"
#include "Reader.inl"
#include "Update.inl"
#include "Import.inl"
#include "Pdf.inl"
#include "Backup.inl"
#include "Suggest.inl"
#include "Burn.inl"
#include "SaveAsk.inl"
#include "Hold.inl"
#include "Shot.inl"

void BrowserWindow::DrawAgentIcon(HDC hdc, RECT rc, bool open) {   // two thin sparkles; filled when the assistant is open
    float k = m_dpi / 96.f, cx = (rc.left + rc.right) / 2.f - 1 * k, cy = (rc.top + rc.bottom) / 2.f + 1 * k;
    Gdiplus::Graphics g(hdc); g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    auto star = [&](Gdiplus::GraphicsPath& p, float x, float y, float r) {
        p.AddBezier(x, y - r, x + r * .12f, y - r * .45f, x + r * .45f, y - r * .12f, x + r, y);
        p.AddBezier(x + r, y, x + r * .45f, y + r * .12f, x + r * .12f, y + r * .45f, x, y + r);
        p.AddBezier(x, y + r, x - r * .12f, y + r * .45f, x - r * .45f, y + r * .12f, x - r, y);
        p.AddBezier(x - r, y, x - r * .45f, y - r * .12f, x - r * .12f, y - r * .45f, x, y - r); p.CloseFigure(); };
    Gdiplus::GraphicsPath a, b; star(a, cx - 1 * k, cy - 1 * k, 8.5f * k); star(b, cx + 8 * k, cy + 7 * k, 4.2f * k);   // big sparkle top-left, small one bottom-right
    Gdiplus::Pen pen(GC(Color::TextPrimary), 1.9f * k); pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::SolidBrush br(GC(Color::TextPrimary));
    if (open) g.FillPath(&br, &a); else g.DrawPath(&pen, &a);
    g.FillPath(&br, &b);
}

void BrowserWindow::DrawShotIcon(HDC hdc, RECT rc) {   // thin camera outline
    float k = m_dpi / 96.f, cx = (rc.left + rc.right) / 2.f, cy = (rc.top + rc.bottom) / 2.f + 0.5f * k;
    Gdiplus::Graphics g(hdc); g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(GC(Color::TextPrimary), 1.6f * k); pen.SetLineJoin(Gdiplus::LineJoinRound);
    float w = 20.f * k, h = 14.f * k, x = cx - w / 2, y = cy - h / 2 + 1.f * k, r = 3.f * k;
    Gdiplus::GraphicsPath p;   // body with the little bump on top
    p.AddLine(x + r, y, x + w * 0.30f, y); p.AddLine(x + w * 0.30f, y, x + w * 0.38f, y - 2.6f * k); p.AddLine(x + w * 0.38f, y - 2.6f * k, x + w * 0.62f, y - 2.6f * k); p.AddLine(x + w * 0.62f, y - 2.6f * k, x + w * 0.70f, y); p.AddLine(x + w * 0.70f, y, x + w - r, y);
    p.AddArc(x + w - 2 * r, y, 2 * r, 2 * r, 270, 90); p.AddLine(x + w, y + r, x + w, y + h - r); p.AddArc(x + w - 2 * r, y + h - 2 * r, 2 * r, 2 * r, 0, 90);
    p.AddLine(x + w - r, y + h, x + r, y + h); p.AddArc(x, y + h - 2 * r, 2 * r, 2 * r, 90, 90); p.AddLine(x, y + h - r, x, y + r); p.AddArc(x, y, 2 * r, 2 * r, 180, 90); p.CloseFigure();
    g.DrawPath(&pen, &p); float lr = 3.6f * k; g.DrawEllipse(&pen, cx - lr, y + h / 2 - lr, 2 * lr, 2 * lr);
}

#include "Voice.inl"
