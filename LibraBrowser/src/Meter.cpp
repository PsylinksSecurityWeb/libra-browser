// Libra Meter: live CPU load in a digital-clock style, plus battery and memory. Runs without the browser (`--meter`). Reads Windows counters only; no network, no logging.
// Look: same light, thin, flat style as the browser (white card, thin border, accent colour from Color mode). Load colour = green / blue / amber / red.
// Edge dock: after a drag the meter tucks into the nearest screen edge as a thin colour strip and slides out when the mouse touches it.
#include "pch.h"
#include <cstdio>
#include <cmath>
#include <ctime>
using std::min;
using std::max;
#include <gdiplus.h>
#include <shellscalingapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include "Meter.h"
#include "Theme.h"
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "pdh.lib")

namespace Mt {
namespace G = Gdiplus;
static const UINT WM_TRAY = WM_APP + 1;
enum Side { None = 0, Left, Right, Top, Bottom };
struct View { int x = 0, y = 0, w = 0, h = 0, sx = 0, sy = 0; };
struct State {
    HWND hw = nullptr; ULONGLONG pi = 0, pk = 0, pu = 0; bool have = false;
    float cpu = 0, disp = 0, hist[60] = {}; int hn = 0;        // cpu = smoothed CPU load (what the digits show), disp = eased value on screen
    double ema = 0, dEma = 0; int shown = 0, diskShown = 0, lvl = 0, driver = 0; float press = 0; bool primed = false;   // smoothing state; press = overall pressure, driver 0 CPU / 1 Disk / 2 Memory
    PDH_HQUERY q = nullptr; PDH_HCOUNTER cCpu = nullptr, cIdle = nullptr; bool pdh = false;
    int mem = 0, batt = -1; bool ac = true, charging = false; DWORD secs = 0xFFFFFFFF;
    bool card = false, top = true, dock = true; int opa = 100; int dpi = 96;
    bool userHidden = false, autoHidden = false; int tick = 0;
    NOTIFYICONDATAW nid{}; HICON ico = nullptr; int icoVal = -1; bool trayOk = false; UINT taskbarMsg = 0;
    // edge dock: side, how far it is slid out (t: 0 = thin strip, 1 = fully out), expanded top-left (ex, ey), work area of its screen
    int side = None; float t = 1, tTarget = 1; DWORD leaveAt = 0; int ex = 0, ey = 0; RECT wk{}; bool moving = false, menuOpen = false; View view;
    Settings set;
} g;

static const wchar_t* WordL(int l) { return l == 0 ? L"Calm" : l == 1 ? L"Working" : l == 2 ? L"Busy" : L"Maxed out"; }
static COLORREF LevelC(int l) {   // green / blue (or the colour-mode accent) / amber / red, readable on white
    if (l == 0) return RGB(22, 163, 74);
    if (l == 1) return Color::AccentOn ? Color::FromHsl(Color::Ac.h, (std::max)(0.55, Color::Ac.s), 0.45) : RGB(37, 99, 235);
    if (l == 2) return RGB(217, 119, 6);
    return RGB(220, 38, 38);
}
static int NextLevel(float p, int cur) { static const float th[3] = { 25, 60, 85 }; int l = cur; while (l < 3 && p >= th[l] + 2) ++l; while (l > 0 && p < th[l - 1] - 4) --l; return l; }   // changes only when clearly past a border
static COLORREF Paper() { return Color::AccentOn ? Color::Tint(0.985) : RGB(255, 255, 255); }
static COLORREF Rim() { return Color::AccentOn ? Color::Tint(0.80) : RGB(214, 214, 214); }
static COLORREF Ghost() { return Color::AccentOn ? Color::Tint(0.91) : RGB(232, 232, 232); }
static COLORREF Muted() { return Color::AccentOn ? Color::TextSecondary : RGB(110, 110, 110); }
static G::Color C(COLORREF c, int a = 255) { return G::Color(a, GetRValue(c), GetGValue(c), GetBValue(c)); }
static float K() { return g.dpi / 96.f; }

static void Sample() {
    // CPU: Windows' "Processor Utility" counter = the number Task Manager shows (it follows the real clock speed); if it is missing, fall back to idle-time maths
    double raw = -1, diskBusy = -1;
    if (g.pdh && PdhCollectQueryData(g.q) == ERROR_SUCCESS) {
        PDH_FMT_COUNTERVALUE v{}; if (PdhGetFormattedCounterValue(g.cCpu, PDH_FMT_DOUBLE, nullptr, &v) == ERROR_SUCCESS && v.CStatus == PDH_CSTATUS_VALID_DATA) raw = v.doubleValue;
        if (g.cIdle && PdhGetFormattedCounterValue(g.cIdle, PDH_FMT_DOUBLE, nullptr, &v) == ERROR_SUCCESS && v.CStatus == PDH_CSTATUS_VALID_DATA) diskBusy = 100 - v.doubleValue;
    }
    FILETIME i, k, u; GetSystemTimes(&i, &k, &u);
    auto q = [](const FILETIME& f) { return ((ULONGLONG)f.dwHighDateTime << 32) | f.dwLowDateTime; };
    ULONGLONG ci = q(i), ck = q(k), cu = q(u);
    if (raw < 0 && g.have) { ULONGLONG di = ci - g.pi, tot = (ck - g.pk) + (cu - g.pu); if (tot > 0) raw = 100.0 * (double)(tot - di) / (double)tot; }   // kernel time includes idle
    g.have = true; g.pi = ci; g.pk = ck; g.pu = cu;
    if (raw >= 0) {
        raw = (std::max)(0.0, (std::min)(100.0, raw)); diskBusy = (std::max)(0.0, (std::min)(100.0, diskBusy < 0 ? 0 : diskBusy));
        if (!g.primed) { g.ema = raw; g.dEma = diskBusy; g.primed = true; g.shown = (int)(raw + 0.5); g.diskShown = (int)(diskBusy + 0.5); }
        else { g.ema += 0.4 * (raw - g.ema); g.dEma += 0.4 * (diskBusy - g.dEma);   // smoothing: a short burst cannot make the number jump
               if (std::fabs(g.ema - g.shown) >= 1.0) g.shown = (int)(g.ema + 0.5); if (std::fabs(g.dEma - g.diskShown) >= 1.5) g.diskShown = (int)(g.dEma + 0.5); }
        g.cpu = (float)g.shown;
    }
    g.hist[g.hn % 60] = (float)g.ema; ++g.hn;
    MEMORYSTATUSEX m{ sizeof m }; if (GlobalMemoryStatusEx(&m)) g.mem = (int)m.dwMemoryLoad;
    // overall pressure = the busiest of CPU, disk and memory (memory only counts once it is really full: below 70 % it is just in use, not pressure)
    float memP = (std::max)(0.f, (std::min)(100.f, (g.mem - 70) * 100.f / 30.f)); g.press = (float)g.shown; g.driver = 0;
    if (g.diskShown > g.press) { g.press = (float)g.diskShown; g.driver = 1; } if (memP > g.press) { g.press = memP; g.driver = 2; }
    g.lvl = NextLevel(g.press, g.lvl);
    SYSTEM_POWER_STATUS p{}; g.batt = -1;
    if (GetSystemPowerStatus(&p)) { if (p.BatteryFlag != 128 && p.BatteryLifePercent <= 100) g.batt = p.BatteryLifePercent; g.ac = p.ACLineStatus == 1; g.charging = (p.BatteryFlag & 8) != 0; g.secs = p.BatteryLifeTime; }
}

// ---- 7-segment digits drawn as polygons (no font, crisp at any size) ----
static void Seg(G::Graphics& gr, float x, float y, float w, float h, float t, int mask, COLORREF on, COLORREF off) {
    static const int dig[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };   // bits a b c d e f g
    int bits = mask < 0 ? 0 : dig[mask]; const float e = t * 0.12f + 0.8f, ht = t / 2;
    G::SolidBrush bOn(C(on)), bOff(C(off));
    auto hz = [&](int bit, float yy) { G::PointF p[6] = { { x + e, yy }, { x + ht + e, yy - ht }, { x + w - ht - e, yy - ht }, { x + w - e, yy }, { x + w - ht - e, yy + ht }, { x + ht + e, yy + ht } }; gr.FillPolygon((bits & bit) ? &bOn : &bOff, p, 6); };
    auto vt = [&](int bit, float xx, float y0, float y1) { G::PointF p[6] = { { xx, y0 + e }, { xx + ht, y0 + ht + e }, { xx + ht, y1 - ht - e }, { xx, y1 - e }, { xx - ht, y1 - ht - e }, { xx - ht, y0 + ht + e } }; gr.FillPolygon((bits & bit) ? &bOn : &bOff, p, 6); };
    float m = y + h / 2; hz(1, y); vt(2, x + w, y, m); vt(4, x + w, m, y + h); hz(8, y + h); vt(16, x, m, y + h); vt(32, x, y, m); hz(64, m);
}
static void RoundRect(G::GraphicsPath& p, float x, float y, float w, float h, float r) { r = (std::min)(r, (std::min)(w, h) / 2); p.AddArc(x, y, r * 2, r * 2, 180, 90); p.AddArc(x + w - r * 2, y, r * 2, r * 2, 270, 90); p.AddArc(x + w - r * 2, y + h - r * 2, r * 2, r * 2, 0, 90); p.AddArc(x, y + h - r * 2, r * 2, r * 2, 90, 90); p.CloseFigure(); }

// ---- sizes and edge dock geometry ----
static void CardSize(int& w, int& h) { float k = K(); w = (int)((g.card ? 236 : 168) * k); h = (int)((g.card ? 158 : 60) * k); }
static int Strip() { return (std::max)(10, (int)(14 * K())); }
static void FullSize(int& w, int& h) { CardSize(w, h); if (g.side == Left || g.side == Right) w += Strip(); else if (g.side == Top || g.side == Bottom) h += Strip(); }
static RECT WorkAt(POINT p) { MONITORINFO mi{ sizeof mi }; GetMonitorInfoW(MonitorFromPoint(p, MONITOR_DEFAULTTONEAREST), &mi); return mi.rcWork; }
static float Ease(float t) { return t * t * (3 - 2 * t); }

static void Apply() {   // put the window where the dock state says (slice of the full picture that is slid out)
    if (!g.hw || g.moving) return; int fw, fh; FullSize(fw, fh); int st = Strip(); View v; v.w = fw; v.h = fh; const RECT& wk = g.wk; float e = Ease(g.t);
    auto cl = [](int a, int lo, int hi) { return hi < lo ? lo : a < lo ? lo : a > hi ? hi : a; };
    if (g.side == None) { v.x = g.ex; v.y = g.ey; }
    else if (g.side == Left || g.side == Right) { int vis = st + (int)((fw - st) * e); v.w = vis; v.y = cl(g.ey, wk.top, wk.bottom - fh); if (g.side == Left) { v.x = wk.left; v.sx = fw - vis; } else { v.x = wk.right - vis; v.sx = 0; } }
    else { int vis = st + (int)((fh - st) * e); v.h = vis; v.x = cl(g.ex, wk.left, wk.right - fw); if (g.side == Top) { v.y = wk.top; v.sy = fh - vis; } else { v.y = wk.bottom - vis; v.sy = 0; } }
    g.view = v; SetWindowPos(g.hw, nullptr, v.x, v.y, v.w, v.h, SWP_NOZORDER | SWP_NOACTIVATE);
}

static void Render() {
    if (!g.hw) return; int fw, fh; FullSize(fw, fh), K(); float k = K(); int cw, ch; CardSize(cw, ch); int st = Strip();
    HDC sdc = GetDC(nullptr); HDC mdc = CreateCompatibleDC(sdc); BITMAPINFO bi{}; bi.bmiHeader = { sizeof(BITMAPINFOHEADER), fw, -fh, 1, 32, BI_RGB }; void* bits = nullptr;
    HBITMAP bmp = CreateDIBSection(sdc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0); HGDIOBJ old = SelectObject(mdc, bmp);
    { G::Bitmap img(fw, fh, fw * 4, PixelFormat32bppPARGB, (BYTE*)bits); G::Graphics gr(&img); gr.SetSmoothingMode(G::SmoothingModeAntiAlias); gr.SetTextRenderingHint(G::TextRenderingHintAntiAlias); gr.Clear(G::Color(0, 0, 0, 0));
      COLORREF col = LevelC(g.lvl); const wchar_t* word = WordL(g.lvl); COLORREF ink = Color::AccentOn ? Color::TextPrimary : RGB(17, 17, 17);
      float ox = 0, oy = 0;
      if (g.side) {   // the colour strip sits on the side that faces the screen: it is what stays visible when the meter slides away
          float sx = 0, sy = 0, sw = (float)st, sh = (float)fh;
          if (g.side == Left) { sx = (float)cw; } else if (g.side == Right) { ox = (float)st; } else if (g.side == Top) { sx = 0; sy = (float)ch; sw = (float)fw; sh = (float)st; } else { sw = (float)fw; sh = (float)st; oy = (float)st; }
          G::GraphicsPath sp; RoundRect(sp, sx + 0.5f, sy + 0.5f, sw - 1, sh - 1, (std::min)(sw, sh) / 2); G::SolidBrush sb(C(col)); gr.FillPath(&sb, &sp); }
      gr.TranslateTransform(ox, oy); int w = cw, h = ch;
      G::GraphicsPath bg; RoundRect(bg, 0.5f, 0.5f, w - 1.f, h - 1.f, 14 * k); G::SolidBrush bb(C(Paper(), 247)); gr.FillPath(&bb, &bg); G::Pen bd(C(Rim()), 1.f); gr.DrawPath(&bd, &bg);
      // digits: three cells, value right-aligned (unused cells show as faint ghost segments, like a real LCD)
      int v = (int)(g.disp + 0.5f); if (v > 100) v = 100; float dw = 21 * k, dh = 32 * k, t = 4.6f * k, gx = 7 * k, x0 = 16 * k, y0 = 9 * k;
      int d[3] = { v >= 100 ? 1 : -1, v >= 10 ? (v / 10) % 10 : -1, v % 10 }; if (v >= 100) { d[1] = 0; d[2] = 0; }
      for (int i = 0; i < 3; ++i) Seg(gr, x0 + i * (dw + gx), y0, dw, dh, t, d[i], ink, Ghost());
      G::FontFamily ff(L"Segoe UI"); G::Font fs(&ff, 15 * k, G::FontStyleBold, G::UnitPixel), fm(&ff, 11 * k, G::FontStyleRegular, G::UnitPixel), fx(&ff, 9 * k, G::FontStyleRegular, G::UnitPixel);
      G::SolidBrush tc(C(col)), ti(C(ink)), tg(C(Muted()));
      gr.DrawString(L"%", -1, &fs, G::PointF(x0 + 3 * (dw + gx) - 3 * k, y0 + 9 * k), &ti);
      float rx = x0 + 3 * (dw + gx) + 22 * k; gr.FillEllipse(&tc, rx, y0 + 4 * k, 7 * k, 7 * k); gr.DrawString(word, -1, &fm, G::PointF(rx + 12 * k, y0 + 0.5f * k), &ti);
      { const wchar_t* cap = g.lvl > 0 && g.driver == 1 ? L"CPU  \u00B7  limited by Disk" : g.lvl > 0 && g.driver == 2 ? L"CPU  \u00B7  memory is full" : L"CPU"; gr.DrawString(cap, -1, &fm, G::PointF(rx, y0 + 18 * k), &tg); }
      float bx = 16 * k, bw = w - 32 * k, by = 49 * k, bh = 4 * k; G::GraphicsPath tr; RoundRect(tr, bx, by, bw, bh, bh / 2); G::SolidBrush trb(C(Ghost())); gr.FillPath(&trb, &tr);
      float fl = (std::max)(bh, bw * (std::min)(100.f, g.disp) / 100.f); G::GraphicsPath fp; RoundRect(fp, bx, by, fl, bh, bh / 2); gr.FillPath(&tc, &fp);
      if (g.card) {
          wchar_t b[96]; swprintf(b, 96, L"Memory  %d%%", g.mem); gr.DrawString(b, -1, &fm, G::PointF(16 * k, 62 * k), &ti); swprintf(b, 96, L"Disk  %d%%", g.diskShown); gr.DrawString(b, -1, &fm, G::PointF(124 * k, 62 * k), &ti);
          if (g.batt >= 0) { wchar_t t2[64]; if (g.charging) swprintf(t2, 64, L"Battery  %d%%  charging", g.batt); else if (g.ac) swprintf(t2, 64, L"Battery  %d%%  plugged in", g.batt);
              else if (g.secs != 0xFFFFFFFF) swprintf(t2, 64, L"Battery  %d%%  %dh %02dm left", g.batt, (int)(g.secs / 3600), (int)((g.secs / 60) % 60)); else swprintf(t2, 64, L"Battery  %d%%", g.batt);
              gr.DrawString(t2, -1, &fm, G::PointF(16 * k, 80 * k), &ti); }
          else gr.DrawString(L"No battery (desktop PC)", -1, &fm, G::PointF(16 * k, 80 * k), &tg);
          float sx = 16 * k, sy = 102 * k, sw = w - 32 * k, sh = 36 * k; G::GraphicsPath sp; RoundRect(sp, sx, sy, sw, sh, 6 * k); G::SolidBrush sb(C(Ghost(), 150)); gr.FillPath(&sb, &sp);
          int n = (std::min)(g.hn, 60); if (n > 1) { std::vector<G::PointF> pts; for (int i = 0; i < n; ++i) { float val = g.hist[(g.hn - n + i) % 60]; pts.push_back(G::PointF(sx + sw * i / 59.f + (60 - n) * sw / 59.f, sy + sh - 2 * k - (sh - 4 * k) * val / 100.f)); }
              std::vector<G::PointF> area = pts; area.push_back(G::PointF(pts.back().X, sy + sh)); area.push_back(G::PointF(pts.front().X, sy + sh)); G::SolidBrush ab(C(col, 50)); gr.FillPolygon(&ab, area.data(), (int)area.size());
              G::Pen lp(C(col), 1.6f * k); lp.SetLineJoin(G::LineJoinRound); gr.DrawLines(&lp, pts.data(), (int)pts.size()); }
          gr.DrawString(L"Powered by Libra Browser", -1, &fx, G::PointF(16 * k, 140 * k), &tg);
      }
    }
    POINT pt = { g.view.x, g.view.y }, ps = { g.view.sx, g.view.sy }; SIZE sz = { g.view.w, g.view.h }; BLENDFUNCTION bf = { AC_SRC_OVER, 0, (BYTE)(g.opa * 255 / 100), AC_SRC_ALPHA };
    UpdateLayeredWindow(g.hw, sdc, &pt, &sz, mdc, &ps, 0, &bf, ULW_ALPHA);
    SelectObject(mdc, old); DeleteObject(bmp); DeleteDC(mdc); ReleaseDC(nullptr, sdc);
}

// ---- tray icon showing the live number (tile = load colour, white digits: readable on light and dark taskbars) ----
static void Tray() {
    int v = (int)(g.cpu + 0.5f); if (v == g.icoVal && g.trayOk) return; g.icoVal = v; int S = (std::max)(16, GetSystemMetrics(SM_CXSMICON)); COLORREF col = LevelC(g.lvl);
    G::Bitmap bm(S, S, PixelFormat32bppARGB); { G::Graphics gr(&bm); gr.SetSmoothingMode(G::SmoothingModeAntiAlias); gr.SetTextRenderingHint(G::TextRenderingHintAntiAlias); gr.Clear(G::Color(0, 0, 0, 0));
      G::GraphicsPath p; RoundRect(p, 0.5f, 0.5f, S - 1.f, S - 1.f, S * 0.24f); G::SolidBrush bb(C(col)); gr.FillPath(&bb, &p);
      wchar_t t[8]; swprintf(t, 8, L"%d", v > 99 ? 99 : v); G::FontFamily ff(L"Segoe UI"); G::Font f(&ff, S * (v >= 10 ? 0.58f : 0.7f), G::FontStyleBold, G::UnitPixel); G::StringFormat sf; sf.SetAlignment(G::StringAlignmentCenter); sf.SetLineAlignment(G::StringAlignmentCenter);
      G::SolidBrush tb(G::Color(255, 255, 255, 255)); gr.DrawString(t, -1, &f, G::RectF(0, S * 0.02f, (G::REAL)S, (G::REAL)S), &sf, &tb); }
    HICON ni = nullptr; bm.GetHICON(&ni); if (!ni) return;
    g.nid.hIcon = ni; swprintf(g.nid.szTip, 128, L"Libra Meter: CPU %d%% (%s)", v, WordL(g.lvl));
    g.trayOk = Shell_NotifyIconW(g.ico ? NIM_MODIFY : NIM_ADD, &g.nid) != FALSE; if (g.ico) DestroyIcon(g.ico); g.ico = ni;
}

static void SetPref(const wchar_t* k, const std::wstring& v) { g.set.Set(k, v); }
static void StartAnim() { SetTimer(g.hw, 4, 15, nullptr); }
static void SavePos() { SetPref(L"meter_x", std::to_wstring(g.ex)); SetPref(L"meter_y", std::to_wstring(g.ey)); }
static void DockNearest() {   // after a drag (or at start): pick the closest screen edge, sit flush against it and slide in when the mouse is away
    RECT r; GetWindowRect(g.hw, &r); POINT c = { (r.left + r.right) / 2, (r.top + r.bottom) / 2 }; g.wk = WorkAt(c);
    int d[4] = { c.x - g.wk.left, g.wk.right - c.x, c.y - g.wk.top, g.wk.bottom - c.y }, best = 0; for (int i = 1; i < 4; ++i) if (d[i] < d[best]) best = i;
    g.side = Left + best; g.ex = r.left; g.ey = r.top; g.t = 1; g.tTarget = 1; g.leaveAt = GetTickCount() + 900; SetTimer(g.hw, 3, 80, nullptr); Apply(); Render(); SavePos();
}
static void Float() { g.side = None; g.t = g.tTarget = 1; KillTimer(g.hw, 3); KillTimer(g.hw, 4); RECT r; GetWindowRect(g.hw, &r); g.wk = WorkAt({ r.left, r.top }); Apply(); Render(); }
static void Resize() { if (g.moving) return; Apply(); Render(); }

static void ApplySettings(bool first) {
    bool card = g.set.Get(L"meter_size") == L"card", top = g.set.Get(L"meter_top", L"1") != L"0", dock = g.set.Get(L"meter_dock", L"1") != L"0"; int opa = _wtoi(g.set.Get(L"meter_opacity", L"100").c_str()); if (opa < 30 || opa > 100) opa = 100;
    std::wstring a = g.set.Get(L"accent"); unsigned v = 0; bool on = a.size() == 7 && a[0] == L'#' && swscanf_s(a.c_str() + 1, L"%x", &v) == 1; Color::ApplyAccent(on, RGB((v >> 16) & 255, (v >> 8) & 255, v & 255));
    g.card = card; g.opa = opa; if (top != g.top || first) { g.top = top; SetWindowPos(g.hw, top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); }
    if (dock != g.dock && !first) { g.dock = dock; if (dock) DockNearest(); else Float(); } else g.dock = dock;
}

static void Menu(HWND hw) {
    HMENU m = CreatePopupMenu(), op = CreatePopupMenu();
    for (int p : { 100, 85, 70, 50 }) { wchar_t t[16]; swprintf(t, 16, L"%d%%", p); AppendMenuW(op, MF_STRING | (g.opa == p ? MF_CHECKED : 0), 300 + p, t); }
    AppendMenuW(m, MF_STRING, 1, g.card ? L"Compact view" : L"Show details"); AppendMenuW(m, MF_STRING | (g.dock ? MF_CHECKED : 0), 7, L"Hide at the screen edge"); AppendMenuW(m, MF_STRING | (g.top ? MF_CHECKED : 0), 2, L"Always on top"); AppendMenuW(m, MF_POPUP, (UINT_PTR)op, L"Opacity");
    AppendMenuW(m, MF_STRING | (RunOn() ? MF_CHECKED : 0), 3, L"Start with Windows"); AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, 4, L"Open Libra Browser"); AppendMenuW(m, MF_STRING, 5, IsWindowVisible(hw) ? L"Hide meter (keep tray icon)" : L"Show meter"); AppendMenuW(m, MF_SEPARATOR, 0, nullptr); AppendMenuW(m, MF_STRING, 6, L"Exit meter");
    POINT p; GetCursorPos(&p); SetForegroundWindow(hw); g.menuOpen = true; int c = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, p.x, p.y, 0, hw, nullptr); g.menuOpen = false; g.leaveAt = GetTickCount() + 700; PostMessageW(hw, WM_NULL, 0, 0); DestroyMenu(m);
    if (c == 1) { g.card = !g.card; SetPref(L"meter_size", g.card ? L"card" : L"pill"); Resize(); }
    else if (c == 7) { g.dock = !g.dock; SetPref(L"meter_dock", g.dock ? L"1" : L"0"); if (g.dock) DockNearest(); else Float(); }
    else if (c == 2) { g.top = !g.top; SetPref(L"meter_top", g.top ? L"1" : L"0"); SetWindowPos(hw, g.top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); }
    else if (c >= 350 && c <= 400) { g.opa = c - 300; SetPref(L"meter_opacity", std::to_wstring(g.opa)); Render(); }
    else if (c == 3) { bool on = !RunOn(); SetRun(on); SetPref(L"meter_start", on ? L"1" : L"0"); }
    else if (c == 4) { wchar_t e[MAX_PATH]; GetModuleFileNameW(nullptr, e, MAX_PATH); std::wstring cmd = L"\"" + std::wstring(e) + L"\""; STARTUPINFOW si{ sizeof(si) }; PROCESS_INFORMATION pi{}; if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); } }
    else if (c == 5) { if (IsWindowVisible(hw)) { ShowWindow(hw, SW_HIDE); g.userHidden = true; } else { ShowWindow(hw, SW_SHOWNOACTIVATE); g.userHidden = false; g.autoHidden = false; } }
    else if (c == 6) { SetPref(L"meter_on", L"0"); DestroyWindow(hw); }
}

