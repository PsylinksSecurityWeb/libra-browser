// Sponsored content: signed feed parsing and verification (pure code, no UI).
// A feed is {"kid","payload","sig"}: payload is the exact JSON text that the Worker signed (ECDSA P-256, SHA-256, raw r||s, base64url).
#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cstring>
#pragma comment(lib, "bcrypt.lib")

struct SpKeyA { std::string kid, x, y; };   // public key slots (base64url x and y, 32 bytes each)
struct SpItem {
    std::string id, kind, title, advertiser, dest, click, img, imgB64;
    long long start = 0, end = 0; int weight = 1; std::vector<std::string> countries;
};
struct SpFeed { long long seq = 0, issued = 0, expires = 0; bool disabled = false; std::vector<SpItem> items; };

inline std::string SpB64uDec(const std::string& s) {
    std::string o; unsigned v = 0; int bits = 0;
    for (char c : s) {
        int d = (c >= 'A' && c <= 'Z') ? c - 'A' : (c >= 'a' && c <= 'z') ? c - 'a' + 26 : (c >= '0' && c <= '9') ? c - '0' + 52 : (c == '-' || c == '+') ? 62 : (c == '_' || c == '/') ? 63 : -1;
        if (d < 0) { if (c == '=') continue; return {}; }
        v = (v << 6) | d; bits += 6; if (bits >= 8) { bits -= 8; o += (char)((v >> bits) & 0xFF); }
    }
    return o;
}

