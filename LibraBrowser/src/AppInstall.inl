// Web-app install (PLAN-webapps.md section 8, step A1). Click on the install icon -> one native sheet -> manifest read from the engine, the site's icon downloaded ONCE
// through Libra's own proxy (no cookies) -> multi-size .ico in DataDir\apps -> apps.txt (encrypted) -> the app opens. Manifest values are data only (length caps, no control characters).
#include <random>
#include <shlobj.h>
#include <propsys.h>
static std::wstring ApJStr(const std::wstring& s, size_t& p) {   // p on the opening quote; returns the decoded string, p after the closing quote
    std::wstring o; ++p;
    while (p < s.size() && s[p] != L'"') {
        wchar_t c = s[p++];
        if (c == L'\\' && p < s.size()) { wchar_t e = s[p++]; if (e == L'n') o += L'\n'; else if (e == L't') o += L'\t'; else if (e == L'u' && p + 4 <= s.size()) { o += (wchar_t)wcstoul(s.substr(p, 4).c_str(), nullptr, 16); p += 4; } else o += e; }
        else o += c;
        if (o.size() > 4000) break;
    }
    while (p < s.size() && s[p] != L'"') ++p;
    if (p < s.size()) ++p; return o;
}
static bool ApFind(const std::wstring& j, const wchar_t* key, std::wstring& out) {   // first  "key" : "string value"
    std::wstring k = L"\"" + std::wstring(key) + L"\"";
    for (size_t p = 0; (p = j.find(k, p)) != std::wstring::npos; p += k.size()) {
        size_t q = p + k.size(); while (q < j.size() && iswspace(j[q])) ++q;
        if (q >= j.size() || j[q] != L':') continue; ++q; while (q < j.size() && iswspace(j[q])) ++q;
        if (q < j.size() && j[q] == L'"') { out = ApJStr(j, q); return true; }
    }
    return false;
}
static std::wstring ApResolve(const std::wstring& base, const std::wstring& rel) {
    if (rel.empty()) return L""; if (rel.rfind(L"https://", 0) == 0 || rel.rfind(L"http://", 0) == 0) return rel;
    size_t a = base.find(L"://"); if (a == std::wstring::npos) return L""; size_t e = base.find(L'/', a + 3); std::wstring origin = base.substr(0, e);
    if (rel.rfind(L"//", 0) == 0) return base.substr(0, a + 1) + rel; if (rel[0] == L'/') return origin + rel;
    std::wstring b = base.substr(0, base.find_first_of(L"?#")); size_t sl = b.find_last_of(L'/'); if (sl == std::wstring::npos || sl < a + 3) return origin + L"/" + rel; return b.substr(0, sl + 1) + rel;
}
struct ApIcon { std::wstring src; int size; };
static std::vector<ApIcon> ApIcons(const std::wstring& raw, const std::wstring& base) {
    std::vector<ApIcon> out; size_t p = raw.find(L"\"icons\""); if (p == std::wstring::npos) return out; p = raw.find(L'[', p); if (p == std::wstring::npos) return out;
    for (size_t i = p + 1; i < raw.size() && raw[i] != L']' && out.size() < 30;) {
        if (raw[i] == L'"') { ApJStr(raw, i); continue; }
        if (raw[i] != L'{') { ++i; continue; }
        size_t j = i + 1; int d = 1; while (j < raw.size() && d) { if (raw[j] == L'"') { ApJStr(raw, j); continue; } if (raw[j] == L'{') ++d; else if (raw[j] == L'}') --d; ++j; }
        std::wstring o = raw.substr(i, j - i), src, sz, pur, ty; i = j;
        if (!ApFind(o, L"src", src)) continue; ApFind(o, L"sizes", sz); ApFind(o, L"purpose", pur); ApFind(o, L"type", ty);
        if (pur.find(L"maskable") != std::wstring::npos && pur.find(L"any") == std::wstring::npos) continue;   // maskable-only art is cropped by design: not a good window icon
        std::wstring lo = src.substr(0, src.find_first_of(L"?#")); for (auto& c : lo) c = (wchar_t)towlower(c);
        auto ends = [&](const wchar_t* x) { size_t l = wcslen(x); return lo.size() >= l && lo.compare(lo.size() - l, l, x) == 0; };
        if (ends(L".svg") || ends(L".webp") || ty.find(L"svg") != std::wstring::npos || ty.find(L"webp") != std::wstring::npos) continue;   // the window icon decoder reads PNG/JPEG/GIF/ICO
        int best = 0; for (size_t k = 0; k < sz.size();) { int w = _wtoi(sz.c_str() + k), h = 0; size_t x = sz.find_first_of(L"xX", k), sp = sz.find(L' ', k); if (x != std::wstring::npos && (sp == std::wstring::npos || x < sp)) h = _wtoi(sz.c_str() + x + 1); if (w > 0 && w == h && w > best) best = w; if (sp == std::wstring::npos) break; k = sp + 1; }
        std::wstring u = ApResolve(base, src); if (AppHostOk(u)) out.push_back({ u, best ? best : 1 });
    }
    return out;
}
static bool ApPngClsid(CLSID* c) {
    UINT n = 0, sz = 0; Gdiplus::GetImageEncodersSize(&n, &sz); if (!sz) return false; std::vector<char> b(sz); auto* e = (Gdiplus::ImageCodecInfo*)b.data(); Gdiplus::GetImageEncoders(n, sz, e);
    for (UINT k = 0; k < n; ++k) if (wcscmp(e[k].MimeType, L"image/png") == 0) { *c = e[k].Clsid; return true; } return false;
}
static bool ApWriteIco(Gdiplus::Image& img, const std::wstring& path) {   // 16/32/48/256 px PNG entries, picture fitted into a square
    CLSID png; if (!ApPngClsid(&png) || img.GetWidth() == 0 || img.GetHeight() == 0) return false;
    const int sizes[4] = { 16, 32, 48, 256 }; std::string blobs[4];
    for (int i = 0; i < 4; ++i) {
        int s = sizes[i]; Gdiplus::Bitmap b(s, s, PixelFormat32bppARGB); { Gdiplus::Graphics g(&b); g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic); g.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf); g.Clear(Gdiplus::Color(0, 0, 0, 0));
            float sc = std::min((float)s / img.GetWidth(), (float)s / img.GetHeight()); float w = img.GetWidth() * sc, h = img.GetHeight() * sc; g.DrawImage(&img, (s - w) / 2, (s - h) / 2, w, h); }
        IStream* st = nullptr; if (FAILED(CreateStreamOnHGlobal(nullptr, TRUE, &st))) return false;
        bool ok = b.Save(st, &png) == Gdiplus::Ok; if (ok) { STATSTG ss{}; st->Stat(&ss, STATFLAG_NONAME); LARGE_INTEGER z{}; st->Seek(z, STREAM_SEEK_SET, nullptr); blobs[i].resize((size_t)ss.cbSize.QuadPart); ULONG rd = 0; st->Read(&blobs[i][0], (ULONG)blobs[i].size(), &rd); ok = rd == blobs[i].size(); }
        st->Release(); if (!ok) return false;
    }
    std::string o; auto u16 = [&](unsigned v) { o += (char)(v & 255); o += (char)((v >> 8) & 255); }; auto u32 = [&](unsigned v) { u16(v & 0xFFFF); u16(v >> 16); };
    u16(0); u16(1); u16(4); unsigned off = 6 + 16 * 4;
    for (int i = 0; i < 4; ++i) { o += (char)(sizes[i] == 256 ? 0 : sizes[i]); o += (char)(sizes[i] == 256 ? 0 : sizes[i]); o += (char)0; o += (char)0; u16(1); u16(32); u32((unsigned)blobs[i].size()); u32(off); off += (unsigned)blobs[i].size(); }
    for (auto& b : blobs) o += b;
    std::wstring tmp = path + L".tmp"; { std::ofstream f(tmp, std::ios::binary); if (!f) return false; f.write(o.data(), (std::streamsize)o.size()); if (!f.good()) return false; }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
}
static bool ApLetterIco(const std::wstring& name, const std::wstring& path) {   // no usable site icon: accent circle with the first letter
    Gdiplus::Bitmap b(256, 256, PixelFormat32bppARGB); { Gdiplus::Graphics g(&b); g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias); g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAliasGridFit); g.Clear(Gdiplus::Color(0, 0, 0, 0));
        Gdiplus::SolidBrush bg(Gdiplus::Color(255, GetRValue(Color::Accent), GetGValue(Color::Accent), GetBValue(Color::Accent))); g.FillEllipse(&bg, 8, 8, 240, 240);
        std::wstring L1 = name.empty() ? L"A" : std::wstring(1, (wchar_t)towupper(name[0])); Gdiplus::Font f(L"Segoe UI", 120, Gdiplus::FontStyleBold, Gdiplus::UnitPixel); Gdiplus::StringFormat sf; sf.SetAlignment(Gdiplus::StringAlignmentCenter); sf.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        Gdiplus::SolidBrush fg(Gdiplus::Color(255, 255, 255, 255)); g.DrawString(L1.c_str(), -1, &f, Gdiplus::RectF(0, 0, 256, 256), &sf, &fg); }
    return ApWriteIco(b, path);
}
struct AppInst { std::wstring id, name, url, scope, host; std::string icon; };
static std::wstring ApNewId() { std::random_device rd; wchar_t b[16]; swprintf_s(b, L"%06x%06x", rd() & 0xFFFFFF, rd() & 0xFFFFFF); return b; }
static bool ApLooksImage(const std::string& b) { return b.size() > 100 && (b.compare(0, 4, "\x89PNG") == 0 || b.compare(0, 2, "\xFF\xD8") == 0 || b.compare(0, 3, "GIF") == 0 || b.compare(0, 4, std::string("\0\0\1\0", 4)) == 0); }

