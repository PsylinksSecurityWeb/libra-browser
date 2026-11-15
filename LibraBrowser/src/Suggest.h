// Auto suggest matcher (pure code, no UI, no network, no AI). Notes -> key/value pairs; a form field description -> best values.
// See PLAN-autosuggest.md. Sensitive fields (passwords, user names, cards, ID numbers, one-time codes...) are NEVER matched.
#pragma once
#include "Sponsor.h"   // SpReadStr (JSON string reader)
#include <set>
#include <string>
#include <vector>

struct SgPair { std::string key, val, kind; };
struct SgField { std::string label, placeholder, name, id, autocomplete, type, aria; bool textarea = false; int maxlen = 0; };
struct SgHit { std::string label, value; };

inline std::string SgNorm(const std::string& s) {   // lower case, camelCase and punctuation split into words (UTF-8 bytes are kept)
    std::string o; char prev = ' ';
    for (unsigned char c : s) {
        char ch = (char)c;
        if (c >= 'A' && c <= 'Z') { if (prev >= 'a' && prev <= 'z') o += ' '; o += (char)(c + 32); }
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c >= 128) o += ch;
        else o += ' ';
        prev = ch;
    }
    std::string r; bool sp = true; for (char c : o) { if (c == ' ') { if (!sp) r += ' '; sp = true; } else { r += c; sp = false; } }
    while (!r.empty() && r.back() == ' ') r.pop_back();
    return r;
}
inline std::vector<std::string> SgWords(const std::string& norm) { std::vector<std::string> w; size_t p = 0; while (p < norm.size()) { size_t e = norm.find(' ', p); if (e == std::string::npos) e = norm.size(); if (e > p) w.push_back(norm.substr(p, e - p)); p = e + 1; } return w; }
inline bool SgHas(const std::string& norm, const char* phrase) { std::string h = " " + norm + " ", n = std::string(" ") + phrase + " "; return h.find(n) != std::string::npos; }
inline bool SgAny(const std::string& norm, std::initializer_list<const char*> l) { for (auto* p : l) if (SgHas(norm, p)) return true; return false; }

// "!" = sensitive (never offer, never store), "" = unknown, else a kind name
inline std::string SgKindOf(const std::string& norm) {
    if (norm.empty()) return "";
    if (SgAny(norm, { "pin code", "pincode", "postal code", "postcode", "zip", "zip code", "postal" })) return "zip";
    if (SgAny(norm, { "password", "passcode", "passwd", "pwd", "username", "user name", "user id", "userid", "login", "log in", "sign in", "signin", "card number", "credit card", "debit card", "card no", "cardnumber", "card holder", "cardholder", "cvv", "cvc", "cvv2", "security code", "expiry", "expiration", "exp date", "expires", "iban", "routing", "swift", "bic", "account number", "account no", "acct", "upi", "pan", "pan number", "pan card", "aadhaar", "aadhar", "ssn", "social security", "passport", "driver license", "drivers license", "driving licence", "driving license", "dob", "date of birth", "birthday", "birth date", "birthdate", "otp", "one time", "verification code", "verify code", "pin", "captcha", "secret", "token", "api key", "tax id", "tin", "gstin", "bank", "mother maiden", "security answer", "security question", "card", "cc number", "ifsc" })) return "!";
    if (SgAny(norm, { "email", "e mail", "mail id", "mailid", "emailid" })) return "email";
    if (SgAny(norm, { "linkedin", "linked in" })) return "linkedin";
    if (SgAny(norm, { "twitter", "x handle", "x com", "x profile" })) return "twitter";
    if (SgAny(norm, { "github" })) return "github";
    if (SgAny(norm, { "instagram" })) return "instagram";
    if (SgAny(norm, { "phone", "mobile", "telephone", "tel", "contact number", "whatsapp", "cell", "phone number", "mobile number", "contact no" })) return "phone";
    if (SgAny(norm, { "bio", "about you", "about yourself", "about me", "describe", "tell us", "pitch", "summary", "introduction", "elevator", "one line", "oneliner", "one liner", "what does your company do", "description" })) return "bio";
    if (SgAny(norm, { "website", "web site", "site url", "homepage", "home page", "web address", "domain", "url", "link", "company url" })) return "website";
    if (SgAny(norm, { "company", "startup", "start up", "organization", "organisation", "business name", "employer", "firm", "fund name", "venture", "brand", "institution", "affiliation" })) return "company";
    if (SgAny(norm, { "job title", "your role", "designation", "position", "occupation", "current role", "your title", "role" })) return "job_title";
    if (SgAny(norm, { "city", "town" })) return "city";
    if (SgAny(norm, { "state", "province", "region", "county" })) return "state";
    if (SgAny(norm, { "country", "nation", "nationality" })) return "country";
    if (SgAny(norm, { "address", "street", "address line", "address 1", "address 2", "location" })) return "address";
    if (SgAny(norm, { "first name", "given name", "firstname", "fname", "forename" })) return "first_name";
    if (SgAny(norm, { "last name", "surname", "family name", "lastname", "lname" })) return "last_name";
    if (SgAny(norm, { "middle name" })) return "";
    if (SgAny(norm, { "full name", "fullname", "your name", "contact name", "legal name", "applicant name", "name", "name and surname" }) && SgWords(norm).size() <= 4) return "full_name";
    return "";
}
inline bool SgSensitiveText(const std::string& raw) { return SgKindOf(SgNorm(raw)) == "!"; }
inline std::set<std::string> SgModifiers(const std::string& norm) {   // "co-founder name" is not "your name"
    std::set<std::string> m;
    for (const char* w : { "cofounder", "founder", "referral", "referrer", "friend", "spouse", "parent", "guardian", "emergency", "reference", "colleague", "manager", "supervisor", "partner", "their", "other", "second", "secondary", "alternate", "alternative", "previous", "former" })
        if (SgHas(norm, w)) m.insert(w);
    if (SgHas(norm, "co founder")) m.insert("cofounder");
    if (m.count("founder") && SgHas(norm, "co founder")) m.erase("founder");
    return m;
}