static void LogLine() {
    wchar_t p[MAX_PATH]; if (!TestEnv(L"LIBRA_TEST_MLOG", p, MAX_PATH) || !p[0]) return; RECT r; GetWindowRect(g.hw, &r);
    FILE* f = nullptr; if (_wfopen_s(&f, p, L"a") || !f) return; fwprintf(f, L"raw=%.1f disk=%d press=%.0f lvl=%d drv=%d pdh=%d cpu=%.1f mem=%d batt=%d tray=%d card=%d top=%d opa=%d x=%ld y=%ld w=%ld h=%ld vis=%d side=%d t=%.2f dock=%d\n", g.ema, g.diskShown, g.press, g.lvl, g.driver, g.pdh ? 1 : 0, g.cpu, g.mem, g.batt, g.trayOk ? 1 : 0, g.card ? 1 : 0, g.top ? 1 : 0, g.opa, r.left, r.top, r.right - r.left, r.bottom - r.top, IsWindowVisible(g.hw) ? 1 : 0, g.side, g.t, g.dock ? 1 : 0); fclose(f);
}

static void Hover() {   // docked: the mouse touching the strip slides it out; leaving slides it back in
    if (!g.side || g.moving || g.menuOpen || !IsWindowVisible(g.hw)) return;
    POINT p; GetCursorPos(&p); RECT r; GetWindowRect(g.hw, &r); int m = (int)(4 * K()); InflateRect(&r, m, m); DWORD now = GetTickCount();
    if (PtInRect(&r, p)) { g.leaveAt = 0; if (g.tTarget < 1) { g.tTarget = 1; StartAnim(); } }
    else if (g.tTarget > 0) { if (!g.leaveAt) g.leaveAt = now + 450; else if ((int)(now - g.leaveAt) >= 0) { g.tTarget = 0; g.leaveAt = 0; StartAnim(); } }
}

