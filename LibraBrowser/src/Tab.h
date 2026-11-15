#pragma once
#include "pch.h"

// Represents one browser tab (owns a WebView2 controller + content webview)
struct Tab {
    std::wstring title    = L"New Tab";
    std::wstring url      = L"";
    bool         loading  = false;
    bool         canBack  = false;
    bool         canFwd   = false;
    bool         incognito = false;
    bool         pinned    = false;
    bool         idleAsked = false;   // already asked 'close this unused tab?'
    DWORD        hiddenSince = 0;   // when the tab was last put in the background (0 = visible)
    bool         suspended   = false;
    DWORD        dlTick    = 0;      // when a download last started from this tab
    DWORD        errTick   = 0;      // when the error page was shown (to undo it if a download started right after)
    int          letThrough = 0;                      // ads allowed through (Balanced mode)
    std::vector<std::pair<std::wstring, long long>> vids;   // videos found on this page (Save videos)
    bool         vidOn = false; EventRegistrationToken tokVid{};
    std::wstring fpScriptId, sgScriptId, upHost, upUrl; DWORD upTick = 0;   // fingerprint script id; https-upgrade bookkeeping
    int          blockedAds = 0;                      // of those, the ones that looked like ad networks
    int          blockedReq = 0;                      // requests the DNS refused while this page was open
    std::unordered_map<std::string, int> blockedHosts;   // host -> hits    // ...from how many different ad/tracker servers
    std::wstring scriptId;                            // injected page script (re-registered when a site is allowed)
    float        shown     = 0.f;      // animated load progress painted behind the tab (0 = none)
    bool         dlNav     = false;     // the navigation in flight turned into a download
    std::wstring prevUrl;               // page shown before the current navigation started
    std::shared_ptr<void> fav;          // Gdiplus::Bitmap of the site icon (null = letter badge)
    std::wstring renderedUrl;       // internal page currently shown (to keep scroll on re-render)
    std::wstring allowUri;          // one-shot: a download we re-started after seeing a real click
    bool         navUser = true;    // was the newest navigation started by a real user action?
    DWORD        navTick = 0;
    UINT64       navId = 0;         // newest navigation; older completions are ignored
    std::wstring errUrl, errKind;   // set while libra://error is shown
    float        progress  = 0.f;   // 0..1 during load

    wil::com_ptr<ICoreWebView2Controller> controller;
    wil::com_ptr<ICoreWebView2>           webView;
    std::vector<wil::com_ptr<ICoreWebView2Frame2>> frames;   // embedded frames (also other-site ones), tracked for the Agent

    // Event registration tokens — stored so we can unregister on close
    EventRegistrationToken tokNavStart   {};
    EventRegistrationToken tokNavComplete{};
    EventRegistrationToken tokTitleChange{};
    EventRegistrationToken tokResReq     {};
    EventRegistrationToken tokHistChange {};
    EventRegistrationToken tokDocTitleChange{};
    EventRegistrationToken tokFavIcon    {};
    EventRegistrationToken tokNewWindow  {};
};
