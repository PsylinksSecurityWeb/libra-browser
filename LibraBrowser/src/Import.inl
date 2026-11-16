// Bookmark import from the Bookmarks page. Included after Update.inl. The user picks the source; Libra reads the file (read only), shows what it found,
// and adds the bookmarks only after the user confirms. Nothing leaves the PC. Plan: reader/importer files are parsed with size caps (Import.h).

static std::string ImReadFile(const std::wstring& p) {
    HANDLE h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);   // the other browser may be running
    if (h == INVALID_HANDLE_VALUE) return {};
    LARGE_INTEGER sz{}; std::string d;
    if (GetFileSizeEx(h, &sz) && sz.QuadPart > 0 && sz.QuadPart <= (long long)IM_MAX_FILE) { d.resize((size_t)sz.QuadPart); DWORD got = 0; if (!ReadFile(h, d.data(), (DWORD)d.size(), &got, nullptr) || got != d.size()) d.clear(); }
    CloseHandle(h); return d;
}
static std::wstring ImEnv(const wchar_t* n) { wchar_t b[MAX_PATH]; DWORD k = GetEnvironmentVariableW(n, b, MAX_PATH); return k && k < MAX_PATH ? b : L""; }
static std::wstring ImFirefoxBackup() {   // newest bookmarks-*.jsonlz4 over all profiles
    std::wstring root = ImEnv(L"APPDATA") + L"\\Mozilla\\Firefox\\Profiles\\", best, bestName; WIN32_FIND_DATAW pf;
    HANDLE hp = FindFirstFileW((root + L"*").c_str(), &pf); if (hp == INVALID_HANDLE_VALUE) return L"";
    do { if (!(pf.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || pf.cFileName[0] == L'.') continue;
        std::wstring dir = root + pf.cFileName + L"\\bookmarkbackups\\"; WIN32_FIND_DATAW bf; HANDLE hb = FindFirstFileW((dir + L"bookmarks-*.jsonlz4").c_str(), &bf);
        if (hb != INVALID_HANDLE_VALUE) { do { if (bestName.empty() || wcscmp(bf.cFileName, bestName.c_str()) > 0) { bestName = bf.cFileName; best = dir + bf.cFileName; } } while (FindNextFileW(hb, &bf)); FindClose(hb); }
    } while (FindNextFileW(hp, &pf)); FindClose(hp); return best;
}
static std::wstring ImJs(const std::wstring& s) {   // text for a JavaScript string literal
    std::wstring o; for (wchar_t c : s) { if (c == L'"' || c == L'\\') { o += L'\\'; o += c; } else if (c < 0x20 || c == L'<' || c == L'>' || c == 0x2028 || c == 0x2029) o += L' '; else o += c; } return o;
}
void BrowserWindow::OnImport(int idx, const std::wstring& c, const std::wstring& a) {
    Tab* t = m_tabs[idx].get(); if (!t->webView) return;
    auto push = [&](const std::wstring& js) { t->webView->ExecuteScript((L"lbImp(" + js + L")").c_str(), nullptr); };
    if (c == L"bm.impno") { m_imp.clear(); m_impSet.clear(); push(L"{clear:1}"); return; }
    if (c == L"bm.impgo") {
        size_t add = 0; for (auto& kv : m_imp) { if (m_bm.size() >= IM_MAX_ITEMS) break; m_bm.push_back({ FromUtf8(kv.first), FromUtf8(kv.second) }); ++add; }
        m_imp.clear(); if (add) SaveBookmarks();
        for (auto& kv : m_impSet) m_set.Set(kv.first, kv.second);   // from a backup: validated settings, they apply after a restart
        m_impSet.clear(); RenderTab(idx); return;
    }
    if (c != L"bm.imp") return;
    std::wstring path, src; std::string data;
    wchar_t tf[MAX_PATH];
    if (a == L"file") {
        src = L"the file";
        if (TestEnv(L"LIBRA_TEST_IMPFILE", tf, MAX_PATH)) path = tf;
        else { wil::com_ptr<IFileOpenDialog> d;
            if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) {
                COMDLG_FILTERSPEC fs[] = { { L"Bookmarks (html, json, jsonlz4)", L"*.html;*.htm;*.json;*.jsonlz4" }, { L"All files", L"*.*" } }; d->SetFileTypes(2, fs);
                DWORD o = 0; d->GetOptions(&o); d->SetOptions(o | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST); d->SetTitle(L"Choose a bookmarks file to import");
                if (SUCCEEDED(d->Show(m_hWnd))) { wil::com_ptr<IShellItem> it; PWSTR ps = nullptr; if (SUCCEEDED(d->GetResult(&it)) && SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &ps))) { path = ps; CoTaskMemFree(ps); } }
            }
            if (path.empty()) return; }   // cancelled
    } else if (a == L"chrome") { src = L"Chrome"; path = ImEnv(L"LOCALAPPDATA") + L"\\Google\\Chrome\\User Data\\Default\\Bookmarks"; }
    else if (a == L"edge") { src = L"Edge"; path = ImEnv(L"LOCALAPPDATA") + L"\\Microsoft\\Edge\\User Data\\Default\\Bookmarks"; }
    else if (a == L"brave") { src = L"Brave"; path = ImEnv(L"LOCALAPPDATA") + L"\\BraveSoftware\\Brave-Browser\\User Data\\Default\\Bookmarks"; }
    else if (a == L"firefox") { src = L"Firefox"; path = ImFirefoxBackup(); }
    else return;
    data = path.empty() ? std::string() : ImReadFile(path);
    if (data.empty()) { push(L"{err:\"No bookmarks file was found for " + (a == L"file" ? std::wstring(L"that choice") : src) + L" on this PC (or it could not be read).\"}"); return; }
    ImList all; ImParse(data, all); data.clear();
    std::set<std::string> have, seen; for (auto& b : m_bm) have.insert(ToUtf8(b.url));
    size_t found = 0, dup = 0, drop = 0; m_imp.clear(); m_impSet.clear();
    for (auto& kv : all) {
        if (!ImUrlOk(kv.first)) { ++drop; continue; } ++found;
        if (have.count(kv.first) || !seen.insert(kv.first).second) { ++dup; continue; }
        std::wstring ti = FromUtf8(kv.second); std::wstring cl; for (wchar_t ch : ti) if (ch >= 0x20 && ch != 0x7f) cl += ch; if (cl.size() > 200) cl.resize(200);
        m_imp.push_back({ kv.first, ToUtf8(cl) });
    }
    if (all.empty()) { push(L"{err:\"No bookmarks were found in this file.\"}"); return; }
    std::wstring sample; for (size_t i = 0; i < m_imp.size() && i < 5; ++i) sample += L"\"" + ImJs(FromUtf8(m_imp[i].second.empty() ? m_imp[i].first : m_imp[i].second)).substr(0, 60) + L"\",";
    push(L"{src:\"" + ImJs(src) + L"\",found:" + std::to_wstring(found) + L",dup:" + std::to_wstring(dup) + L",drop:" + std::to_wstring(drop) + L",n:" + std::to_wstring(m_imp.size()) + L",sample:[" + sample + L"]}");
}