static void Tick() {
    Sample(); Tray(); LogLine();
    if ((++g.tick & 1) == 0) {   // settings may change from the browser: re-read every 2 s
        g.set.Load(); if (g.set.Get(L"meter_on") != L"1") { DestroyWindow(g.hw); return; } ApplySettings(false); Resize();
    }
    QUERY_USER_NOTIFICATION_STATE s; if (SUCCEEDED(SHQueryUserNotificationState(&s))) {   // hide while a full-screen app / game / presentation is in front
        bool busy = s == QUNS_BUSY || s == QUNS_RUNNING_D3D_FULL_SCREEN || s == QUNS_PRESENTATION_MODE;
        if (busy && IsWindowVisible(g.hw) && !g.userHidden) { ShowWindow(g.hw, SW_HIDE); g.autoHidden = true; } else if (!busy && g.autoHidden) { g.autoHidden = false; if (!g.userHidden) ShowWindow(g.hw, SW_SHOWNOACTIVATE); } }
    if (std::fabs(g.cpu - g.disp) > 0.3f) SetTimer(g.hw, 2, 60, nullptr);
    if (IsWindowVisible(g.hw)) Render();
}

static LRESULT CALLBACK Proc(HWND hw, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_NCHITTEST: return HTCAPTION;   // drag anywhere
    case WM_NCLBUTTONDBLCLK: g.card = !g.card; SetPref(L"meter_size", g.card ? L"card" : L"pill"); Resize(); return 0;
    case WM_NCRBUTTONUP: case WM_CONTEXTMENU: Menu(hw); return 0;
    case WM_ENTERSIZEMOVE: g.moving = true; g.leaveAt = 0; return 0;
    case WM_MOVING: { RECT* r = (RECT*)lp; RECT wk = WorkAt({ r->left, r->top }); int s = (int)(14 * K()), w = r->right - r->left, h = r->bottom - r->top;   // magnetic screen edges
        if (abs(r->left - wk.left) < s) r->left = wk.left; if (abs(wk.right - r->right) < s) r->left = wk.right - w; if (abs(r->top - wk.top) < s) r->top = wk.top; if (abs(wk.bottom - r->bottom) < s) r->top = wk.bottom - h;
        r->right = r->left + w; r->bottom = r->top + h; return TRUE; }
    case WM_EXITSIZEMOVE: g.moving = false; { RECT r; GetWindowRect(hw, &r); g.ex = r.left; g.ey = r.top; g.wk = WorkAt({ r.left, r.top }); }
        if (g.dock) DockNearest(); else { Apply(); SavePos(); Render(); } return 0;
    case WM_TIMER:
        if (wp == 1) Tick();
        else if (wp == 2) { g.disp += (g.cpu - g.disp) * 0.4f; if (std::fabs(g.cpu - g.disp) < 0.3f) { g.disp = g.cpu; KillTimer(hw, 2); } if (IsWindowVisible(hw)) Render(); }
        else if (wp == 3) Hover();
        else if (wp == 4) { float d = g.tTarget - g.t, step = 0.12f; if (std::fabs(d) <= step) { g.t = g.tTarget; KillTimer(hw, 4); } else g.t += d > 0 ? step : -step; Apply(); if (IsWindowVisible(hw)) Render(); }
        return 0;
    case WM_DPICHANGED: g.dpi = HIWORD(wp); { RECT* r = (RECT*)lp; g.ex = r->left; g.ey = r->top; g.wk = WorkAt({ r->left, r->top }); } Resize(); return 0;
    case WM_POWERBROADCAST: if (wp == PBT_APMRESUMEAUTOMATIC) g.have = false; return TRUE;   // no CPU spike after sleep
    case WM_TRAY: if (LOWORD(lp) == WM_LBUTTONUP) { if (IsWindowVisible(hw)) { ShowWindow(hw, SW_HIDE); g.userHidden = true; } else { ShowWindow(hw, SW_SHOWNOACTIVATE); g.userHidden = false; g.autoHidden = false; Render(); } } else if (LOWORD(lp) == WM_RBUTTONUP) Menu(hw); return 0;
    case WM_CLOSE: DestroyWindow(hw); return 0;
    case WM_DESTROY: KillTimer(hw, 1); Shell_NotifyIconW(NIM_DELETE, &g.nid); if (g.ico) DestroyIcon(g.ico); g.ico = nullptr; PostQuitMessage(0); return 0;
    default: if (m == g.taskbarMsg && g.taskbarMsg) { g.trayOk = false; g.icoVal = -1; g.ico = nullptr; Tray(); return 0; }
    }
    return DefWindowProcW(hw, m, wp, lp);
}