// ---- notes -> pairs ----
inline std::string SgTrim(std::string s) { size_t a = 0; while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) ++a; size_t b = s.size(); while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) --b; return s.substr(a, b - a); }
inline void SgAdd(std::vector<SgPair>& out, std::string key, std::string val) {
    key = SgTrim(key); val = SgTrim(val);
    if (key.empty() || val.empty() || key.size() > 80 || out.size() >= 2000) return;
    if (val.size() > 2000) val.resize(2000);
    if (val.size() >= 2 && ((val.front() == '"' && val.back() == '"') || (val.front() == '\'' && val.back() == '\''))) val = val.substr(1, val.size() - 2);
    std::string n = SgNorm(key); if (n.empty() || SgWords(n).size() > 8) return;
    std::string k = SgKindOf(n); if (k == "!") return;   // sensitive notes are never used
    out.push_back({ key, val, k });
}
inline std::vector<std::string> SgSplitLines(const std::string& t) { std::vector<std::string> l; size_t p = 0; while (p <= t.size()) { size_t e = t.find('\n', p); if (e == std::string::npos) e = t.size(); l.push_back(t.substr(p, e - p)); p = e + 1; } return l; }
inline bool SgJsonValue(const std::string& b, size_t& p, const std::string& path, std::vector<SgPair>& out, int depth) {
    while (p < b.size() && isspace((unsigned char)b[p])) ++p; if (p >= b.size() || depth > 5 || out.size() > 2000) return false;
    if (b[p] == '"') { std::string s; if (!SpReadStr(b, p, s)) return false; SgAdd(out, path, s); return true; }
    if (b[p] == '{') {
        ++p; for (;;) {
            while (p < b.size() && isspace((unsigned char)b[p])) ++p; if (p < b.size() && b[p] == '}') { ++p; return true; }
            std::string k; if (!SpReadStr(b, p, k)) return false; while (p < b.size() && isspace((unsigned char)b[p])) ++p; if (p >= b.size() || b[p] != ':') return false; ++p;
            if (!SgJsonValue(b, p, path.empty() ? k : path + " " + k, out, depth + 1)) return false;
            while (p < b.size() && isspace((unsigned char)b[p])) ++p; if (p < b.size() && b[p] == ',') ++p; else if (p < b.size() && b[p] == '}') { ++p; return true; } else return false;
        }
    }
    if (b[p] == '[') {
        ++p; std::string joined; int n = 0; for (;;) {
            while (p < b.size() && isspace((unsigned char)b[p])) ++p; if (p < b.size() && b[p] == ']') { ++p; if (!joined.empty()) SgAdd(out, path, joined); return true; }
            if (p < b.size() && b[p] == '"') { std::string s; if (!SpReadStr(b, p, s)) return false; joined += (joined.empty() ? "" : ", ") + s; }
            else if (!SgJsonValue(b, p, path + " " + std::to_string(++n), out, depth + 1)) return false;
            while (p < b.size() && isspace((unsigned char)b[p])) ++p; if (p < b.size() && b[p] == ',') ++p; else if (p < b.size() && b[p] == ']') continue; else return false;
        }
    }
    size_t s = p; while (p < b.size() && b[p] != ',' && b[p] != '}' && b[p] != ']' && !isspace((unsigned char)b[p])) ++p;
    std::string tok = b.substr(s, p - s); if (tok != "null" && tok != "true" && tok != "false" && !tok.empty()) SgAdd(out, path, tok); return p > s;
}
inline std::vector<std::string> SgCsvRow(const std::string& l) { std::vector<std::string> c; std::string cur; bool q = false; for (char ch : l) { if (ch == '"') q = !q; else if (ch == ',' && !q) { c.push_back(SgTrim(cur)); cur.clear(); } else cur += ch; } c.push_back(SgTrim(cur)); return c; }
inline std::vector<SgPair> SgParse(const std::string& text) {
    std::vector<SgPair> out; if (text.empty() || text.size() > 700000) return out;
    std::string t = SgTrim(text);
    if (!t.empty() && t[0] == '{') { size_t p = 0; std::vector<SgPair> j; if (SgJsonValue(t, p, "", j, 0)) return j; }
    auto lines = SgSplitLines(text);
    { size_t i = 0; while (i < lines.size() && SgTrim(lines[i]).empty()) ++i;   // CSV: a header line and a row
      if (i + 1 < lines.size()) { std::string h = lines[i], r = lines[i + 1]; if (std::count(h.begin(), h.end(), ',') >= 2 && h.find(':') == std::string::npos && std::count(r.begin(), r.end(), ',') == std::count(h.begin(), h.end(), ',')) {
          auto hc = SgCsvRow(h), rc = SgCsvRow(r); for (size_t k = 0; k < hc.size() && k < rc.size(); ++k) SgAdd(out, hc[k], rc[k]); return out; } } }
    for (size_t i = 0; i < lines.size() && out.size() < 2000; ++i) {
        std::string l = SgTrim(lines[i]); if (l.empty()) continue;
        while (!l.empty() && (l[0] == '-' || l[0] == '*' || l[0] == '#' || l[0] == '>' || l[0] == ' ' || (unsigned char)l[0] == 0xE2)) { if ((unsigned char)l[0] == 0xE2 && l.size() >= 3) l = l.substr(3); else l = l.substr(1); }   // bullets (including the UTF-8 bullet and dashes)
        { size_t d = 0; while (d < l.size() && isdigit((unsigned char)l[d])) ++d; if (d > 0 && d + 1 < l.size() && (l[d] == '.' || l[d] == ')') && l[d + 1] == ' ') l = SgTrim(l.substr(d + 1)); }
        for (const char* m : { "**", "__", "`" }) { size_t q; while ((q = l.find(m)) != std::string::npos) l.erase(q, strlen(m)); }
        l = SgTrim(l); if (l.empty()) continue;
        if (l[0] == '|') {   // table row
            std::vector<std::string> c; size_t p = 1; while (p < l.size()) { size_t e = l.find('|', p); if (e == std::string::npos) e = l.size(); c.push_back(SgTrim(l.substr(p, e - p))); p = e + 1; }
            bool sepRow = true; for (auto& x : c) for (char ch : x) if (ch != '-' && ch != ':' && ch != ' ') sepRow = false;
            if (!sepRow && c.size() >= 2) SgAdd(out, c[0], c[1]); continue;
        }
        size_t sep = std::string::npos, seplen = 1;
        for (size_t p = 0; p < l.size() && p <= 60; ++p) if (l[p] == ':' && (p + 1 >= l.size() || l[p + 1] == ' ' || l[p + 1] == '\t')) { sep = p; break; }
        if (sep == std::string::npos) { size_t p = l.find(" - "); if (p == std::string::npos) p = l.find(" \xE2\x80\x93 "); if (p == std::string::npos) p = l.find(" \xE2\x80\x94 "); if (p == std::string::npos) p = l.find('='); if (p != std::string::npos && p <= 60) { sep = p; seplen = l[p] == '=' ? 1 : (l[p + 1] == '-' ? 3 : 5); } }
        if (sep == std::string::npos) continue;
        std::string k = l.substr(0, sep), v = SgTrim(l.substr(std::min(l.size(), sep + seplen)));
        if (SgWords(SgNorm(k)).size() > 8) continue;
        if (v.empty()) {   // the value follows on the next lines, up to a blank line or the next "key: value"
            std::string joined; size_t j = i + 1;
            for (; j < lines.size() && joined.size() < 1500; ++j) { std::string n = SgTrim(lines[j]); if (n.empty()) break; size_t c = n.find(": "); if (c != std::string::npos && c <= 40 && SgWords(SgNorm(n.substr(0, c))).size() <= 5) break; joined += (joined.empty() ? "" : "\n") + n; }
            if (!joined.empty()) { SgAdd(out, k, joined); i = j - 1; }
            continue;
        }
        SgAdd(out, k, v);
    }
    return out;
}

