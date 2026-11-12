#pragma once
// Loopback-only HTTP/CONNECT proxy. WebView2 sends every request here; we resolve the
// hostname ONLY through the DoH server (RFC 8484 GET) and tunnel to the answer. A name the
// server blocks (NXDOMAIN or 0.0.0.0) is refused, so trackers never load. No system DNS,
// no fallback: if the DoH server is unreachable the site fails to load (by design).
#include "pch.h"
#include "Data.h"
#include <functional>
#include <iphlpapi.h>
#include <tlhelp32.h>
#pragma comment(lib, "iphlpapi.lib")
#include <set>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <atomic>
#include <chrono>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")
static_assert(sizeof(long long) == 8);

class DohProxy {
public:
    static DohProxy& Get() { static DohProxy* p = new DohProxy; return *p; }   // lives for the process
    std::atomic<int> blocked{0}, resolved{0};

    int Port() const { return m_port; }
    std::function<void(const std::string&)> onBlockedHit;   // every refused request (counter)
    std::function<void(const std::string&)> onLetThrough;   // Balanced: an ad server was let through
    void SetMode(int m) { m_mode = m; }                       // 0 Max, 1 Balanced, 2 Off (system DNS)
    // Child Safety: second check against a public family DoH resolver (adult / malware / phishing). Ad blocking above is untouched.
    // Which resolver does the family check. Default CleanBrowsing; later our own server (setting family_dns = https://host/path).
    void SetFamilyUpstream(const std::wstring& host, const std::wstring& path) {
        std::lock_guard<std::mutex> g(m_famMu);
        if (host == m_famHost && path == m_famPath) return;
        m_famHost = host; m_famPath = path; m_fcon = nullptr; m_famC.clear();   // (the old connection handle is left to the process: changes are rare)
    }
    std::wstring FamilyHost() { std::lock_guard<std::mutex> g(m_famMu); return m_famHost; }
    void SetFamily(bool on) { m_family = on; }
    bool FamilyOn() const { return m_family; }
    void FamilyAllow(const std::string& h) { std::lock_guard<std::mutex> g(m_famMu); m_famOk[h] = time(nullptr) + 1800; }   // parent override: 30 min
    bool FamilyBlocked(const std::string& host) {
        long long now = time(nullptr);
        { std::lock_guard<std::mutex> g(m_famMu);
          auto o = m_famOk.find(host); if (o != m_famOk.end() && o->second > now) return false;
          auto it = m_famC.find(host); if (it != m_famC.end() && it->second.second > now) return it->second.first; }
        if (!m_fcon) { std::lock_guard<std::mutex> g(m_famMu); if (!m_fcon) m_fcon = WinHttpConnect(m_ses, m_famHost.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0); }
        if (!m_fcon) return false;
        int rc = 0, ttl = 0; std::vector<Addr> a;
        if (!Query(host, 1, rc, a, ttl, m_fcon, m_famPath.c_str())) return false;   // network trouble: do not cache, fail open (our own ad DNS still ran)
        bool blk = rc == 3; if (!blk && !a.empty()) { blk = true; for (auto& x : a) if (!IsZero(x)) { blk = false; break; } }
        std::lock_guard<std::mutex> g(m_famMu); if (m_famC.size() > 4000) m_famC.clear(); m_famC[host] = { blk, now + 21600 };
        return blk;
    }
    bool FamilyWasBlocked(const std::string& h) { std::lock_guard<std::mutex> g(m_famMu); auto it = m_famC.find(h); return it != m_famC.end() && it->second.first && it->second.second > time(nullptr); }
    std::function<void(const std::string&)> onFamilyBlock;
    void SetNonce(unsigned n) { m_nonce = n; std::lock_guard<std::mutex> g(m_decMu); m_dec.clear(); m_decAllowed = 0; }               // changes every page load: which ads get through changes too
    std::function<void(const std::string&)> onBlocked;   // called once per newly blocked host

