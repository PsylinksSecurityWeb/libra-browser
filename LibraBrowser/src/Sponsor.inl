// Sponsored tiles and background on the new-tab page. Included at the end of BrowserWindow.cpp. See PLAN-sponsored.md.
// Inert until the owner's public key is set below. Sends nothing about the user: one anonymous GET of the signed feed per day.
// Hidden for private windows, Child Safety, first run, and when the user switched it off (Settings > Privacy).

struct SpCfg { std::wstring feed; std::string base; std::vector<SpKeyA> keys; bool test = false; };
static SpCfg SpConfig() {
    SpCfg c; c.feed = L"https://feed.psylinks-beta.com/v1/feed";   // OWNER: the https address where the Worker answers (same host as PUBLIC_BASE)
    static const SpKeyA owner[2] = { { "k1", "", "" }, { "k2", "", "" } };   // OWNER: paste x and y printed by sponsor-worker\genkey.mjs (k2 is the spare for key rotation)
    for (auto& k : owner) c.keys.push_back(k);
    wchar_t f[512], k[512];
    if (TestEnv(L"LIBRA_TEST_SPFEED", f, 512) && TestEnv(L"LIBRA_TEST_SPKEY", k, 512)) {   // tests only: "kid|x|y"
        c.feed = f; c.test = true; c.keys.clear(); std::string ks = ToUtf8(k); size_t a = ks.find('|'), b = a == std::string::npos ? a : ks.find('|', a + 1);
        if (b != std::string::npos) c.keys.push_back({ ks.substr(0, a), ks.substr(a + 1, b - a - 1), ks.substr(b + 1) });
    }
    dl::Url u; if (!dl::Crack(c.feed, u) || (!u.https && !c.test)) { c.keys.clear(); return c; }
    std::string port = ((u.https && u.port != 443) || (!u.https && u.port != 80)) ? ":" + std::to_string(u.port) : "";
    c.base = std::string(u.https ? "https://" : "http://") + ToUtf8(u.host) + port;
    bool any = false; for (auto& k2 : c.keys) if (!k2.x.empty()) any = true;
    if (!any) c.keys.clear();
    return c;
}

