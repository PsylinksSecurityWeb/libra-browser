#pragma once
#include "pch.h"

// --- Colour Tokens: white, thin black edges, nothing else ----------------------
// No accent colours, no gradients. Incognito inverts to black with white edges.
namespace Color {
    inline COLORREF Surface        = RGB(236, 236, 236);  // window / tab strip
    inline COLORREF SurfaceHigh    = RGB(236, 236, 236);
    inline COLORREF ToolbarBg      = RGB(236, 236, 236);
    inline COLORREF TabActive      = RGB(255, 255, 255);
    inline COLORREF TabInactive    = RGB(236, 236, 236);
    inline COLORREF TabHover       = RGB(246, 246, 246);
    inline COLORREF OmniboxBg      = RGB(255, 255, 255);
    inline COLORREF OmniboxBorder  = RGB(255, 255, 255);   // thin black edge
    inline COLORREF OmniboxFocus   = RGB(0,   0,   0);
    inline COLORREF TextPrimary    = RGB(0,   0,   0);
    inline COLORREF TextSecondary  = RGB(102, 102, 102);
    inline COLORREF Accent         = RGB(0,   0,   0);
    inline COLORREF AccentHover    = RGB(51,  51,  51);
    inline COLORREF ButtonHover    = RGB(246, 246, 246);
    inline COLORREF ButtonPress    = RGB(255, 255, 255);
    inline COLORREF Border         = RGB(236, 236, 236);
    inline COLORREF ProgressBar    = RGB(0,   0,   0);
    constexpr COLORREF ErrorRed       = RGB(0,   0,   0);
    constexpr COLORREF SecureGreen    = RGB(0,   0,   0);
    inline COLORREF TabClose       = RGB(102, 102, 102);
    constexpr COLORREF Shadow         = RGB(0,   0,   0);

    // Incognito: still bright, just a light-grey tint
    constexpr COLORREF IncognitoChrome = RGB(32, 33, 36);
    constexpr COLORREF IncognitoTabBg  = RGB(41, 42, 46);  // toolbar
    constexpr COLORREF IncognitoText   = RGB(240, 240, 240);
    constexpr COLORREF DarkSurface     = RGB(32, 33, 36);
    constexpr COLORREF DarkToolbar     = RGB(41, 42, 46);
    constexpr COLORREF DarkTab         = RGB(32, 33, 36);
    constexpr COLORREF DarkTabActive   = RGB(41, 42, 46);
    constexpr COLORREF DarkOmnibox     = RGB(60, 62, 68);
    constexpr COLORREF DarkText        = RGB(240, 240, 240);
    constexpr COLORREF DarkBorder      = RGB(70, 72, 78);
}

// --- Layout Metrics (96-dpi px) ----------------------------------------------
namespace Layout {
    constexpr int TabBarHeight    = 42;
    constexpr int ToolbarHeight   = 52;
    constexpr int TabMinWidth     = 84;
    constexpr int TabMaxWidth     = 220;
    constexpr int TabPaddingH     = 12;
    constexpr int TabPaddingV     = 8;
    constexpr int TabCloseSize    = 18;
    constexpr int TabMargin       = 8;
    constexpr int TabInset        = 5;
    constexpr int TabGap          = 4;
    constexpr int TabRadius       = 8;
    constexpr int OmniboxHeight   = 34;
    constexpr int ButtonSize      = 34;
    constexpr int ToolbarPadding  = 8;
    constexpr int ProgressHeight  = 2;
    constexpr int StatusBarHeight = 22;
}

// --- DPI-aware helpers ---------------------------------------------------------
inline int Scale(int px, UINT dpi = 96) {
    return MulDiv(px, static_cast<int>(dpi), 96);
}

