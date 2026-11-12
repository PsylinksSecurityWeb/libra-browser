// Child Safety mode. The truth lives in a small DPAPI-protected registry record (HKCU\Software\Psylinks\Libra\Safety)
// that the installer does not delete. A damaged record counts as ON (fail safe). Only native code can change it,
// and only after the Windows sign-in window says yes. The Settings page can only ask.
int WinAuthBlocking(HWND owner, const wchar_t* msg);
static const wchar_t* kSafeKey = L"Software\\Psylinks\\Libra";
static const wchar_t* kSafeVal = L"Safety";

static bool SafeRecordOn() {
    DWORD n = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, kSafeKey, kSafeVal, RRF_RT_REG_BINARY, nullptr, nullptr, &n) != ERROR_SUCCESS) return false;   // no record: off
    return true;   // a record exists: on (even when unreadable)
}
static void SafeRecordSet(bool on) {
    if (!on) { HKEY k; if (RegOpenKeyExW(HKEY_CURRENT_USER, kSafeKey, 0, KEY_SET_VALUE, &k) == ERROR_SUCCESS) { RegDeleteValueW(k, kSafeVal); RegCloseKey(k); } return; }
    BYTE rnd[16]; BCryptGenRandom(nullptr, rnd, sizeof(rnd), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
    std::string plain = "ON1:"; plain.append((char*)rnd, sizeof(rnd));
    static const BYTE ent[] = "LibraSafety1"; DATA_BLOB in{ (DWORD)plain.size(), (BYTE*)plain.data() }, e{ sizeof(ent), (BYTE*)ent }, out{};
    if (!CryptProtectData(&in, L"Libra", &e, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return;
    HKEY k; if (RegCreateKeyExW(HKEY_CURRENT_USER, kSafeKey, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        RegSetValueExW(k, kSafeVal, 0, REG_BINARY, out.pbData, out.cbData); RegCloseKey(k);
    }
    LocalFree(out.pbData);
}
void BrowserWindow::ChildApply(bool on, bool noLock) {
    m_child = on; SafeRecordSet(on);
    m_set.Set(L"child_safe", on ? L"1" : L"0"); m_set.Set(L"child_nolock", on && noLock ? L"1" : L"0");
    if (on && m_set.Get(L"block_default", L"0") == L"2") m_set.Set(L"block_default", L"1");
    DohProxy::Get().SetFamily(on); DohProxy::Get().ClearCache();   // old answers must not hide the change
    UpdateBypass(); for (auto& tb : m_tabs) InstallAdScript(tb.get());
    for (auto& tb : m_tabs) if (tb->webView) { wil::com_ptr<ICoreWebView2Settings> st; if (SUCCEEDED(tb->webView->get_Settings(&st))) st->put_AreDevToolsEnabled(on ? FALSE : TRUE); }
    for (int i = 0; i < (int)m_tabs.size(); ++i) if (m_tabs[i]->url == L"libra://settings") RenderTab(i);
}
static void FamilyUpstreamFromSetting(const std::wstring& v) {   // setting family_dns = https://host/path ; anything odd falls back to the default
    std::wstring host = L"doh.cleanbrowsing.org", path = L"/doh/family-filter/";
    if (v.rfind(L"https://", 0) == 0 && v.size() < 200) {
        std::wstring rest = v.substr(8); size_t sl = rest.find(L'/');
        std::wstring h = rest.substr(0, sl), p = sl == std::wstring::npos ? L"/dns-query" : rest.substr(sl);
        bool ok = !h.empty() && h.find_first_not_of(L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.-") == std::wstring::npos && p.find_first_of(L" \t\r\n\"<>?#") == std::wstring::npos;
        if (ok) { host = h; path = p; }
    }
    DohProxy::Get().SetFamilyUpstream(host, path);
}
void BrowserWindow::ChildLoad() {
    FamilyUpstreamFromSetting(m_set.Get(L"family_dns", L""));
    m_child = !m_incognito && SafeRecordOn();   // the record wins over settings.ini
    m_set.Set(L"child_safe", m_child ? L"1" : L"0");
    DohProxy::Get().SetFamily(m_child);
}
void BrowserWindow::AskWindows(int purpose) {   // purposes: 1 turn on, 2 turn off, 4 unlock one site
    const wchar_t* msg = purpose == 1 ? L"Turn on Libra Child Safety mode" : purpose == 2 ? L"Turn off Libra Child Safety mode" : purpose == 5 ? L"View saved passwords in Libra" : L"Unlock this site in Libra";
    HWND hw = m_hWnd;
    std::thread([hw, purpose, msg] { int r = WinAuthBlocking(hw, msg); PostMessage(hw, WM_APP + 30, purpose, r); }).detach();
}
void BrowserWindow::OnWinAuth(int purpose, int r) {   // r: 0 verified, 1 this Windows account has no lock, 2 refused
    if (r == 2) { m_pwPend.clear(); return; }   // wrong password or cancelled: nothing changes
    if (purpose == 5) { m_pwUntil = GetTickCount64() + 300000; int ix = IndexOf(m_pwPendTab); if (ix >= 0 && !m_pwPend.empty()) { auto v = m_pwPend; m_pwPend.clear(); OnPwPage(ix, v); } return; }
    if (purpose == 1) ChildApply(true, r == 1);
    else if (purpose == 2) ChildApply(false, false);
    else if (purpose == 4 && m_child && !m_authHost.empty()) {
        DohProxy::Get().FamilyAllow(ToUtf8(m_authHost));   // this site, 30 minutes
        if (m_activeTab >= 0 && m_activeTab < (int)m_tabs.size()) { Tab* t = m_tabs[m_activeTab].get(); if (t->webView && !t->errUrl.empty()) { t->url = t->errUrl; t->webView->Navigate(t->errUrl.c_str()); } }
    }
}