static bool SpGet(int port, bool direct, const std::wstring& url, size_t cap, std::string& body) {   // plain GET, no cookies, no redirects
    dl::Url u; body.clear(); if (!dl::Crack(url, u)) return false;
    HINTERNET ses = direct ? WinHttpOpen(L"Libra", WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0) : dl::Session(port, 1); if (!ses) return false;
    WinHttpSetTimeouts(ses, 5000, 5000, 8000, 12000); bool ok = false;
    if (HINTERNET con = WinHttpConnect(ses, u.host.c_str(), u.port, 0)) {
        if (HINTERNET rq = WinHttpOpenRequest(con, L"GET", u.path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, u.https ? WINHTTP_FLAG_SECURE : 0)) {
            DWORD nc = WINHTTP_DISABLE_COOKIES | WINHTTP_DISABLE_REDIRECTS; WinHttpSetOption(rq, WINHTTP_OPTION_DISABLE_FEATURE, &nc, sizeof nc);
            static const wchar_t* hdr = L"User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0 Safari/537.36\r\nAccept: application/json, image/*";
            if (WinHttpSendRequest(rq, hdr, (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) && WinHttpReceiveResponse(rq, nullptr)) {
                DWORD st = 0, sl = sizeof st; WinHttpQueryHeaders(rq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &st, &sl, nullptr);
                if (st == 200) { char buf[8192]; DWORD n = 0; while (body.size() <= cap && WinHttpReadData(rq, buf, sizeof buf, &n) && n) body.append(buf, n); ok = body.size() <= cap; }
            }
            WinHttpCloseHandle(rq);
        }
        WinHttpCloseHandle(con);
    }
    WinHttpCloseHandle(ses); return ok;
}
static const char* SpImgMime(const std::string& b) {   // by content, never by what the server says
    const unsigned char* p = (const unsigned char*)b.data();
    if (b.size() >= 8 && !memcmp(p, "\x89PNG\r\n\x1a\n", 8)) return "image/png";
    if (b.size() >= 3 && p[0] == 0xFF && p[1] == 0xD8 && p[2] == 0xFF) return "image/jpeg";
    if (b.size() >= 12 && !memcmp(p, "RIFF", 4) && !memcmp(p + 8, "WEBP", 4)) return "image/webp";
    return nullptr;
}
static std::string SpB64(const std::string& b) {
    DWORD n = 0; if (!CryptBinaryToStringA((const BYTE*)b.data(), (DWORD)b.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &n)) return {};
    std::string o(n, 0); if (!CryptBinaryToStringA((const BYTE*)b.data(), (DWORD)b.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, o.data(), &n)) return {};
    o.resize(n); while (!o.empty() && o.back() == 0) o.pop_back(); return o;
}
static const char* SpMimeOfB64(const std::string& b) { return b.rfind("iVBOR", 0) == 0 ? "image/png" : b.rfind("/9j/", 0) == 0 ? "image/jpeg" : b.rfind("UklGR", 0) == 0 ? "image/webp" : nullptr; }

struct SpResult { std::string env; SpFeed feed; };
static SpResult* SpFetch(const SpCfg& c, int port, long long prevSeq, long long now) {   // runs on a worker thread
    std::string body; if (!SpGet(port, c.test, c.feed, 262144, body)) return nullptr;
    while (!body.empty() && isspace((unsigned char)body.back())) body.pop_back();
    if (body.find('\n') != std::string::npos || body.find('\r') != std::string::npos) return nullptr;
    std::unique_ptr<SpResult> r(new SpResult); r->env = body;
    if (!SpParse(body, c.base, c.keys, r->feed).empty() || r->feed.seq < prevSeq || r->feed.expires < now) return nullptr;   // unsigned, rolled back or expired: ignore
    int n = 0;
    for (auto& it : r->feed.items) {
        if (it.img.empty() || n >= 6 || it.end < now || it.start > now + 3 * 86400) continue;
        std::string img; if (!SpGet(port, c.test, FromUtf8(it.img), 450 * 1024, img) || img.size() < 100 || !SpImgMime(img)) continue;
        it.imgB64 = SpB64(img); if (!it.imgB64.empty()) ++n;
    }
    return r.release();
}

static std::map<std::string, long long> SpHiddenLoad(const Settings& s) {   // "id=until,id=until"
    std::map<std::string, long long> m; std::string t = ToUtf8(s.Get(L"sp_hide")); size_t p = 0; long long now = (long long)time(nullptr);
    while (p < t.size() && m.size() < 40) { size_t e = t.find(',', p); std::string kv = t.substr(p, e == std::string::npos ? e : e - p), k = kv.substr(0, kv.find('='));
        long long until = SpNum(kv.find('=') == std::string::npos ? "" : kv.substr(kv.find('=') + 1)); if (SpId(k) && until > now) m[k] = until; if (e == std::string::npos) break; p = e + 1; }
    return m;
}
static std::string SpRegion() {
    wchar_t b[16] = {}; if (TestEnv(L"LIBRA_TEST_SPREGION", b, 16)) return ToUtf8(b);
    GEOID g = GetUserGeoID(GEOCLASS_NATION); if (g != GEOID_NOT_AVAILABLE && GetGeoInfoW(g, GEO_ISO2, b, 16, 0) > 0) return ToUtf8(b);
    return "";
}
bool BrowserWindow::SponsorOn() const { return !m_incognito && !m_setup && !m_child && m_set.Get(L"sponsored", L"1") != L"0"; }

void BrowserWindow::SponsorLoad() {   // cached feed from disk, checked again (signature) every time
    m_sp = SpFeed(); if (!SponsorOn()) return;
    SpCfg c = SpConfig(); if (c.keys.empty()) return;
    std::string d = ReadAll(DataDir() + L"\\spons.dat"); if (d.empty() || d.size() > 4u << 20) return;
    size_t e = d.find('\n'); std::string env = d.substr(0, e);
    if (!SpParse(env, c.base, c.keys, m_sp).empty()) { m_sp = SpFeed(); return; }
    while (e != std::string::npos && e + 1 < d.size()) {
        size_t s = e + 1; e = d.find('\n', s); std::string ln = d.substr(s, e == std::string::npos ? e : e - s);
        if (ln.rfind("IMG\t", 0) != 0) continue; size_t t2 = ln.find('\t', 4); if (t2 == std::string::npos) continue;
        std::string id = ln.substr(4, t2 - 4), b64 = ln.substr(t2 + 1);
        if (b64.size() > 700000 || !SpMimeOfB64(b64) || !SafeB64(b64)) continue;
        for (auto& it : m_sp.items) if (it.id == id && !it.img.empty()) it.imgB64 = b64;
    }
}
void BrowserWindow::SponsorKick() {   // once a day, in the background
    if (m_spBusy || !SponsorOn()) return;
    SpCfg c = SpConfig(); if (c.keys.empty()) return;
    long long now = (long long)time(nullptr), last = _wtoi64(m_set.Get(L"sp_last", L"0").c_str());
    if (now >= last && now - last < 20 * 3600) return;
    int port = DohProxy::Get().Port(); if (!port && !c.test) return;
    m_spBusy = true; HWND hw = m_hWnd; long long prev = _wtoi64(m_set.Get(L"sp_seq", L"0").c_str());
    std::thread([hw, c, port, prev, now] { PostMessage(hw, WM_APP + 37, 0, (LPARAM)SpFetch(c, port, prev, now)); }).detach();
}
void BrowserWindow::SponsorGot(void* p) {
    m_spBusy = false; std::unique_ptr<SpResult> r((SpResult*)p); if (!r || !SponsorOn()) return;
    long long prev = _wtoi64(m_set.Get(L"sp_seq", L"0").c_str()); if (r->feed.seq < prev) return;
    m_sp = r->feed; m_set.Set(L"sp_seq", std::to_wstring(m_sp.seq)); m_set.Set(L"sp_last", std::to_wstring((long long)time(nullptr)));
    std::string o = r->env + "\n"; for (auto& it : m_sp.items) if (!it.imgB64.empty()) o += "IMG\t" + it.id + "\t" + it.imgB64 + "\n";
    CreateDirectoryW(DataDir().c_str(), nullptr); WriteAll(DataDir() + L"\\spons.dat", o);
    for (size_t i = 0; i < m_tabs.size(); ++i) if (m_tabs[i]->url == L"libra://home" && m_tabs[i]->webView) RenderTab((int)i);   // show it on open home pages
}
void BrowserWindow::SponsorHide(const std::wstring& id) {   // hide one campaign for 30 days
    std::string k = ToUtf8(id); if (!SpId(k)) return;
    auto m = SpHiddenLoad(m_set); m[k] = (long long)time(nullptr) + 30 * 86400; std::string o;
    for (auto& kv : m) o += (o.empty() ? "" : ",") + kv.first + "=" + std::to_string(kv.second);
    m_set.Set(L"sp_hide", FromUtf8(o));
}
void BrowserWindow::SponsorHtml(std::wstring& tiles, std::wstring& bg) {
    tiles.clear(); bg.clear(); if (!SponsorOn() || m_sp.items.empty()) return;
    long long now = (long long)time(nullptr); std::string region = SpRegion(); auto hid = SpHiddenLoad(m_set);
    auto tl = SpPick(m_sp, "tile", 2, now, region, hid), bl = SpPick(m_sp, "background", 1, now, region, hid);
    for (auto* it : tl) {
        std::wstring id = FromUtf8(it->id), name = FromUtf8(it->title); unsigned hh = 5381; for (wchar_t ch : name) hh = hh * 33 + ch; int hue = hh % 360;
        tiles += L"<div class=\"tw sp\" data-id=\"" + Esc(id) + L"\"><button class=\"spx\" data-sp=\"home.sphide\" data-id=\"" + Esc(id) + L"\" aria-label=\"Hide this\" title=\"Hide this\">&#215;</button><a class=\"site\" href=\"" + Esc(FromUtf8(it->click)) + L"\"><div class=\"ico\"";
        if (!it->imgB64.empty() && SpMimeOfB64(it->imgB64)) tiles += L"><img alt=\"\" src=\"data:" + FromUtf8(SpMimeOfB64(it->imgB64)) + L";base64," + FromUtf8(it->imgB64) + L"\">";
        else tiles += L" style=\"background:hsl(" + std::to_wstring(hue) + L",70%,92%);color:hsl(" + std::to_wstring(hue) + L",55%,32%)\">" + std::wstring(1, (wchar_t)towupper(name.empty() ? L'?' : name[0]));
        tiles += L"</div><span>" + Esc(name) + L"</span><em class=\"spl\">Sponsored</em></a></div>";
    }
    if (tl.empty() && bl.empty()) return;
    for (auto* it : bl) {
        const char* mime = SpMimeOfB64(it->imgB64); if (!mime) continue; std::wstring id = FromUtf8(it->id);
        bg += L"<div class=\"spbg\" style=\"background-image:url(data:" + FromUtf8(mime) + L";base64," + FromUtf8(it->imgB64) + L")\"></div>"
              L"<div class=\"spill\"><span>Sponsored &middot; " + Esc(FromUtf8(it->advertiser)) + L"</span><a href=\"" + Esc(FromUtf8(it->click)) + L"\">Visit</a><button data-sp=\"home.sphide\" data-id=\"" + Esc(id) + L"\">Hide</button></div>";
    }
    bg += L"<a class=\"spoff\" href=\"#\" data-sp=\"home.spoff\">Turn off sponsored content</a>"
          L"<script>document.addEventListener('click',e=>{const b=e.target.closest('[data-sp]');if(b){e.preventDefault();chrome.webview.postMessage(b.dataset.sp+'\\t'+(b.dataset.id||''))}})</script>";
}
