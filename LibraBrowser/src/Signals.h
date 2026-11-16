// Anonymous counts for sponsored content (pure code, no UI, no network).
// The browser counts locally; one fixed-shape report is built from the totals. Nothing here can hold an id, a time, a host name or a version.
#pragma once
#include <string>
#include <map>
#include <vector>
#include <algorithm>
#include <cstdlib>

struct SnLedger {
    std::map<std::string, std::pair<int, int>> c;   // campaign id -> (shown, clicked) since the last send
    long long ba = 0, bt = 0;                       // blocked ads, blocked trackers since the last send
    long long since = 0;                            // when the first count after the last send happened (stale ledgers are dropped)
};
constexpr int SN_CAP = 20;                          // most "shown" counted per campaign per report (the server refuses more)

inline bool SnIdOk(const std::string& s) {
    if (s.size() < 3 || s.size() > 40 || !((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= '0' && s[0] <= '9'))) return false;
    for (char ch : s) if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-')) return false;
    return true;
}
inline void SnShown(SnLedger& l, const std::string& id, long long now) {
    if (!SnIdOk(id) || (!l.c.count(id) && l.c.size() >= 80)) return;
    if (!l.since) l.since = now;
    auto& e = l.c[id]; if (e.first < SN_CAP) ++e.first;
}
inline void SnClick(SnLedger& l, const std::string& id) {   // a click only counts for a campaign that was shown (clicked <= shown)
    auto it = l.c.find(id); if (it != l.c.end() && it->second.second < it->second.first) ++it->second.second;
}
inline void SnBlocked(SnLedger& l, bool ad, long long now) { if (!l.since) l.since = now; long long& v = ad ? l.ba : l.bt; if (v < 10000000) ++v; }
inline long long SnRound(long long v) { v = std::max(0LL, v); return std::min(100000LL, (v + 5) / 10 * 10); }   // nearest 10: small counts can not fingerprint a person

// Drops campaigns that are no longer in the feed (they can never be reported).
inline void SnPrune(SnLedger& l, const std::vector<std::string>& active) {
    for (auto it = l.c.begin(); it != l.c.end();) it = std::find(active.begin(), active.end(), it->first) == active.end() ? l.c.erase(it) : std::next(it);
}
inline bool SnHasData(const SnLedger& l, const std::vector<std::string>& active) {
    if (SnRound(l.ba) > 0 || SnRound(l.bt) > 0) return true;
    for (auto& id : active) { auto it = l.c.find(id); if (it != l.c.end() && it->second.first > 0) return true; }
    return false;
}
// The report: EVERY active campaign (zeros too), rounded block totals, padded to a fixed size so its length says nothing.
// `sent` receives exactly what was reported, to be subtracted after the server accepted it.
inline std::string SnBuild(const SnLedger& l, std::vector<std::string> active, SnLedger* sent) {
    std::sort(active.begin(), active.end()); active.erase(std::unique(active.begin(), active.end()), active.end()); if (active.size() > 40) active.resize(40);
    SnLedger s; std::string j = "{\"v\":1,\"c\":[";
    for (size_t i = 0; i < active.size(); ++i) {
        auto it = l.c.find(active[i]); int sh = 0, k = 0;
        if (it != l.c.end()) { sh = std::min(it->second.first, SN_CAP); k = std::min(it->second.second, sh); }
        if (sh || k) s.c[active[i]] = { sh, k };
        j += std::string(i ? "," : "") + "{\"id\":\"" + active[i] + "\",\"s\":" + std::to_string(sh) + ",\"k\":" + std::to_string(k) + "}";
    }
    s.ba = SnRound(l.ba); s.bt = SnRound(l.bt);
    j += "],\"b\":{\"a\":" + std::to_string(s.ba) + ",\"t\":" + std::to_string(s.bt) + "}";
    size_t target = std::max<size_t>(2048, (j.size() + 10 + 1023) / 1024 * 1024);   // + ,"pad":"" and the closing brace
    j += ",\"pad\":\"" + std::string(target - j.size() - 10, 'x') + "\"}";
    if (sent) *sent = s;
    return j;
}
inline void SnSubtract(SnLedger& l, const SnLedger& s) {
    for (auto& kv : s.c) { auto it = l.c.find(kv.first); if (it == l.c.end()) continue; it->second.first = std::max(0, it->second.first - kv.second.first); it->second.second = std::max(0, it->second.second - kv.second.second); if (!it->second.first && !it->second.second) l.c.erase(it); }
    l.ba = std::max(0LL, l.ba - s.ba); l.bt = std::max(0LL, l.bt - s.bt);
    if (l.c.empty() && !l.ba && !l.bt) l.since = 0;
}
// File format (encrypted by WriteAll): "SN1", "S\t<since>", "A\t<ads>\t<trackers>", "C\t<id>\t<shown>\t<clicked>" ..., "L\t<epoch>\t<kind>\t<bytes>\t<ok>" (log lines, newest last)
struct SnLogLine { long long t = 0; std::string kind; int bytes = 0; bool ok = false; };
inline std::string SnSerialize(const SnLedger& l, const std::vector<SnLogLine>& log) {
    std::string o = "SN1\nS\t" + std::to_string(l.since) + "\nA\t" + std::to_string(l.ba) + "\t" + std::to_string(l.bt) + "\n";
    for (auto& kv : l.c) o += "C\t" + kv.first + "\t" + std::to_string(kv.second.first) + "\t" + std::to_string(kv.second.second) + "\n";
    for (auto& g : log) o += "L\t" + std::to_string(g.t) + "\t" + g.kind + "\t" + std::to_string(g.bytes) + "\t" + (g.ok ? "1" : "0") + "\n";
    return o;
}
inline void SnParse(const std::string& d, SnLedger& l, std::vector<SnLogLine>& log) {
    l = SnLedger(); log.clear(); if (d.rfind("SN1\n", 0) != 0 || d.size() > 65536) return;
    size_t p = 4; int lines = 0;
    while (p < d.size() && ++lines < 400) {
        size_t e = d.find('\n', p); if (e == std::string::npos) e = d.size();
        std::vector<std::string> f; size_t a = p; for (;;) { size_t t = d.find('\t', a); if (t == std::string::npos || t >= e) { f.push_back(d.substr(a, e - a)); break; } f.push_back(d.substr(a, t - a)); a = t + 1; }
        p = e + 1; if (f.empty() || f.size() > 5) continue;
        auto num = [](const std::string& s) { return s.size() > 12 ? 0LL : atoll(s.c_str()); };
        if (f[0] == "S" && f.size() == 2) l.since = num(f[1]);
        else if (f[0] == "A" && f.size() == 3) { l.ba = std::min(num(f[1]), 10000000LL); l.bt = std::min(num(f[2]), 10000000LL); }
        else if (f[0] == "C" && f.size() == 4 && SnIdOk(f[1]) && l.c.size() < 80) l.c[f[1]] = { (int)std::min(num(f[2]), (long long)SN_CAP), (int)std::min(num(f[3]), (long long)SN_CAP) };
        else if (f[0] == "L" && f.size() == 5 && (f[2] == "report" || f[2] == "feed") && log.size() < 30) log.push_back({ num(f[1]), f[2], (int)num(f[3]), f[4] == "1" });
    }
}
