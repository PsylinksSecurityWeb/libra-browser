#include "pch.h"
#include "AdBlocker.h"

// ─── Minimal built-in block list (top ad/tracker domains) ─────────────────
// In production this would be downloaded from a URL like AdGuard's list.
static const wchar_t* kDefaultBlockList[] = {
    L"doubleclick.net", L"googlesyndication.com", L"googleadservices.com",
    L"google-analytics.com", L"googletagmanager.com", L"googletagservices.com",
    L"adnxs.com", L"adsrvr.org", L"advertising.com", L"scorecardresearch.com",
    L"outbrain.com", L"taboola.com", L"moatads.com", L"adsafeprotected.com",
    L"pubmatic.com", L"rubiconproject.com", L"openx.net", L"casalemedia.com",
    L"criteo.com", L"criteo.net", L"mxpnl.com", L"segment.io", L"segment.com",
    L"hotjar.com", L"fullstory.com", L"logrocket.com", L"newrelic.com",
    L"nr-data.net", L"cdn.mxpanel.com", L"static.ads-twitter.com",
    L"ads.linkedin.com", L"ads.facebook.com", L"pixel.facebook.com",
    L"facebook.com/plugins/pixel", L"sp.analytics.yahoo.com",
    L"analytics.tiktok.com", L"bat.bing.com", L"c.bing.com",
    L"pagead2.googlesyndication.com", L"tpc.googlesyndication.com",
    L"stats.g.doubleclick.net", L"cm.g.doubleclick.net",
    L"track.adform.net", L"smartadserver.com", L"adtech.de",
    L"yieldmanager.com", L"contextweb.com", L"mathtag.com",
    L"demdex.net", L"turn.com", L"bluekai.com", L"krxd.net",
    L"quantserve.com", L"rlcdn.com", L"agkn.com", L"bidswitch.net",
    L"tradedesk.net", L"thetradedesk.com",
    nullptr // sentinel
};

void AdBlocker::LoadDefaults() {
    for (int i = 0; kDefaultBlockList[i]; ++i)
        m_domains.insert(kDefaultBlockList[i]);
}

void AdBlocker::LoadFile(const std::wstring& path) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;

    DWORD size = GetFileSize(hFile, nullptr);
    if (size == 0 || size > 20 * 1024 * 1024) { CloseHandle(hFile); return; }

    std::string buf(size, '\0');
    DWORD read = 0;
    ReadFile(hFile, buf.data(), size, &read, nullptr);
    CloseHandle(hFile);

    std::istringstream ss(buf);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '!') continue;
        // Strip leading "0.0.0.0 " or "127.0.0.1 " prefix (hosts format)
        size_t sp = line.find(' ');
        std::string domain = (sp != std::string::npos) ? line.substr(sp + 1) : line;
        // Trim whitespace and CR
        while (!domain.empty() && (domain.back() == '\r' || domain.back() == ' '))
            domain.pop_back();
        if (domain.empty() || domain[0] == '#') continue;
        // Convert to wstring
        int len = MultiByteToWideChar(CP_UTF8, 0, domain.c_str(), -1, nullptr, 0);
        std::wstring wdomain(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, domain.c_str(), -1, wdomain.data(), len);
        wdomain.resize(len - 1); // remove null term
        m_domains.insert(std::move(wdomain));
    }
}

/*static*/
std::wstring AdBlocker::ExtractHost(const wchar_t* url) {
    if (!url) return {};
    // Skip scheme (https://, http://)
    const wchar_t* p = url;
    const wchar_t* scheme = wcsstr(p, L"://");
    if (scheme) p = scheme + 3;
    // Find end of host (before '/', '?', '#', ':')
    const wchar_t* end = p;
    while (*end && *end != L'/' && *end != L'?' && *end != L'#' && *end != L':')
        ++end;
    return std::wstring(p, end);
}

bool AdBlocker::ShouldBlock(const wchar_t* url) const {
    if (!url) return false;
    std::wstring host = ExtractHost(url);
    if (host.empty()) return false;
    // Check full host, then strip subdomains progressively
    // e.g. "sub.ads.example.com" → check → "ads.example.com" → "example.com"
    std::wstring check = host;
    while (true) {
        if (m_domains.count(check)) {
            ++m_blocked;
            return true;
        }
        size_t dot = check.find(L'.');
        if (dot == std::wstring::npos || check.find(L'.', dot + 1) == std::wstring::npos)
            break; // no more parent domains worth checking
        check = check.substr(dot + 1);
    }
    return false;
}
