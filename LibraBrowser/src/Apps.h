#pragma once
// Installed web apps (PLAN-webapps.md section 8). apps.txt (encrypted like the other private files, see IsSecretFile):
// one line per app = id <TAB> name <TAB> start address <TAB> scope origin <TAB> icon file <TAB> date.  The id is 12 lowercase hex characters.
struct AppRec { std::wstring id, name, url, scope, icon, date; };
inline bool AppIdOk(const std::wstring& s) { if (s.size() != 12) return false; for (wchar_t c : s) if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f'))) return false; return true; }
inline std::wstring AppClean(const std::wstring& in, size_t cap = 40) {   // names come from a website: no control characters, no text-direction tricks, short
    std::wstring o; for (wchar_t c : in) { if (c < 32 || (c >= 0x202A && c <= 0x202E) || (c >= 0x2066 && c <= 0x2069) || c == 0x200E || c == 0x200F) continue; if (o.size() < cap) o += c; }
    while (!o.empty() && o.back() == L' ') o.pop_back(); while (!o.empty() && o.front() == L' ') o.erase(o.begin()); return o;
}
inline bool AppFind(const std::wstring& id, AppRec& out) {
    for (auto& ln : Lines(ReadAll(DataDir() + L"\\apps.txt"))) {
        std::vector<std::wstring> f; size_t p = 0; for (;;) { size_t q = ln.find(L'\t', p); f.push_back(ln.substr(p, q == std::wstring::npos ? q : q - p)); if (q == std::wstring::npos) break; p = q + 1; }
        if (f.size() < 4 || f[0] != id) continue;
        if (f[2].rfind(L"https://", 0) != 0 && f[2].rfind(L"http://127.0.0.1", 0) != 0 && f[2].rfind(L"http://localhost", 0) != 0) return false;   // only secure addresses (the local test host aside)
        out.id = id; out.name = AppClean(f[1]); out.url = f[2]; out.scope = f[3]; out.icon = f.size() > 4 ? f[4] : L""; out.date = f.size() > 5 ? f[5] : L"";
        if (out.name.empty()) out.name = L"Libra app"; return true;
    }
    return false;
}
// The app window attaches to the running browser's engine (same profile = still signed in, one extra renderer instead of a whole new engine).
// The browser window advertises its proxy port and data folder as window properties; an app finds it with these.
inline HANDLE AppDirTag() { return (HANDLE)(std::hash<std::wstring>{}(DataDir()) | 1); }
inline HWND AppFindHost() {
    for (HWND w = nullptr; (w = FindWindowExW(nullptr, w, L"LibraBrowserWnd", nullptr)) != nullptr;)
        if (GetPropW(w, L"lbdir") == AppDirTag() && GetPropW(w, L"lbport")) return w;
    return nullptr;
}