    // Returns the loopback port (0 on failure).
    int Start(const wchar_t* dohHost, const wchar_t* dohPath, const std::wstring& cacheFile) {
        WSADATA w; if (WSAStartup(MAKEWORD(2, 2), &w)) return 0;
        m_path = dohPath; m_file = cacheFile; LoadCache();
        m_ses = WinHttpOpen(L"Libra", WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0);
        if (!m_ses) return 0;
        DWORD maxc = 32;   // WinHTTP allows only 2 simultaneous requests per server by default: a cold page queued behind that
        WinHttpSetOption(m_ses, WINHTTP_OPTION_MAX_CONNS_PER_SERVER, &maxc, sizeof(maxc));
        DWORD h2 = WINHTTP_PROTOCOL_FLAG_HTTP2;   // one connection, many lookups at once
        WinHttpSetOption(m_ses, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &h2, sizeof(h2));
        m_con = WinHttpConnect(m_ses, dohHost, INTERNET_DEFAULT_HTTPS_PORT, 0);
        if (!m_con) return 0;
        m_ls = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = 0;
        InetPtonA(AF_INET, "127.0.0.1", &a.sin_addr);
        int len = sizeof(a);
        if (bind(m_ls, (sockaddr*)&a, sizeof(a)) || listen(m_ls, 256) || getsockname(m_ls, (sockaddr*)&a, &len)) return 0;
        std::thread([this] {
            for (;;) {
                SOCKET c = accept(m_ls, nullptr, nullptr);
                if (c == INVALID_SOCKET) { Sleep(50); continue; }
                if (m_live >= 400) { closesocket(c); continue; }   // a runaway page cannot exhaust the machine
                ++m_live; std::thread([this, c] { Handle(c); --m_live; }).detach();
            }
        }).detach();
        std::thread([this] { for (;;) { Sleep(15000); SaveCache(); } }).detach();   // persist lookups
        m_port = ntohs(a.sin_port);
        return m_port;
    }

    // Resolve these hosts in the background so the first visit is already cached.
    void Prewarm(std::vector<std::string> hosts) {
        std::thread([this, hosts] { for (auto& h : hosts) { std::vector<Addr> a; bool b; Resolve(h, a, b); } }).detach();
    }
    std::vector<std::string> BlockedHosts() {
        std::lock_guard<std::mutex> g(m_mu); std::vector<std::string> v;
        for (auto& kv : m_cache) if (kv.second.blocked) v.push_back(kv.first);
        return v;
    }
    long long LastOk() const { return m_lastOk; }
    static bool AdLike(const std::string& h) { return IsAdLike(h); }
    static int Cat(const std::string& h) { return Category(h); }
    size_t CacheSize() { std::lock_guard<std::mutex> g(m_mu); return m_cache.size(); }
    void ClearCache() { std::lock_guard<std::mutex> g(m_mu); m_cache.clear(); m_sys.clear(); m_failed.clear(); m_dirty = true; }
    bool IsBlocked(const std::string& h) { std::lock_guard<std::mutex> g(m_mu); auto it = m_cache.find(h); return it != m_cache.end() && it->second.blocked; }
    bool DnsFailed(const std::string& h) { std::lock_guard<std::mutex> g(m_mu); auto it = m_failed.find(h); return it != m_failed.end() && time(nullptr) - it->second < 60; }

private:
    struct Addr { int fam; unsigned char b[16]; };
    struct Entry { std::vector<Addr> a; bool blocked; long long exp; };   // exp = unix seconds
    HINTERNET m_ses = nullptr, m_con = nullptr;
    SOCKET m_ls = INVALID_SOCKET;
    std::wstring m_path;
    std::mutex m_mu;
    std::atomic<int> m_mode{0};
    std::atomic<unsigned> m_nonce{0}; std::atomic<bool> m_family{false}; HINTERNET m_fcon = nullptr; std::mutex m_famMu; std::wstring m_famHost = L"doh.cleanbrowsing.org", m_famPath = L"/doh/family-filter/";
    std::unordered_map<std::string, std::pair<bool, long long>> m_famC; std::unordered_map<std::string, long long> m_famOk;
    std::mutex m_decMu; std::unordered_map<std::string, bool> m_dec; int m_decAllowed = 0;   // Balanced: per-page decision for each ad host
    std::atomic<long long> m_lastOk{0};
    std::unordered_map<std::string, std::pair<std::vector<Addr>, long long>> m_sys;
    std::atomic<int> m_live{0};
    std::unordered_map<std::string, Entry> m_cache;
    std::unordered_map<std::string, long long> m_failed;
    std::set<std::string> m_refreshing;
    std::wstring m_file; bool m_dirty = false; int m_port = 0;

