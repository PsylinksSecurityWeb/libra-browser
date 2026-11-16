// Encrypted backup of bookmarks + a short list of settings, in ONE file protected by a passphrase (no server, no cloud). Included after Pdf.inl.
// File = "LBK1" + iterations(u32) + salt(16) + nonce(12) + tag(16) + AES-256-GCM ciphertext. Key = PBKDF2-HMAC-SHA256 (600,000 rounds). The header is authenticated too.
// Never included: saved passwords, the assistant key and memory, history, cookies. Restoring shows a preview first; settings apply after a restart.

static const wchar_t* kBkSet[] = { L"engine", L"engine_url", L"startup", L"homepage", L"block_default", L"local_block", L"block_images", L"perf", L"save_video", L"https_upgrade", L"fingerprint", L"tz_utc", L"web_suggest", L"idle_prompt", L"idle_min", L"block_3p", L"sn_count" };
static bool BkSetOk(const std::wstring& k, const std::wstring& v) {
    if (v.size() > 300) return false; for (wchar_t c : v) if (c < 0x20) return false;
    for (const wchar_t* t : { L"local_block", L"block_images", L"save_video", L"https_upgrade", L"fingerprint", L"tz_utc", L"web_suggest", L"idle_prompt", L"block_3p", L"sn_count" }) if (k == t) return v == L"0" || v == L"1";
    if (k == L"block_default") return v == L"0" || v == L"1" || v == L"2";
    if (k == L"perf") return v == L"auto" || v == L"light" || v == L"normal";
    if (k == L"startup") return v == L"home" || v == L"last" || v == L"url";
    if (k == L"idle_min") { if (v.empty() || v.size() > 4) return false; for (wchar_t c : v) if (c < L'0' || c > L'9') return false; return true; }
    if (k == L"engine") return v == L"Custom" || std::any_of(std::begin(kEngines), std::end(kEngines), [&](auto& e) { return v == e.name; });
    if (k == L"engine_url") return v.empty() || v.rfind(L"https://", 0) == 0;
    if (k == L"homepage") return v.empty() || IsHttp(v);
    return false;
}
static bool BkKey(const std::string& pass, const BYTE* salt, ULONG iter, BYTE key[32]) {
    BCRYPT_ALG_HANDLE h = nullptr; if (BCryptOpenAlgorithmProvider(&h, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) return false;
    NTSTATUS s = BCryptDeriveKeyPBKDF2(h, (PUCHAR)pass.data(), (ULONG)pass.size(), (PUCHAR)salt, 16, iter, key, 32, 0); BCryptCloseAlgorithmProvider(h, 0); return s == 0;
}
static bool BkAes(bool enc, const BYTE key[32], const BYTE nonce[12], const std::string& aad, std::string& data, BYTE tag[16]) {
    if (data.empty()) return false; BCRYPT_ALG_HANDLE a = nullptr; BCRYPT_KEY_HANDLE k = nullptr; bool ok = false;
    if (BCryptOpenAlgorithmProvider(&a, BCRYPT_AES_ALGORITHM, nullptr, 0) == 0) {
        if (BCryptSetProperty(a, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0) == 0 && BCryptGenerateSymmetricKey(a, &k, nullptr, 0, (PUCHAR)key, 32, 0) == 0) {
            BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO ci; BCRYPT_INIT_AUTH_MODE_INFO(ci); ci.pbNonce = (PUCHAR)nonce; ci.cbNonce = 12; ci.pbAuthData = (PUCHAR)aad.data(); ci.cbAuthData = (ULONG)aad.size(); ci.pbTag = tag; ci.cbTag = 16;
            std::string out(data.size(), 0); ULONG n = 0;
            NTSTATUS s = enc ? BCryptEncrypt(k, (PUCHAR)data.data(), (ULONG)data.size(), &ci, nullptr, 0, (PUCHAR)out.data(), (ULONG)out.size(), &n, 0) : BCryptDecrypt(k, (PUCHAR)data.data(), (ULONG)data.size(), &ci, nullptr, 0, (PUCHAR)out.data(), (ULONG)out.size(), &n, 0);
            if (s == 0 && n == out.size()) { data = out; ok = true; }
            BCryptDestroyKey(k); }
        BCryptCloseAlgorithmProvider(a, 0); }
    return ok;
}
void BrowserWindow::OnBackup(int idx, const std::vector<std::wstring>& v) {
    Tab* t = m_tabs[idx].get(); if (!t->webView || v.size() < 2) return;
    auto push = [&](const std::wstring& js) { t->webView->ExecuteScript((L"lbImp(" + js + L")").c_str(), nullptr); };
    std::string pass = ToUtf8(v[1]); const std::wstring& c = v[0];
    struct Wipe { std::string& s; ~Wipe() { SecureZeroMemory(s.data(), s.size()); } } wipe{ pass };
    if (pass.size() < 8 || pass.size() > 200) { push(L"{err:\"The passphrase must have at least 8 characters.\"}"); return; }
    wchar_t tf[MAX_PATH];
    if (c == L"bm.bkexp") {
        std::string plain = "LBK1\n"; size_t nb = 0, ns = 0;
        for (auto& b : m_bm) { if (nb >= IM_MAX_ITEMS) break; plain += "B\t" + ToUtf8(Flat(b.url)) + "\t" + ToUtf8(Flat(b.title)) + "\n"; ++nb; }
        for (const wchar_t* k : kBkSet) { std::wstring val = m_set.Get(k, L"\x01"); if (val == L"\x01" || !BkSetOk(k, val)) continue; plain += "S\t" + ToUtf8(k) + "\t" + ToUtf8(val) + "\n"; ++ns; }
        BYTE salt[16], nonce[12], tag[16], key[32]; ULONG iter = 600000; BCryptGenRandom(nullptr, salt, 16, BCRYPT_USE_SYSTEM_PREFERRED_RNG); BCryptGenRandom(nullptr, nonce, 12, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        std::string hdr = "LBK1"; hdr.append((char*)&iter, 4); hdr.append((char*)salt, 16); hdr.append((char*)nonce, 12);
        std::string body = plain; SecureZeroMemory(plain.data(), plain.size());
        if (!BkKey(pass, salt, iter, key) || !BkAes(true, key, nonce, hdr, body, tag)) { SecureZeroMemory(key, 32); push(L"{err:\"The backup could not be made.\"}"); return; }
        SecureZeroMemory(key, 32); std::string file = hdr; file.append((char*)tag, 16); file += body;
        std::wstring path, name = L"Libra-backup.libra-backup";
        if (TestEnv(L"LIBRA_TEST_SAVEPATH", tf, MAX_PATH)) { if (wcscmp(tf, L"CANCEL") == 0) { push(L"{info:\"Not saved.\"}"); return; } path = std::wstring(tf) + L"\\" + name; }
        else { wil::com_ptr<IFileSaveDialog> d;
            if (SUCCEEDED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) {
                COMDLG_FILTERSPEC fs[] = { { L"Libra backup", L"*.libra-backup" } }; d->SetFileTypes(1, fs); d->SetDefaultExtension(L"libra-backup"); d->SetFileName(name.c_str()); d->SetTitle(L"Save the encrypted backup");
                PWSTR dl = nullptr; if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &dl))) { wil::com_ptr<IShellItem> fi; if (SUCCEEDED(SHCreateItemFromParsingName(dl, nullptr, IID_PPV_ARGS(&fi)))) d->SetDefaultFolder(fi.get()); CoTaskMemFree(dl); }
                if (SUCCEEDED(d->Show(m_hWnd))) { wil::com_ptr<IShellItem> it; PWSTR ps = nullptr; if (SUCCEEDED(d->GetResult(&it)) && SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &ps))) { path = ps; CoTaskMemFree(ps); } } }
            if (path.empty()) { push(L"{info:\"Not saved.\"}"); return; } }
        WriteAllRaw(path, file, false);
        push(L"{info:\"Saved " + std::to_wstring(nb) + L" bookmarks and " + std::to_wstring(ns) + L" settings, encrypted with your passphrase. Keep the passphrase: it can not be recovered.\"}"); return;
    }
    if (c != L"bm.bkimp") return;
    std::wstring path;
    if (TestEnv(L"LIBRA_TEST_IMPFILE", tf, MAX_PATH)) path = tf;
    else { wil::com_ptr<IFileOpenDialog> d;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) {
            COMDLG_FILTERSPEC fs[] = { { L"Libra backup", L"*.libra-backup" }, { L"All files", L"*.*" } }; d->SetFileTypes(2, fs); DWORD o = 0; d->GetOptions(&o); d->SetOptions(o | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST); d->SetTitle(L"Choose a Libra backup");
            if (SUCCEEDED(d->Show(m_hWnd))) { wil::com_ptr<IShellItem> it; PWSTR ps = nullptr; if (SUCCEEDED(d->GetResult(&it)) && SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &ps))) { path = ps; CoTaskMemFree(ps); } } }
        if (path.empty()) return; }
    std::string f = ImReadFile(path);
    if (f.size() < 4 + 4 + 16 + 12 + 16 + 8 || f.compare(0, 4, "LBK1") != 0) { push(L"{err:\"That is not a Libra backup file.\"}"); return; }
    ULONG iter = 0; memcpy(&iter, f.data() + 4, 4); if (iter < 100000 || iter > 5000000) { push(L"{err:\"That backup file is damaged.\"}"); return; }
    const BYTE* salt = (const BYTE*)f.data() + 8; const BYTE* nonce = salt + 16; BYTE tag[16]; memcpy(tag, nonce + 12, 16);
    std::string hdr = f.substr(0, 4 + 4 + 16 + 12), body = f.substr(hdr.size() + 16); BYTE key[32];
    bool ok = BkKey(pass, salt, iter, key) && BkAes(false, key, nonce, hdr, body, tag); SecureZeroMemory(key, 32);
    if (!ok || body.compare(0, 5, "LBK1\n") != 0) { push(L"{err:\"Wrong passphrase, or the file is damaged. Nothing was changed.\"}"); return; }
    std::set<std::string> have, seen; for (auto& b : m_bm) have.insert(ToUtf8(b.url));
    m_imp.clear(); m_impSet.clear(); size_t found = 0, dup = 0, drop = 0, p = 5;
    while (p < body.size()) {
        size_t e = body.find('\n', p); if (e == std::string::npos) e = body.size(); std::string ln = body.substr(p, e - p); p = e + 1;
        size_t t1 = ln.find('\t'), t2 = t1 == std::string::npos ? t1 : ln.find('\t', t1 + 1); if (t1 != 1 || t2 == std::string::npos) continue;
        std::string a = ln.substr(2, t2 - 2), b = ln.substr(t2 + 1);
        if (ln[0] == 'B') {
            if (!ImUrlOk(a)) { ++drop; continue; } ++found; if (have.count(a) || !seen.insert(a).second) { ++dup; continue; }
            std::wstring ti = FromUtf8(b), cl; for (wchar_t ch : ti) if (ch >= 0x20 && ch != 0x7f) cl += ch; if (cl.size() > 200) cl.resize(200); if (m_imp.size() < IM_MAX_ITEMS) m_imp.push_back({ a, ToUtf8(cl) });
        } else if (ln[0] == 'S' && m_impSet.size() < 40) { std::wstring k = FromUtf8(a), val = FromUtf8(b); bool known = false; for (const wchar_t* kk : kBkSet) if (k == kk) known = true; if (known && BkSetOk(k, val)) m_impSet.push_back({ k, val }); }
    }
    push(L"{src:\"the backup\",found:" + std::to_wstring(found) + L",dup:" + std::to_wstring(dup) + L",drop:" + std::to_wstring(drop) + L",n:" + std::to_wstring(m_imp.size()) + L",sets:" + std::to_wstring(m_impSet.size()) + L",sample:[]}");
}
