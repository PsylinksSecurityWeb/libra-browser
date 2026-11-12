#pragma once
// Saved logins: one DPAPI-encrypted file (vault.dat). DPAPI = tied to this Windows user; other users of the PC cannot read it.
// Passwords are never written anywhere in plain text and never logged.
#include "Data.h"
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")

struct Cred { std::wstring origin, user, pass; long long created = 0, used = 0; };

inline const char* kVaultEntropy = "LibraVault-v1";

inline std::string PwEnc(const std::wstring& w) {   // keep tab/newline/% out of the record format
    std::string u = ToUtf8(w), o;
    for (unsigned char c : u) { if (c == '%' || c == '\t' || c == '\n' || c == '\r') { char b[4]; sprintf_s(b, "%%%02X", c); o += b; } else o += (char)c; }
    return o;
}
inline std::wstring PwDec(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) { if (s[i] == '%' && i + 2 < s.size()) { o += (char)strtol(s.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; } else o += s[i]; }
    return FromUtf8(o);
}
inline bool VaultSave(const std::vector<Cred>& v) {
    std::string plain;
    for (auto& c : v) plain += PwEnc(c.origin) + "\t" + PwEnc(c.user) + "\t" + PwEnc(c.pass) + "\t" + std::to_string(c.created) + "\t" + std::to_string(c.used) + "\n";
    DATA_BLOB in{ (DWORD)plain.size(), (BYTE*)plain.data() }, ent{ (DWORD)strlen(kVaultEntropy), (BYTE*)kVaultEntropy }, out{};
    BOOL ok = CryptProtectData(&in, L"Libra vault", &ent, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out);
    if (!plain.empty()) SecureZeroMemory(plain.data(), plain.size());
    if (!ok) return false;
    WriteAll(DataDir() + L"\\vault.dat", std::string((const char*)out.pbData, out.cbData));
    LocalFree(out.pbData);
    return true;
}
inline bool VaultLoad(std::vector<Cred>& v) {
    v.clear();
    std::string enc = ReadAll(DataDir() + L"\\vault.dat");
    if (enc.empty()) return false;
    DATA_BLOB in{ (DWORD)enc.size(), (BYTE*)enc.data() }, ent{ (DWORD)strlen(kVaultEntropy), (BYTE*)kVaultEntropy }, out{};
    if (!CryptUnprotectData(&in, nullptr, &ent, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return false;
    std::string plain((const char*)out.pbData, out.cbData);
    SecureZeroMemory(out.pbData, out.cbData); LocalFree(out.pbData);
    size_t s = 0;
    while (s < plain.size()) {
        size_t e = plain.find('\n', s); if (e == std::string::npos) e = plain.size();
        std::string line = plain.substr(s, e - s); s = e + 1;
        std::vector<std::string> f; size_t p = 0;
        for (;;) { size_t t = line.find('\t', p); f.push_back(line.substr(p, t == std::string::npos ? t : t - p)); if (t == std::string::npos) break; p = t + 1; }
        if (f.size() == 5) { Cred c; c.origin = PwDec(f[0]); c.user = PwDec(f[1]); c.pass = PwDec(f[2]); c.created = _atoi64(f[3].c_str()); c.used = _atoi64(f[4].c_str()); v.push_back(std::move(c)); }
    }
    if (!plain.empty()) SecureZeroMemory(plain.data(), plain.size());
    return true;
}