    void LoadCache() {
        long long now = time(nullptr);
        for (auto& wl : Lines(ReadAll(m_file))) {
            auto f = SplitTab(wl); if (f.size() < 4) continue;
            Entry e{}; e.exp = _wtoi64(f[1].c_str()); e.blocked = f[2] == L"1";
            if (now - e.exp > 43200) continue;
            std::string ips = ToUtf8(f[3]); size_t s0 = 0;
            while (s0 < ips.size()) {
                size_t c = ips.find(',', s0); if (c == std::string::npos) c = ips.size();
                std::string ip = ips.substr(s0, c - s0); s0 = c + 1; Addr a{};
                if (InetPtonA(AF_INET, ip.c_str(), a.b) == 1) a.fam = AF_INET;
                else if (InetPtonA(AF_INET6, ip.c_str(), a.b) == 1) a.fam = AF_INET6; else continue;
                e.a.push_back(a);
            }
            if (e.blocked || !e.a.empty()) m_cache[ToUtf8(f[0])] = e;
        }
    }
    void SaveCache() {
        std::string out;
        { std::lock_guard<std::mutex> g(m_mu); if (!m_dirty) return; m_dirty = false;
          for (auto& kv : m_cache) {
              out += kv.first + "\t" + std::to_string(kv.second.exp) + "\t" + (kv.second.blocked ? "1" : "0") + "\t";
              for (auto& a : kv.second.a) { char b[64]; InetNtopA(a.fam, a.b, b, 64); out += std::string(b) + ","; }
              out += "\n";
          } }
        WriteAll(m_file, out);
    }

    static int be16(const unsigned char* p) { return (p[0] << 8) | p[1]; }

