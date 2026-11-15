// Path rules: ads that live on a site's OWN server (so the DNS blocklist cannot see them) are refused by their address before the request is sent.
// A short reviewed list, written for Libra Browser. Deliberately specific: never a bare "/banners/" or the word "advertising" (real sites keep real pictures there).
// Max mode only (the shield's Balanced and Off keep working as the escape).
#pragma once
#include <string>
#include <vector>

struct AdPathRule { const wchar_t* host; const wchar_t* path; };   // host "" = any site; path lower-case, a trailing '$' means "the address must end here"
inline const AdPathRule kAdPathRules[] = {
    { L"adblock-tester.com", L"/banners/" },   // the AdBlock Tester's own banner folder (EasyList carries the same rule)
    { L"", L".swf$" },                         // Flash is dead and the engine cannot run it: such a file is only ever an empty box
    { L"", L"/ads/banners/" }, { L"", L"/ads/images/" }, { L"", L"/ads/image/" }, { L"", L"/ads/img/" },
    { L"", L"/adbanner." }, { L"", L"/adbanners/" }, { L"", L"/ad_banner." }, { L"", L"/ad-banner." },
    { L"", L"/banner_ads/" }, { L"", L"/bannerads/" }, { L"", L"/banners/ads/" }, { L"", L"/advertising/banners/" },
    { L"", L"_ads_banner." }, { L"", L"-ads-banner." },
};

inline std::wstring AdLowerW(std::wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }
inline void AdSplitUrl(const std::wstring& url, std::wstring& host, std::wstring& path) {   // lower-case host and path of an http(s) address, no query, no fragment
    host.clear(); path.clear();
    size_t s = url.find(L"://"); if (s == std::wstring::npos || url.size() > 4000) return;
    std::wstring sch = AdLowerW(url.substr(0, s)); if (sch != L"http" && sch != L"https") return;
    size_t e = url.find_first_of(L"/?#", s + 3);
    std::wstring auth = url.substr(s + 3, e == std::wstring::npos ? std::wstring::npos : e - (s + 3));
    size_t at = auth.rfind(L'@'); if (at != std::wstring::npos) auth = auth.substr(at + 1);
    size_t c = auth.find(L':'); if (c != std::wstring::npos) auth.resize(c);
    host = AdLowerW(auth);
    if (e == std::wstring::npos || url[e] != L'/') { path = L"/"; return; }
    size_t q = url.find_first_of(L"?#", e); path = AdLowerW(url.substr(e, q == std::wstring::npos ? std::wstring::npos : q - e));
}
inline const std::pair<std::wstring, std::wstring>& AdExtraRule() {   // tests only (needs libra-test.enabled): LIBRA_TEST_ADRULE="host|path"
    static std::pair<std::wstring, std::wstring> r; static bool done = false;
    if (!done) { done = true; wchar_t b[300]; if (TestEnv(L"LIBRA_TEST_ADRULE", b, 300)) { std::wstring v = b; size_t p = v.find(L'|'); if (p != std::wstring::npos) r = { AdLowerW(v.substr(0, p)), AdLowerW(v.substr(p + 1)) }; } }
    return r;
}
inline bool AdRuleMatch(const std::wstring& rh, const std::wstring& rpIn, const std::wstring& host, const std::wstring& path) {
    if (rpIn.empty()) return false;
    if (!rh.empty() && host != rh && !(host.size() > rh.size() + 1 && host.compare(host.size() - rh.size() - 1, rh.size() + 1, L"." + rh) == 0)) return false;
    std::wstring rp = rpIn; const bool end = rp.back() == L'$'; if (end) rp.pop_back();
    return end ? (path.size() >= rp.size() && path.compare(path.size() - rp.size(), rp.size(), rp) == 0) : path.find(rp) != std::wstring::npos;
}
inline bool AdPathHit(const std::wstring& host, const std::wstring& path) {
    for (auto& r : kAdPathRules) if (AdRuleMatch(r.host, r.path, host, path)) return true;
    return AdRuleMatch(AdExtraRule().first, AdExtraRule().second, host, path);
}
inline std::vector<std::wstring> AdRuleFilters() {   // the engine only calls us for addresses that match one of these wildcards: every other request costs nothing
    std::vector<std::wstring> f;
    auto add = [&](std::wstring h, std::wstring p) { if (p.empty()) return; if (p.back() == L'$') p.pop_back(); f.push_back(L"*" + (h.empty() ? std::wstring() : h + L"*") + p + L"*"); };
    for (auto& r : kAdPathRules) add(r.host, r.path);
    add(AdExtraRule().first, AdExtraRule().second);
    return f;
}
inline std::wstring AdRulesJson() {   // same rules for the page script, which removes what is left over
    std::wstring o = L"[";
    auto add = [&](std::wstring h, std::wstring p) { if (p.empty()) return; if (o.size() > 1) o += L","; o += L"[\"" + h + L"\",\"" + p + L"\"]"; };
    for (auto& r : kAdPathRules) add(r.host, r.path);
    add(AdExtraRule().first, AdExtraRule().second);
    return o + L"]";
}

inline void AdTestLog(const std::wstring& url) {   // tests only (needs libra-test.enabled): LIBRA_TEST_ADLOG=<file> gets one line per refused address
    wchar_t p[MAX_PATH]; if (!TestEnv(L"LIBRA_TEST_ADLOG", p, MAX_PATH)) return;
    HANDLE f = CreateFileW(p, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr); if (f == INVALID_HANDLE_VALUE) return;
    std::string l = ToUtf8(url) + "\n"; DWORD w; WriteFile(f, l.data(), (DWORD)l.size(), &w, nullptr); CloseHandle(f);
}
