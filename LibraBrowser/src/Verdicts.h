// Local block memory (Max protection only): names the cloud DNS said "blocked" are remembered on this PC.
// Stores only a keyed 64-bit hash of the name (SipHash-2-4, per-install random key kept DPAPI-wrapped in the registry) + an expiry day.
// The table can only say "block": a damaged or forged file can over-block at worst and is rejected by its HMAC. The cloud stays the source of truth.
#pragma once
#include "pch.h"
#include <shared_mutex>
#include <bcrypt.h>

class Verdicts {
public:
    static Verdicts& Get() { static Verdicts v; return v; }
    // 0 = unknown, 1 = remembered (fresh), 2 = remembered but older than a week (answer locally, re-check in the background)
    int Lookup(const std::string& host) {
        if (!m_ready) return 0;
        uint64_t h = Hash(host); uint32_t today = Today();
        std::shared_lock<std::shared_mutex> g(m_mu);
        if (m_slots.empty()) return 0;
        for (size_t i = h & m_mask, n = 0; n < m_slots.size(); i = (i + 1) & m_mask, ++n) {
            const Slot& s = m_slots[i]; if (s.h == 0) return 0;
            if (s.h == h) return s.day > today + 7 ? 1 : s.day > today ? 2 : 0;
        }
        return 0;
    }
    void Put(const std::string& host) { Set(host, Today() + kLife); }
    void Remove(const std::string& host) { Set(host, 0); }      // expired entry stays as a dead slot (keeps probe chains intact)
    size_t Count() { std::shared_lock<std::shared_mutex> g(m_mu); return m_count; }
    void Clear() { { std::unique_lock<std::shared_mutex> g(m_mu); m_slots.assign(kStart, Slot{}); m_mask = kStart - 1; m_count = 0; m_dirty = true; } Save(); }
    void SetPrivate(bool p) { m_private = p; }                    // private windows learn nothing
    std::atomic<unsigned> localHits{ 0 }, cloudAsks{ 0 };
    void Init(const std::wstring& file) {
        m_file = file; m_slots.assign(kStart, Slot{}); m_mask = kStart - 1;
        std::thread([this] { LoadKey(); Load(); m_ready = true; for (;;) { Sleep(60000); Save(); } }).detach();
    }
    void Save() {   // compacts (drops expired, trims to 70% of the cap), signs, writes atomically; only when something changed
        if (!m_keyOk) return;
        std::vector<Slot> keep; { std::shared_lock<std::shared_mutex> g(m_mu); if (!m_dirty) return; uint32_t today = Today(); for (auto& s : m_slots) if (s.h && s.day > today) keep.push_back(s); }
        if (keep.size() > kCap * 7 / 10) { std::nth_element(keep.begin(), keep.begin() + (keep.size() - kCap * 7 / 10), keep.end(), [](const Slot& a, const Slot& b) { return a.day < b.day; }); keep.erase(keep.begin(), keep.begin() + (keep.size() - kCap * 7 / 10)); }
        size_t cap = kStart; while (cap < keep.size() * 2 + 2) cap <<= 1;
        std::vector<Slot> t(cap); for (auto& s : keep) { size_t i = s.h & (cap - 1); while (t[i].h) i = (i + 1) & (cap - 1); t[i] = s; }
        std::string out; Header h{}; memcpy(h.magic, "LVT1", 4); h.cap = (uint32_t)cap; h.count = (uint32_t)keep.size();
        out.assign((const char*)&h, sizeof h); out.append((const char*)t.data(), t.size() * sizeof(Slot));
        Mac(out, ((Header*)&out[0])->mac);
        { std::unique_lock<std::shared_mutex> g(m_mu); m_slots.swap(t); m_mask = cap - 1; m_count = keep.size(); m_dirty = false; }
        std::wstring tmp = m_file + L".tmp"; HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f == INVALID_HANDLE_VALUE) return; DWORD w = 0; BOOL ok = WriteFile(f, out.data(), (DWORD)out.size(), &w, nullptr); CloseHandle(f);
        if (ok && w == out.size()) MoveFileExW(tmp.c_str(), m_file.c_str(), MOVEFILE_REPLACE_EXISTING); else DeleteFileW(tmp.c_str());
    }