    // One DoH query. Returns false on transport failure. rcode/addrs/ttl filled on success.
    bool Query(const std::string& host, int qtype, int& rcode, std::vector<Addr>& out, int& ttl, HINTERNET con = nullptr, const wchar_t* pth = nullptr) {
        std::string q; size_t s;
        q.assign("\0\0\1\0\0\1\0\0\0\0\0\0", 12);
        for (s = 0;;) {
            size_t e = host.find('.', s); if (e == std::string::npos) e = host.size();
            if (e == s || e - s > 63) return false;
            q += (char)(e - s); q += host.substr(s, e - s);
            if (e >= host.size()) break; s = e + 1;
        }
        q.push_back(0); q.push_back(0); q.push_back((char)qtype); q.push_back(0); q.push_back(1);

        static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        std::string b64;
        for (size_t i = 0; i < q.size(); i += 3) {
            unsigned v = ((unsigned char)q[i] << 16) | (i + 1 < q.size() ? (unsigned char)q[i + 1] << 8 : 0) | (i + 2 < q.size() ? (unsigned char)q[i + 2] : 0);
            b64 += T[v >> 18 & 63]; b64 += T[v >> 12 & 63];
            if (i + 1 < q.size()) b64 += T[v >> 6 & 63];
            if (i + 2 < q.size()) b64 += T[v & 63];
        }
        std::wstring path = std::wstring(pth ? pth : m_path.c_str()) + L"?dns=" + std::wstring(b64.begin(), b64.end());
        HINTERNET r = WinHttpOpenRequest(con ? con : m_con, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
        if (!r) return false;
        WinHttpSetTimeouts(r, 4000, 4000, 4000, 8000);
        std::string body; bool ok = false;
        if (WinHttpSendRequest(r, L"accept: application/dns-message", (DWORD)-1, nullptr, 0, 0, 0) && WinHttpReceiveResponse(r, nullptr)) {
            DWORD code = 0, sz = sizeof(code);
            WinHttpQueryHeaders(r, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &code, &sz, nullptr);
            char buf[2048]; DWORD n = 0;
            while (code == 200 && WinHttpReadData(r, buf, sizeof(buf), &n) && n) { body.append(buf, n); if (body.size() > 8192) break; }
            ok = (code == 200 && body.size() >= 12);
        }
        WinHttpCloseHandle(r);
        if (!ok) return false;

        const unsigned char* d = (const unsigned char*)body.data(); size_t L = body.size();
        rcode = d[3] & 15; int qd = be16(d + 4), an = be16(d + 6); size_t p = 12;
        auto skipName = [&](size_t& i) { while (i < L) { unsigned c = d[i]; if (c == 0) { ++i; return; } if ((c & 0xC0) == 0xC0) { i += 2; return; } i += c + 1; } };
        for (int i = 0; i < qd; ++i) { skipName(p); p += 4; }
        ttl = 600;
        for (int i = 0; i < an && p + 10 <= L; ++i) {
            skipName(p); if (p + 10 > L) break;
            int type = be16(d + p), rd = be16(d + p + 8); int t = (d[p + 4] << 24) | (d[p + 5] << 16) | (d[p + 6] << 8) | d[p + 7];
            p += 10; if (p + rd > L) break;
            if ((type == 1 && rd == 4) || (type == 28 && rd == 16)) {
                Addr a{}; a.fam = type == 1 ? AF_INET : AF_INET6; memcpy(a.b, d + p, rd); out.push_back(a);
                ttl = std::min(ttl, t);
            }
            p += rd;
        }
        return true;
    }

    // Addresses a public website name must never resolve to (DNS rebinding: evil.com -> your router / your PC).
    static bool IsPrivate(const Addr& a) {
        const unsigned char* b = a.b;
        if (a.fam == AF_INET) {
            return b[0] == 0 || b[0] == 10 || b[0] == 127 || (b[0] == 169 && b[1] == 254) || (b[0] == 172 && b[1] >= 16 && b[1] <= 31) ||
                   (b[0] == 192 && b[1] == 168) || (b[0] == 100 && b[1] >= 64 && b[1] <= 127) || b[0] >= 224;
        }
        bool allZero = true; for (int i = 0; i < 15; ++i) if (b[i]) allZero = false;
        if (allZero && (b[15] == 0 || b[15] == 1)) return true;                                   // :: and ::1
        if ((b[0] & 0xFE) == 0xFC || (b[0] == 0xFE && (b[1] & 0xC0) == 0x80) || b[0] == 0xFF) return true;   // fc00::/7, fe80::/10, multicast
        bool mapped = true; for (int i = 0; i < 10; ++i) if (b[i]) mapped = false;
        if (mapped && b[10] == 0xFF && b[11] == 0xFF) { Addr v4{}; v4.fam = AF_INET; memcpy(v4.b, b + 12, 4); return IsPrivate(v4); }   // ::ffff:a.b.c.d
        return false;
    }
    // Balanced mode: only things that look like AD networks may be let through; trackers/analytics and unknown names stay blocked.
    static bool IsAdLike(const std::string& h) {
        static const char* trk[] = { "analytics", "track", "telemetry", "pixel", "beacon", "stats", "metric", "collect", "insight", "segment", "mixpanel", "hotjar", "clarity", "quantserve", "bluekai", "scorecard", "krxd", "omtrdc", "demdex", "amplitude", "fullstory", "newrelic", "sentry", "tagmanager" };
        static const char* ads[] = { "doubleclick", "googlesyndication", "adservice", "adsystem", "adnxs", "adform", "adtech", "taboola", "outbrain", "criteo", "advert", "banner", "sponsor", "syndication", "promo", "rubicon", "pubmatic", "openx", "smartadserver", "adsrvr", "teads", "revcontent", "mgid" };
        for (auto t : trk) if (h.find(t) != std::string::npos) return false;
        for (auto a : ads) if (h.find(a) != std::string::npos) return true;
        std::string tok; std::vector<std::string> T;
        for (char c : h + ".") { if (isalnum((unsigned char)c)) tok += c; else { if (!tok.empty()) T.push_back(tok); tok.clear(); } }
        for (auto& t : T) { if (t == "log" || t == "logs" || t == "event" || t == "events" || t == "hit" || t == "count" || t == "ping" || t == "tag" || t == "tags" || t == "pxl") return false; }
        for (auto& t : T) if (t == "ad" || t == "ads" || t == "adserver" || t == "adsrv" || t == "adx" || t == "dsp" || t == "rtb") return true;
        return false;
    }
    // Popup category (estimated from the name): 0 Ads, 1 Trackers, 2 Analytics, 3 Social
    static int Category(const std::string& h) {
        static const char* soc[] = { "facebook", "fbcdn", "twitter", "twimg", "linkedin", "tiktok", "pinterest", "snapchat", "instagram", "reddit" };
        static const char* ana[] = { "analytics", "stats", "metric", "insight", "segment", "mixpanel", "hotjar", "clarity", "amplitude", "fullstory", "newrelic", "sentry", "scorecard", "quantserve", "chartbeat" };
        for (auto s : soc) if (h.find(s) != std::string::npos) return 3;
        for (auto a : ana) if (h.find(a) != std::string::npos) return 2;
        return IsAdLike(h) ? 0 : 1;
    }
    // Balanced: an ad server by name, or any blocked name that is NOT clearly a tracker/analytics/social host (many ad networks have plain brand names)
    static bool MayPass(const std::string& h) { if (IsAdLike(h)) return true; int c = Category(h); return c == 1 && !LooksTracker(h); }
    static bool LooksTracker(const std::string& h) {
        static const char* trk[] = { "analytics", "track", "telemetry", "pixel", "beacon", "stats", "metric", "collect", "insight", "segment", "mixpanel", "hotjar", "clarity", "quantserve", "bluekai", "scorecard", "krxd", "omtrdc", "demdex", "amplitude", "fullstory", "newrelic", "sentry", "tagmanager", "log", "event", "hit", "count", "ping", "pxl", "id5", "idsync", "sync", "cookie", "uid", "fingerprint", "cdp", "dmp" };
        for (auto t : trk) if (h.find(t) != std::string::npos) return true;
        return false;
    }
    // Balanced = half of the ads on ANY page: every new ad host is judged against the running count for this page load
    // (allowed so far vs seen so far), so the share stays near 50% whatever the site or ad format; ties use a per-load hash.
    // The choice is remembered per host, so one ad network is never half-loaded.
    bool Pass(const std::string& host) {
        std::lock_guard<std::mutex> g(m_decMu);
        auto it = m_dec.find(host); if (it != m_dec.end()) return it->second;
        unsigned h = 2166136261u; for (unsigned char c : host) { h ^= c; h *= 16777619u; }
        bool coin = ((h ^ (m_nonce.load() * 2654435761u)) >> 7) & 1u;
        int seen = (int)m_dec.size() + 1;                         // including this host
        bool allow = 2 * m_decAllowed < seen || (2 * m_decAllowed == seen && coin);
        if (allow) ++m_decAllowed;
        m_dec[host] = allow; return allow;
    }
    static bool IsZero(const Addr& a) { for (int i = 0; i < (a.fam == AF_INET ? 4 : 16); ++i) if (a.b[i]) return false; return true; }

    // Normal Windows DNS, used only while an allowed site is on screen. Private answers are still refused (rebinding protection).
    bool SysResolve(const std::string& host, std::vector<Addr>& out) {
        long long now = time(nullptr);
        { std::lock_guard<std::mutex> g(m_mu); auto it = m_sys.find(host); if (it != m_sys.end() && it->second.second > now) { out = it->second.first; return !out.empty(); } }
        addrinfo hints{}; hints.ai_socktype = SOCK_STREAM; addrinfo* res = nullptr;
        if (getaddrinfo(host.c_str(), nullptr, &hints, &res) != 0) return false;
        std::vector<Addr> v4, v6;
        for (addrinfo* p = res; p; p = p->ai_next) {
            Addr a{};
            if (p->ai_family == AF_INET)       { a.fam = AF_INET;  memcpy(a.b, &((sockaddr_in*)p->ai_addr)->sin_addr, 4); }
            else if (p->ai_family == AF_INET6) { a.fam = AF_INET6; memcpy(a.b, &((sockaddr_in6*)p->ai_addr)->sin6_addr, 16); }
            else continue;
            if (!IsPrivate(a)) (a.fam == AF_INET ? v4 : v6).push_back(a);
        }
        freeaddrinfo(res);
        out = v4; out.insert(out.end(), v6.begin(), v6.end());
        std::lock_guard<std::mutex> g(m_mu); if (m_sys.size() > 3000) m_sys.clear(); m_sys[host] = { out, now + 120 };
        return !out.empty();
    }
    // Fresh hit: instant. Expired (< 12 h) non-blocked hit: answer instantly, refresh in the background.
    bool Resolve(const std::string& host, std::vector<Addr>& addrs, bool& isBlocked) {
        if (m_mode == 2) { isBlocked = false; return SysResolve(host, addrs); }
        isBlocked = false; long long now = time(nullptr); bool found = false, stale = false;
        { std::lock_guard<std::mutex> g(m_mu);
          auto it = m_cache.find(host);
          if (it != m_cache.end() && (it->second.exp > now || (!it->second.blocked && now - it->second.exp < 43200))) {
              found = true; stale = it->second.exp <= now; addrs = it->second.a; isBlocked = it->second.blocked;
          } }
        if (found) { if (stale) Refresh(host); return !addrs.empty(); }
        return Fetch(host, addrs, isBlocked);
    }
    void Refresh(const std::string& host) {
        { std::lock_guard<std::mutex> g(m_mu); if (!m_refreshing.insert(host).second) return; }
        std::thread([this, host] {
            std::vector<Addr> a; bool b; Fetch(host, a, b);
            std::lock_guard<std::mutex> g(m_mu); m_refreshing.erase(host);
        }).detach();
    }
    bool Fetch(const std::string& host, std::vector<Addr>& addrs, bool& isBlocked) {
        int rc = 0, ttl = 300; std::vector<Addr> v;
        if (!Query(host, 1, rc, v, ttl)) { std::lock_guard<std::mutex> g(m_mu); m_failed[host] = time(nullptr); return false; }
        if (rc == 0 && v.empty()) { int t2; std::vector<Addr> v6; if (Query(host, 28, rc, v6, t2)) { v = v6; ttl = t2; } }
        size_t nBefore = v.size();
        v.erase(std::remove_if(v.begin(), v.end(), [](const Addr& a) { return IsPrivate(a); }), v.end());
        bool blk = (nBefore && v.empty()) || (rc == 3) || (!v.empty() && std::all_of(v.begin(), v.end(), IsZero));   // private-only answer = rebinding attempt
        if (blk) v.clear();
        ttl = std::clamp(ttl, 300, 3600);
        m_lastOk = (long long)time(nullptr);   // the cloud answered
        bool fresh = false;
        { std::lock_guard<std::mutex> g(m_mu);
          auto it = m_cache.find(host); fresh = blk && (it == m_cache.end() || !it->second.blocked);
          if (m_cache.size() > 6000) m_cache.clear();
          m_cache[host] = { v, blk, (long long)time(nullptr) + (blk ? 3600 : ttl) }; m_dirty = true; }
        if (fresh && onBlocked) onBlocked(host);
        addrs = v; isBlocked = blk;
        return !v.empty();
    }

    static SOCKET ConnectTo(const Addr& a, int port) {
        SOCKET s = socket(a.fam, SOCK_STREAM, IPPROTO_TCP); if (s == INVALID_SOCKET) return s;
        u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);
        sockaddr_storage ss{}; int len;
        if (a.fam == AF_INET) { auto* p = (sockaddr_in*)&ss; p->sin_family = AF_INET; p->sin_port = htons((u_short)port); memcpy(&p->sin_addr, a.b, 4); len = sizeof(*p); }
        else { auto* p = (sockaddr_in6*)&ss; p->sin6_family = AF_INET6; p->sin6_port = htons((u_short)port); memcpy(&p->sin6_addr, a.b, 16); len = sizeof(*p); }
        connect(s, (sockaddr*)&ss, len);
        fd_set wf; FD_ZERO(&wf); FD_SET(s, &wf); timeval tv{ 8, 0 };
        int ok = 0, ol = sizeof(ok);
        if (select(0, nullptr, &wf, nullptr, &tv) > 0) getsockopt(s, SOL_SOCKET, SO_ERROR, (char*)&ok, &ol); else ok = 1;
        if (ok) { closesocket(s); return INVALID_SOCKET; }
        nb = 0; ioctlsocket(s, FIONBIO, &nb);
        return s;
    }