static std::wstring ImFound() {   // first-run offer: which browsers have a bookmarks file worth importing (file size only, nothing is read)
    std::wstring r; auto big = [](const std::wstring& p) { WIN32_FILE_ATTRIBUTE_DATA d; return GetFileAttributesExW(p.c_str(), GetFileExInfoStandard, &d) && d.nFileSizeLow > 1500; };
    std::wstring la = ImEnv(L"LOCALAPPDATA");
    if (big(la + L"\\Google\\Chrome\\User Data\\Default\\Bookmarks")) r += L"chrome,";
    if (big(la + L"\\Microsoft\\Edge\\User Data\\Default\\Bookmarks")) r += L"edge,";
    if (big(la + L"\\BraveSoftware\\Brave-Browser\\User Data\\Default\\Bookmarks")) r += L"brave,";
    if (!ImFirefoxBackup().empty()) r += L"firefox,";
    return r;
}
void BrowserWindow::OnboardImport(int idx, const std::wstring& a) {   // first run: one click on "Import from X" reads that file and adds the bookmarks
    OnImport(idx, L"bm.imp", a); if (m_imp.empty()) return;
    size_t add = 0; for (auto& kv : m_imp) { if (m_bm.size() >= IM_MAX_ITEMS) break; m_bm.push_back({ FromUtf8(kv.first), FromUtf8(kv.second) }); ++add; }
    m_imp.clear(); if (add) SaveBookmarks();
    Tab* t = m_tabs[idx].get(); if (t->webView) t->webView->ExecuteScript((L"lbImp({done:" + std::to_wstring(add) + L"})").c_str(), nullptr);
}
