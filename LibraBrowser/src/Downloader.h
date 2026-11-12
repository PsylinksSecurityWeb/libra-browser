#pragma once
// Segmented ("download manager") downloader. Splits a file into N byte ranges, fetches them on parallel
// connections and writes each range straight to its place in the file. All traffic goes through the local
// DoH proxy (WinHTTP named proxy), so every hostname is still resolved only by the Psylinks DNS server.
#include "pch.h"
#include "Data.h"
#include <winhttp.h>
#include <thread>
#include <atomic>
#pragma comment(lib, "winhttp.lib")

struct DlJob {
    std::wstring url, path, part, hdr, ref;    // hdr = extra request headers ("Name: value\r\n"...)
    int port = 0;                         // local proxy port
    long long total = 0;
    std::atomic<long long> got{ 0 };
    std::atomic<int> state{ 0 };          // 0 active, 1 done, 2 failed or cancelled
    std::atomic<bool> cancel{ false };
    HANDLE file = INVALID_HANDLE_VALUE;
    std::vector<long long> segCur, segHi;   // next byte to fetch / last byte of each range (lets Retry continue from the break)
    int conns = 8;
    std::atomic<bool> resumable{ false };   // failed but the partial file is kept
};

namespace dl {
// Tell Windows (SmartScreen, Office Protected View) this file came from the internet.
inline void MarkOfTheWeb(const std::wstring& path, const std::wstring& referrer, const std::wstring& url) {
    auto clean = [](std::wstring s) { for (auto& c : s) if (c < 32) c = L' '; return s; };
    std::string z = "[ZoneTransfer]\r\nZoneId=3\r\n";
    if (!referrer.empty()) z += "ReferrerUrl=" + ToUtf8(clean(referrer)) + "\r\n";
    z += "HostUrl=" + ToUtf8(clean(url)) + "\r\n";
    HANDLE h = CreateFileW((path + L":Zone.Identifier").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD w = 0; WriteFile(h, z.data(), (DWORD)z.size(), &w, nullptr); CloseHandle(h);
}

inline HINTERNET Session(int port, int conns) {
    std::wstring proxy = L"127.0.0.1:" + std::to_wstring(port);
    HINTERNET s = WinHttpOpen(L"Libra", WINHTTP_ACCESS_TYPE_NAMED_PROXY, proxy.c_str(), WINHTTP_NO_PROXY_BYPASS, 0);
    if (s) { DWORD m = (DWORD)conns + 2; WinHttpSetOption(s, WINHTTP_OPTION_MAX_CONNS_PER_SERVER, &m, sizeof(m)); }
    return s;
}

struct Url { std::wstring host, path; INTERNET_PORT port = 443; bool https = true; };
inline bool Crack(const std::wstring& u, Url& o) {
    URL_COMPONENTS c{ sizeof(c) }; c.dwHostNameLength = c.dwUrlPathLength = c.dwExtraInfoLength = (DWORD)-1;
    if (!WinHttpCrackUrl(u.c_str(), (DWORD)u.size(), 0, &c)) return false;
    o.host.assign(c.lpszHostName, c.dwHostNameLength);
    o.path.assign(c.lpszUrlPath, c.dwUrlPathLength + c.dwExtraInfoLength);
    o.port = c.nPort; o.https = c.nScheme == INTERNET_SCHEME_HTTPS;
    return !o.host.empty();
}

// Opens a ranged GET. Caller closes req and con. Returns the HTTP status (0 on failure).
inline int OpenRange(HINTERNET ses, const Url& u, const std::wstring& hdr, long long lo, long long hi, HINTERNET& con, HINTERNET& req) {
    con = WinHttpConnect(ses, u.host.c_str(), u.port, 0); if (!con) return 0;
    req = WinHttpOpenRequest(con, L"GET", u.path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, u.https ? WINHTTP_FLAG_SECURE : 0);
    if (!req) return 0;
    WinHttpSetTimeouts(req, 15000, 15000, 20000, 30000);
    std::wstring h = hdr + L"Range: bytes=" + std::to_wstring(lo) + L"-" + (hi >= 0 ? std::to_wstring(hi) : L"") + L"\r\n";
    if (!WinHttpSendRequest(req, h.c_str(), (DWORD)-1, nullptr, 0, 0, 0) || !WinHttpReceiveResponse(req, nullptr)) return 0;
    DWORD code = 0, sz = sizeof(code);
    WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &code, &sz, nullptr);
    return (int)code;
}

// Can this URL be fetched in ranges, and how big is it? (one-byte ranged GET)
inline bool Probe(int port, const std::wstring& url, const std::wstring& hdr, long long& total) {
    Url u; if (!Crack(url, u)) return false;
    HINTERNET ses = Session(port, 1); if (!ses) return false;
    HINTERNET con = nullptr, req = nullptr; bool ok = false;
    if (OpenRange(ses, u, hdr, 0, 0, con, req) == 206) {
        wchar_t cr[128] = {}; DWORD n = sizeof(cr);
        if (WinHttpQueryHeaders(req, WINHTTP_QUERY_CUSTOM, L"Content-Range", cr, &n, WINHTTP_NO_HEADER_INDEX)) {
            std::wstring s(cr); size_t sl = s.find(L'/');
            if (sl != std::wstring::npos) { total = _wtoi64(s.c_str() + sl + 1); ok = total > 0; }
        }
    }
    if (req) WinHttpCloseHandle(req); if (con) WinHttpCloseHandle(con); WinHttpCloseHandle(ses);
    return ok;
}

inline void Segment(std::shared_ptr<DlJob> j, HINTERNET ses, int idx, std::atomic<bool>* bad) {
    Url u; if (!Crack(j->url, u)) { *bad = true; return; }
    std::vector<char> buf(256 * 1024);
    long long hi = j->segHi[idx]; int tries = 0;
    while (j->segCur[idx] <= hi && !j->cancel && !*bad) {
        long long cur = j->segCur[idx];
        HINTERNET con = nullptr, req = nullptr;
        int st = OpenRange(ses, u, j->hdr, cur, hi, con, req);
        if (st == 206) {
            DWORD n = 0;
            while (!j->cancel && WinHttpReadData(req, buf.data(), (DWORD)buf.size(), &n) && n) {
                long long room = hi - cur + 1; if ((long long)n > room) n = (DWORD)room;    // never write past our range
                OVERLAPPED ov{}; ov.Offset = (DWORD)cur; ov.OffsetHigh = (DWORD)(cur >> 32);
                DWORD w = 0;
                if (!WriteFile(j->file, buf.data(), n, &w, &ov) || w != n) { *bad = true; break; }   // disk error
                cur += n; j->segCur[idx] = cur; j->got += n; tries = 0;
                if (cur > hi) break;
            }
        }
        if (req) WinHttpCloseHandle(req); if (con) WinHttpCloseHandle(con);
        if (*bad || j->segCur[idx] > hi || j->cancel) break;
        if (++tries > 6) { *bad = true; break; }          // dropped connection: resume from where this range stopped
        Sleep(400 * tries);
    }
}

// resume = continue an interrupted job from every range's break point (the .libra-part file is kept on failure)
inline void Start(std::shared_ptr<DlJob> j, int conns, bool resume = false) {
    j->conns = conns;
    std::thread([j, conns, resume] {
        bool fresh = !resume || j->segCur.empty();
        j->file = CreateFileW(j->part.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, fresh ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        HINTERNET ses = j->file == INVALID_HANDLE_VALUE ? nullptr : Session(j->port, conns);
        if (!ses) { if (j->file != INVALID_HANDLE_VALUE) CloseHandle(j->file); j->resumable = !fresh; j->state = 2; return; }
        if (fresh) {
            LARGE_INTEGER sz; sz.QuadPart = j->total; SetFilePointerEx(j->file, sz, nullptr, FILE_BEGIN); SetEndOfFile(j->file);   // reserve space
            long long seg = (j->total + conns - 1) / conns;
            j->segCur.assign(conns, 0); j->segHi.assign(conns, -1); j->got = 0;
            for (int i = 0; i < conns; ++i) { long long lo = i * seg; j->segCur[i] = lo; j->segHi[i] = std::min(j->total - 1, lo + seg - 1); }
        }
        bool done = false;
        for (int attempt = 0; attempt < 3 && !j->cancel && !done; ++attempt) {      // a broken link is retried by itself, from the break
            std::atomic<bool> bad{ false };
            std::vector<std::thread> th;
            for (int i = 0; i < (int)j->segCur.size(); ++i) if (j->segCur[i] <= j->segHi[i]) th.emplace_back(Segment, j, ses, i, &bad);
            for (auto& t : th) t.join();
            done = true; for (size_t i = 0; i < j->segCur.size(); ++i) if (j->segCur[i] <= j->segHi[i]) done = false;
            if (!done && !j->cancel) Sleep(1500 * (attempt + 1));
        }
        WinHttpCloseHandle(ses);
        CloseHandle(j->file);
        if (done && MoveFileExW(j->part.c_str(), j->path.c_str(), MOVEFILE_REPLACE_EXISTING)) { MarkOfTheWeb(j->path, j->ref, j->url); j->state = 1; }
        else if (j->cancel || done) { DeleteFileW(j->part.c_str()); j->state = 2; }
        else { j->resumable = true; j->state = 2; }          // keep the part: "Retry" continues here
    }).detach();
}
// One plain GET, no ranges: small files and servers that cannot send pieces. total may be 0 (unknown).
inline void StartPlain(std::shared_ptr<DlJob> j) {
    std::thread([j] {
        Url u; HINTERNET ses = Session(j->port, 1), con = nullptr, req = nullptr; bool ok = false; HANDLE f = INVALID_HANDLE_VALUE;
        if (ses && Crack(j->url, u)) {
            con = WinHttpConnect(ses, u.host.c_str(), u.port, 0);
            if (con) req = WinHttpOpenRequest(con, L"GET", u.path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, u.https ? WINHTTP_FLAG_SECURE : 0);
            if (req) WinHttpSetTimeouts(req, 15000, 15000, 20000, 30000);
            if (req && WinHttpSendRequest(req, j->hdr.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : j->hdr.c_str(), (DWORD)-1, nullptr, 0, 0, 0) && WinHttpReceiveResponse(req, nullptr)) {
                DWORD code = 0, sz = sizeof(code); WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &code, &sz, nullptr);
                wchar_t cl[32] = {}; DWORD n = sizeof(cl); if (WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH, nullptr, cl, &n, WINHTTP_NO_HEADER_INDEX)) j->total = _wtoi64(cl);
                if (code == 200) {
                    f = CreateFileW(j->part.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
                    std::vector<char> buf(256 * 1024); DWORD r = 0; bool wr = f != INVALID_HANDLE_VALUE;
                    while (wr && !j->cancel && WinHttpReadData(req, buf.data(), (DWORD)buf.size(), &r) && r) { DWORD w = 0; if (!WriteFile(f, buf.data(), r, &w, nullptr) || w != r) { wr = false; break; } j->got += r; }
                    ok = wr && !j->cancel && (j->total <= 0 || j->got >= j->total);
                    if (ok && j->total <= 0) j->total = j->got;
                }
            }
        }
        if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
        if (req) WinHttpCloseHandle(req); if (con) WinHttpCloseHandle(con); if (ses) WinHttpCloseHandle(ses);
        if (ok && MoveFileExW(j->part.c_str(), j->path.c_str(), MOVEFILE_REPLACE_EXISTING)) { MarkOfTheWeb(j->path, j->ref, j->url); j->state = 1; }
        else { DeleteFileW(j->part.c_str()); j->state = 2; }
    }).detach();
}
}  // namespace dl