static const PROPERTYKEY kApAumid = { { 0x9F4C2855, 0x9F79, 0x4B39, { 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3 } }, 5 };
static std::wstring ApLinkDir(bool desktop) {   // test hook LIBRA_TEST_APPLNK=<folder> keeps the shortcuts out of the real Start menu / Desktop
    wchar_t t[MAX_PATH]; if (TestEnv(L"LIBRA_TEST_APPLNK", t, MAX_PATH) > 0) { std::wstring d = std::wstring(t) + (desktop ? L"\\Desktop" : L"\\Start"); std::error_code ec; std::filesystem::create_directories(d, ec); return d; }
    PWSTR p = nullptr; std::wstring r; if (SUCCEEDED(SHGetKnownFolderPath(desktop ? FOLDERID_Desktop : FOLDERID_Programs, 0, nullptr, &p)) && p) r = p; CoTaskMemFree(p); return r;
}
static std::wstring ApLinkPath(bool desktop, const std::wstring& name) {
    std::wstring d = ApLinkDir(desktop), n; if (d.empty()) return L""; for (wchar_t c : name) if (!wcschr(L"\\/:*?\"<>|", c)) n += c;
    while (!n.empty() && (n.back() == L'.' || n.back() == L' ')) n.pop_back(); if (n.empty()) n = L"Libra app"; return d + L"\\" + n + L".lnk";
}
static bool ApMakeLink(bool desktop, const std::wstring& id, const std::wstring& name) {   // shortcut -> LibraBrowser.exe --app=<id>, the site's icon, own taskbar identity
    std::wstring lp = ApLinkPath(desktop, name); if (lp.empty()) return false; wchar_t ex[MAX_PATH]; GetModuleFileNameW(nullptr, ex, MAX_PATH);
    IShellLinkW* sl = nullptr; if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&sl)))) return false; bool ok = false;
    sl->SetPath(ex); sl->SetArguments((L"--app=" + id).c_str()); sl->SetDescription(name.c_str()); sl->SetIconLocation((DataDir() + L"\\apps\\" + id + L".ico").c_str(), 0);
    if (IPropertyStore* ps = nullptr; SUCCEEDED(sl->QueryInterface(IID_PPV_ARGS(&ps)))) { PROPVARIANT pv; PropVariantInit(&pv); pv.vt = VT_LPWSTR; if (SUCCEEDED(SHStrDupW((L"Psylinks.Libra.App." + id).c_str(), &pv.pwszVal))) { ps->SetValue(kApAumid, pv); ps->Commit(); } PropVariantClear(&pv); ps->Release(); }
    if (IPersistFile* pf = nullptr; SUCCEEDED(sl->QueryInterface(IID_PPV_ARGS(&pf)))) { ok = SUCCEEDED(pf->Save(lp.c_str(), TRUE)); pf->Release(); }
    sl->Release(); return ok;
}
static void ApDropLink(bool desktop, const std::wstring& id, const std::wstring& name) {   // only a shortcut that really starts this app is deleted
    std::wstring lp = ApLinkPath(desktop, name); if (lp.empty() || GetFileAttributesW(lp.c_str()) == INVALID_FILE_ATTRIBUTES) return;
    IShellLinkW* sl = nullptr; if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&sl)))) return; bool ours = false;
    if (IPersistFile* pf = nullptr; SUCCEEDED(sl->QueryInterface(IID_PPV_ARGS(&pf)))) { if (SUCCEEDED(pf->Load(lp.c_str(), STGM_READ))) { wchar_t ar[300] = {}; sl->GetArguments(ar, 300); ours = std::wstring(ar).find(L"--app=" + id) != std::wstring::npos; } pf->Release(); }
    sl->Release(); if (ours) DeleteFileW(lp.c_str());
}
bool BrowserWindow::AppInScope(const std::wstring& u) const {   // same site as the app's start page (www.x.com and m.x.com count), plus the big sign-in hosts so "Sign in with Google/Microsoft" keeps working
    auto hostOf = [](const std::wstring& x) { size_t a = x.find(L"://"); if (a == std::wstring::npos) return std::wstring(); a += 3; size_t e = x.find_first_of(L"/?#", a); std::wstring h = x.substr(a, e == std::wstring::npos ? e : e - a);
        size_t at = h.rfind(L'@'); if (at != std::wstring::npos) h = h.substr(at + 1); if (!h.empty() && h[0] != L'[') { size_t c = h.find(L':'); if (c != std::wstring::npos) h = h.substr(0, c); } for (auto& ch : h) ch = (wchar_t)towlower(ch); return h; };
    const std::wstring h = hostOf(u); if (h.empty()) return false;   // unknown = not ours (also user@host tricks)
    if (RegDom(h) == RegDom(hostOf(m_appUrl))) return true;
    for (const wchar_t* l : { L"accounts.google.com", L"accounts.youtube.com", L"login.microsoftonline.com", L"login.live.com", L"appleid.apple.com" }) if (h == l) return true;
    return false;
}
void BrowserWindow::AppToBrowser(const std::wstring& u) {   // an address the app does not own goes to the browser (running one, or a fresh one)
    if (!IsHttp(u) || u.size() > 2000) return;
    wchar_t tf[MAX_PATH]; if (TestEnv(L"LIBRA_TEST_APPOUT", tf, MAX_PATH) > 0) { WriteAllRaw(tf, ToUtf8(u) + "\n", true); return; }   // test hook: record instead of opening
    wchar_t ex[MAX_PATH]; GetModuleFileNameW(nullptr, ex, MAX_PATH); std::wstring a = L"\"" + u + L"\""; ShellExecuteW(nullptr, L"open", ex, a.c_str(), nullptr, SW_SHOWNORMAL);
}
static void AppSetWindowProps(HWND w, const std::wstring& id, const std::wstring& name) {   // so "Pin to taskbar" on a running app window pins THIS app (relaunch = exe --app=id, its name and icon), not plain Libra Browser
    IPropertyStore* ps = nullptr; if (FAILED(SHGetPropertyStoreForWindow(w, IID_PPV_ARGS(&ps)))) return;
    wchar_t ex[MAX_PATH]; GetModuleFileNameW(nullptr, ex, MAX_PATH);
    auto set = [&](DWORD pid, const std::wstring& v) { PROPERTYKEY k = kApAumid; k.pid = pid; PROPVARIANT pv; PropVariantInit(&pv); pv.vt = VT_LPWSTR; if (SUCCEEDED(SHStrDupW(v.c_str(), &pv.pwszVal))) ps->SetValue(k, pv); PropVariantClear(&pv); };
    set(5, L"Psylinks.Libra.App." + id); set(2, L"\"" + std::wstring(ex) + L"\" --app=" + id); set(3, DataDir() + L"\\apps\\" + id + L".ico,0"); set(4, name);
    ps->Commit(); ps->Release();
}
void BrowserWindow::AppWindowProps() { if (m_app) AppSetWindowProps(m_hWnd, m_appId, m_appName); }
void BrowserWindow::AppLaunch(const std::wstring& id) {
    AppRec r; if (!AppIdOk(id) || !AppFind(id, r)) return;
    wchar_t ex[MAX_PATH]; GetModuleFileNameW(nullptr, ex, MAX_PATH); std::wstring cl = L"\"" + std::wstring(ex) + L"\" --app=" + id; STARTUPINFOW si{ sizeof si }; PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, cl.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
}
void BrowserWindow::AppRemove(const std::wstring& id) {   // list entry, icon file and both shortcuts (an open window of that app is left alone)
    AppRec r; if (!AppIdOk(id) || !AppFind(id, r)) return;
    wchar_t tv[8]; bool yes = TestEnv(L"LIBRA_TEST_APPASK", tv, 8) > 0 ? tv[0] == L'y' : MessageBoxW(m_hWnd, (L"Remove " + r.name + L"?\n\nIts shortcuts are deleted. Your sign-in stays in Libra Browser.").c_str(), L"Remove app", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES;
    if (!yes) return;
    std::string keep; for (auto& ln : Lines(ReadAll(DataDir() + L"\\apps.txt"))) if (ln.rfind(id + L"\t", 0) != 0) keep += ToUtf8(ln) + "\n";
    WriteAll(DataDir() + L"\\apps.txt", keep, false); DeleteFileW((DataDir() + L"\\apps\\" + id + L".ico").c_str());
    ApDropLink(false, id, r.name); ApDropLink(true, id, r.name); AppLoadInstalled(); AppRelayout(); InvalidateRect(m_hWnd, nullptr, FALSE);
}
std::wstring BrowserWindow::AppsHtml() {   // rows for Settings > Apps (empty when nothing is installed)
    std::wstring o;
    for (auto& ln : Lines(ReadAll(DataDir() + L"\\apps.txt"))) {
        std::vector<std::wstring> f; size_t p = 0; for (;;) { size_t q = ln.find(L'\t', p); f.push_back(ln.substr(p, q == std::wstring::npos ? q : q - p)); if (q == std::wstring::npos) break; p = q + 1; }
        if (f.size() < 3 || !AppIdOk(f[0])) continue;
        o += L"<div class=row><div><span class=lbl>" + Esc(AppClean(f[1])) + L"</span><small>" + Esc(HostOfUrl(f[2])) + L"</small></div><span><button data-c=app.open data-a=" + f[0] + L">Open</button> <button data-c=app.del data-a=" + f[0] + L">Remove</button></span></div>";
    }
    return o;
}
void BrowserWindow::AppLoadInstalled() {
    m_appInstLoaded = true; m_appInst.clear();
    for (auto& ln : Lines(ReadAll(DataDir() + L"\\apps.txt"))) { size_t a = ln.find(L'\t'); if (a == std::wstring::npos) continue; size_t b = ln.find(L'\t', a + 1), c = b == std::wstring::npos ? b : ln.find(L'\t', b + 1); if (c == std::wstring::npos) continue; m_appInst.insert(HostOfUrl(ln.substr(b + 1, c - b - 1))); }
}
void BrowserWindow::AppInstallAsk(bool manual) {   // the install icon (or the menu item) was used
    if (m_appInstalling || m_app || m_incognito || m_child || m_activeTab < 0 || m_activeTab >= (int)m_tabs.size()) return;
    Tab* t = m_tabs[m_activeTab].get(); if (!AppHostOk(t->url) || t->incognito) return;
    if (!m_appInstLoaded) AppLoadInstalled();
    const std::wstring host = HostOf(t->url); const bool seen = AppOffer();
    if (!seen && !manual) return;
    if (m_appInst.count(host)) { MessageBoxW(m_hWnd, L"This site is already installed as an app. Open it from Settings > Apps.", L"Libra Browser", MB_OK | MB_ICONINFORMATION); return; }
    std::wstring name = seen ? AppOfferName() : AppClean(t->title); if (name.empty()) name = AppClean(host);
    bool yes = false; wchar_t tv[8]; m_appDesk = false;
    if (TestEnv(L"LIBRA_TEST_APPASK", tv, 8) > 0) { yes = tv[0] == L'y'; m_appDesk = yes && TestEnv(L"LIBRA_TEST_APPDESK", tv, 8) > 0; }
    else {
        std::wstring title = L"Install " + name + L" as an app?", det = host + L"\n\n" + (seen ? L"" : L"This site did not say it is an app, so it may not work like one. ") + L"Libra Browser opens it in its own window, signed in the same way as in the browser. Only the site's icon is downloaded, once. You can remove the app later in Settings.";
        TASKDIALOG_BUTTON btns[] = { { 201, L"&Install" }, { 202, L"&Cancel" } };
        TASKDIALOGCONFIG tc{ sizeof tc }; tc.hwndParent = m_hWnd; tc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW; tc.pszWindowTitle = L"Libra Browser";
        tc.pszMainInstruction = title.c_str(); tc.pszContent = det.c_str(); tc.pButtons = btns; tc.cButtons = 2; tc.nDefaultButton = 201; tc.pszMainIcon = TD_INFORMATION_ICON; tc.pszVerificationText = L"Add a shortcut on the Desktop";
        int btn = 0; BOOL vf = FALSE; if (SUCCEEDED(TaskDialogIndirect(&tc, &btn, nullptr, &vf))) { yes = btn == 201; m_appDesk = vf != FALSE; }
    }
    if (!yes || IndexOf(t) < 0 || !t->webView) return;
    m_appInstalling = true; HWND hw = m_hWnd; const std::wstring pageUrl = t->url; const int port = DohProxy::Get().Port();
    t->webView->CallDevToolsProtocolMethod(L"Page.getAppManifest", L"{}", Microsoft::WRL::Callback<ICoreWebView2CallDevToolsProtocolMethodCompletedHandler>(
        [hw, name, pageUrl, host, port](HRESULT hr, LPCWSTR json) -> HRESULT {
            std::wstring rep = SUCCEEDED(hr) && json ? json : L"", murl, raw, mname, sname, start; ApFind(rep, L"url", murl); ApFind(rep, L"data", raw);
            ApFind(raw, L"name", mname); ApFind(raw, L"short_name", sname); ApFind(raw, L"start_url", start);
            auto ai = std::make_unique<AppInst>(); ai->id = ApNewId(); ai->host = host;
            ai->name = AppClean(mname.empty() ? sname : mname); if (ai->name.empty()) ai->name = AppClean(name); if (ai->name.empty()) ai->name = AppClean(host);
            size_t pe = pageUrl.find(L'/', pageUrl.find(L"://") + 3); const std::wstring origin = pageUrl.substr(0, pe);
            std::wstring su = ApResolve(murl.empty() ? pageUrl : murl, start); if (su.empty() || !AppHostOk(su) || HostOfUrl(su) != host) su = origin + L"/";   // the start page must be on the same site
            ai->url = su; ai->scope = origin;
            std::vector<std::wstring> cands; auto icons = ApIcons(raw, murl.empty() ? pageUrl : murl);
            std::stable_sort(icons.begin(), icons.end(), [](const ApIcon& a, const ApIcon& b) { bool x = a.size >= 192, y = b.size >= 192; if (x != y) return x; return x ? a.size < b.size : a.size > b.size; });   // smallest that is at least 192 px, else the biggest there is
            for (auto& i : icons) if (cands.size() < 4) cands.push_back(i.src);
            cands.push_back(origin + L"/apple-touch-icon.png"); cands.push_back(origin + L"/favicon.ico");
            AppInst* raw_ai = ai.release();
            std::thread([hw, raw_ai, cands, port] {
                for (auto& u : cands) { std::string b = SiteFetchBytes(u.rfind(L"https://", 0) == 0 ? port : 0, u, 1 << 20); if (ApLooksImage(b)) { raw_ai->icon = std::move(b); break; } }
                PostMessage(hw, WM_APP + 71, 0, (LPARAM)raw_ai);
            }).detach();
            return S_OK;
        }).Get());
}
void BrowserWindow::AppInstalled(void* p) {   // the icon download is done (UI thread): make the .ico, save the app, open it
    std::unique_ptr<AppInst> a((AppInst*)p); m_appInstalling = false;
    std::wstring dir = DataDir() + L"\\apps"; CreateDirectoryW(dir.c_str(), nullptr); const std::wstring ico = dir + L"\\" + a->id + L".ico";
    bool ok = false;
    if (!a->icon.empty()) if (IStream* ms = SHCreateMemStream((const BYTE*)a->icon.data(), (UINT)a->icon.size())) { { Gdiplus::Bitmap bm(ms); if (bm.GetLastStatus() == Gdiplus::Ok) ok = ApWriteIco(bm, ico); } ms->Release(); }
    if (!ok) ok = ApLetterIco(a->name, ico);
    std::string cur = ReadAll(DataDir() + L"\\apps.txt"); int n = 0; for (char c : cur) if (c == '\n') ++n;
    if (!ok || n >= 30) { MessageBoxW(m_hWnd, n >= 30 ? L"You already have 30 apps. Remove one first." : L"The app could not be saved.", L"Libra Browser", MB_OK | MB_ICONINFORMATION); DeleteFileW(ico.c_str()); return; }
    SYSTEMTIME st; GetLocalTime(&st); wchar_t dt[16]; swprintf_s(dt, L"%04d-%02d-%02d", st.wYear, st.wMonth, st.wDay);
    std::wstring line = a->id + L"\t" + a->name + L"\t" + a->url + L"\t" + a->scope + L"\t" + a->id + L".ico\t" + dt + L"\n";
    WriteAll(DataDir() + L"\\apps.txt", cur + ToUtf8(line), false);
    ApMakeLink(false, a->id, a->name); if (m_appDesk) ApMakeLink(true, a->id, a->name);   // Start menu always, Desktop only when ticked
    m_appInst.insert(a->host); AppRelayout(); InvalidateRect(m_hWnd, nullptr, FALSE);
    wchar_t tv[8]; if (TestEnv(L"LIBRA_TEST_APPNOLAUNCH", tv, 8) > 0) return;
    AppLaunch(a->id);   // the app opens in its own window
}