// ---- field -> kind ----
inline std::string SgFieldKind(const SgField& f, std::set<std::string>* mods = nullptr) {
    std::string ac = SgNorm(f.autocomplete), type = SgNorm(f.type);
    std::string all = SgNorm(f.label + " " + f.placeholder + " " + f.aria + " " + f.name + " " + f.id);
    if (SgAny(ac, { "username", "current password", "new password", "one time code", "cc name", "cc given name", "cc family name", "cc number", "cc exp", "cc exp month", "cc exp year", "cc csc", "cc type", "bday", "bday day", "bday month", "bday year", "transaction amount", "sex", "nickname" })) return "!";
    if (type == "password" || type == "hidden" || type == "file" || type == "checkbox" || type == "radio" || type == "submit" || type == "button" || type == "image" || type == "reset" || type == "range" || type == "color" || type == "search" || type == "date" || type == "datetime local" || type == "month" || type == "week" || type == "time") return "!";
    if (SgKindOf(all) == "!") return "!";
    for (const std::string& t : { SgNorm(f.label), SgNorm(f.aria), SgNorm(f.placeholder), SgNorm(f.name + " " + f.id) }) if (SgKindOf(t) == "!") return "!";
    if (mods) *mods = SgModifiers(all);
    if (ac == "name") return "full_name"; if (ac == "given name") return "first_name"; if (ac == "family name") return "last_name"; if (ac == "email") return "email";
    if (ac == "tel" || ac == "tel national") return "phone"; if (ac == "organization") return "company"; if (ac == "organization title") return "job_title";
    if (ac == "street address" || ac == "address line1" || ac == "address line2") return "address"; if (ac == "address level2") return "city"; if (ac == "address level1") return "state";
    if (ac == "postal code") return "zip"; if (ac == "country" || ac == "country name") return "country"; if (ac == "url") return "website";
    if (type == "email") return "email"; if (type == "tel") return "phone";
    for (const std::string& t : { SgNorm(f.label), SgNorm(f.aria), SgNorm(f.placeholder), SgNorm(f.name + " " + f.id) }) { std::string k = SgKindOf(t); if (!k.empty()) return k; }
    if (type == "url") return "website";
    return "";
}
inline bool SgSensitiveField(const SgField& f) { return SgFieldKind(f) == "!"; }