inline bool SpVerify(const std::string& payload, const std::string& sig64, const std::string& x64, const std::string& y64) {
    std::string sig = SpB64uDec(sig64), X = SpB64uDec(x64), Y = SpB64uDec(y64);
    if (sig.size() != 64 || X.size() != 32 || Y.size() != 32) return false;
    BCRYPT_ALG_HANDLE alg = nullptr, ha = nullptr; BCRYPT_KEY_HANDLE key = nullptr; bool ok = false;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_ECDSA_P256_ALGORITHM, nullptr, 0) != 0) return false;
    std::string blob(sizeof(BCRYPT_ECCKEY_BLOB) + 64, 0); auto* h = (BCRYPT_ECCKEY_BLOB*)blob.data();
    h->dwMagic = BCRYPT_ECDSA_PUBLIC_P256_MAGIC; h->cbKey = 32; memcpy(&blob[sizeof(BCRYPT_ECCKEY_BLOB)], X.data(), 32); memcpy(&blob[sizeof(BCRYPT_ECCKEY_BLOB) + 32], Y.data(), 32);
    if (BCryptImportKeyPair(alg, nullptr, BCRYPT_ECCPUBLIC_BLOB, &key, (PUCHAR)blob.data(), (ULONG)blob.size(), 0) == 0) {
        if (BCryptOpenAlgorithmProvider(&ha, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0) {
            UCHAR hash[32];
            if (BCryptHash(ha, nullptr, 0, (PUCHAR)payload.data(), (ULONG)payload.size(), hash, 32) == 0) ok = BCryptVerifySignature(key, nullptr, hash, 32, (PUCHAR)sig.data(), 64, 0) == 0;
            BCryptCloseAlgorithmProvider(ha, 0);
        }
        BCryptDestroyKey(key);
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

// ---- tiny JSON reader: only what the feed needs, every access is bounds-checked ----
inline void SpUtf8(std::string& o, unsigned cp) {
    if (cp < 0x80) o += (char)cp; else if (cp < 0x800) { o += (char)(0xC0 | cp >> 6); o += (char)(0x80 | (cp & 63)); }
    else if (cp < 0x10000) { o += (char)(0xE0 | cp >> 12); o += (char)(0x80 | (cp >> 6 & 63)); o += (char)(0x80 | (cp & 63)); }
    else { o += (char)(0xF0 | cp >> 18); o += (char)(0x80 | (cp >> 12 & 63)); o += (char)(0x80 | (cp >> 6 & 63)); o += (char)(0x80 | (cp & 63)); }
}
inline bool SpReadStr(const std::string& b, size_t& p, std::string& out) {   // p is at the opening quote; ends after the closing quote
    out.clear(); if (p >= b.size() || b[p] != '"') return false; ++p;
    while (p < b.size()) {
        char c = b[p];
        if (c == '"') { ++p; return true; }
        if (c == '\\') {
            if (++p >= b.size()) return false; char e = b[p];
            if (e == 'u') {
                auto hex = [&](size_t at, unsigned& v) { if (at + 4 > b.size()) return false; v = 0; for (size_t i = 0; i < 4; ++i) { char h = b[at + i]; v <<= 4; if (h >= '0' && h <= '9') v |= h - '0'; else if (h >= 'a' && h <= 'f') v |= h - 'a' + 10; else if (h >= 'A' && h <= 'F') v |= h - 'A' + 10; else return false; } return true; };
                unsigned cp; if (!hex(p + 1, cp)) return false; p += 4;
                if (cp >= 0xD800 && cp < 0xDC00 && p + 6 < b.size() && b[p + 1] == '\\' && b[p + 2] == 'u') { unsigned lo; if (hex(p + 3, lo) && lo >= 0xDC00 && lo < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); p += 6; } }
                SpUtf8(out, cp);
            } else if (e == 'n') out += '\n'; else if (e == 't') out += '\t'; else if (e == 'r') out += '\r'; else if (e == 'b') out += '\b'; else if (e == 'f') out += '\f'; else if (e == '"' || e == '\\' || e == '/') out += e; else return false;
            ++p;
        } else { if ((unsigned char)c < 0x20) return false; out += c; ++p; }
        if (out.size() > 262144) return false;
    }
    return false;
}
inline void SpWs(const std::string& b, size_t& p) { while (p < b.size() && (b[p] == ' ' || b[p] == '\n' || b[p] == '\r' || b[p] == '\t')) ++p; }
// One flat object {"k":value,...} starting at p: strings are unescaped, numbers/true/false kept as text, null becomes "", arrays kept as raw text.
inline bool SpObject(const std::string& b, size_t& p, std::map<std::string, std::string>& m) {
    m.clear(); SpWs(b, p); if (p >= b.size() || b[p] != '{') return false; ++p;
    for (;;) {
        SpWs(b, p); if (p < b.size() && b[p] == '}') { ++p; return true; }
        std::string k, v; if (!SpReadStr(b, p, k)) return false; SpWs(b, p); if (p >= b.size() || b[p] != ':') return false; ++p; SpWs(b, p); if (p >= b.size()) return false;
        if (b[p] == '"') { if (!SpReadStr(b, p, v)) return false; }
        else if (b[p] == '[') { size_t s = p; int d = 0; bool in = false; for (; p < b.size(); ++p) { char c = b[p]; if (in) { if (c == '\\') ++p; else if (c == '"') in = false; } else if (c == '"') in = true; else if (c == '[') ++d; else if (c == ']' && --d == 0) { ++p; break; } } if (d != 0) return false; v = b.substr(s, p - s); }
        else { size_t s = p; while (p < b.size() && b[p] != ',' && b[p] != '}' && b[p] != ' ' && b[p] != '\n') ++p; v = b.substr(s, p - s); if (v == "null") v.clear(); }
        if (m.size() < 40) m[k] = v;
        SpWs(b, p); if (p < b.size() && b[p] == ',') ++p; else if (p < b.size() && b[p] == '}') { ++p; return true; } else return false;
    }
}
inline bool SpId(const std::string& s) { if (s.size() < 3 || s.size() > 40 || !((s[0] >= 'a' && s[0] <= 'z') || (s[0] >= '0' && s[0] <= '9'))) return false; for (char c : s) if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false; return true; }
inline long long SpNum(const std::string& s) { if (s.empty() || s.size() > 12) return -1; long long v = 0; for (char c : s) { if (c < '0' || c > '9') return -1; v = v * 10 + (c - '0'); } return v; }

// Parses and checks a feed envelope. base = "https://host" (no slash): every link and picture must live there.
// The advertiser address the tile opens directly: https only, a real host name, no login part, no port, no odd characters.
inline bool SpGoOk(const std::string& u) {
    if (u.size() < 12 || u.size() > 500 || u.rfind("https://", 0) != 0) return false;
    for (unsigned char c : u) if (c <= 0x20 || c >= 0x7f || c == '\\' || c == '"' || c == '<' || c == '>') return false;
    size_t e = u.find_first_of("/?#", 8); std::string a = u.substr(8, e == std::string::npos ? std::string::npos : e - 8);
    return !a.empty() && a.find('@') == std::string::npos && a.find(':') == std::string::npos && a.find('.') != std::string::npos && a.front() != '.' && a.back() != '.';
}
// Returns "" when fine, else a short reason. Does NOT check the clock or seq: the caller does (expires, rollback).
inline std::string SpParse(const std::string& env, const std::string& base, const std::vector<SpKeyA>& keys, SpFeed& out) {
    out = SpFeed(); if (env.size() > 262144) return "too big";
    size_t p = 0; std::map<std::string, std::string> top;
    if (!SpObject(env, p, top)) return "bad envelope";
    auto kid = top.find("kid"), pl = top.find("payload"), sg = top.find("sig");
    if (kid == top.end() || pl == top.end() || sg == top.end()) return "missing parts";
    bool ok = false; for (auto& k : keys) if (!k.x.empty() && k.kid == kid->second && SpVerify(pl->second, sg->second, k.x, k.y)) { ok = true; break; }
    if (!ok) return "bad signature";
    const std::string& js = pl->second; size_t it = js.find("\"items\":[");
    if (it == std::string::npos) return "no items";
    { std::string h2 = js.substr(0, it);   // flat head part: v, seq, issued, expires, disabled
      while (!h2.empty() && (h2.back() == ',' || h2.back() == ' ')) h2.pop_back(); h2 += "}"; size_t q = 0; std::map<std::string, std::string> hm; if (!SpObject(h2, q, hm)) return "bad head";
      if (hm["v"] != "1") return "unknown version"; out.seq = SpNum(hm["seq"]); out.issued = SpNum(hm["issued"]); out.expires = SpNum(hm["expires"]); out.disabled = hm["disabled"] == "true";
      if (out.seq < 0 || out.issued < 0 || out.expires <= out.issued) return "bad numbers"; }
    size_t q = it + 9;
    for (;;) {
        SpWs(js, q); if (q < js.size() && js[q] == ']') break; if (q < js.size() && js[q] == ',') { ++q; continue; }
        std::map<std::string, std::string> m; if (!SpObject(js, q, m)) return "bad item";
        if (out.items.size() >= 40) return "too many items";
        SpItem x; x.id = m["id"]; x.kind = m["kind"]; x.title = m["title"]; x.advertiser = m["advertiser"]; x.dest = m["dest"]; x.click = m["go"]; x.img = m["img"];
        x.start = SpNum(m["start"]); x.end = SpNum(m["end"]); long long w = SpNum(m["weight"]); x.weight = (w < 1 || w > 10) ? 1 : (int)w;
        if (!SpId(x.id) || (x.kind != "tile" && x.kind != "background") || x.title.empty() || x.title.size() > 120 || x.advertiser.empty() || x.advertiser.size() > 120 || x.start < 0 || x.end <= x.start) return "bad item fields";
        if (!SpGoOk(x.click)) return "bad advertiser address";
        if (!x.img.empty() && x.img.rfind(base + "/img/", 0) != 0) return "picture outside the feed host";
        if (x.kind == "background" && x.img.empty()) return "background without picture";
        for (size_t i = 0; i + 1 < m["countries"].size(); ++i) { char a = m["countries"][i], b = m["countries"][i + 1]; if (a >= 'A' && a <= 'Z' && b >= 'A' && b <= 'Z' && x.countries.size() < 60) { x.countries.push_back(std::string{ a, b }); ++i; } }
        out.items.push_back(x);
    }
    return "";
}

// What may be shown right now (the caller's clock, region and hidden list decide). Weighted by day so the choice is steady within a day.
inline unsigned SpHash(const std::string& s, long long day) { unsigned h = 2166136261u ^ (unsigned)day; for (char c : s) { h ^= (unsigned char)c; h *= 16777619u; } h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; return h; }
inline std::vector<const SpItem*> SpPick(const SpFeed& f, const char* kind, size_t n, long long now, const std::string& region, const std::map<std::string, long long>& hidden) {
    std::vector<std::pair<double, const SpItem*>> c;
    if (f.disabled || now >= f.expires || now < f.issued - 3600) return {};
    for (auto& it : f.items) {
        if (it.kind != kind || now < it.start || now > it.end) continue;
        if (it.kind == "background" && it.imgB64.empty()) continue;   // picture not downloaded yet: show nothing
        if (!it.countries.empty()) { bool in = false; for (auto& cc : it.countries) if (cc == region) in = true; if (!in) continue; }
        auto h = hidden.find(it.id); if (h != hidden.end() && h->second > now) continue;
        c.push_back({ (double)(SpHash(it.id, now / 86400) % 100000 + 1) / it.weight, &it });
    }
    std::sort(c.begin(), c.end(), [](auto& a, auto& b) { return a.first < b.first || (a.first == b.first && a.second->id < b.second->id); });
    std::vector<const SpItem*> r; for (auto& x : c) if (r.size() < n) r.push_back(x.second); return r;
}
