// Filter rules for ads that live on a site's OWN domain (the DNS blocklist cannot see them). Max mode only; the shield's Balanced / Off are the escape.
// Rule syntax is a safe subset of the Adblock Plus / uBlock list format, so a user's own list file works:
//   network   ||host^path   /path/part   |http://start   end|     with  @@ (allow)  and  $third-party  $~third-party  $image  $script  $xmlhttprequest  $stylesheet  $media  $other  $domain=a.com|~b.com
//   cosmetic  ##selector   domain1,domain2##selector      (procedural / scriptlet / regex rules are ignored, never guessed)
// Pure header (no Windows calls) so tools/tests/filtertest.cpp can check it on its own.
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

namespace Flt {
enum Type : unsigned { T_IMAGE = 1, T_SCRIPT = 2, T_XHR = 4, T_OTHER = 8, T_STYLE = 16, T_MEDIA = 32 };

struct Rule {
    std::string pat, host;            // pat: lower-case pattern after the host (or the whole pattern); host: for || rules
    bool hostAnchor = false, startAnchor = false, endAnchor = false, exc = false;
    int third = 0;                    // 0 any, 1 third-party only, 2 first-party only
    unsigned types = 0;               // 0 = every type
    std::vector<std::string> inDom, notDom;
};
struct Cos { std::string doms, sel; };   // doms: comma list ("" = every site); negated entries start with ~

inline std::string Low(std::string s) { for (auto& c : s) if (c >= 'A' && c <= 'Z') c += 32; return s; }
inline std::string Trim(const std::string& s) { size_t a = s.find_first_not_of(" \t\r\n"), b = s.find_last_not_of(" \t\r\n"); return a == std::string::npos ? "" : s.substr(a, b - a + 1); }
inline bool HostIn(const std::string& h, const std::string& d) { return h == d || (h.size() > d.size() + 1 && h.compare(h.size() - d.size() - 1, d.size() + 1, "." + d) == 0); }
inline bool IsSep(char c) { return !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '-' || c == '.' || c == '%'); }
inline bool AlNum(char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z'); }

// does the pattern (with * and ^) match the text starting exactly at ui? (end anchor: the match must reach the end)
inline bool MatchAt(const std::string& p, size_t pi, const std::string& u, size_t ui, bool endAnchor) {
    while (pi < p.size()) {
        char c = p[pi];
        if (c == '*') { while (pi < p.size() && p[pi] == '*') ++pi; if (pi == p.size()) return true; for (size_t k = ui; k <= u.size(); ++k) if (MatchAt(p, pi, u, k, endAnchor)) return true; return false; }
        if (c == '^') { if (ui == u.size()) { ++pi; continue; } if (!IsSep(u[ui])) return false; ++pi; ++ui; continue; }
        if (ui >= u.size() || u[ui] != c) return false; ++pi; ++ui;
    }
    return !endAnchor || ui == u.size();
}

struct Set {
    std::vector<Rule> rules; std::vector<Cos> cos;
    std::unordered_map<std::string, std::vector<int>> byHost, byTok; std::vector<int> rest;
    size_t Count() const { return rules.size(); }
    bool Empty() const { return rules.empty(); }
    // Engine-side address filters that are a superset of the block rules, so only addresses that may match ever reach Libra's thread.
    // Empty result = cannot narrow (too many or too loose rules): the caller keeps the catch-all filter.
    std::vector<std::wstring> Prefilters() const {
        std::vector<std::wstring> o;
        for (auto& r : rules) {
            if (r.exc) continue;
            std::string p = r.hostAnchor ? r.host + (r.pat.empty() ? "" : "*" + r.pat) : r.pat;
            std::replace(p.begin(), p.end(), '^', '*'); while (!p.empty() && p.back() == '|') p.pop_back(); while (!p.empty() && p.front() == '|') p.erase(0, 1);
            size_t lit = 0; for (char c : p) if (c != '*') ++lit;
            if (lit < 4 || o.size() >= 400) return {};
            o.push_back(L"*" + std::wstring(p.begin(), p.end()) + L"*");
        }
        std::sort(o.begin(), o.end()); o.erase(std::unique(o.begin(), o.end()), o.end()); return o;
    }

    static bool SafeSel(const std::string& s) {   // a selector is injected as CSS: no braces, no escapes, no urls, nothing procedural
        if (s.empty() || s.size() > 300) return false;
        for (char c : s) { if (c == '{' || c == '}' || c == ';' || c == '<' || c == '\\' || c == '\n' || c == '\r') return false; }
        std::string l = Low(s);
        for (const char* bad : { "url(", "expression(", "javascript:", ":xpath", ":-abp-", ":has-text", ":matches-css", ":contains(", ":upward", ":remove(", ":style(", "+js(", ":not(:has-text", "@import" }) if (l.find(bad) != std::string::npos) return false;
        return true;
    }
    static bool SafeDomain(const std::string& d) { if (d.empty() || d.size() > 100) return false; for (char c : d) if (!(AlNum(c) || c == '.' || c == '-')) return false; return d.find('.') != std::string::npos; }

    // 0 = ignored line, 1 = network rule, 2 = cosmetic rule
    int Add(const std::string& line0) {
        std::string line = Trim(line0);
        if (line.empty() || line[0] == '!' || line[0] == '[' || line.size() > 600) return 0;
        size_t cp = line.find("##");
        if (cp != std::string::npos && line.find("#@#") == std::string::npos && line.find("#?#") == std::string::npos && line.find("#$#") == std::string::npos) {
            std::string doms = Trim(line.substr(0, cp)), sel = Trim(line.substr(cp + 2));
            if (!SafeSel(sel)) return 0;
            std::string clean; size_t i = 0;
            while (i <= doms.size()) { size_t e = doms.find(',', i); if (e == std::string::npos) e = doms.size(); std::string d = Low(doms.substr(i, e - i)); std::string bare = !d.empty() && d[0] == '~' ? d.substr(1) : d; if (!d.empty()) { if (!SafeDomain(bare)) return 0; clean += (clean.empty() ? "" : ",") + d; } i = e + 1; }
            cos.push_back({ clean, sel }); return 2;
        }
        if (line.find("#@#") != std::string::npos || line.find("#?#") != std::string::npos || line.find("#$#") != std::string::npos || line.find("##+js") != std::string::npos) return 0;
        Rule r; std::string body = line;
        if (body.compare(0, 2, "@@") == 0) { r.exc = true; body = body.substr(2); }
        size_t dp = body.rfind('$');
        if (dp != std::string::npos) {
            std::string opt = body.substr(dp + 1); bool optOk = !opt.empty(); for (char c : opt) if (!(AlNum(c) || c == ',' || c == '~' || c == '=' || c == '|' || c == '.' || c == '-' || c == '_')) optOk = false;
            if (optOk) {
                body = body.substr(0, dp); size_t i = 0;
                while (i <= opt.size()) {
                    size_t e = opt.find(',', i); if (e == std::string::npos) e = opt.size(); std::string o = opt.substr(i, e - i); i = e + 1; if (o.empty()) continue;
                    bool neg = o[0] == '~'; std::string n = neg ? o.substr(1) : o;
                    if (n == "third-party" || n == "3p") r.third = neg ? 2 : 1; else if (n == "first-party" || n == "1p") r.third = neg ? 1 : 2;
                    else if (n.compare(0, 7, "domain=") == 0) { std::string v = n.substr(7); size_t j = 0; while (j <= v.size()) { size_t k = v.find('|', j); if (k == std::string::npos) k = v.size(); std::string d = v.substr(j, k - j); j = k + 1; if (d.empty()) continue; bool nd = d[0] == '~'; std::string bare = nd ? d.substr(1) : d; if (!SafeDomain(bare)) return 0; (nd ? r.notDom : r.inDom).push_back(bare); } }
                    else if (neg) return 0;   // "everything except one type": not supported, never guessed
                    else if (n == "image") r.types |= T_IMAGE; else if (n == "script") r.types |= T_SCRIPT; else if (n == "xmlhttprequest" || n == "xhr" || n == "fetch") r.types |= T_XHR;
                    else if (n == "stylesheet" || n == "css") r.types |= T_STYLE; else if (n == "media") r.types |= T_MEDIA; else if (n == "other") r.types |= T_OTHER;
                    else if (n == "important" || n == "match-case" || n == "all") {}
                    else return 0;   // popup, document, subdocument, csp=, redirect=, removeparam, badfilter ...: unsupported, the rule is dropped
                }
            }
        }
        if (body.empty() || body[0] == '/' && body.size() > 1 && body.back() == '/' && body.find('*') == std::string::npos && body.find('\\') != std::string::npos) return 0;   // regex rule
        if (body.find('\\') != std::string::npos) return 0;
        body = Low(body);
        if (body.compare(0, 2, "||") == 0) {
            r.hostAnchor = true; body = body.substr(2); size_t e = body.find_first_of("/^*?:|"); std::string h = body.substr(0, e); if (!SafeDomain(h)) return 0;
            r.host = h; r.pat = e == std::string::npos ? "" : body.substr(e);
        } else {
            if (body[0] == '|') { r.startAnchor = true; body = body.substr(1); }
            r.pat = body;
        }
        if (!r.pat.empty() && r.pat.back() == '|') { r.endAnchor = true; r.pat.pop_back(); }
        if (!r.hostAnchor) {
            std::string core = r.pat; core.erase(std::remove(core.begin(), core.end(), '*'), core.end()); core.erase(std::remove(core.begin(), core.end(), '^'), core.end());
            if (core.size() < 5) return 0;   // too short to be specific: would hide real content
        }
        int idx = (int)rules.size(); rules.push_back(r);
        if (r.hostAnchor) { byHost[r.host].push_back(idx); return 1; }
        std::string best, cur; auto flush = [&] { if (cur.size() > best.size()) best = cur; cur.clear(); };
        for (char c : r.pat) { if (AlNum(c)) cur += c; else flush(); } flush();   // the longest word in the pattern is its index key
        if (best.size() >= 4) byTok[best].push_back(idx); else if (rest.size() < 400) rest.push_back(idx); else { rules.pop_back(); return 0; }
        return 1;
    }
    int AddText(const std::string& text, size_t maxRules = 60000) {
        int n = 0; size_t i = 0;
        while (i < text.size() && rules.size() + cos.size() < maxRules) { size_t e = text.find('\n', i); if (e == std::string::npos) e = text.size(); if (Add(text.substr(i, e - i))) ++n; i = e + 1; }
        return n;
    }

    static bool ParseUrl(const std::string& u, std::string& host, size_t& hostEnd) {   // host (lower-case, no port/userinfo) and the position after it
        size_t s = u.find("://"); if (s == std::string::npos) return false; s += 3; size_t e = u.find_first_of("/?#", s); if (e == std::string::npos) e = u.size();
        size_t at = u.rfind('@', e); size_t hs = (at != std::string::npos && at >= s && at < e) ? at + 1 : s; size_t pe = u.find(':', hs); size_t he = (pe != std::string::npos && pe < e) ? pe : e;
        host = u.substr(hs, he - hs); hostEnd = he; return !host.empty();
    }
    bool Ok(const Rule& r, const std::string& pageHost, bool third, unsigned type) const {
        if (r.third == 1 && !third) return false; if (r.third == 2 && third) return false;
        if (r.types && !(r.types & type)) return false;
        if (!r.inDom.empty()) { bool in = false; for (auto& d : r.inDom) if (HostIn(pageHost, d)) in = true; if (!in) return false; }
        for (auto& d : r.notDom) if (HostIn(pageHost, d)) return false;
        return true;
    }
    bool Hit(const Rule& r, const std::string& u, size_t hostEnd) const {
        if (r.hostAnchor) {
            if (MatchAt(r.pat, 0, u, hostEnd, r.endAnchor)) return true;
            if (hostEnd < u.size() && u[hostEnd] == ':') { size_t p = hostEnd + 1; while (p < u.size() && u[p] >= '0' && u[p] <= '9') ++p; return p > hostEnd + 1 && MatchAt(r.pat, 0, u, p, r.endAnchor); }   // a port number does not hide the path
            return false;
        }
        if (r.startAnchor) return MatchAt(r.pat, 0, u, 0, r.endAnchor);
        for (size_t i = 0; i <= u.size(); ++i) if (MatchAt(r.pat, 0, u, i, r.endAnchor)) return true;
        return false;
    }
    // should this request be refused?  url any case; pageHost = the page's host; third = request is on another site than the page
    bool Block(const std::string& url0, const std::string& pageHost0, bool third, unsigned type) const {
        if (rules.empty() || url0.size() > 3000) return false;
        std::string u = Low(url0), host, pageHost = Low(pageHost0); size_t he = 0; if (!ParseUrl(u, host, he)) return false;
        bool block = false, allow = false;
        auto test = [&](int i) { const Rule& r = rules[i]; if (!Ok(r, pageHost, third, type)) return; if (Hit(r, u, he)) { if (r.exc) allow = true; else block = true; } };
        for (size_t p = 0;; ) { auto it = byHost.find(host.substr(p)); if (it != byHost.end()) for (int i : it->second) test(i); size_t d = host.find('.', p); if (d == std::string::npos) break; p = d + 1; }
        { size_t i = 0, n = 0; while (i < u.size() && n < 120) { if (!AlNum(u[i])) { ++i; continue; } size_t j = i; while (j < u.size() && AlNum(u[j])) ++j; if (j - i >= 4) { auto it = byTok.find(u.substr(i, j - i)); if (it != byTok.end()) for (int k : it->second) test(k); ++n; } i = j; } }
        for (int i : rest) test(i);
        return block && !allow;
    }
    // cosmetic selectors that apply to this page host
    std::vector<std::string> SelectorsFor(const std::string& host0) const {
        std::string host = Low(host0); std::vector<std::string> out;
        for (auto& c : cos) {
            if (c.doms.empty()) { out.push_back(c.sel); continue; }
            bool in = false, neg = false; size_t i = 0;
            while (i <= c.doms.size()) { size_t e = c.doms.find(',', i); if (e == std::string::npos) e = c.doms.size(); std::string d = c.doms.substr(i, e - i); i = e + 1; if (d.empty()) continue; if (d[0] == '~') { if (HostIn(host, d.substr(1))) neg = true; } else if (HostIn(host, d)) in = true; }
            if (in && !neg) out.push_back(c.sel);
        }
        return out;
    }
};
}   // namespace Flt
