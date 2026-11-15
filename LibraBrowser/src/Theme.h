#pragma once
#include "pch.h"

// --- Colour Tokens: white, thin black edges, nothing else ----------------------
// No accent colours, no gradients. Incognito inverts to black with white edges.
namespace Color {
    constexpr COLORREF Surface        = RGB(236, 236, 236);  // window / tab strip
    constexpr COLORREF SurfaceHigh    = RGB(236, 236, 236);
    constexpr COLORREF ToolbarBg      = RGB(236, 236, 236);
    constexpr COLORREF TabActive      = RGB(255, 255, 255);
    constexpr COLORREF TabInactive    = RGB(236, 236, 236);
    constexpr COLORREF TabHover       = RGB(246, 246, 246);
    constexpr COLORREF OmniboxBg      = RGB(255, 255, 255);
    constexpr COLORREF OmniboxBorder  = RGB(255, 255, 255);   // thin black edge
    constexpr COLORREF OmniboxFocus   = RGB(0,   0,   0);
    constexpr COLORREF TextPrimary    = RGB(0,   0,   0);
    constexpr COLORREF TextSecondary  = RGB(102, 102, 102);
    constexpr COLORREF Accent         = RGB(0,   0,   0);
    constexpr COLORREF AccentHover    = RGB(51,  51,  51);
    constexpr COLORREF ButtonHover    = RGB(246, 246, 246);
    constexpr COLORREF ButtonPress    = RGB(255, 255, 255);
    constexpr COLORREF Border         = RGB(236, 236, 236);
    constexpr COLORREF ProgressBar    = RGB(0,   0,   0);
    constexpr COLORREF ErrorRed       = RGB(0,   0,   0);
    constexpr COLORREF SecureGreen    = RGB(0,   0,   0);
    constexpr COLORREF TabClose       = RGB(102, 102, 102);
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
