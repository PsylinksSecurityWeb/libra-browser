// Filter engine glue (Filters.h + FilterList.h): load the rules, decide on a request, and the page script part (cosmetic rules + "disable your ad blocker" wall remover). Max mode only.
#include "FilterList.h"
void BrowserWindow::LoadFilters() {
    m_flt = Flt::Set(); m_flt.AddText(kOwnFilters);
    std::wstring p = m_set.Get(L"filter_list", L"");
    if (!p.empty() && p.size() < 520 && p.find(L"..") == std::wstring::npos) {   // the user's own list file: a plain file of Adblock-style lines, 5 MB at most, read only
        HANDLE f = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (f != INVALID_HANDLE_VALUE) { LARGE_INTEGER sz{}; GetFileSizeEx(f, &sz); if (sz.QuadPart > 0 && sz.QuadPart <= 5 * 1024 * 1024) { std::string d((size_t)sz.QuadPart, '\0'); DWORD rd = 0; if (ReadFile(f, &d[0], (DWORD)d.size(), &rd, nullptr)) { d.resize(rd); m_flt.AddText(d); } } CloseHandle(f); }
    }
    wchar_t tf[MAX_PATH]; if (TestEnv(L"LIBRA_TEST_FILTERLOG", tf, MAX_PATH)) WriteAllRaw(tf, "filters " + std::to_string(m_flt.rules.size()) + " cosmetic " + std::to_string(m_flt.cos.size()) + "\n", true);   // tests only
}
bool BrowserWindow::FilterBlocks(const wchar_t* uri, const std::wstring& pageUrl, int ctxI) const {
    if (m_flt.Empty() || !uri) return false;
    auto ctx = (COREWEBVIEW2_WEB_RESOURCE_CONTEXT)ctxI; unsigned type;
    switch (ctx) { case COREWEBVIEW2_WEB_RESOURCE_CONTEXT_IMAGE: type = Flt::T_IMAGE; break; case COREWEBVIEW2_WEB_RESOURCE_CONTEXT_SCRIPT: type = Flt::T_SCRIPT; break;
        case COREWEBVIEW2_WEB_RESOURCE_CONTEXT_XML_HTTP_REQUEST: case COREWEBVIEW2_WEB_RESOURCE_CONTEXT_FETCH: type = Flt::T_XHR; break; case COREWEBVIEW2_WEB_RESOURCE_CONTEXT_STYLESHEET: type = Flt::T_STYLE; break;
        case COREWEBVIEW2_WEB_RESOURCE_CONTEXT_MEDIA: type = Flt::T_MEDIA; break; case COREWEBVIEW2_WEB_RESOURCE_CONTEXT_OTHER: type = Flt::T_OTHER; break; default: return false; }   // never a page or frame address
    std::string u = ToUtf8(uri), pu = ToUtf8(pageUrl), rh, ph; size_t e1 = 0, e2 = 0;
    if (!Flt::Set::ParseUrl(u, rh, e1) || !Flt::Set::ParseUrl(pu, ph, e2)) return false;
    bool third = RegDom(FromUtf8(rh)) != RegDom(FromUtf8(ph));
    return m_flt.Block(u, ph, third, type);
}
static std::wstring FltJsStr(const std::string& s) { std::wstring o = L"\""; for (char c : s) { if (c == '"' || c == '\\') o += L'\\'; if ((unsigned char)c >= 32 && (unsigned char)c < 127) o += (wchar_t)c; } return o + L"\""; }
std::wstring BrowserWindow::FilterJs() const {   // spliced into the ad script, inside its Max-only region
    std::wstring cf = L"[", g; size_t ng = 0, nd = 0;
    for (auto& c : m_flt.cos) { if (c.doms.empty()) { if (ng++ >= 1500) continue; } else if (nd++ >= 4000) continue; cf += L"[" + FltJsStr(c.doms) + L"," + FltJsStr(c.sel) + L"],"; }
    cf += L"]";
    return L"if(MODE===0){var CF=" + cf + LR"js(;
(function(){var h=th;function inD(ds){if(!ds)return true;var a=ds.split(','),ok=false;for(var i=0;i<a.length;i++){var d=a[i];if(d.charAt(0)==='~'){if(h===d.slice(1)||h.endsWith('.'+d.slice(1)))return false}else if(h===d||h.endsWith('.'+d))ok=true}return ok}
try{var cs=new CSSStyleSheet(),n=0;for(var i=0;i<CF.length&&n<5000;i++){if(!inD(CF[i][0]))continue;try{cs.insertRule(CF[i][1]+'{display:none!important}');n++}catch(e){}}if(n)document.adoptedStyleSheets=[].concat(document.adoptedStyleSheets||[],[cs])}catch(e){}})();
/* "disable your ad blocker" walls: a notice box (fixed/absolute, dialog, or named like a modal/notice) whose own short text says so is hidden and the scroll lock is released. Article text is never touched. */
(function(){var S=/ad[\s-]?block(er|ing)?\s*(is\s*)?(detected|enabled|active)|disable\s+(your\s+)?ad[\s-]?block|turn\s+off\s+(your\s+)?ad[\s-]?block|pause\s+(your\s+)?ad[\s-]?block|(please\s+)?(whitelist|allowlist)\s+(us|this\s+(site|page))|allow\s+ads\s+on\s+this\s+(site|page)|ad[\s-]?blocker\s+(detected|is\s+on|is\s+blocking)/i;
var runs=0,seen=new WeakSet(),tm=0;
function pos(e){try{var p=getComputedStyle(e).position;return p==='fixed'||p==='absolute'||p==='sticky'}catch(x){return false}}
function box(e){return pos(e)||e.getAttribute('role')==='dialog'||e.getAttribute('aria-modal')==='true'||/adblock|ad-block|anti-?ab|detect|notice|modal|popup|overlay|wall|interstitial/i.test((e.id||'')+' '+(typeof e.className==='string'?e.className:''))}
function scan(){tm=0;if(runs++>30||!document.body)return;var c=document.body.querySelectorAll('div,section,aside,dialog,p,span,h1,h2,h3,[role=dialog]'),hid=0;
for(var i=0;i<c.length&&i<4000;i++){var e=c[i];if(seen.has(e)||e.childElementCount>8)continue;var t=e.textContent;if(!t||t.length<25||t.length>600||!S.test(t))continue;seen.add(e);
var top=e;for(var k=0;k<4&&top.parentElement&&top.parentElement!==document.body&&top.parentElement!==document.documentElement;k++){var p=top.parentElement;if((p.textContent||'').length-t.length>80)break;top=p}
if(!(box(top)||box(e)))continue;if(top.closest('article,main')&&!pos(top)&&!box(top))continue;
top.style.setProperty('display','none','important');hid++}
if(hid)[document.documentElement,document.body].forEach(function(x){try{if(getComputedStyle(x).overflow==='hidden')x.style.setProperty('overflow','auto','important')}catch(q){}})}
function later(){if(!tm)tm=setTimeout(scan,700)}
document.addEventListener('DOMContentLoaded',scan);window.addEventListener('load',function(){scan();setTimeout(scan,2500)});
try{new MutationObserver(later).observe(document.documentElement,{childList:true,subtree:true})}catch(e){}})();}
)js";
}