inline std::string SgNice(const std::string& k) { std::string s = SgTrim(k); if (!s.empty() && s[0] >= 'a' && s[0] <= 'z') s[0] = (char)(s[0] - 32); if (s.size() > 40) s.resize(40); return s; }
inline std::vector<SgHit> SgMatch(const std::vector<SgPair>& pairs, const SgField& f, size_t max = 3) {
    std::set<std::string> mods; std::string fk = SgFieldKind(f, &mods); std::vector<SgHit> none;
    if (fk == "!" || pairs.empty()) return none;
    std::string type = SgNorm(f.type);
    static const std::set<std::string> stop = { "your", "the", "of", "a", "an", "please", "enter", "what", "is", "are", "you", "do", "we", "to", "for", "and", "or", "in", "on", "with", "field", "required", "optional", "text", "input", "type", "here", "me", "us", "my", "our", "i", "name" };
    std::string ftext = SgNorm(f.label + " " + f.aria + " " + (f.label.empty() ? f.placeholder : std::string()) + " " + (f.label.empty() && f.placeholder.empty() ? f.name + " " + f.id : std::string()));
    std::set<std::string> fw; for (auto& w : SgWords(ftext)) if (!stop.count(w)) fw.insert(w);
    struct C { int score; size_t order; SgHit h; }; std::vector<C> c;
    auto words = [](const std::string& k) { return SgWords(SgNorm(k)); };
    for (size_t i = 0; i < pairs.size(); ++i) {
        const SgPair& p = pairs[i]; std::set<std::string> pm = SgModifiers(SgNorm(p.key));
        if (pm != mods) continue;   // "Name" never gets the co-founder's name, and the other way round
        std::string v = p.val; if (!f.textarea) { for (auto& ch : v) if (ch == '\n' || ch == '\r') ch = ' '; if (v.size() > 400) continue; }
        if (type == "email" && p.kind != "email") continue; if (type == "tel" && p.kind != "phone") continue; if (type == "url" && p.kind != "website" && p.kind != "linkedin" && p.kind != "twitter" && p.kind != "github" && p.kind != "instagram") continue;
        int score = 0; std::string value = v;
        if (!fk.empty() && p.kind == fk) { score = 100; int ov = 0; for (auto& w : words(p.key)) if (fw.count(w)) ++ov; score += ov * 5 - (int)words(p.key).size(); }
        else if (fk == "first_name" && p.kind == "full_name") { auto w = SgWords(SgNorm(v)); size_t sp = v.find(' '); if (w.size() >= 2 && sp != std::string::npos) { value = v.substr(0, sp); score = 60; } }
        else if (fk == "last_name" && p.kind == "full_name") { size_t sp = v.rfind(' '); if (sp != std::string::npos && SgWords(SgNorm(v)).size() >= 2) { value = v.substr(sp + 1); score = 55; } }
        else if (fk == "full_name" && p.kind == "first_name") { for (auto& q : pairs) if (q.kind == "last_name" && SgNorm(q.key) != "") { value = v + " " + q.val; score = 70; break; } }
        else if (fk.empty() || fk == "bio") {
            if (fk == "bio" && p.kind != "bio") continue;
            std::set<std::string> kw; for (auto& w : words(p.key)) if (!stop.count(w)) kw.insert(w);
            int ov = 0; for (auto& w : kw) if (fw.count(w)) ++ov;
            if (ov >= 1 && (int)std::min(fw.size(), kw.size()) > 0 && ov * 2 >= (int)std::min(fw.size(), kw.size())) score = 40 + ov * 10;
        }
        if (score <= 0) continue;
        if (f.maxlen > 0 && (int)value.size() > f.maxlen) continue;   // would not fit: do not offer a cut-off value
        c.push_back({ score, i, { SgNice(p.key), value } });
    }
    std::stable_sort(c.begin(), c.end(), [](const C& a, const C& b) { return a.score > b.score; });
    std::vector<SgHit> out; std::set<std::string> seen;
    for (auto& x : c) { if (out.size() >= max) break; if (seen.insert(x.h.value).second) out.push_back(x.h); }
    return out;
}