    static bool SendAll(SOCKET s, const char* p, int n) { while (n > 0) { int k = send(s, p, n, 0); if (k <= 0) return false; p += k; n -= k; } return true; }

    // Only Libra itself (and the browser engine it started) may use this proxy: other programs on the PC are refused.
    std::mutex m_pidMu; std::unordered_map<DWORD, std::pair<bool, ULONGLONG>> m_pidOk;
    bool PidIsOurs(DWORD pid) {
        if (pid == GetCurrentProcessId()) return true;
        ULONGLONG now = GetTickCount64();
        { std::lock_guard<std::mutex> g(m_pidMu); auto it = m_pidOk.find(pid); if (it != m_pidOk.end() && it->second.second > now) return it->second.first; }
        std::unordered_map<DWORD, DWORD> parent;
        HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); if (sn == INVALID_HANDLE_VALUE) return true;   // cannot tell: do not break browsing
        PROCESSENTRY32W pe{ sizeof(pe) }; for (BOOL ok = Process32FirstW(sn, &pe); ok; ok = Process32NextW(sn, &pe)) parent[pe.th32ProcessID] = pe.th32ParentProcessID;
        CloseHandle(sn);
        bool ours = false; DWORD me = GetCurrentProcessId(), p = pid;
        for (int i = 0; i < 16 && p; ++i) { auto it = parent.find(p); if (it == parent.end()) break; p = it->second; if (p == me) { ours = true; break; } }
        std::lock_guard<std::mutex> g(m_pidMu); if (m_pidOk.size() > 500) m_pidOk.clear(); m_pidOk[pid] = { ours, now + 30000 }; return ours;
    }
    bool CallerAllowed(SOCKET c) {
        sockaddr_in pa{}, la{}; int pl = sizeof(pa), ll = sizeof(la);
        if (getpeername(c, (sockaddr*)&pa, &pl) || getsockname(c, (sockaddr*)&la, &ll)) return true;
        ULONG sz = 0; GetExtendedTcpTable(nullptr, &sz, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
        std::vector<char> buf(sz + 8192);
        if (GetExtendedTcpTable(buf.data(), &sz, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR) return true;
        auto* t = (MIB_TCPTABLE_OWNER_PID*)buf.data();
        for (DWORD i = 0; i < t->dwNumEntries; ++i) { auto& r = t->table[i];
            if ((u_short)r.dwLocalPort == pa.sin_port && (u_short)r.dwRemotePort == la.sin_port && r.dwLocalAddr == pa.sin_addr.s_addr) return PidIsOurs(r.dwOwningPid); }
        return true;   // connection vanished before we looked
    }
    void Handle(SOCKET c) {
        if (!CallerAllowed(c)) { closesocket(c); return; }
        DWORD to = 5000; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (char*)&to, sizeof(to));   // idle connections that never send a request are dropped fast
        std::string req; static thread_local char buf[65536]; size_t hend;
        while ((hend = req.find("\r\n\r\n")) == std::string::npos) {
            int n = recv(c, buf, sizeof(buf), 0); if (n <= 0 || req.size() > 65536) { closesocket(c); return; }
            req.append(buf, n);
        }
        std::string head = req.substr(0, hend), rest = req.substr(hend + 4);
        size_t l1 = head.find("\r\n"); std::string line = head.substr(0, l1);
        size_t sp1 = line.find(' '), sp2 = line.rfind(' ');
        if (sp1 == std::string::npos || sp2 <= sp1) { closesocket(c); return; }
        std::string method = line.substr(0, sp1), target = line.substr(sp1 + 1, sp2 - sp1 - 1), host, fwd;
        int port = 80; bool tunnel = (method == "CONNECT");
        std::string hp = target, path = "/";
        if (!tunnel) {
            if (target.rfind("http://", 0) != 0) { closesocket(c); return; }
            hp = target.substr(7); size_t sl = hp.find_first_of("/?"); if (sl != std::string::npos) { path = hp.substr(sl); if (path[0] == '?') path = "/" + path; hp = hp.substr(0, sl); }
        } else port = 443;
        size_t col = hp.rfind(':');
        if (!hp.empty() && hp[0] == '[') { size_t rb = hp.find(']'); host = hp.substr(1, rb - 1); if (col != std::string::npos && col > rb) port = atoi(hp.c_str() + col + 1); }
        else if (col != std::string::npos) { host = hp.substr(0, col); port = atoi(hp.c_str() + col + 1); }
        else host = hp;
        for (auto& ch : host) ch = (char)tolower((unsigned char)ch);
        if (host.empty() || port <= 0 || port > 65535) { closesocket(c); return; }
        DWORD to2 = 15000; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (char*)&to2, sizeof(to2));   // header arrived: normal idle time for the tunnel
        // Hardening: the proxy must not become a tool for other programs or pages. No mail/file-share/database/remote-desktop ports.
        static const int badPorts[] = { 21, 22, 23, 25, 69, 110, 111, 135, 137, 138, 139, 143, 161, 445, 465, 587, 993, 995, 1433, 2049, 3306, 3389, 5432, 5900, 6379, 11211, 27017 };
        for (int bp : badPorts) if (port == bp) { static const char r[] = "HTTP/1.1 403 Blocked\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"; SendAll(c, r, (int)strlen(r)); closesocket(c); return; }
        bool strict = m_family || m_mode == 0;   // Max / Child Safety: no way around the name filters
        if (strict) {
            // Public DNS-over-HTTPS servers would let a page look names up itself and skip our blocking.
            static const char* doh[] = { "dns.google", "cloudflare-dns.com", "one.one.one.one", "dns.quad9.net", "doh.opendns.com", "dns.adguard-dns.com", "dns.nextdns.io", "doh.dns.sb", "doh.cleanbrowsing.org", "dns.twnic.tw", "doh.libredns.gr", "dns.alidns.com", "doh.pub", "dns.sb" };
            for (auto d : doh) { std::string k = d; if (host == k || (host.size() > k.size() && host.compare(host.size() - k.size() - 1, std::string::npos, "." + k) == 0)) { static const char r[] = "HTTP/1.1 403 Blocked\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"; SendAll(c, r, (int)strlen(r)); closesocket(c); return; } }
        }

        std::vector<Addr> addrs; bool blk = false; bool ok;
        in_addr v4; in6_addr v6;
        if (InetPtonA(AF_INET, host.c_str(), &v4) == 1) { Addr a{}; a.fam = AF_INET; memcpy(a.b, &v4, 4); addrs.push_back(a); ok = true; }
        else if (InetPtonA(AF_INET6, host.c_str(), &v6) == 1) { Addr a{}; a.fam = AF_INET6; memcpy(a.b, &v6, 16); addrs.push_back(a); ok = true; }
        else if (host == "localhost") { Addr a{}; a.fam = AF_INET; a.b[0] = 127; a.b[3] = 1; addrs.push_back(a); ok = true; }
        else {
            ok = Resolve(host, addrs, blk);
            if (!ok && blk && m_mode == 1 && MayPass(host) && Pass(host) && SysResolve(host, addrs)) {   // Balanced: let some ads through
                ok = true; blk = false; if (onLetThrough) onLetThrough(host);
            }
        }

        auto reply = [&](const char* s) { SendAll(c, s, (int)strlen(s)); closesocket(c); };
        if (ok && strict && host != "localhost" && !addrs.empty() && (InetPtonA(AF_INET, host.c_str(), &v4) == 1 || InetPtonA(AF_INET6, host.c_str(), &v6) == 1) && !IsPrivate(addrs[0])) {
            reply("HTTP/1.1 403 Blocked\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"); return;   // public IP address typed in directly: it would skip every name filter
        }
        if (ok && m_family && host != "localhost" && !blk && InetPtonA(AF_INET, host.c_str(), &v4) != 1 && FamilyBlocked(host)) {
            if (onFamilyBlock) onFamilyBlock(host);
            reply("HTTP/1.1 403 Blocked\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"); return;
        }
        if (!ok) {
            if (blk) { ++blocked; if (onBlockedHit) onBlockedHit(host); reply("HTTP/1.1 403 Blocked\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"); }
            else reply("HTTP/1.1 502 DNS failed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            return;
        }
        ++resolved;
        BOOL nd = TRUE; setsockopt(c, IPPROTO_TCP, TCP_NODELAY, (char*)&nd, sizeof(nd));
        SOCKET u = INVALID_SOCKET;
        for (size_t i = 0; i < addrs.size() && i < 4 && u == INVALID_SOCKET; ++i) u = ConnectTo(addrs[i], port);
        if (u == INVALID_SOCKET) { reply("HTTP/1.1 502 Connect failed\r\nContent-Length: 0\r\nConnection: close\r\n\r\n"); return; }

        if (tunnel) {
            if (!SendAll(c, "HTTP/1.1 200 Connection Established\r\n\r\n", 39)) { closesocket(c); closesocket(u); return; }
            if (!rest.empty()) SendAll(u, rest.data(), (int)rest.size());
        } else {   // origin-form request line, one request per connection
            std::string out = method + " " + path + " HTTP/1.1\r\n";
            size_t p = l1 + 2;
            while (p < head.size()) {
                size_t e = head.find("\r\n", p); if (e == std::string::npos) e = head.size();
                std::string h = head.substr(p, e - p); p = e + 2;
                std::string low = h.substr(0, 17); for (auto& ch : low) ch = (char)tolower((unsigned char)ch);
                if (low.rfind("proxy-connection:", 0) == 0 || low.rfind("connection:", 0) == 0) continue;
                out += h + "\r\n";
            }
            out += "Connection: close\r\n\r\n" + rest;
            if (!SendAll(u, out.data(), (int)out.size())) { closesocket(c); closesocket(u); return; }
        }
        setsockopt(u, IPPROTO_TCP, TCP_NODELAY, (char*)&nd, sizeof(nd));
        DWORD none = 0; setsockopt(c, SOL_SOCKET, SO_RCVTIMEO, (char*)&none, sizeof(none));
        for (;;) {   // pump both ways until either side closes
            fd_set rf; FD_ZERO(&rf); FD_SET(c, &rf); FD_SET(u, &rf); timeval tv{ 120, 0 };
            if (select(0, &rf, nullptr, nullptr, &tv) <= 0) break;
            bool dead = false;
            for (SOCKET from : { c, u }) {
                if (!FD_ISSET(from, &rf)) continue;
                int n = recv(from, buf, sizeof(buf), 0);
                if (n <= 0 || !SendAll(from == c ? u : c, buf, n)) { dead = true; break; }
            }
            if (dead) break;
        }
        closesocket(c); closesocket(u);
    }
};
