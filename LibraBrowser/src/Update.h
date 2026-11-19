// Update notifier (pure code): a signed manifest says which version is newest and where the installer is. Nothing is installed without the user's yes.
// Same signature check as the sponsored feed (ECDSA P-256), so a hacked host can not push a file: the installer's SHA-256 is inside the signed manifest.
#pragma once
#include "Sponsor.h"

constexpr const char* kLibraVer = "3.9.8";   // keep in step with the version in LibraBrowser.rc and Pages.h (About)

struct UpInfo { long long seq = 0, expires = 0, size = 0; std::string ver, url, sha, notes; };

inline bool UpVer(const std::string& s, int v[3]) {   // "a.b.c", each 0..999
    int n = 0; size_t i = 0; v[0] = v[1] = v[2] = 0;
    while (n < 3) { size_t st = i; int x = 0; while (i < s.size() && s[i] >= '0' && s[i] <= '9' && i - st < 3) x = x * 10 + (s[i++] - '0'); if (i == st) return false; v[n++] = x; if (n < 3) { if (i >= s.size() || s[i] != '.') return false; ++i; } }
    return i == s.size();
}
inline bool UpNewer(const std::string& a, const std::string& b) { int x[3], y[3]; if (!UpVer(a, x) || !UpVer(b, y)) return false; for (int i = 0; i < 3; ++i) if (x[i] != y[i]) return x[i] > y[i]; return false; }

// "" when fine, else a short reason. Does not check the clock or seq (the caller does).
inline std::string UpParse(const std::string& env, const std::vector<SpKeyA>& keys, UpInfo& o, bool test = false) {
    o = UpInfo(); if (env.size() > 16384) return "too big";
    size_t p = 0; std::map<std::string, std::string> top;
    if (!SpObject(env, p, top)) return "bad envelope";
    auto kid = top.find("kid"), pl = top.find("payload"), sg = top.find("sig");
    if (kid == top.end() || pl == top.end() || sg == top.end()) return "missing parts";
    bool ok = false; for (auto& k : keys) if (!k.x.empty() && k.kid == kid->second && SpVerify(pl->second, sg->second, k.x, k.y)) { ok = true; break; }
    if (!ok) return "bad signature";
    size_t q = 0; std::map<std::string, std::string> m; if (!SpObject(pl->second, q, m)) return "bad payload";
    if (m["v"] != "1" || m["kind"] != "update") return "unknown version";
    o.seq = SpNum(m["seq"]); o.expires = SpNum(m["expires"]); o.size = SpNum(m["size"]); o.ver = m["version"]; o.url = m["url"]; o.sha = m["sha256"]; o.notes = m["notes"];
    int v[3]; if (o.seq < 0 || o.expires < 0 || !UpVer(o.ver, v)) return "bad numbers";
    if (!SpGoOk(o.url) && !(test && o.url.rfind("http://localhost:", 0) == 0)) return "bad address";   // tests may use a local http server
    if (o.sha.size() != 64) return "bad hash"; for (char c : o.sha) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return "bad hash";
    if (o.size < 100000 || o.size > 33554432) return "bad size";
    if (o.notes.size() > 300) o.notes.resize(300);
    for (char& c : o.notes) if (c == '<' || c == '>' || (unsigned char)c < 0x20) c = ' ';
    return "";
}
