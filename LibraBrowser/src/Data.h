#pragma once
// Plain-text storage in %LOCALAPPDATA%\LibraBrowser\ : settings.ini, history.txt, bookmarks.txt, session.txt
#include "pch.h"
#include <map>
#include <ctime>
#include <shlobj.h>
#include <wincrypt.h>

struct Engine { const wchar_t* name; const wchar_t* url; bool ai = false; };
struct TabInfo { std::wstring title, url; bool pinned = false, loading = false, current = false; };
inline const Engine kEngines[] = {
    { L"Google",     L"https://www.google.com/search?q=" },
    { L"DuckDuckGo", L"https://duckduckgo.com/?q=" },
    { L"Bing",       L"https://www.bing.com/search?q=" },
    { L"Brave",      L"https://search.brave.com/search?q=" },
    { L"Startpage",  L"https://www.startpage.com/do/search?q=" },
    { L"Ecosia",     L"https://www.ecosia.org/search?q=" },
    { L"Perplexity", L"https://www.perplexity.ai/search?q=", true },
    { L"Brave Ask",  L"https://search.brave.com/ask?q=",     true },
};

struct HistoryItem  { long long t = 0; std::wstring url, title; };
struct BookmarkItem { std::wstring url, title; };
struct DlJob;
struct DownloadItem {
    std::shared_ptr<DlJob> job;   // set when our segmented downloader owns this download
    std::wstring name, path; long long total = 0, got = 0; int state = 0; double speed = 0; long long lastGot = 0; DWORD lastTick = 0; bool canRetry = false; char toast = 0;   // 0 active, 1 done, 2 failed/cancelled
    wil::com_ptr<ICoreWebView2DownloadOperation> op;
};

inline std::string ToUtf8(const std::wstring& w) {
    if (w.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), s.data(), n, nullptr, nullptr);
    return s;
}
inline std::wstring FromUtf8(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), w.data(), n);
    return w;
}

