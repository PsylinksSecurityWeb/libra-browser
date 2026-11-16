// Update notifier. Included after Sponsor.inl. Once a week a signed manifest is fetched (no id, no cookies, same generic headers as the sponsored feed).
// A newer version shows a small bar on the new-tab page and a row in Settings > About. Update = download, check the signed SHA-256, ask, then open the installer. Never silent.
// Inert until SpConfig() has the public key (same switch as the sponsored feed).

static std::string UpSha256(const std::string& d) {
    BCRYPT_ALG_HANDLE a = nullptr; BCRYPT_HASH_HANDLE h = nullptr; BYTE out[32] = {}; std::string hex;
    if (BCryptOpenAlgorithmProvider(&a, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0) {
        if (BCryptCreateHash(a, &h, nullptr, 0, nullptr, 0, 0) == 0) { if (BCryptHashData(h, (PUCHAR)d.data(), (ULONG)d.size(), 0) == 0 && BCryptFinishHash(h, out, 32, 0) == 0) { static const char* X = "0123456789abcdef"; for (BYTE b : out) { hex += X[b >> 4]; hex += X[b & 15]; } } BCryptDestroyHash(h); }
        BCryptCloseAlgorithmProvider(a, 0);
    }
    return hex;
}
struct UpDl { std::string err; std::wstring path; std::string ver; };

void BrowserWindow::UpLoad() {   // the last good manifest, checked again every time
    m_up = UpInfo(); SpCfg c = SpConfig(); if (c.keys.empty()) return;
    std::string d = ReadAll(DataDir() + L"\\update.dat"); UpInfo i; long long now = (long long)time(nullptr);
    if (d.empty() || !UpParse(d, c.keys, i, c.test).empty() || i.expires < now || !UpNewer(i.ver, kLibraVer) || ToUtf8(m_set.Get(L"up_skip")) == i.ver) return;
    m_up = i;
}
void BrowserWindow::UpKick(bool force) {
    if (m_upBusy || m_incognito || m_setup) return; SpCfg c = SpConfig(); if (c.keys.empty()) return;
    long long now = (long long)time(nullptr), last = _wtoi64(m_set.Get(L"up_last", L"0").c_str());
    if (!force && now >= last && now - last < 7 * 86400) return;
    int port = DohProxy::Get().Port(); if (!port && !c.test) return;
    m_upBusy = true; HWND hw = m_hWnd; std::wstring url = FromUtf8(c.base) + L"/v1/update"; bool direct = c.test;
    std::thread([hw, port, direct, url] { std::string* b = new std::string; if (!SpGet(port, direct, url, 16384, *b)) { delete b; b = nullptr; } PostMessage(hw, WM_APP + 39, 0, (LPARAM)b); }).detach();
}
void BrowserWindow::UpGot(void* p) {
    m_upBusy = false; std::unique_ptr<std::string> b((std::string*)p); long long now = (long long)time(nullptr);
    if (!b) { m_set.Set(L"up_last", std::to_wstring(now - 6 * 86400)); return; }   // could not reach: try again tomorrow
    while (!b->empty() && isspace((unsigned char)b->back())) b->pop_back();
    SpCfg c = SpConfig(); UpInfo i; if (c.keys.empty() || !UpParse(*b, c.keys, i, c.test).empty() || i.expires < now || i.seq < _wtoi64(m_set.Get(L"up_seq", L"0").c_str())) return;   // unsigned, expired or rolled back: ignored
    m_set.Set(L"up_seq", std::to_wstring(i.seq)); m_set.Set(L"up_last", std::to_wstring(now)); WriteAll(DataDir() + L"\\update.dat", *b);
    UpLoad(); for (size_t t = 0; t < m_tabs.size(); ++t) if (m_tabs[t]->url == L"libra://home" && m_tabs[t]->webView) RenderTab((int)t);
}
std::wstring BrowserWindow::UpHtml() {   // bar on the new-tab page
    if (m_up.ver.empty() || m_incognito) return L"";
    return L"<div class=\"upd\" style=\"position:fixed;left:50%;bottom:22px;transform:translateX(-50%);display:flex;gap:14px;align-items:center;padding:10px 18px;border-radius:999px;background:#111;color:#fff;font:13px 'Segoe UI',sans-serif;z-index:9\">Libra Browser " + Esc(FromUtf8(m_up.ver)) +
           L" is available<button data-up=\"go\" style=\"background:#fff;color:#111;border:0;border-radius:999px;padding:6px 14px;cursor:pointer\">Update</button><button data-up=\"skip\" style=\"background:none;color:#aaa;border:0;cursor:pointer\">Skip this version</button></div>"
           L"<script>document.addEventListener('click',e=>{const b=e.target.closest('[data-up]');if(b){e.preventDefault();chrome.webview.postMessage('home.update\\t'+b.dataset.up)}})</script>";
}
void BrowserWindow::UpAct(const std::wstring& what) {
    if (m_up.ver.empty()) return;
    if (what == L"skip") { m_set.Set(L"up_skip", FromUtf8(m_up.ver)); m_up = UpInfo(); for (size_t t = 0; t < m_tabs.size(); ++t) if (m_tabs[t]->url == L"libra://home" && m_tabs[t]->webView) RenderTab((int)t); return; }
    if (what != L"go" || m_upBusy) return;
    SpCfg c = SpConfig(); int port = DohProxy::Get().Port(); if (c.keys.empty() || (!port && !c.test)) return;
    m_upBusy = true; HWND hw = m_hWnd; UpInfo u = m_up; bool direct = c.test; std::wstring dir = DataDir() + L"\\update";
    std::thread([hw, port, direct, u, dir] {
        std::unique_ptr<UpDl> r(new UpDl); r->ver = u.ver; std::string body;
        if (!SpGet(port, direct, FromUtf8(u.url), (size_t)u.size + 4096, body, true)) r->err = "download failed";
        else if ((long long)body.size() != u.size || body.size() < 2 || body[0] != 'M' || body[1] != 'Z') r->err = "the file is not the expected installer";
        else if (UpSha256(body) != u.sha) r->err = "the file does not match the signed checksum, so it was thrown away";
        else { CreateDirectoryW(dir.c_str(), nullptr); r->path = dir + L"\\LibraBrowser-Setup-" + FromUtf8(u.ver) + L".exe"; WriteAllRaw(r->path, body, false); }
        PostMessage(hw, WM_APP + 42, 0, (LPARAM)r.release()); }).detach();
}
void BrowserWindow::UpReady(void* p) {
    m_upBusy = false; std::unique_ptr<UpDl> r((UpDl*)p); if (!r) return;
    { wchar_t tf[MAX_PATH]; if (TestEnv(L"LIBRA_TEST_UPASK", tf, MAX_PATH)) { FILE* f = nullptr; _wfopen_s(&f, tf, L"a"); if (f) { fprintf(f, "%s %s\n", r->err.empty() ? "ready" : "error", r->err.empty() ? ToUtf8(r->path).c_str() : r->err.c_str()); fclose(f); } return; } }   // test hook: no dialog, no installer
    if (!r->err.empty()) { MessageBoxW(m_hWnd, (L"The update could not be installed: " + FromUtf8(r->err) + L".\n\nNothing was changed.").c_str(), L"Libra Browser update", MB_ICONWARNING); return; }
    if (MessageBoxW(m_hWnd, (L"Libra Browser " + FromUtf8(r->ver) + L" is downloaded and its checksum matches.\n\nLibra will close and the installer will open.\n\nInstall now?").c_str(), L"Libra Browser update", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) return;
    if ((INT_PTR)ShellExecuteW(m_hWnd, L"open", r->path.c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32) PostMessage(m_hWnd, WM_CLOSE, 0, 0);
}
