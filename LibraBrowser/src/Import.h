// Bookmark import (pure code): Chrome/Edge/Brave "Bookmarks" JSON, Firefox backup (.jsonlz4) and the HTML export every browser can make.
// Files are only read and parsed with size caps, never executed. Only http(s) addresses survive; everything is checked again before it is saved.
#pragma once
#include <string>
#include <vector>
#include <utility>
#include <algorithm>
#include <cstdint>

typedef std::vector<std::pair<std::string, std::string>> ImList;   // (address, title), UTF-8
constexpr size_t IM_MAX_ITEMS = 20000, IM_MAX_FILE = 32u << 20;

// LZ4 block decoder with bounds checks (Firefox backups: "mozLz40\0" + u32 size + one LZ4 block).
inline bool ImLz4(const unsigned char* s, size_t n, std::string& out, size_t outMax) {
    size_t i = 0; out.clear();
    while (i < n) {
        unsigned tok = s[i++]; size_t lit = tok >> 4;
        if (lit == 15) for (;;) { if (i >= n) return false; unsigned b = s[i++]; lit += b; if (lit > outMax) return false; if (b != 255) break; }
        if (i + lit > n || out.size() + lit > outMax) return false;
        out.append((const char*)s + i, lit); i += lit;
        if (i >= n) break;   // the last sequence has literals only
        if (i + 2 > n) return false;
        size_t off = s[i] | (s[i + 1] << 8); i += 2; if (off == 0 || off > out.size()) return false;
        size_t ml = tok & 15; if (ml == 15) for (;;) { if (i >= n) return false; unsigned b = s[i++]; ml += b; if (ml > outMax) return false; if (b != 255) break; }
        ml += 4; if (out.size() + ml > outMax) return false;
        size_t from = out.size() - off; for (size_t k = 0; k < ml; ++k) out.push_back(out[from + k]);
    }
    return true;
}
inline bool ImMozlz4(const std::string& f, std::string& json) {
    if (f.size() < 13 || f.compare(0, 8, std::string("mozLz40\0", 8)) != 0) return false;
    const unsigned char* p = (const unsigned char*)f.data(); size_t sz = p[8] | (p[9] << 8) | (p[10] << 16) | ((size_t)p[11] << 24);
    if (sz == 0 || sz > IM_MAX_FILE) return false;
    return ImLz4(p + 12, f.size() - 12, json, sz) && json.size() == sz;
}