// Test hooks (LIBRA_TEST_* environment variables) work ONLY when the file libra-test.enabled sits next to the exe. Release packages never contain it,
// so a program that starts Libra with such variables cannot switch off a safety check or auto-answer a permission question.
inline DWORD TestEnv(const wchar_t* name, wchar_t* buf, DWORD n) {
    static int ok = -1;
    if (ok < 0) { wchar_t p[MAX_PATH]; GetModuleFileNameW(nullptr, p, MAX_PATH); wchar_t* s = wcsrchr(p, L'\\'); if (s) { s[1] = 0; wcscat_s(p, L"libra-test.enabled"); } ok = GetFileAttributesW(p) != INVALID_FILE_ATTRIBUTES; }
    if (!ok) { if (buf && n) buf[0] = 0; return 0; }
    return GetEnvironmentVariableW(name, buf, n);
}
inline std::wstring DataDir() {
    wchar_t p[MAX_PATH] = {};
    GetEnvironmentVariableW(L"LOCALAPPDATA", p, MAX_PATH);
    std::wstring d = std::wstring(p) + L"\\LibraBrowser";
    CreateDirectoryW(d.c_str(), nullptr);
    return d;
}
// Files that list where you have been are stored encrypted (Windows DPAPI, this Windows user only). Old plain files are read, then upgraded on the next write.
inline bool IsSecretFile(const std::wstring& p) {
    for (const wchar_t* n : { L"\\history.txt", L"\\dns.cache", L"\\stats.txt", L"\\session.txt", L"\\bookmarks.txt", L"\\sitemodes.txt", L"\\keepid.txt", L"\\pinned.txt", L"\\tiles.txt", L"\\pwnever.txt", L"\\sites.txt", L"\\memory.dat", L"\\agent_perms.txt", L"\\spons.dat", L"\\sgnever.txt" }) { size_t l = wcslen(n); if (p.size() > l && _wcsicmp(p.c_str() + p.size() - l, n) == 0) return true; }
    return false;
}
inline std::string SecretChunk(const std::string& plain) {   // "LBE1" + length + DPAPI blob
    static const BYTE ent[] = "LibraFiles1"; DATA_BLOB in{ (DWORD)plain.size(), (BYTE*)plain.data() }, e{ sizeof(ent), (BYTE*)ent }, out{};
    if (!CryptProtectData(&in, L"Libra", &e, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return std::string();
    std::string r = "LBE1"; DWORD n = out.cbData; r.append((char*)&n, 4); r.append((char*)out.pbData, out.cbData); LocalFree(out.pbData); return r;
}
inline std::string SecretOpen(const std::string& raw) {
    if (raw.compare(0, 4, "LBE1") != 0) return raw;   // old plain file
    std::string plain; size_t p = 0;
    while (p + 8 <= raw.size() && raw.compare(p, 4, "LBE1") == 0) {
        DWORD n; memcpy(&n, raw.data() + p + 4, 4); if (p + 8 + n > raw.size()) break;
        static const BYTE ent[] = "LibraFiles1"; DATA_BLOB in{ n, (BYTE*)raw.data() + p + 8 }, e{ sizeof(ent), (BYTE*)ent }, out{};
        if (CryptUnprotectData(&in, nullptr, &e, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) { plain.append((char*)out.pbData, out.cbData); LocalFree(out.pbData); }
        p += 8 + n;
    }
    return plain;
}
inline std::string ReadAllRaw(const std::wstring& path) {
    std::string data;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return data;
    DWORD sz = GetFileSize(h, nullptr), rd = 0;
    if (sz != INVALID_FILE_SIZE && sz < (16u << 20)) { data.resize(sz); ReadFile(h, data.data(), sz, &rd, nullptr); data.resize(rd); }
    CloseHandle(h);
    return data;
}
inline std::string ReadAll(const std::wstring& path) { std::string d = ReadAllRaw(path); return IsSecretFile(path) ? SecretOpen(d) : d; }
inline void WriteAllRaw(const std::wstring& path, const std::string& data, bool append) {
    HANDLE h = CreateFileW(path.c_str(), append ? FILE_APPEND_DATA : GENERIC_WRITE, 0, nullptr,
                           append ? OPEN_ALWAYS : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD wr = 0; WriteFile(h, data.data(), (DWORD)data.size(), &wr, nullptr); CloseHandle(h);
}
inline void WriteAll(const std::wstring& path, const std::string& data, bool append = false) {
    if (!IsSecretFile(path)) { WriteAllRaw(path, data, append); return; }
    if (append) {
        std::string old = ReadAllRaw(path);
        if (!old.empty() && old.compare(0, 4, "LBE1") != 0) { std::string c = SecretChunk(old); if (!c.empty()) WriteAllRaw(path, c, false); }   // upgrade an old plain file
        std::string c = SecretChunk(data); if (!c.empty()) WriteAllRaw(path, c, true);
        return;
    }
    if (data.empty()) { WriteAllRaw(path, data, false); return; }
    std::string c = SecretChunk(data); if (!c.empty()) WriteAllRaw(path, c, false);
}
// one record per line, fields tab-separated: keep fields free of tab/newline
inline std::wstring Flat(std::wstring s) { for (auto& c : s) if (c == L'\t' || c == L'\r' || c == L'\n') c = L' '; return s; }
inline std::vector<std::wstring> SplitTab(const std::wstring& line) {
    std::vector<std::wstring> v; size_t s = 0;
    for (;;) { size_t e = line.find(L'\t', s); v.push_back(line.substr(s, e == std::wstring::npos ? e : e - s)); if (e == std::wstring::npos) break; s = e + 1; }
    return v;
}
inline std::vector<std::wstring> Lines(const std::string& data) {
    std::vector<std::wstring> out; size_t s = 0;
    while (s < data.size()) {
        size_t e = data.find('\n', s); if (e == std::string::npos) e = data.size();
        std::string l = data.substr(s, e - s); if (!l.empty() && l.back() == '\r') l.pop_back();
        if (!l.empty()) out.push_back(FromUtf8(l));
        s = e + 1;
    }
    return out;
}

inline std::wstring Esc(const std::wstring& s) {
    std::wstring o; o.reserve(s.size());
    for (wchar_t c : s) switch (c) {
        case L'&': o += L"&amp;"; break; case L'<': o += L"&lt;"; break; case L'>': o += L"&gt;"; break;
        case L'"': o += L"&quot;"; break; case L'\'': o += L"&#39;"; break; default: o += c; }
    return o;
}
inline std::wstring UrlEnc(const std::wstring& w) {   // UTF-8 percent-encoding, space -> '+'
    std::string u = ToUtf8(w); std::wstring o;
    for (unsigned char c : u) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o += (wchar_t)c;
        else if (c == ' ') o += L'+';
        else { wchar_t b[4]; swprintf(b, 4, L"%%%02X", c); o += b; }
    }
    return o;
}
inline bool IsHttp(const std::wstring& u) { return u.rfind(L"https://", 0) == 0 || u.rfind(L"http://", 0) == 0; }

struct Settings {
    std::map<std::wstring, std::wstring> m;
    void Load() { m.clear(); for (auto& l : Lines(ReadAll(DataDir() + L"\\settings.ini"))) { size_t e = l.find(L'='); if (e != std::wstring::npos) m[l.substr(0, e)] = l.substr(e + 1); } }
    void Save() const { std::string o; for (auto& kv : m) o += ToUtf8(kv.first + L"=" + Flat(kv.second)) + "\n"; WriteAll(DataDir() + L"\\settings.ini", o); }
    std::wstring Get(const std::wstring& k, const std::wstring& d = L"") const { auto it = m.find(k); return it == m.end() ? d : it->second; }
    bool On(const std::wstring& k) const { return Get(k) == L"1"; }
    void Set(const std::wstring& k, const std::wstring& v) { m[k] = Flat(v); Save(); }
};

inline std::wstring DownloadsDir(const Settings& s) {
    std::wstring d = s.Get(L"dl_dir");
    DWORD a = d.empty() ? INVALID_FILE_ATTRIBUTES : GetFileAttributesW(d.c_str());
    if (a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY)) return d;
    PWSTR p = nullptr; std::wstring r = L".";
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &p))) { r = p; CoTaskMemFree(p); }
    return r;
}