private:
#pragma pack(push, 1)
    struct Slot { uint64_t h = 0; uint32_t day = 0; };
    struct Header { char magic[4]; uint32_t cap, count; unsigned char mac[32]; };
#pragma pack(pop)
    static constexpr size_t kStart = 4096, kCap = 1000000; static constexpr uint32_t kLife = 14;
    std::shared_mutex m_mu; std::vector<Slot> m_slots; size_t m_mask = 0, m_count = 0; bool m_dirty = false;
    std::atomic<bool> m_ready{ false }, m_private{ false }; bool m_keyOk = false; unsigned char m_key[32]{}; std::wstring m_file;

    static uint32_t Today() { return (uint32_t)(time(nullptr) / 86400); }
    static uint64_t Rotl(uint64_t x, int b) { return (x << b) | (x >> (64 - b)); }
    uint64_t Hash(const std::string& s) const {   // SipHash-2-4, 128-bit key = first 16 bytes of the install key
        uint64_t k0, k1; memcpy(&k0, m_key, 8); memcpy(&k1, m_key + 8, 8);
        uint64_t v0 = 0x736f6d6570736575ULL ^ k0, v1 = 0x646f72616e646f6dULL ^ k1, v2 = 0x6c7967656e657261ULL ^ k0, v3 = 0x7465646279746573ULL ^ k1;
        auto round = [&] { v0 += v1; v1 = Rotl(v1, 13); v1 ^= v0; v0 = Rotl(v0, 32); v2 += v3; v3 = Rotl(v3, 16); v3 ^= v2; v0 += v3; v3 = Rotl(v3, 21); v3 ^= v0; v2 += v1; v1 = Rotl(v1, 17); v1 ^= v2; v2 = Rotl(v2, 32); };
        size_t n = s.size(), i = 0; for (; i + 8 <= n; i += 8) { uint64_t m; memcpy(&m, s.data() + i, 8); v3 ^= m; round(); round(); v0 ^= m; }
        uint64_t b = (uint64_t)n << 56; for (size_t j = 0; i + j < n; ++j) b |= (uint64_t)(unsigned char)s[i + j] << (8 * j);
        v3 ^= b; round(); round(); v0 ^= b; v2 ^= 0xff; round(); round(); round(); round();
        uint64_t r = v0 ^ v1 ^ v2 ^ v3; return r ? r : 1;
    }
    void Set(const std::string& host, uint32_t day) {
        if (!m_ready || (m_private && day)) return;
        uint64_t h = Hash(host); std::unique_lock<std::shared_mutex> g(m_mu);
        for (size_t i = h & m_mask, n = 0; n < m_slots.size(); i = (i + 1) & m_mask, ++n) {
            Slot& s = m_slots[i];
            if (s.h == h) { s.day = day; m_dirty = true; return; }
            if (s.h == 0) {
                if (!day || m_count >= kCap) return;
                s.h = h; s.day = day; ++m_count; m_dirty = true;
                if (m_count * 10 > m_slots.size() * 6) Grow();
                return;
            }
        }
    }
    void Grow() {   // caller holds the write lock
        std::vector<Slot> t(m_slots.size() * 2); size_t mask = t.size() - 1;
        for (auto& s : m_slots) if (s.h) { size_t i = s.h & mask; while (t[i].h) i = (i + 1) & mask; t[i] = s; }
        m_slots.swap(t); m_mask = mask;
    }
    void Mac(const std::string& data, unsigned char* out32) const {   // HMAC-SHA256 over everything except the MAC field itself
        BCRYPT_ALG_HANDLE a = nullptr; memset(out32, 0, 32);
        if (BCryptOpenAlgorithmProvider(&a, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) return;
        BCRYPT_HASH_HANDLE h = nullptr; unsigned char tmp[32]{};
        if (BCryptCreateHash(a, &h, nullptr, 0, (PUCHAR)m_key + 16, 16, 0) == 0) {
            BCryptHashData(h, (PUCHAR)data.data(), 12, 0);   // magic, cap, count
            BCryptHashData(h, (PUCHAR)data.data() + sizeof(Header), (ULONG)(data.size() - sizeof(Header)), 0);
            BCryptFinishHash(h, tmp, 32, 0); BCryptDestroyHash(h); memcpy(out32, tmp, 32);
        }
        BCryptCloseAlgorithmProvider(a, 0);
    }
    void LoadKey() {   // random per install, DPAPI-wrapped in the registry; without it the table is session-only
        const wchar_t* sub = L"Software\\Psylinks\\Libra\\Vk"; DWORD n = 0;
        if (RegGetValueW(HKEY_CURRENT_USER, sub, L"k", RRF_RT_REG_BINARY, nullptr, nullptr, &n) == ERROR_SUCCESS && n) {
            std::string raw(n, '\0');
            if (RegGetValueW(HKEY_CURRENT_USER, sub, L"k", RRF_RT_REG_BINARY, nullptr, &raw[0], &n) == ERROR_SUCCESS) { std::string k = SecretOpen(raw); if (k.size() == 32 && raw.compare(0, 4, "LBE1") == 0) { memcpy(m_key, k.data(), 32); m_keyOk = true; return; } }
        }
        BCryptGenRandom(nullptr, m_key, 32, BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        std::string w = SecretChunk(std::string((char*)m_key, 32)); HKEY k;
        if (!w.empty() && RegCreateKeyExW(HKEY_CURRENT_USER, sub, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
            m_keyOk = RegSetValueExW(k, L"k", 0, REG_BINARY, (const BYTE*)w.data(), (DWORD)w.size()) == ERROR_SUCCESS; RegCloseKey(k);
        }
        DeleteFileW(m_file.c_str());   // a table made with another key is useless
    }
    void Load() {
        if (!m_keyOk) return;
        HANDLE f = CreateFileW(m_file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr); if (f == INVALID_HANDLE_VALUE) return;
        LARGE_INTEGER sz; std::string d; if (GetFileSizeEx(f, &sz) && sz.QuadPart >= (long long)sizeof(Header) && sz.QuadPart < (long long)(sizeof(Header) + (1u << 23) * sizeof(Slot))) { d.resize((size_t)sz.QuadPart); DWORD r = 0; if (!ReadFile(f, &d[0], (DWORD)d.size(), &r, nullptr) || r != d.size()) d.clear(); }
        CloseHandle(f); if (d.size() < sizeof(Header)) return;
        Header h; memcpy(&h, d.data(), sizeof h); unsigned char mac[32];
        if (memcmp(h.magic, "LVT1", 4) || h.cap < kStart || (h.cap & (h.cap - 1)) || h.cap > (1u << 23) || d.size() != sizeof(Header) + (size_t)h.cap * sizeof(Slot)) return;
        Mac(d, mac); unsigned char diff = 0; for (int i = 0; i < 32; ++i) diff |= mac[i] ^ h.mac[i]; if (diff) return;   // tampered or damaged: ignore it, ask the cloud
        std::vector<Slot> t(h.cap); memcpy(t.data(), d.data() + sizeof(Header), (size_t)h.cap * sizeof(Slot));
        size_t cnt = 0; for (auto& s : t) if (s.h) ++cnt; if (cnt != h.count) return;
        std::unique_lock<std::shared_mutex> g(m_mu); m_slots.swap(t); m_mask = h.cap - 1; m_count = cnt;
    }
};