inline void ImUtf8(std::string& o, unsigned cp) {
    if (cp < 0x80) o += (char)cp; else if (cp < 0x800) { o += (char)(0xC0 | cp >> 6); o += (char)(0x80 | (cp & 63)); }
    else if (cp < 0x10000) { o += (char)(0xE0 | cp >> 12); o += (char)(0x80 | ((cp >> 6) & 63)); o += (char)(0x80 | (cp & 63)); }
    else { o += (char)(0xF0 | cp >> 18); o += (char)(0x80 | ((cp >> 12) & 63)); o += (char)(0x80 | ((cp >> 6) & 63)); o += (char)(0x80 | (cp & 63)); }
}
// JSON string at t[i]=='"': fills s (capped), returns the index after the closing quote, or npos.
inline size_t ImReadStr(const std::string& t, size_t i, std::string& s) {
    s.clear(); ++i;
    auto hex4 = [&](size_t k, unsigned& v) { if (k + 4 > t.size()) return false; v = 0; for (int j = 0; j < 4; ++j) { char c = t[k + j]; v <<= 4; if (c >= '0' && c <= '9') v |= c - '0'; else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10; else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10; else return false; } return true; };
    while (i < t.size()) {
        char c = t[i];
        if (c == '"') return i + 1;
        if (c == '\\') {
            if (++i >= t.size()) return std::string::npos; char e = t[i++]; std::string add;
            switch (e) { case 'n': case 'r': case 't': case 'b': case 'f': add = " "; break; case '/': add = "/"; break; case '\\': add = "\\"; break; case '"': add = "\""; break;
                case 'u': { unsigned v; if (!hex4(i, v)) return std::string::npos; i += 4; if (v >= 0xD800 && v < 0xDC00 && i + 6 <= t.size() && t[i] == '\\' && t[i + 1] == 'u') { unsigned lo; if (hex4(i + 2, lo) && lo >= 0xDC00 && lo < 0xE000) { v = 0x10000 + ((v - 0xD800) << 10) + (lo - 0xDC00); i += 6; } } ImUtf8(add, v); break; }
                default: return std::string::npos; }
            if (s.size() < 4096) s += add; continue;
        }
        if (s.size() < 4096) s += c; ++i;
    }
    return std::string::npos;
}
// Walks any JSON and collects objects that look like a bookmark: Chrome {"type":"url","name","url"} and Firefox {"typeCode":1,"title","uri"}.
inline void ImJson(const std::string& t, ImList& out) {
    struct F { std::string name, title, url, type; int tc = -1; }; std::vector<F> st; std::string pk; size_t i = 0, n = t.size();
    while (i < n && out.size() < IM_MAX_ITEMS) {
        char c = t[i];
        if (c == '{') { if (st.size() >= 64) return; st.emplace_back(); pk.clear(); ++i; }
        else if (c == '}') { if (!st.empty()) { F& f = st.back(); if (!f.url.empty() && (f.type == "url" || f.tc == 1)) out.push_back({ f.url, f.title.empty() ? f.name : f.title }); st.pop_back(); } pk.clear(); ++i; }
        else if (c == '"') {
            std::string s; size_t e = ImReadStr(t, i, s); if (e == std::string::npos) return;
            size_t j = e; while (j < n && (t[j] == ' ' || t[j] == '\n' || t[j] == '\r' || t[j] == '\t')) ++j;
            if (j < n && t[j] == ':') { pk = s; i = j + 1; continue; }
            if (!st.empty()) { F& f = st.back(); if (pk == "url" || pk == "uri") f.url = s; else if (pk == "name") f.name = s; else if (pk == "title") f.title = s; else if (pk == "type") f.type = s; }
            pk.clear(); i = e;
        }
        else if ((c >= '0' && c <= '9') || c == '-') { size_t st0 = i; long v = 0; while (i < n && ((t[i] >= '0' && t[i] <= '9') || t[i] == '-' || t[i] == '.' || t[i] == 'e' || t[i] == 'E' || t[i] == '+')) { if (t[i] >= '0' && t[i] <= '9' && i - st0 < 9) v = v * 10 + (t[i] - '0'); ++i; } if (!st.empty() && pk == "typeCode") st.back().tc = (int)v; pk.clear(); }
        else ++i;
    }
}
inline std::string ImEntities(const std::string& s) {
    std::string o; for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '&') { o += s[i]; continue; }
        size_t e = s.find(';', i); if (e == std::string::npos || e - i > 8) { o += '&'; continue; }
        std::string k = s.substr(i + 1, e - i - 1); unsigned cp = 0;
        if (k == "amp") cp = '&'; else if (k == "lt") cp = '<'; else if (k == "gt") cp = '>'; else if (k == "quot") cp = '"'; else if (k == "apos" || k == "#39") cp = '\'';
        else if (k.size() > 1 && k[0] == '#') { bool hx = k[1] == 'x' || k[1] == 'X'; for (size_t j = hx ? 2 : 1; j < k.size(); ++j) { char c = k[j]; unsigned d = (c >= '0' && c <= '9') ? c - '0' : (hx && c >= 'a' && c <= 'f') ? c - 'a' + 10 : (hx && c >= 'A' && c <= 'F') ? c - 'A' + 10 : 99; if (d == 99) { cp = 0; break; } cp = cp * (hx ? 16 : 10) + d; if (cp > 0x10FFFF) { cp = 0; break; } } }
        if (cp) { ImUtf8(o, cp); i = e; } else o += '&';
    }
    return o;
}
// Netscape bookmark HTML (every browser's "Export bookmarks").
inline void ImHtml(const std::string& t, ImList& out) {
    std::string l = t; for (char& c : l) if (c >= 'A' && c <= 'Z') c += 32; size_t p = 0;
    while (out.size() < IM_MAX_ITEMS && (p = l.find("<a ", p)) != std::string::npos) {
        size_t gt = l.find('>', p); if (gt == std::string::npos) break;
        size_t h = l.find("href=", p); std::string url;
        if (h != std::string::npos && h < gt) { size_t q = h + 5; if (q < gt && (t[q] == '"' || t[q] == '\'')) { char qc = t[q]; size_t e = t.find(qc, q + 1); if (e != std::string::npos && e <= gt) url = t.substr(q + 1, e - q - 1); } }
        size_t end = l.find("</a>", gt); if (end == std::string::npos) break;
        std::string title; bool tag = false; for (size_t k = gt + 1; k < end; ++k) { if (t[k] == '<') tag = true; else if (t[k] == '>') tag = false; else if (!tag) title += t[k]; }
        if (!url.empty()) out.push_back({ ImEntities(url), ImEntities(title) });
        p = end + 4;
    }
}
inline bool ImUrlOk(const std::string& u) {
    if (u.size() < 11 || u.size() > 2000 || (u.rfind("http://", 0) != 0 && u.rfind("https://", 0) != 0)) return false;
    for (unsigned char c : u) if (c <= 0x20 || c == 0x7f) return false;
    size_t s = u.find("://") + 3, e = u.find_first_of("/?#", s); std::string host = u.substr(s, e == std::string::npos ? std::string::npos : e - s);
    return !host.empty() && host.find('@') == std::string::npos && (host.find('.') != std::string::npos || host.rfind("localhost", 0) == 0);
}
// Format by content: mozlz4, JSON, or HTML. Returns the number of items found before checking (0 = unknown format).
inline size_t ImParse(const std::string& file, ImList& out) {
    out.clear(); if (file.empty() || file.size() > IM_MAX_FILE) return 0;
    std::string t; if (ImMozlz4(file, t)) ImJson(t, out);
    else { size_t i = 0; while (i < file.size() && (unsigned char)file[i] <= ' ') ++i; if (file.compare(0, 3, "\xEF\xBB\xBF") == 0) i = 3; while (i < file.size() && (unsigned char)file[i] <= ' ') ++i;
           if (i < file.size() && file[i] == '{') ImJson(file, out); else ImHtml(file, out); }
    return out.size();
}