// --- Colour mode: one picked colour tints the whole browser (toolbar, tabs, address bar, icons, internal pages) -----
#include <cmath>
#include <string>
namespace Color {
    struct Hsl { double h, s, l; };
    inline Hsl ToHsl(COLORREF c) {
        double r = GetRValue(c) / 255., g = GetGValue(c) / 255., b = GetBValue(c) / 255.;
        double mx = (std::max)({ r, g, b }), mn = (std::min)({ r, g, b }), l = (mx + mn) / 2, d = mx - mn, h = 0, s = 0;
        if (d > 0) { s = d / (1 - std::fabs(2 * l - 1)); if (mx == r) h = std::fmod((g - b) / d + 6, 6); else if (mx == g) h = (b - r) / d + 2; else h = (r - g) / d + 4; h *= 60; }
        return { h, s, l };
    }
    inline COLORREF FromHsl(double h, double s, double l) {
        double c = (1 - std::fabs(2 * l - 1)) * s, x = c * (1 - std::fabs(std::fmod(h / 60, 2) - 1)), m = l - c / 2, r = 0, g = 0, b = 0;
        switch ((int)(h / 60) % 6) { case 0: r = c; g = x; break; case 1: r = x; g = c; break; case 2: g = c; b = x; break; case 3: g = x; b = c; break; case 4: r = x; b = c; break; default: r = c; b = x; }
        return RGB((int)((r + m) * 255 + .5), (int)((g + m) * 255 + .5), (int)((b + m) * 255 + .5));
    }
    inline bool AccentOn = false; inline Hsl Ac{ 0, 0, 0 };
    inline double Cl(double v, double a, double b) { return v < a ? a : v > b ? b : v; }
    inline COLORREF Tint(double l) { return FromHsl(Ac.h, (std::min)(0.6, Ac.s * 0.6), l); }   // pale version of the picked colour
    // on=false restores the black/grey default
    inline void ApplyAccent(bool on, COLORREF a = 0) {
        AccentOn = on; if (on) Ac = ToHsl(a);
        if (!on) {
            Surface = SurfaceHigh = ToolbarBg = TabInactive = RGB(236, 236, 236); TabActive = OmniboxBg = OmniboxBorder = ButtonPress = RGB(255, 255, 255);
            TabHover = ButtonHover = RGB(246, 246, 246); TextPrimary = Accent = OmniboxFocus = ProgressBar = RGB(0, 0, 0); TextSecondary = TabClose = RGB(102, 102, 102);
            AccentHover = RGB(51, 51, 51); Border = RGB(236, 236, 236); return;
        }
        Surface = SurfaceHigh = ToolbarBg = TabInactive = Tint(0.88); Border = Tint(0.82);
        TabActive = OmniboxBg = OmniboxBorder = ButtonPress = Tint(0.985); TabHover = ButtonHover = Tint(0.94);
        TextPrimary = Accent = OmniboxFocus = ProgressBar = FromHsl(Ac.h, Ac.s * 0.9, Cl(Ac.l, 0.2, 0.38));
        AccentHover = FromHsl(Ac.h, Ac.s * 0.8, Cl(Ac.l, 0.3, 0.5)); TextSecondary = TabClose = FromHsl(Ac.h, Ac.s * 0.5, 0.45);
    }
    inline std::wstring Hex(COLORREF c) { wchar_t b[10]; swprintf(b, 10, L"#%02x%02x%02x", GetRValue(c), GetGValue(c), GetBValue(c)); return b; }
}
// Internal pages: swap the page's black and grey for the colour-mode palette (same tokens the CSS already uses).
inline void ReplHex(std::wstring& s, const wchar_t* from, const std::wstring& to) {
    size_t n = wcslen(from), p = 0;
    while ((p = s.find(from, p)) != std::wstring::npos) { wchar_t nx = p + n < s.size() ? s[p + n] : L' '; if (iswxdigit(nx)) { p += n; continue; } s.replace(p, n, to); p += to.size(); }
}
inline std::wstring Themed(std::wstring h) {
    if (!Color::AccentOn) return h;
    using namespace Color;
    ReplHex(h, L"#111", Hex(TextPrimary));
    for (const wchar_t* g : { L"#f3f3f3", L"#f4f4f4", L"#f5f5f5", L"#f0f0f0", L"#efefef", L"#eeeeee", L"#eee" }) ReplHex(h, g, Hex(Tint(0.94)));
    for (const wchar_t* g : { L"#ececec", L"#e8e8e8" }) ReplHex(h, g, Hex(Tint(0.91)));
    for (const wchar_t* g : { L"#e6e6e6", L"#e5e5e5", L"#e2e2e2", L"#ddd" }) ReplHex(h, g, Hex(Tint(0.86)));
    for (const wchar_t* g : { L"#fafafa", L"#f8f8f8", L"#f7f7f7" }) ReplHex(h, g, Hex(Tint(0.975)));
    size_t p = h.find(L"</head>"); if (p != std::wstring::npos) h.insert(p, L"<style>:root{--chipbg:" + Hex(Tint(0.972)) + L"}html,body{background:" + Hex(Tint(0.972)) + L"!important}</style>");
    return h;
}
