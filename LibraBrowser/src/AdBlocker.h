#pragma once
#include "pch.h"

// Simple domain-based ad blocker — lookup via hash set (O(1) per request).
// Loaded from a plain text "hosts"-style block list (one domain per line, 
// lines starting with # are comments).
class AdBlocker {
public:
    AdBlocker() = default;

    // Load from embedded default block list (bundled at compile time)
    void LoadDefaults();

    // Load additional rules from a file (one domain per line)
    void LoadFile(const std::wstring& path);

    // Returns true if the given URL's hostname should be blocked
    bool ShouldBlock(const wchar_t* url) const;

    size_t BlockedCount() const { return m_blocked; }
    size_t RuleCount()    const { return m_domains.size(); }
    void   ResetCounter()       { m_blocked = 0; }

private:
    std::unordered_set<std::wstring> m_domains;
    mutable size_t m_blocked = 0;

    // Extract the registrable host from a full URL string
    static std::wstring ExtractHost(const wchar_t* url);
};