int Run(HINSTANCE hi) {
    HANDLE mx = CreateMutexW(nullptr, TRUE, Test() ? L"Local\\LibraMeterTest" : L"Local\\LibraMeter");
    if (GetLastError() == ERROR_ALREADY_EXISTS) { if (HWND w = Find()) { ShowWindow(w, SW_SHOWNOACTIVATE); } return 0; }   // already running: just bring it back
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    G::GdiplusStartupInput gin; ULONG_PTR tok; G::GdiplusStartup(&tok, &gin, nullptr);
    g.set.Load(); g.set.Set(L"meter_on", L"1");   // started on purpose (Settings, Start with Windows): it is "on"
    WNDCLASSEXW wc{ sizeof wc }; wc.lpfnWndProc = Proc; wc.hInstance = hi; wc.lpszClassName = Cls(); wc.hCursor = LoadCursor(nullptr, IDC_SIZEALL); RegisterClassExW(&wc);
    g.set.Load(); g.card = g.set.Get(L"meter_size") == L"card"; g.top = g.set.Get(L"meter_top", L"1") != L"0"; g.dock = g.set.Get(L"meter_dock", L"1") != L"0";
    POINT pos = { _wtoi(g.set.Get(L"meter_x", L"-99999").c_str()), _wtoi(g.set.Get(L"meter_y", L"-99999").c_str()) }; bool saved = g.set.Get(L"meter_x") != L"" && MonitorFromPoint(pos, MONITOR_DEFAULTTONULL);
    HMONITOR hm = MonitorFromPoint(saved ? pos : POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY); UINT dx = 96, dy = 96; GetDpiForMonitor(hm, MDT_EFFECTIVE_DPI, &dx, &dy); g.dpi = dx;
    int w, h; CardSize(w, h); g.wk = WorkAt(saved ? pos : POINT{ 0, 0 }); g.ex = saved ? pos.x : g.wk.right - w - 24 * (int)K(); g.ey = saved ? pos.y : g.wk.bottom - h - 24 * (int)K();
    g.hw = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | (g.top ? WS_EX_TOPMOST : 0), Cls(), L"Libra Meter", WS_POPUP, g.ex, g.ey, w, h, nullptr, nullptr, hi, nullptr);
    if (!g.hw) return 1;
    if (PdhOpenQueryW(nullptr, 0, &g.q) == ERROR_SUCCESS) {
        g.pdh = PdhAddEnglishCounterW(g.q, L"\\Processor Information(_Total)\\% Processor Utility", 0, &g.cCpu) == ERROR_SUCCESS;
        PdhAddEnglishCounterW(g.q, L"\\PhysicalDisk(_Total)\\% Idle Time", 0, &g.cIdle); if (g.pdh) PdhCollectQueryData(g.q); }   // first collect only sets the baseline
    ApplySettings(true); Sleep(250); Sample(); g.disp = g.cpu; g.taskbarMsg = RegisterWindowMessageW(L"TaskbarCreated");
    g.nid = { sizeof(g.nid) }; g.nid.hWnd = g.hw; g.nid.uID = 1; g.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP; g.nid.uCallbackMessage = WM_TRAY; Tray();
    Apply(); ShowWindow(g.hw, SW_SHOWNOACTIVATE); Render(); SetTimer(g.hw, 1, 1000, nullptr);
    if (g.dock) { DockNearest(); g.t = g.tTarget = 0; g.leaveAt = 0; Apply(); Render(); }   // shown for a few seconds so you see where it lives, then it slides into the edge
    MSG msg; while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    G::GdiplusShutdown(tok); if (mx) CloseHandle(mx); return 0;
}
}
