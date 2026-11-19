#pragma once
// libra:// pages: tiny server-rendered HTML. Buttons carry data-c (command) / data-a (arg);
// one click handler posts "cmd<TAB>arg" to native code. No data is ever put into JS.
// Look: dull grey canvas, white raised cards, soft shadows, no divider lines, black/white only.
#include "Data.h"
#include "Logos.h"
#include "HeroAssets.h"

inline std::wstring LogoHtml(const std::wstring& name) {   // engine logo, or a letter badge when we have none
    std::wstring b = EngineLogo(name);
    if (!b.empty()) return L"<img class=lg alt=\"\" src=\"data:image/png;base64," + b + L"\">";
    return L"<i class=lgl>" + Esc(name.substr(0, 1)) + L"</i>";
}

inline std::wstring NavItem(const wchar_t* key, const wchar_t* glyph, const wchar_t* label, const std::wstring& active) {
    return std::wstring(L"<a data-c=nav data-a=") + key + (active == key ? L" class=on" : L"") + L"><i class=ico>&#x" + glyph + L";</i>" + label + L"</a>";
}

inline std::wstring PageShell(const std::wstring& title, const std::wstring& body, bool grey, const std::wstring& js = L"",
                              const std::wstring& active = L"", bool sidebar = true, bool footer = true) {
    std::wstring side;
    if (sidebar)
        side = L"<aside><div class=brand>LIBRA<span class=beta>BETA</span></div><nav>" +
               NavItem(L"history", L"E81C", L"History", active) + NavItem(L"bookmarks", L"E734", L"Bookmarks", active) + NavItem(L"passwords", L"E72E", L"Passwords", active) +
               NavItem(L"downloads", L"E896", L"Downloads", active) + NavItem(L"privacy", L"E7B3", L"Privacy", active) + NavItem(L"appearance", L"E790", L"Appearance", active) + NavItem(L"settings", L"E713", L"Settings", active) + L"</nav><div class=made>Built by Psylinks Security Private Limited</div></aside>";
    if (!sidebar && footer) side = L"<div class=made style=\"position:fixed;bottom:16px;left:0;right:0;text-align:center\">Built by Psylinks Security Private Limited</div>";
    if (!sidebar) side = L"<style>main{max-width:none}</style>" + side;   // pages without the rail (errors, reader, signer) centre on the window, not on a 900 px column
    return std::wstring(LR"HTML(<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta http-equiv="Content-Security-Policy" content="default-src 'none'; img-src data: https://libra-hero.local; media-src https://libra-hero.local; style-src 'unsafe-inline'; script-src 'unsafe-inline'; form-action https:; base-uri 'none'"><title>)HTML") + Esc(title) + LR"HTML(</title><style>
:root{--bg:)HTML" + L"#fff" + LR"HTML(;--fg:#111;--mute:#8a8a8a;--card:#fff}
*{box-sizing:border-box;margin:0;padding:0}
body{font:14px 'Segoe UI Variable Text','Segoe UI',system-ui,sans-serif;background:var(--bg);color:var(--fg);display:flex;min-height:100vh;-webkit-font-smoothing:antialiased}
html:has(aside){scrollbar-gutter:stable}
main{animation:pgin .2s ease-out both;transition:opacity .12s}main.lv{opacity:0}
@keyframes pgin{from{opacity:0}to{opacity:1}}
@media (prefers-reduced-motion:reduce){main{animation:none}}
::-webkit-scrollbar{width:10px}::-webkit-scrollbar-thumb{background:rgba(0,0,0,.22);border-radius:6px;border:3px solid transparent;background-clip:content-box}::-webkit-scrollbar-thumb:hover{background:rgba(0,0,0,.4);background-clip:content-box}::-webkit-scrollbar-track{background:transparent}
aside{width:240px;padding:30px 16px;position:sticky;top:0;height:100vh;flex-shrink:0;display:flex;flex-direction:column;gap:8px}
.made{margin-top:auto;padding:0 12px;font-size:11px;color:var(--mute);letter-spacing:.04em}
.brand{font-weight:300;letter-spacing:.38em;font-size:15px;margin:0 12px 18px}
nav{margin-top:14px}
nav a{display:flex;gap:12px;align-items:center;padding:11px 12px;border-radius:12px;color:#555;cursor:pointer;margin-bottom:2px;transition:background .12s}
nav a:hover{background:rgba(255,255,255,.65);color:#000}
nav a.on{background:var(--card);color:#000;font-weight:600;box-shadow:none}
.ico{font:normal 16px 'Segoe Fluent Icons','Segoe MDL2 Assets';width:20px;text-align:center}
main{flex:1;min-width:0;padding:34px 44px 70px;max-width:900px}
h1{font-size:28px;font-weight:600;letter-spacing:-.02em;margin-bottom:22px}
h2{font-size:12px;font-weight:600;letter-spacing:.1em;text-transform:uppercase;color:var(--mute);margin:26px 0 10px 6px}
.card{background:var(--card);border-radius:20px;padding:6px 22px;box-shadow:none}
.row{display:flex;align-items:center;justify-content:space-between;gap:18px;padding:17px 0}
.row+.row{margin-top:0}
.row small{display:block;color:var(--mute);margin-top:3px;max-width:380px}
input[type=text],input[type=password],select{font:inherit;color:var(--fg);background:#f2f2f2;border:0;outline:0;border-radius:12px;padding:10px 14px;min-width:250px;transition:box-shadow .12s,background .12s}
input[type=text]:focus,select:focus{background:#fff;box-shadow:0 0 0 2px #111}
button{font:inherit;color:var(--fg);background:#efefef;border:0;border-radius:999px;padding:9px 18px;cursor:pointer;transition:background .12s,transform .08s}
button:hover{background:#e2e2e2}button:active{transform:scale(.97)}
button.pri{background:#111;color:#fff}button.pri:hover{background:#333}
aside button{width:100%;padding:11px 16px;text-align:left;background:rgba(255,255,255,.7)}
aside button.pri{background:#111}
input[type=checkbox]{appearance:none;-webkit-appearance:none;width:44px;height:26px;border-radius:13px;background:#d2d2d2;position:relative;cursor:pointer;flex-shrink:0;transition:background .15s}
input[type=checkbox]::after{content:'';position:absolute;top:3px;left:3px;width:20px;height:20px;border-radius:50%;background:#fff;box-shadow:none;transition:left .15s}
input[type=checkbox]:checked{background:#111}input[type=checkbox]:checked::after{left:21px}
input.cb{width:20px;height:20px;border-radius:6px;border:1.6px solid #b4b4b4;background:#fff;transition:background .12s,border-color .12s}
input.cb::after{top:1px;left:5.5px;width:5px;height:10px;border:solid #fff;border-width:0 2px 2px 0;border-radius:0;background:transparent;transform:rotate(45deg) scale(0);transition:transform .12s}
input.cb:checked{background:#111;border-color:#111}input.cb:checked::after{left:5.5px;transform:rotate(45deg) scale(1)}
.top{display:flex;gap:12px;margin-bottom:16px}.top input{flex:1;background:var(--card);box-shadow:none}
.item{display:flex;gap:14px;align-items:center;padding:13px 0}
.item .t a,.item small,.item .nm{unicode-bidi:isolate}   /* a page title with a right-to-left override can not reorder text outside its own line */
.t{flex:1;min-width:0}.t a{display:block;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;color:var(--fg);text-decoration:none;font-weight:500}
.t a:hover{text-decoration:underline}
.t small{color:var(--mute);display:block;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;margin-top:2px}
.bar{height:5px;border-radius:3px;background:#e6e6e6;margin-top:9px;overflow:hidden}.bar i{display:block;height:5px;background:#111;border-radius:3px}
.empty{color:var(--mute);padding:26px 0;text-align:center}
.center{max-width:680px;margin:0 auto;padding-top:9vh;text-align:center}
.center h1{font-size:34px;font-weight:300;letter-spacing:.3em;text-transform:uppercase;margin-bottom:8px}
.center p{color:var(--mute)}
.eng{display:grid;grid-template-columns:repeat(auto-fill,minmax(190px,1fr));gap:14px;margin-top:30px}
.eng button{background:var(--card);padding:20px 18px;border-radius:18px;text-align:left;font-size:15px;font-weight:600;box-shadow:none}
.eng button:hover{background:#111;color:#fff}.eng button small{display:block;font-weight:400;color:var(--mute);margin-top:3px}.eng button:hover small{color:#bbb}
@keyframes rise{from{opacity:0;transform:translateY(14px)}to{opacity:1;transform:none}}
@keyframes sweep{0%{transform:scaleX(0);opacity:1}70%{opacity:1}100%{transform:scaleX(1);opacity:0}}
@keyframes spin{to{transform:rotate(360deg)}}
.card,h1,h2,.top,.err,.eng,.center>*{animation:rise .55s cubic-bezier(.2,.7,.2,1) both}
main>:nth-child(2){animation-delay:.05s}main>:nth-child(3){animation-delay:.1s}main>:nth-child(4){animation-delay:.15s}main>:nth-child(5){animation-delay:.2s}main>:nth-child(6){animation-delay:.25s}main>:nth-child(7){animation-delay:.3s}
.err{max-width:560px;margin:11vh auto 0;text-align:center}
.eic{width:84px;height:84px;display:block;margin:0 auto 26px;fill:none;stroke:#111;stroke-width:1.3;stroke-linecap:round;stroke-linejoin:round;animation:ebreath 3.4s 1.4s ease-in-out infinite}
.eic path{stroke-dasharray:1;stroke-dashoffset:1;animation:edraw 1s cubic-bezier(.4,.1,.2,1) forwards}.eic path+path{animation-delay:.2s}.eic path:nth-child(n+3){animation-delay:.5s;opacity:.5}
@keyframes edraw{to{stroke-dashoffset:0}}@keyframes ebreath{50%{transform:translateY(-4px)}}
.err h1{font-size:30px;margin-bottom:10px}.err p{color:#666;line-height:1.55}.err code{display:block;margin:18px 0 26px;color:var(--mute);font-size:12px;word-break:break-all}
.err .btns{display:flex;gap:10px;justify-content:center;flex-wrap:wrap}.err small{display:block;margin-top:22px;color:var(--mute);font-size:11px}
.lgl{display:inline-grid;place-items:center;width:22px;height:22px;border-radius:6px;background:#eee;font:600 12px "Segoe UI",sans-serif;font-style:normal;margin-right:10px;vertical-align:-6px;color:#111}
.cols{display:grid;grid-template-columns:1fr 1fr;gap:30px;margin-top:26px;text-align:left}
.cols h2{margin:0 0 4px 6px}
@media(max-width:720px){.cols{grid-template-columns:1fr}}
.eng button:hover .lgl{background:#333;color:#fff}
.eng button img,.row .lg{width:22px;height:22px;border-radius:6px;vertical-align:-6px;margin-right:10px;object-fit:contain}
.eng button:hover{transform:translateY(-2px);box-shadow:none}
.eng button{transition:background .15s,color .15s,transform .15s,box-shadow .15s}
/* shared look: white canvas, flat 1px cards, heavy headline, black active item */
aside{border-right:1px solid #eee}.brand{font-weight:800;letter-spacing:.02em;font-size:20px}.beta{display:inline-block;margin-left:8px;padding:2px 7px;border:1px solid #111;border-radius:99px;font-size:9px;font-weight:700;letter-spacing:.08em;vertical-align:4px}
.bnote{color:#777;font-size:12.5px;margin:-14px 0 18px}
nav a:hover{background:#f3f3f3}nav a.on{background:#111;color:#fff;box-shadow:none}
h1{font:800 40px/1.1 'Segoe UI Variable Display','Segoe UI',sans-serif;letter-spacing:-.02em;margin-bottom:26px}
.card{box-shadow:none;border:1px solid #eee;border-radius:18px}.top input{box-shadow:none;border:1px solid #eee;background:#fafafa}
aside button{background:#f3f3f3}aside button.pri{background:#111}
@media(prefers-reduced-motion:no-preference){main{animation:pgin .5s cubic-bezier(.2,.7,.2,1) both}}@keyframes pgin{from{opacity:0;transform:translate3d(0,14px,0)}to{opacity:1;transform:none}}
.card>.item{padding:14px 0;border-top:1px solid #f0f0f0}.card>.item:first-child,.card>.day+.item{border-top:0}
.item .t{min-width:0;flex:1}.item .t a{display:block;font-size:15px;font-weight:500;color:var(--fg);text-decoration:none;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.item .t a:hover{text-decoration:underline}
.item small{display:block;color:var(--mute);font-size:13px;margin-top:2px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.item button{flex-shrink:0;padding:7px 14px;font-size:13px;background:none;color:var(--mute);transition:opacity .15s,background .15s}.item button:hover{background:#f3f3f3;color:#111}.hv .item button{opacity:0}.hv .item:hover button,.hv .item:focus-within button{opacity:1}
.day{padding:18px 0 4px;font-size:11.5px;font-weight:600;letter-spacing:.09em;text-transform:uppercase;color:#a0a0a0;border-top:1px solid #f0f0f0}.card>.day:first-child{border-top:0;padding-top:14px}
.empty{padding:48px 20px;text-align:center;color:var(--mute);font-size:14px;line-height:1.5}.empty b{display:block;color:var(--fg);font-size:16px;font-weight:600;margin-bottom:4px}
.card:has(>.empty){padding:0}
</style></head><body>)HTML" + side + L"<main>" + body + LR"HTML(</main><style>@media(max-width:860px){   /* narrow window (half-screen snap, small laptop): the rail becomes a top strip, rows wrap, nothing runs past the edge */
body:has(aside){flex-direction:column}
aside{width:auto;height:auto;position:static;flex-direction:row;align-items:center;gap:6px;padding:10px 14px;border-right:0;border-bottom:1px solid #eee;overflow-x:auto}
aside::-webkit-scrollbar{display:none}.brand,.made{display:none}nav{margin:0;display:flex;gap:2px}nav a{padding:9px 12px;margin:0;white-space:nowrap}
body:has(aside) main{padding:20px 18px 60px;max-width:none}
.top,.row,.card .row,.ctl,.rg,.seg,.acts,.bt{flex-wrap:wrap!important}.row,.card .row{gap:12px!important}
main .card .row input[type=text],main .card .row input[type=password],main .card .row select,main .top input{width:100%!important;min-width:0!important;max-width:100%}
.item{min-width:0}.item .t{min-width:0}img,svg,canvas{max-width:100%}
}</style><script>
const P=(...a)=>chrome.webview.postMessage(a.join('\t'));
document.addEventListener('click',e=>{const b=e.target.closest('[data-c]');if(b){if(b.dataset.c==='nav'&&b.dataset.a!=='agent'&&!b.classList.contains('on')){document.querySelectorAll('nav a.on').forEach(x=>x.classList.remove('on'));b.classList.add('on');const m=document.querySelector('main');if(m){m.classList.add('lv');setTimeout(()=>m.classList.remove('lv'),1500)}}P(b.dataset.c,b.dataset.a||'')}});
document.addEventListener('change',e=>{const t=e.target;if(t.dataset.k)P('set',t.dataset.k,t.type=='checkbox'?(t.checked?'1':'0'):t.value)});
)HTML" + js + L"</script></body></html>";
}

inline std::wstring Fmt(long long b) {
    wchar_t s[32];
    if (b < 1024) swprintf(s, 32, L"%lld B", b);
    else if (b < (1 << 20)) swprintf(s, 32, L"%.0f KB", b / 1024.0);
    else if (b < (1LL << 30)) swprintf(s, 32, L"%.1f MB", b / 1048576.0);
    else swprintf(s, 32, L"%.2f GB", b / 1073741824.0);
    return s;
}
inline std::wstring Chk(const Settings& s, const wchar_t* k) { return s.On(k) ? L" checked" : L""; }
inline std::wstring Toggle(const Settings& s, const wchar_t* k, const wchar_t* label, const wchar_t* sub = L"") {
    return std::wstring(L"<div class=row><div><span class=lbl>") + label + L"</span>" + (*sub ? std::wstring(L"<small>") + sub + L"</small>" : L"") +
           L"</div><input type=checkbox data-k=" + k + Chk(s, k) + L"></div>";
}

inline std::wstring ToggleOn(const Settings& s, const wchar_t* k, const wchar_t* label, const wchar_t* sub, const wchar_t* def = L"1") {   // on unless set to 0
    return std::wstring(L"<div class=row><div><span class=lbl>") + label + L"</span><small>" + sub + L"</small></div><input type=checkbox data-k=" + k + (s.Get(k, def) != L"0" ? L" checked" : L"") + L"></div>";
}
static const wchar_t* kSetCss = LR"H(<style>
.chips{position:sticky;top:0;z-index:5;display:flex;gap:6px;flex-wrap:wrap;padding:12px 0;margin:-6px 0 4px;background:var(--chipbg,rgba(255,255,255,.94));backdrop-filter:blur(8px)}
.chips a{padding:6px 14px;border-radius:999px;background:#f3f3f3;color:#444;font-size:13px;cursor:pointer;user-select:none;transition:background .15s,color .15s}.chips a:hover{background:#111;color:#fff}
main h2{scroll-margin-top:62px;margin:30px 0 10px 4px}
.card{padding:2px 22px}
.card .row{padding:15px 0;gap:28px;min-height:62px;border-top:1px solid #f0f0f0;align-items:center}.card .row:first-child{border-top:0}
.card .row>div:first-child{flex:1;min-width:0}.card .row small{max-width:none;margin-top:2px;font-size:13px;line-height:1.45}
.lbl{font-size:15px;font-weight:500;display:flex;align-items:center;gap:8px}
.card .row select,.card .row input[type=text]{width:236px;min-width:0;flex-shrink:0}
.card .row button{flex-shrink:0;min-width:96px}
.sub{padding:20px 0 6px;border-top:1px solid #f0f0f0;font-size:11.5px;font-weight:600;letter-spacing:.09em;text-transform:uppercase;color:#a0a0a0}.card>.sub:first-child{border-top:0;padding-top:16px}
.stats{display:grid;grid-template-columns:1fr 1fr;gap:12px;padding:16px 0 8px}.stat{border:1px solid #eee;border-radius:14px;padding:14px 18px;background:#fafafa}.stat b{display:block;font:700 26px/1.1 'Segoe UI Variable Display','Segoe UI',sans-serif;letter-spacing:-.02em}.stat span{display:block;color:#8a8a8a;font-size:13px;margin-top:4px}
.foot{color:#8a8a8a;font-size:13px;padding:0 0 15px;margin:-4px 0 0}
details.info{border-top:1px solid #f0f0f0}details.info summary{list-style:none;cursor:pointer;display:flex;justify-content:space-between;align-items:center;padding:15px 0;font-size:15px;font-weight:500}
details.info summary::-webkit-details-marker{display:none}details.info summary::after{content:'';width:7px;height:7px;border-right:2px solid #aaa;border-bottom:2px solid #aaa;transform:rotate(45deg);margin-right:4px;transition:transform .2s}details.info[open] summary::after{transform:rotate(-135deg)}
details.info .body{padding:0 0 16px;color:#777;font-size:13px;line-height:1.6;max-width:640px}
.sub+.row{border-top:0!important}.chips a.on{background:#111;color:#fff}#ssr{flex:1 0 100%;box-sizing:border-box;height:44px;padding:0 16px 0 44px;border-radius:12px;border:1px solid #e5e5e5;background:#f6f6f6 url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' width='18' height='18' fill='none' stroke='%23888' stroke-width='2' stroke-linecap='round'%3E%3Ccircle cx='8' cy='8' r='6'/%3E%3Cpath d='M13 13l4 4'/%3E%3C/svg%3E") no-repeat 15px center;font-size:15px;outline:none}#ssr:focus{background-color:#fff;border-color:#111}.chips.srch a{display:none}#sno{color:#777;text-align:center;padding:40px 0;font-size:15px}
.foot{padding:14px 0 16px!important;margin:0!important;border-top:1px solid #f0f0f0}
</style>)H";
inline std::wstring SettingsPage(const Settings& s, bool grey, const std::wstring& cloud = L"On", long long blocked = 0, const std::wstring& apps = L"") {
    std::wstring eng = s.Get(L"engine", L"Google"), st = s.Get(L"startup", L"home");
    bool cs = s.Get(L"child_safe", L"0") == L"1";
    auto row = [](const std::wstring& lbl, const std::wstring& sub, const std::wstring& ctl) { return L"<div class=row><div><span class=lbl>" + lbl + L"</span>" + (sub.empty() ? std::wstring() : L"<small>" + sub + L"</small>") + L"</div>" + ctl + L"</div>"; };
    auto sec = [](const wchar_t* id, const wchar_t* title, const std::wstring& body) { return std::wstring(L"<h2 id=\"") + id + L"\">" + title + L"</h2><div class=card>" + body + L"</div>"; };
    auto grp = [](const wchar_t* t) { return std::wstring(L"<div class=sub>") + t + L"</div>"; };
    auto txt = [&](const wchar_t* key, const wchar_t* ph) { return std::wstring(L"<input type=text data-k=") + key + L" placeholder=\"" + ph + L"\" value=\"" + Esc(s.Get(key)) + L"\">"; };
    auto btn = [](const wchar_t* label, const wchar_t* c, const wchar_t* a) { return std::wstring(L"<button data-c=") + c + L" data-a=" + a + L">" + label + L"</button>"; };
    auto info = [](const wchar_t* title, const wchar_t* body) { return std::wstring(L"<details class=info><summary>") + title + L"</summary><div class=body>" + body + L"</div></details>"; };
    auto opt = [](const wchar_t* v, const wchar_t* label, const std::wstring& cur) { return std::wstring(L"<option value=") + v + (cur == v ? L" selected" : L"") + L">" + label + L"</option>"; };
    std::wstring b = std::wstring(kSetCss) + LR"H(<h1>Settings</h1><p class=bnote>Beta: small errors can happen. We improve Libra every day.</p>
<div class=chips><input id=ssr type=search placeholder="Search all settings  ( / )" autocomplete=off><a data-go=s-search>Search</a><a data-go=s-start>Startup</a><a data-go=s-dl>Downloads</a><a data-go=s-priv>Privacy</a><a data-go=s-perf>Performance</a><a data-go=s-ai>Assistant</a><a data-go=s-fam>Family</a><a data-go=s-about>About</a></div>)H";
    // Search
    std::wstring e = L"<select data-k=engine>"; for (auto& k : kEngines) if (!k.ai) e += opt(k.name, k.name, eng); e += L"<optgroup label=\"AI search\">"; for (auto& k : kEngines) if (k.ai) e += opt(k.name, k.name, eng); e += L"</optgroup>" + opt(L"Custom", L"Custom...", eng) + L"</select>";
    b += sec(L"s-search", L"Search", row(LogoHtml(eng == L"Custom" ? L"Custom" : eng) + L"Search engine", L"Used by the address bar and the new tab page", e)
        + row(L"Custom search URL", L"Starts with https:// and ends where the search words go", txt(L"engine_url", L"https://example.com/search?q=")));
    // Startup
    b += sec(L"s-start", L"Startup", row(L"When Libra opens", L"", L"<select data-k=startup>" + opt(L"home", L"New tab page", st) + opt(L"last", L"Continue where I left off", st) + opt(L"url", L"A specific page", st) + L"</select>")
        + row(L"Home page", L"Used with \"A specific page\"", txt(L"homepage", L"https://"))
        + Toggle(s, L"site_dock", L"Sidebar sites", L"A slim bar on the right with your favourite sites (Gmail, WhatsApp...). Click one to open it beside the page")
        + row(L"Sidebar site list", L"Addresses separated by commas. The + under the icons adds the page you are on; right-click an icon to remove it", L"<input type=text data-k=site_list value=\"" + Esc(s.Get(L"site_list", L"https://mail.google.com, https://web.whatsapp.com")) + L"\">"));
    // Downloads
    b += sec(L"s-dl", L"Downloads", row(L"Save to", L"Empty means your Downloads folder", txt(L"dl_dir", L"Downloads folder")) + L"<div class=row><div><span class=lbl>Ask where to save</span><small>Libra always asks. Nothing is downloaded until you press Save.</small></div><span style=\"color:#8a8a8a;font-size:13px;width:236px;text-align:right\">Always on</span></div>"
        + Toggle(s, L"save_video", L"Save videos", L"A quiet icon in the address bar when a page has a video"));
    // Privacy
    std::wstring bd = s.Get(L"block_default", L"0"); std::wstring hk = s.Get(L"history_keep", L"");
    b += sec(L"s-priv", L"Privacy",
        L"<div class=stats><div class=stat><b>" + cloud + L"</b><span>Cloud ad blocking</span></div><div class=stat><b>" + std::to_wstring(blocked) + L"</b><span>ads and trackers blocked</span></div></div>"
        + grp(L"Protection")
        + row(L"Default blocking level", L"For sites you have not set yourself (shield button)", L"<select data-k=block_default>" + opt(L"0", L"Max: block all", bd) + opt(L"1", L"Balanced: some ads", bd) + opt(L"2", L"Off", bd) + L"</select>")
        + ToggleOn(s, L"https_upgrade", L"Upgrade to HTTPS", L"Open the secure version of a site when there is one")
        + ToggleOn(s, L"fingerprint", L"Fingerprint protection", L"Tiny random changes to what sites can read from your device")
        + Toggle(s, L"block_3p", L"Block third-party cookies", L"Off by default: can break some CAPTCHAs and sign-ins. Restart to apply")
        + ToggleOn(s, L"fp_a11y", L"Hide accessibility preferences", L"Sites cannot read your contrast and motion settings (Max level)")
        + Toggle(s, L"tz_utc", L"Hide my time zone", L"Sites see UTC. Times on booking sites will show in UTC")
        + Toggle(s, L"block_recall", L"Block Microsoft Recall", L"Windows cannot take pictures of Libra windows. Screen sharing and recorders will also see a black window")
        + Toggle(s, L"local_block", L"Local ad blocker", L"An extra layer. Open a new tab after changing")
        + grp(L"Permissions")
        + Toggle(s, L"perm_allow", L"Allow camera, microphone and location", L"Off (recommended): Libra asks every time. Motion sensors stay blocked")
        + Toggle(s, L"smartscreen_off", L"Turn off Microsoft SmartScreen", L"More private, less protection: it sends site addresses to Microsoft")
        + ToggleOn(s, L"web_suggest", L"Search suggestions", L"Your search engine suggests words while you type")
        + grp(L"Your data")
        + row(L"Keep my history", L"Older pages are removed from this PC automatically", L"<select data-k=history_keep>" + opt(L"", L"Forever", hk) + opt(L"365", L"1 year", hk) + opt(L"90", L"3 months", hk) + opt(L"30", L"1 month", hk) + opt(L"7", L"1 week", hk) + opt(L"1", L"1 day", hk) + opt(L"0", L"Do not keep", hk) + L"</select>")
        + Toggle(s, L"clear_start", L"Clear site data at startup", L"A fresh start every time. Sign-ins are lost except the sites below")
        + row(L"Stay signed in on", L"Sites whose cookies are kept, separated by commas", txt(L"keep_signin", L"google.com, github.com"))
        + row(L"Browsing data", L"History, cookies, sign-ins, cache and more", btn(L"Clear data...", L"nav", L"privacy"))
        + row(L"Privacy dashboard", L"See what Libra remembers", btn(L"Open", L"nav", L"privacy")));
    // Performance
    std::wstring pf = s.Get(L"perf", L"auto");
    b += sec(L"s-perf", L"Performance", row(L"Performance mode", L"Light mode uses less memory on older PCs. Restart to apply", L"<select data-k=perf>" + opt(L"auto", L"Automatic", pf) + opt(L"light", L"Light (older PCs)", pf) + opt(L"normal", L"Normal", pf) + L"</select>")
        + Toggle(s, L"block_images", L"Data saver", L"Don't load images")
        + ToggleOn(s, L"idle_prompt", L"Ask to close unused tabs", L"After a long time unused, Libra asks first"));
    // Assistant
    std::wstring conn = s.Get(L"agent_model").empty() ? L"Not connected. Open the Assistant to add your API key" : L"Model: " + Esc(s.Get(L"agent_model")) + L". Your key is stored encrypted on this PC";
    b += sec(L"s-ai", L"Assistant",
        Toggle(s, L"agent_on", L"Assistant", L"Give Libra a job such as filling a form. While off, nothing runs")
        + ToggleOn(s, L"agent_btn", L"Toolbar button", L"Show the Assistant button in the toolbar (Ctrl+Shift+Space opens it)")
        + row(L"Connection", conn, L"")
        + Toggle(s, L"autosuggest", L"Auto suggest", L"Offer your saved notes when you click an empty name or e-mail box. Works on this PC only")
        + ToggleOn(s, L"agent_opt", L"Shorten my requests", L"Remove filler words so each job uses fewer tokens")
        + ToggleOn(s, L"agent_vauto", L"Send voice requests automatically", L"Send the text right after you speak")
        + info(L"How the Assistant works",
            L"<b>It can</b> open a site you name, read the page, click, type, choose from lists, fill forms from your notes, summarise pages, use up to four research tabs, upload files from its Files folder, and download files after you say yes.<br><br>"
            L"<b>It never</b> types passwords, card numbers or one-time codes, solves CAPTCHAs (it stops and waits), searches the web by itself, opens Libra pages such as settings or the vault, or sees your saved passwords.<br><br>"
            L"<b>You stay in control.</b> Anything risky (submitting a form, paying, leaving for another site, downloading) opens a Windows question first. Review every answer before a form is sent. Press Stop at any time.<br><br>"
            L"<b>Your data.</b> Your key and notes are encrypted with Windows on this PC. Page text goes only to the provider you chose, never to Psylinks. The Assistant is off in private windows and Child Safety mode.<br><br>"
            L"<b>Honest limits.</b> An AI can make mistakes, and websites may try to trick it with hidden text. It treats page text as data, not orders. Check the review card before you submit anything important.")
        + L"<p class=foot style=\"padding-top:4px\">Beta: the Assistant can make mistakes. Check its work.</p>");
    // Family
    if (!apps.empty()) { b += sec(L"s-apps", L"Apps", apps); size_t cp = b.find(L"<a data-go=s-fam>"); if (cp != std::wstring::npos) b.insert(cp, L"<a data-go=s-apps>Apps</a>"); }
    b += sec(L"s-fam", L"Family", row(L"Child Safety mode", std::wstring(cs ? L"On. Adult, malware and scam sites are blocked. Turning it off needs your Windows password" : L"Blocks adult, malware and scam sites. Needs your Windows password")
        + (s.Get(L"child_nolock", L"0") == L"1" ? L" Your Windows account has no password, so anyone can turn this off." : L""), std::wstring(L"<button data-c=family.toggle>") + (cs ? L"Turn off" : L"Turn on") + L"</button>"));
    // About
    b += sec(L"s-about", L"About", row(L"Libra Browser", L"Version 3.9.8 (beta). Secure, private and lightweight. Built by Psylinks Security Private Limited", L"")
        + info(L"What Libra does", L"Blocks ads and trackers before they load. Upgrades sites to secure https. Blocks popups and downloads you did not ask for. Hides your browser fingerprint. Keeps a password vault and generator. Fast multi-connection downloads. Private windows that leave nothing behind. Light on memory and CPU. We collect nothing about you. The new-tab page shows clearly labelled sponsored items that support Libra. Anonymous counts can be switched off on the Privacy page.")
        + info(L"License", L"Source available: you can read the code. You may not change, share or sell it without written permission from Psylinks Security Private Limited. See the LICENSE file.")
        + row(L"Source code", L"<a href=\"https://github.com/PsylinksSecurityWeb/libra-browser\" style=\"color:inherit\">github.com/PsylinksSecurityWeb/libra-browser</a>", L""));
    return PageShell(L"Settings", b, grey, LR"JS(document.querySelectorAll('.chips a').forEach(a=>a.onclick=()=>{const t=document.getElementById(a.dataset.go);if(t)t.scrollIntoView({behavior:'smooth',block:'start'})});const io=new IntersectionObserver(es=>es.forEach(e=>{if(e.isIntersecting)document.querySelectorAll('.chips a').forEach(a=>a.classList.toggle('on',a.dataset.go===e.target.id))}),{rootMargin:'-70px 0px -70% 0px'});document.querySelectorAll('main h2[id]').forEach(h=>io.observe(h));const q=document.getElementById('ssr'),em=document.createElement('p');em.id='sno';em.style.display='none';em.textContent='No settings match your search.';document.querySelector('main').appendChild(em);function fl(){const v=q.value.trim().toLowerCase(),cs=[...document.querySelectorAll('main .card')];document.querySelector('.chips').classList.toggle('srch',!!v);cs.forEach(c=>{let n=0;[...c.children].forEach(x=>{let m=!v;if(v){if(x.classList.contains('sub'))m=false;else{m=x.textContent.toLowerCase().includes(v);if(x.tagName==='DETAILS')x.open=m}}x.style.display=m?'':'none';if(m&&!x.classList.contains('sub'))n++});const sh=n||!v;c.style.display=sh?'':'none';const h=c.previousElementSibling;if(h&&h.tagName==='H2')h.style.display=sh?'':'none'});em.style.display=v&&!cs.some(c=>c.style.display!=='none')?'':'none'}q.oninput=fl;q.onkeydown=e=>{if(e.key==='Escape'){q.value='';fl()}};document.addEventListener('keydown',e=>{const t=document.activeElement.tagName;if(e.key==='/'&&t!=='INPUT'&&t!=='TEXTAREA'&&t!=='SELECT'){e.preventDefault();q.focus()}});)JS", L"settings");
}

// libra://appearance : colour mode (more look-and-feel options will live here)
inline std::wstring AppearancePage(const Settings& s, bool grey) {
    auto sel = [](const wchar_t* v, const wchar_t* t, const std::wstring& cur) { return std::wstring(L"<option value=") + v + (cur == v ? L" selected" : L"") + L">" + t + L"</option>"; };
    std::wstring ms = s.Get(L"meter_size", L"pill"), mo = s.Get(L"meter_opacity", L"100");
    std::wstring b = std::wstring(kSetCss) + L"<h1>Appearance</h1><p class=bnote>Make Libra look the way you like.</p><h2 id=\"s-look\">Color</h2><div class=card>" + L"<style>#hu{-webkit-appearance:none;appearance:none;height:12px;border-radius:6px}#hu::-webkit-slider-thumb{-webkit-appearance:none;width:20px;height:20px;border-radius:50%;background:#fff;border:2px solid #111;cursor:pointer}</style><div class=row><div><span class=lbl>Color mode</span><small>Pick any colour: the toolbar, tabs, address bar, icons and pages follow it</small></div><div style=\"display:flex;flex-direction:column;gap:10px;align-items:flex-end\"><div id=sw style=\"display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end;max-width:260px\"></div><canvas id=sv width=240 height=140 style=\"border-radius:12px;cursor:crosshair;touch-action:none\"></canvas><input id=hu type=range min=0 max=360 style=\"width:240px;accent-color:#111\"><div style=\"display:flex;gap:8px\"><input id=hx type=text maxlength=7 style=\"width:96px\"><button id=rs class=btn>Default</button></div></div></div>" + L"</div><h2 id=\"s-meter\">PC meter</h2><div class=card>" + Toggle(s, L"meter_on", L"Libra Meter", L"A tiny live meter of how hard your PC is working. It keeps running after you close the browser. Powered by Libra Browser.")
        + L"<div class=row><div><span class=lbl>Size</span><small>Double-click the meter to switch too</small></div><select data-k=meter_size>" + sel(L"pill", L"Compact", ms) + sel(L"card", L"With details", ms) + L"</select></div>"
        + ToggleOn(s, L"meter_dock", L"Hide at the screen edge", L"The meter tucks into the nearest edge as a thin colour strip (green, blue, amber, red = load). Touch the strip with the mouse and it slides out")
        + ToggleOn(s, L"meter_top", L"Always on top", L"Stays above other windows. Drag it anywhere; it snaps to screen edges")
        + L"<div class=row><div><span class=lbl>See-through</span><small>Lower numbers let the screen show through</small></div><select data-k=meter_opacity>" + sel(L"100", L"Solid", mo) + sel(L"85", L"Light", mo) + sel(L"70", L"Glass", mo) + sel(L"50", L"Ghost", mo) + L"</select></div>"
        + Toggle(s, L"meter_start", L"Start with Windows", L"Opens quietly when you sign in. Turn it off here any time")
        + L"<div class=row><div><small>The meter reads Windows counters on this PC only. It sends nothing anywhere. CPU temperature is not shown: Windows does not offer a safe reading.</small></div></div></div>";
    return PageShell(L"Appearance", b, grey, std::wstring(L"window.__acc=\"") + s.Get(L"accent") + L"\";" + LR"JS((()=>{const cv=document.getElementById('sv');if(!cv)return;const g=cv.getContext('2d'),hu=document.getElementById('hu'),hx=document.getElementById('hx'),sw=document.getElementById('sw');let H=0,S=0,V=.4;
const rgb=(h,s,v)=>{const f=n=>{const k=(n+h/60)%6;return v-v*s*Math.max(0,Math.min(k,4-k,1))};return[f(5),f(3),f(1)].map(x=>Math.round(x*255))};
const hex=()=>'#'+rgb(H,S,V).map(x=>x.toString(16).padStart(2,'0')).join('');
function parse(c){const m=/^#([0-9a-f]{6})$/i.exec(c);if(!m)return false;const r=parseInt(m[1].slice(0,2),16)/255,q=parseInt(m[1].slice(2,4),16)/255,b=parseInt(m[1].slice(4),16)/255,mx=Math.max(r,q,b),d=mx-Math.min(r,q,b);V=mx;S=mx?d/mx:0;H=d?(mx==r?((q-b)/d+6)%6:mx==q?(b-r)/d+2:(r-q)/d+4)*60:0;return true}
function draw(){const w=cv.width,h=cv.height;g.fillStyle='hsl('+H+',100%,50%)';g.fillRect(0,0,w,h);let a=g.createLinearGradient(0,0,w,0);a.addColorStop(0,'#fff');a.addColorStop(1,'rgba(255,255,255,0)');g.fillStyle=a;g.fillRect(0,0,w,h);a=g.createLinearGradient(0,0,0,h);a.addColorStop(0,'rgba(0,0,0,0)');a.addColorStop(1,'#000');g.fillStyle=a;g.fillRect(0,0,w,h);const x=S*w,y=(1-V)*h;g.lineWidth=2;g.strokeStyle='#fff';g.beginPath();g.arc(x,y,7,0,6.3);g.stroke();g.lineWidth=1;g.strokeStyle='#000';g.beginPath();g.arc(x,y,8.5,0,6.3);g.stroke();hx.value=hex();hu.value=H}
const save=()=>P('set','accent',hex());let dn=false;
const at=e=>{const r=cv.getBoundingClientRect();S=Math.min(1,Math.max(0,(e.clientX-r.left)/r.width));V=1-Math.min(1,Math.max(0,(e.clientY-r.top)/r.height));draw()};
cv.onpointerdown=e=>{dn=true;at(e);try{cv.setPointerCapture(e.pointerId)}catch(x){}};cv.onpointermove=e=>{if(dn)at(e)};cv.onpointerup=()=>{if(dn){dn=false;save()}};
hu.style.background='linear-gradient(90deg,#f00,#ff0,#0f0,#0ff,#00f,#f0f,#f00)';hu.oninput=()=>{H=+hu.value;draw()};hu.onchange=save;
hx.onchange=()=>{if(parse(hx.value)){draw();save()}};document.getElementById('rs').onclick=()=>P('set','accent','');
['#e5484d','#f76b15','#f5a524','#30a46c','#12a594','#3e7bfa','#6e56cf','#d6409f','#6b7280'].forEach(c=>{const b=document.createElement('button');b.style.cssText='width:24px;height:24px;padding:0;min-width:0;flex:none;border-radius:50%;border:2px solid #fff;outline:1px solid #ddd;cursor:pointer;background:'+c;b.onclick=()=>{parse(c);draw();save()};sw.appendChild(b)});
if(!parse(window.__acc||''))parse('#6b7280');draw()})();
)JS", L"appearance");
}


inline std::wstring HistoryPage(const std::vector<HistoryItem>& h, bool grey) {
    std::wstring b = LR"H(<style>#cm{display:none;position:fixed;inset:0;background:rgba(255,255,255,.78);z-index:9;align-items:center;justify-content:center}#cm .box{background:#fff;border:1px solid #e6e6e6;border-radius:18px;width:460px;max-width:92vw;max-height:90vh;overflow:auto;padding:24px 26px}
#cm h3{font-size:19px;font-weight:800;margin:0 0 4px}#cm p{color:#777;font-size:12.5px;margin:0 0 14px}#cm label.r{display:flex;gap:11px;align-items:flex-start;padding:8px 2px;cursor:pointer}#cm label.r input{margin-top:3px;accent-color:#111}#cm label.r small{display:block;color:#888;font-size:12px}
#cm .bt{display:flex;gap:10px;justify-content:flex-end;margin-top:16px}#cm select{margin:0 0 8px;width:100%}#tst{position:fixed;right:22px;bottom:22px;background:#111;color:#fff;padding:11px 18px;border-radius:12px;font-size:13px;opacity:0;transition:opacity .25s;z-index:10}</style>
<h1>History</h1><div class=top><input type=text id=q placeholder="Search history"><button id=clr>Clear browsing data</button></div>
<div id=cm><div class=box><h3>Clear browsing data</h3><p>Choose what to remove. Bookmarks, saved logins, settings and your API key are never touched.</p>
<select id=cr><option value=hour>Last hour</option><option value=day>Last 24 hours</option><option value=week>Last 7 days</option><option value=month>Last 4 weeks</option><option value=all selected>All time</option></select>
<div style="font-size:12.5px;margin:2px 2px 6px"><a href="#" id=sall style="color:#111">Select all</a> &middot; <a href="#" id=snone style="color:#111">None</a></div>
<label class=r><input class=cb type=checkbox value=hist checked><span>Browsing history<small>Pages you visited</small></span></label>
<label class=r><input class=cb type=checkbox value=cook checked><span>Cookies and sign-in data<small>You will be signed out of most sites</small></span></label>
<label class=r><input class=cb type=checkbox value=cache checked><span>Cached images and files<small>Frees disk space</small></span></label>
<label class=r><input class=cb type=checkbox value=site checked><span>Site data<small>Local storage, databases, offline workers, site permissions</small></span></label>
<label class=r><input class=cb type=checkbox value=dl><span>Download list<small>The list only; your files stay where they are</small></span></label>
<label class=r><input class=cb type=checkbox value=rec><span>Closed tabs<small>The reopen list</small></span></label>
<label class=r><input class=cb type=checkbox value=dns><span>Lookup memory<small>Names of the sites Libra looked up</small></span></label>
<label class=r><input class=cb type=checkbox value=ctr><span>Block counters<small>The totals on the shield</small></span></label>
<label class=r><input class=cb type=checkbox value=agt><span>AI Agent data<small>Memory notes, permissions and files. Your API key stays.</small></span></label>
<div class=bt><button id=cx>Cancel</button><button class=pri id=cgo>Clear data</button></div></div></div><div id=tst></div><div class="card hv">)H";
    int shown = 0, lastDay = -1;
    for (int i = (int)h.size() - 1; i >= 0 && shown < 500; --i, ++shown) {
        wchar_t tm[32] = L""; struct tm lt; time_t t = (time_t)h[i].t;
        if (!localtime_s(&lt, &t)) wcsftime(tm, 32, L"%H:%M", &lt);
        { static time_t nowT = time(nullptr); struct tm nt, yt; time_t yT = nowT - 86400; localtime_s(&nt, &nowT); localtime_s(&yt, &yT); int key = lt.tm_year * 400 + lt.tm_yday;
          if (key != lastDay) { lastDay = key; wchar_t dn[48] = L""; if (lt.tm_year == nt.tm_year && lt.tm_yday == nt.tm_yday) wcscpy_s(dn, L"Today"); else if (lt.tm_year == yt.tm_year && lt.tm_yday == yt.tm_yday) wcscpy_s(dn, L"Yesterday"); else wcsftime(dn, 48, L"%A, %d %B", &lt); b += std::wstring(L"<div class=day>") + dn + L"</div>"; } }
        b += L"<div class=item data-t=" + std::to_wstring(h[i].t) + L"><div class=t><a href=\"" + Esc(h[i].url) + L"\">" + Esc(h[i].title.empty() ? h[i].url : h[i].title) + L"</a><small>" + Esc(h[i].url) + L" &middot; " + tm + L"</small></div><button data-c=hist.del data-a=" + std::to_wstring(i) + L">Remove</button></div>";
    }
    if (!shown) b += L"<div class=empty><b>No history yet</b>Pages you visit show up here.</div>";
    b += L"</div>";
    return PageShell(L"History", b, grey, LR"JS(function fixDays(){document.querySelectorAll('.day').forEach(d=>{let n=d.nextElementSibling,v=false;while(n&&!n.classList.contains('day')){if(n.style.display!=='none')v=true;n=n.nextElementSibling}d.style.display=v?'':'none'})}
q.oninput=()=>{const v=q.value.toLowerCase();document.querySelectorAll('.item').forEach(i=>i.style.display=i.textContent.toLowerCase().includes(v)?'':'none');fixDays()};
const cm=document.getElementById('cm'),cgo=document.getElementById('cgo');
document.getElementById('clr').onclick=()=>{cm.style.display='flex'};document.getElementById('cx').onclick=()=>{cm.style.display='none'};
cm.onclick=e=>{if(e.target===cm)cm.style.display='none'};document.addEventListener('keydown',e=>{if(e.key==='Escape')cm.style.display='none'});
cgo.onclick=()=>{const ks=[...cm.querySelectorAll('input:checked')].map(i=>i.value);if(!ks.length){return}cgo.disabled=true;cgo.textContent='Clearing...';P('priv.clear',ks.join(','),document.getElementById('cr').value)};
document.getElementById('sall').onclick=e=>{e.preventDefault();cm.querySelectorAll('input[type=checkbox]').forEach(i=>i.checked=true)};document.getElementById('snone').onclick=e=>{e.preventDefault();cm.querySelectorAll('input[type=checkbox]').forEach(i=>i.checked=false)};
window.lbDone=(since,h,msg)=>{if(h){document.querySelectorAll('.item').forEach(i=>{if(!since||+i.dataset.t>=since)i.remove()});fixDays();if(!document.querySelector('.item')){const c=document.querySelector('main .card:last-of-type');if(c)c.innerHTML='<div class=empty><b>No history yet</b>Pages you visit show up here.</div>'}}
cm.style.display='none';cgo.disabled=false;cgo.textContent='Clear data';const t=document.getElementById('tst');t.textContent=msg||'Cleared';t.style.opacity=1;setTimeout(()=>t.style.opacity=0,msg&&msg.length>40?6000:2200)})JS", L"history");
}

inline std::wstring BookmarksPage(const std::vector<BookmarkItem>& m, bool grey) {
    std::wstring b = LR"H(<style>
.card{padding:2px 22px}.card .row{padding:15px 0;gap:28px;min-height:62px;border-top:1px solid #f0f0f0;align-items:center}.card .row:first-child{border-top:0}
.card .row>div:first-child{flex:1;min-width:0}.card .row small{max-width:none;margin-top:2px;font-size:13px;line-height:1.45}.lbl{font-size:15px;font-weight:500;display:block}
.card .row input[type=password]{width:236px;min-width:0;flex-shrink:0}.ctl{display:flex;gap:8px;flex-shrink:0}.ctl button{min-width:0;padding:9px 16px}
main h2{margin:30px 0 10px 4px}#impbox:empty{display:none}#impbox{border:1px solid #eee;border-radius:14px;padding:14px 18px;margin:0 0 4px;font-size:13px;line-height:1.5;background:#fafafa}#impbox button{margin-top:10px}
.item{padding:14px 0;border-top:1px solid #f0f0f0}.card>.item:first-child{border-top:0}
</style><h1>Bookmarks</h1><div id=impbox></div>
<h2>Import</h2><div class=card><div class=row><div><span class=lbl>Import bookmarks</span><small>From another browser on this PC, or a file. You see a preview first</small></div><div class=ctl><button data-c=bm.imp data-a=chrome>Chrome</button><button data-c=bm.imp data-a=edge>Edge</button><button data-c=bm.imp data-a=brave>Brave</button><button data-c=bm.imp data-a=firefox>Firefox</button><button data-c=bm.imp data-a=file>File...</button></div></div></div>
<h2>Backup</h2><div class=card><div class=row><div><span class=lbl>Passphrase</span><small>8 or more characters. It is never saved and can not be recovered</small></div><input type=password id=bkp placeholder="Passphrase"></div>
<div class=row><div><span class=lbl>Encrypted backup</span><small>Bookmarks and settings in one file. Never passwords or your assistant key</small></div><div class=ctl><button id=bkx>Export</button><button id=bki>Restore</button></div></div></div>
<h2>Your bookmarks</h2><div class="card hv">)H";
    for (int i = 0; i < (int)m.size(); ++i)
        b += L"<div class=item><div class=t><a href=\"" + Esc(m[i].url) + L"\">" + Esc(m[i].title.empty() ? m[i].url : m[i].title) + L"</a><small>" + Esc(m[i].url) + L"</small></div><button data-c=bm.del data-a=" + std::to_wstring(i) + L">Remove</button></div>";
    if (m.empty()) b += L"<div class=empty><b>No bookmarks yet</b>Press Ctrl+D or click the star in the address bar.</div>";
    return PageShell(L"Bookmarks", b + L"</div>", grey, LR"JS(function lbImp(s){const x=document.getElementById('impbox');x.textContent='';if(s.clear)return;const p=document.createElement('div');p.style.cssText='padding:2px 0 14px;font-size:13px;line-height:1.5';
if(s.err||s.info){p.textContent=s.err||s.info;x.appendChild(p);return}
p.textContent='Found '+s.found+' bookmarks in '+s.src+(s.dup?' ('+s.dup+' already here)':'')+(s.drop?', '+s.drop+' skipped (not web addresses)':'')+(s.sets?' and '+s.sets+' settings (they apply after a restart)':'')+'. '+s.n+' bookmarks will be added.';x.appendChild(p);
if(s.sample.length){const u=document.createElement('div');u.style.cssText='color:var(--mute);font-size:12px;margin-bottom:10px';u.textContent='For example: '+s.sample.join(' | ');x.appendChild(u)}
if(s.n>0||s.sets>0){const b=document.createElement('button');b.className='pri';b.textContent='Import '+s.n+(s.sets?' + settings':'');b.dataset.c='bm.impgo';x.appendChild(b)}
const c=document.createElement('button');c.textContent='Cancel';c.dataset.c='bm.impno';c.style.marginLeft='8px';x.appendChild(c)}
const bk=m=>{const p=document.getElementById('bkp').value;if(p.length<8){lbImp({err:'Choose a passphrase with at least 8 characters first.'});return}P(m,p);document.getElementById('bkp').value=''};
document.getElementById('bkx').onclick=()=>bk('bm.bkexp');document.getElementById('bki').onclick=()=>bk('bm.bkimp');)JS", L"bookmarks");
}

inline std::wstring DownloadsPage(const std::vector<DownloadItem>& d, bool grey) {
    std::wstring b = LR"H(<style>
.dh{display:none;align-items:baseline;gap:12px;margin:0 0 20px}.dh #tsp{font:800 56px 'Segoe UI Variable Display','Segoe UI',sans-serif;letter-spacing:-.03em;font-variant-numeric:tabular-nums;line-height:1}.dh small{color:var(--mute);font-size:15px}
.dr{position:relative;display:flex;align-items:center;gap:16px;padding:17px 20px;margin:9px 0;border-radius:18px;background:#fff;overflow:hidden;box-shadow:none}
.dr .fill{position:absolute;inset:0 auto 0 0;width:0;background:linear-gradient(90deg,rgba(17,17,17,.06),rgba(17,17,17,.13));transition:width .26s linear}
.dr>*{position:relative}.dr .t{flex:1;min-width:0}.dr .t b{display:block;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.dr .t small{color:var(--mute);font-variant-numeric:tabular-nums}
.dr .v{font:700 22px 'Segoe UI Variable Display','Segoe UI',sans-serif;min-width:118px;text-align:right;font-variant-numeric:tabular-nums}
.dr button{padding:8px 16px}.dr button.g{background:none;color:var(--mute)}
.dr{border-radius:0;background:transparent;margin:0;padding:14px 22px;gap:14px;border-top:1px solid #f0f0f0}.card>.dr:first-child{border-top:0}.dr.held{background:#fffbf2}.dr.held .t small{white-space:normal}
.dr .ft{flex:0 0 40px;width:40px;height:40px;border-radius:11px;display:flex;align-items:center;justify-content:center;font:700 11px/1 'Segoe UI',sans-serif;letter-spacing:.03em;font-style:normal}
.dr button{padding:7px 14px;font-size:13px}.dr .t b{font-weight:500;font-size:15px}.dr .t small{font-size:13px}
.card.dls{padding:0;overflow:hidden}
</style><h1>Downloads</h1><div class=dh id=dh><span id=tsp>0</span><small id=tsm></small></div><div class="card dls" id=dls>)H";
    bool active = false, opActive = false;
    for (int i = (int)d.size() - 1; i >= 0; --i) {
        auto& x = d[i]; std::wstring a = L" data-a=" + std::to_wstring(i), id = std::to_wstring(i);
        if (x.state == 0) { active = true; if (!x.job) opActive = true; }
        int pc = x.state == 1 ? 100 : (x.total > 0 ? (int)(x.got * 100 / x.total) : 0);
        std::wstring info = x.state == 0 ? Fmt(x.got) + (x.total > 0 ? L" of " + Fmt(x.total) : L"") : x.state == 1 ? Fmt(x.total > 0 ? x.total : x.got) :
                            x.hold == 3 ? L"Removed by Windows Security" : x.canRetry ? L"Interrupted at " + Fmt(x.got) + (x.total > 0 ? L" of " + Fmt(x.total) : L"") : L"Stopped";
        if (x.hold == 1) info = L"Checking with Windows Security...";
        else if (x.hold == 2) info = L"Held for your safety. " + x.scanNote;
        b += L"<div class=\"dr" + std::wstring(x.state == 1 ? L" ok" : x.state == 2 ? L" bad" : L"") + std::wstring(x.state == 1 && x.hold == 2 ? L" held" : L"") + L"\" data-i=" + id + L" data-st=" + std::to_wstring(x.state) + L"><i class=fill style=\"width:" + std::to_wstring(x.state == 0 ? pc : 0) + L"%\"></i>" +
             L"<div class=t><b>" + Esc(x.name.empty() ? L"(starting)" : x.name) + L"</b><small class=inf>" + info + L"</small></div>";
        if (x.state == 0) b += L"<div class=v><span class=sp>&middot;&middot;&middot;</span></div><button class=g data-c=dl.cancel" + a + L">Cancel</button>";
        else if (x.state == 1 && x.hold == 1) b += L"<div class=v><span class=sp>&middot;&middot;&middot;</span></div>";
        else if (x.state == 1 && x.hold == 2) b += L"<button class=pri data-c=dl.release" + a + L">Release</button>" + std::wstring(WinSandboxExe().empty() ? L"" : L"<button data-c=dl.sandbox" + a + L">Open in Sandbox</button>") + L"<button class=g data-c=dl.show" + a + L">Folder</button><button class=g data-c=dl.delhold" + a + L">Delete</button>";
        else if (x.state == 1) b += L"<button data-c=dl.open" + a + L">Open</button><button class=g data-c=dl.show" + a + L">Folder</button><button class=g data-c=dl.remove" + a + L">Remove</button>";
        else b += (x.canRetry ? L"<button class=pri data-c=dl.retry" + a + L">Retry</button>" : std::wstring(L"")) + L"<button class=g data-c=dl.remove" + a + L">Remove</button>";
        b += L"</div>";
    }
    if (d.empty()) b += L"<div class=empty><b>No downloads yet</b>Files you download appear here.</div>";
    b += L"</div>";
    std::wstring js = LR"H(
function ft(){document.querySelectorAll('.dr').forEach(r=>{if(r.querySelector('.ft'))return;const n=(r.querySelector('.t b').textContent.match(/\.([A-Za-z0-9]{1,4})$/)||[,'FILE'])[1].toUpperCase();let h=0;for(const c of n)h=(h*31+c.charCodeAt(0))%360;const i=document.createElement('i');i.className='ft';i.textContent=n;i.style.background='hsl('+h+',70%,94%)';i.style.color='hsl('+h+',55%,34%)';r.insertBefore(i,r.querySelector('.t'))})}ft();
function sz(v){const U=['B','KB','MB','GB'];let i=0;while(v>=1024&&i<3){v/=1024;i++}return (i<2?Math.round(v):v.toFixed(i==3?2:1))+' '+U[i]}
function spd(v){return v>=1048576?[(v/1048576).toFixed(1),'MB/s']:[Math.round(v/1024),'KB/s']}
function eta(s){s=Math.round(s);return s<60?s+' s left':s<3600?Math.round(s/60)+' min left':(s/3600).toFixed(1)+' h left'}
function lbDl(a){let tot=0,n=0,rel=false;
 a.forEach(x=>{const r=document.querySelector('.dr[data-i="'+x.i+'"]');if(!r){rel=true;return}if(+r.dataset.st!==x.s){rel=true;return}
  if(x.s!==0)return;n++;tot+=x.v;r.querySelector('.fill').style.width=(x.t>0?100*x.g/x.t:0)+'%';
  r.querySelector('.inf').textContent=sz(x.g)+(x.t>0?' of '+sz(x.t):'')+(x.v>0&&x.t>0?' \u00b7 '+eta((x.t-x.g)/x.v):'');
  const s=spd(x.v);r.querySelector('.sp').innerHTML=x.v>0?s[0]+' <small style="font-size:12px;color:var(--mute)">'+s[1]+'</small>':'\u00b7\u00b7\u00b7'});
 const h=document.getElementById('dh');h.style.display=n?'flex':'none';if(n){const s=spd(tot);document.getElementById('tsp').textContent=s[0];document.getElementById('tsm').textContent=s[1]+(n>1?' \u00b7 '+n+' downloads':'')}
 if(rel)P('dl.refresh','')}
)H";
    return PageShell(L"Downloads", b, grey, js, L"downloads");
}

inline std::wstring WelcomePage(bool grey, bool isDefault = false, bool resume = false, const std::wstring& imp = L"") {
    auto tiles = [&](bool ai) {
        std::wstring c; int n = 0;
        for (auto& e : kEngines) {
            if (e.ai != ai) continue;
            c += std::wstring(L"<button class=tile style=\"animation-delay:") + std::to_wstring(60 + n++ * 45) + L"ms\" data-c=engine data-a=\"" + e.name + L"\">" + LogoHtml(e.name) + L"<span>" + e.name + L"</span></button>";
        }
        return c;
    };
    std::wstring b = LR"HTML(<style>
main{max-width:none}
.wel{max-width:640px;margin:0 auto;padding-top:11vh;text-align:center}
.wel h1{font-size:40px;font-weight:300;letter-spacing:.36em;text-transform:uppercase;margin:0 0 10px .36em}
.sub{color:var(--mute);font-size:15px;margin-bottom:26px}
.panel{display:grid;grid-template-columns:repeat(4,1fr);gap:12px;animation:rise .4s both}
.panel[hidden]{display:none}
.tile{display:flex;flex-direction:column;align-items:center;gap:12px;padding:22px 8px 16px;background:var(--card);border-radius:20px;font-weight:600;font-size:13px;
  box-shadow:none;animation:rise .5s cubic-bezier(.2,.7,.2,1) both;transition:transform .18s,box-shadow .18s,background .18s,color .18s}
.tile:hover{transform:translateY(-4px);box-shadow:none;background:#111;color:#fff}
.tile .lg{width:30px;height:30px;margin:0}.tile .lgl{width:30px;height:30px;font-size:15px;margin:0}.tile:hover .lgl{background:#333;color:#fff}
.fine{color:var(--mute);font-size:12px;margin-top:30px}
.done{position:fixed;inset:0;display:none;flex-direction:column;align-items:center;justify-content:center;background:var(--bg);cursor:pointer;text-align:center;z-index:20}
body.fin .done{display:flex;animation:fade .45s both}body.fin .wel{display:none}
.thumb{width:76px;height:76px;fill:none;stroke:#111;stroke-width:1.3;stroke-linecap:round;stroke-linejoin:round;margin-bottom:22px}
.thumb path,.thumb circle{stroke-dasharray:1;stroke-dashoffset:1;animation:draw .9s .15s cubic-bezier(.4,.1,.2,1) forwards}.thumb path+path{animation-delay:.05s;animation-duration:1.1s}
@keyframes draw{to{stroke-dashoffset:0}}
.dflt h2,.dflt p,.dflt .btns{opacity:0;animation:fade .7s .7s both}.dflt p{animation-delay:.9s}.dflt .btns{animation-delay:1.1s}.dflt .btns button.g{background:none;color:#666}
.hero{position:fixed;inset:0;z-index:60;background:#fff;overflow:hidden;cursor:pointer;transition:opacity .6s}.hero.out{opacity:0;pointer-events:none}
.h2{position:fixed;inset:0;z-index:59;background:#fff;overflow:hidden;cursor:pointer;transition:opacity .6s}.h2.out{opacity:0;pointer-events:none}.h2 .st{position:absolute;left:50%;bottom:0;width:1344px;height:768px;margin-left:-672px;transform-origin:50% 100%;background:#fff}
.h2 video{position:absolute;left:0;top:0;width:1344px;height:768px;}
.h2 .tx{position:absolute;left:0;right:0;top:300px;text-align:center;font:800 56px/1.15 'Segoe UI Variable Display','Segoe UI',sans-serif;color:#111;opacity:0;transform:translate3d(0,26px,0)}
.h2.txt .tx{opacity:1;transform:none;transition:opacity .9s ease-out,transform 1s cubic-bezier(.2,.7,.2,1)}
.h3{position:fixed;inset:0;z-index:58;background:#fff;overflow:hidden;cursor:pointer;transition:opacity .6s}.h3.out{opacity:0;pointer-events:none}.h3 .st{position:absolute;left:50%;bottom:0;width:1280px;height:720px;margin-left:-640px;transform-origin:50% 100%;background:#fff}
.h3 video{position:absolute;left:0;top:0;width:1280px;height:720px;}
.h3 .tx{position:absolute;left:0;right:0;top:300px;text-align:center;font:800 56px/1.15 'Segoe UI Variable Display','Segoe UI',sans-serif;color:#111;opacity:0;transform:translate3d(0,26px,0)}.h3 .tx b{background:#d6141c;color:#fff;padding:0 16px;display:inline-block}
.h3.txt .tx{opacity:1;transform:none;transition:opacity .9s ease-out,transform 1s cubic-bezier(.2,.7,.2,1)}
.hero .st{position:absolute;left:50%;bottom:0;width:1344px;height:768px;margin-left:-672px;transform-origin:50% 100%;background:#fff}
.hero video{position:absolute;left:768px;top:0;width:576px;height:768px;display:block;}
.hero img{position:absolute;left:252px;top:200px;width:518px;height:330px;opacity:0;transform:translate3d(-70px,0,0);}
.hero.txt img{opacity:1;transform:none;transition:opacity 1s ease-out,transform 1.1s cubic-bezier(.2,.7,.2,1)}
.done .halo{margin-bottom:26px}.done .halo b{font-size:0}.done .halo b .lg{width:40px;height:40px;margin:0}.done .halo b .lgl{width:40px;height:40px;font-size:20px;margin:0}
.done h2{opacity:0;animation:fade .7s .8s both;font:300 24px 'Segoe UI Variable Text','Segoe UI',sans-serif;letter-spacing:.04em;text-transform:none;color:var(--fg);margin:0 0 14px}
.done .by{font-size:13px;color:var(--mute);letter-spacing:.06em;opacity:0;animation:fade .8s .55s both}
.done .by b{display:block;margin-top:6px;font-size:17px;letter-spacing:.18em;text-transform:uppercase;background:linear-gradient(90deg,#111,#7c5cff,#38bdf8,#111);background-size:200% 100%;-webkit-background-clip:text;background-clip:text;-webkit-text-fill-color:transparent;animation:shine 3s linear infinite}
.done .go{margin-top:30px;font-size:12px;color:var(--mute);opacity:0;animation:fade .6s 1.3s both}
.dflt{position:fixed;inset:0;display:none;flex-direction:column;align-items:center;justify-content:center;background:var(--bg);text-align:center;z-index:21;gap:14px}
body.ask .dflt{display:flex;animation:fade .4s both}body.ask .wel,body.fin .dflt{display:none}.dflt h2{font:300 26px 'Segoe UI Variable Text','Segoe UI',sans-serif;letter-spacing:.02em;text-transform:none;color:var(--fg);margin:0}.dflt .halo{margin-bottom:8px}.dflt .btns{display:flex;gap:10px;margin-top:12px}.dflt .btns button{padding:12px 26px;font-weight:600}
/* setup steps = same white stage + heavy type as the animation scenes */
html,body{background:#fff!important}
.wel{max-width:900px;padding-top:9vh}
.wel h1{font:800 56px/1.1 'Segoe UI Variable Display','Segoe UI',sans-serif;letter-spacing:0;text-transform:none;margin:0 0 34px;animation:rise .9s cubic-bezier(.2,.7,.2,1) both}
.sub{color:#555;font-size:16px;line-height:1.5}.fine{color:#666;font-size:13px;margin-top:34px}
.wait{display:none;max-width:430px;color:#555;font-size:16px;line-height:1.5;margin:0}.wait i{display:inline-block;width:6px;height:6px;border-radius:50%;background:#111;margin-left:6px;animation:pulse 1s ease-in-out infinite}@keyframes pulse{50%{opacity:.2}}
.dflt.w .wait{display:block}.dflt p.wait{animation-delay:.05s;animation-duration:.4s}.dflt.w .sub,.dflt.w .btns{display:none}
.scr{position:fixed;inset:0;display:none;flex-direction:column;align-items:center;justify-content:center;background:#fff;text-align:center;z-index:21;gap:14px}
body.prot .sp,body.imp .si{display:flex;animation:fade .4s both}body.prot .wel,body.imp .wel{display:none}body.fin .scr,body.fin .pd{display:none}
.scr h2{font:800 40px/1.15 'Segoe UI Variable Display','Segoe UI',sans-serif;letter-spacing:0;text-transform:none;color:#111;margin:0 0 6px}
.scr .btns{display:flex;gap:10px;margin-top:14px;flex-wrap:wrap;justify-content:center}.scr .btns button{padding:12px 26px;font-weight:600}.scr .btns button.g{background:none;color:#666}
.pts{display:flex;flex-direction:column;gap:18px;max-width:520px;text-align:left;margin-top:8px}.pt{display:flex;gap:16px;align-items:flex-start;color:#555;font-size:15px;line-height:1.5}.pt b{color:#111;font-weight:600}
.pt svg{flex:none;width:26px;height:26px;fill:none;stroke:#111;stroke-width:1.4;stroke-linecap:round;stroke-linejoin:round;margin-top:1px}
.pd{position:fixed;bottom:34px;left:0;right:0;display:flex;justify-content:center;gap:8px;z-index:30;pointer-events:none}.pd i{width:6px;height:6px;border-radius:3px;background:#d4d4d4;transition:all .3s}.pd i.on{width:22px;background:#111}
.tile:focus-visible,.btns button:focus-visible{outline:2px solid #111;outline-offset:3px}
@media(prefers-reduced-motion:reduce){*{animation-duration:.01ms!important;animation-delay:0s!important;transition-duration:.01ms!important}}
.panel{grid-template-columns:repeat(6,1fr);gap:6px;animation:none}.panel#p1{grid-template-columns:repeat(2,minmax(0,150px));justify-content:center;margin-top:6px}
.cap2{color:#666;font-size:12px;letter-spacing:.14em;text-transform:uppercase;margin:34px 0 10px;animation:rise 1s .5s both}
.tile{position:relative;background:transparent;box-shadow:none;border-radius:18px;padding:20px 6px 14px;font-weight:600;transition:transform .2s,background .2s,opacity .35s}
.tile:hover{transform:translateY(-4px);background:#f3f3f3;color:#111}.tile:hover .lgl{background:#111;color:#fff}
.tile .lg{width:42px;height:42px}.tile .lgl{width:42px;height:42px;font-size:20px}
.tile::after{content:"";position:absolute;left:22%;right:22%;bottom:4px;height:2px;background:#d6141c;transform:scaleX(0);transition:transform .5s cubic-bezier(.2,.7,.2,1)}
.wel.pick .tile{opacity:.18;pointer-events:none}.wel.pick .tile.sel{opacity:1;transform:scale(1.08)}.wel.pick .tile.sel::after{transform:scaleX(1)}
.dflt,.done{background:#fff}
.dflt h2,.done h2{font:800 44px/1.15 'Segoe UI Variable Display','Segoe UI',sans-serif;letter-spacing:0;color:#111}
@media(max-width:700px){.panel{grid-template-columns:repeat(3,1fr)}.wel h1{font-size:38px}}
@keyframes fade{from{opacity:0}to{opacity:1}}@keyframes shine{to{background-position:200% 0}}
@media(max-width:560px){.panel{grid-template-columns:repeat(2,1fr)}}
</style>
<div class=hero id=hero><div class=st id=hst><video id=hv muted playsinline preload=auto src="https://libra-hero.local/op.mp4"></video><img id=hi alt="" src="https://libra-hero.local/text.png"></div></div>
<div class=h2 id=h2><div class=st id=h2s><video id=h2v muted playsinline preload=none data-s="https://libra-hero.local/op2.mp4"></video><div class=tx>Eliminate security risk<br>from day one</div></div></div>
<div class=h3 id=h3><div class=st id=h3s><video id=h3v muted playsinline preload=none data-s="https://libra-hero.local/op3.mp4"></video><div class=tx>Eliminated<br>privacy breakers</div></div></div>
<div class=wel><h1>Choose your search.</h1>
<div class=panel id=p0>)HTML" + tiles(false) + L"</div><div class=panel id=p1 hidden>" + tiles(true) + LR"HTML(</div>
<p class=fine>You can change this any time in Settings.</p></div>
<div class=dflt><svg class=thumb viewBox="0 0 24 24"><path pathLength=1 d="M5 4h14a3 3 0 0 1 3 3v10a3 3 0 0 1-3 3H5a3 3 0 0 1-3-3V7a3 3 0 0 1 3-3z"/><path pathLength=1 d="M2 9.5h20"/><path pathLength=1 d="M5.2 6.8h.01M7.6 6.8h.01"/><path pathLength=1 d="M11 6.8h8"/><path pathLength=1 d="M6 13.5h7"/><path pathLength=1 d="M6 16.5h4.5"/></svg><h2 id=dh>Make Libra Browser your default browser?</h2><p class=sub style="max-width:430px;margin:0">Links from other apps will open here. Windows will ask you to confirm the choice.</p><p class=wait id=dwait>Windows opened Settings. Choose Libra Browser there, then come back.<i></i></p><div class=btns><button class=pri data-c=default.set id=dset>Set as default</button><button id=dskip class=g>Not now</button><button id=dcont class=g style="display:none">Continue</button></div></div>
<div class="scr sp"><h2>Protected from the first click</h2><div class=pts>
<div class=pt><svg viewBox="0 0 24 24"><path d="M12 2.5 4.5 5.4v5.7c0 4.6 3.1 8.5 7.5 10.4 4.4-1.9 7.5-5.8 7.5-10.4V5.4L12 2.5z"/><path d="m9 12 2.2 2.2L15.2 10"/></svg><span><b>Ads and trackers are blocked in the cloud.</b> Maximum protection is on. Change any site with the shield button.</span></div>
<div class=pt><svg viewBox="0 0 24 24"><path d="M7 11V7a5 5 0 0 1 10 0v4"/><path d="M6 11h12a2 2 0 0 1 2 2v7a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2v-7a2 2 0 0 1 2-2Z"/></svg><span><b>Your data stays on this PC.</b> History, bookmarks and passwords are stored encrypted.</span></div>
<div class=pt><svg viewBox="0 0 24 24"><circle cx="12" cy="12" r="9.5"/><path d="M12 11v5.5"/><path d="M12 7.6h.01"/></svg><span><b>Sponsored tiles are labelled.</b> They appear on the new-tab page. Libra Browser sends only anonymous counts, never who you are: see Privacy, What Libra sends.</span></div></div>
<div class=btns><button class=pri id=pgo>Continue</button></div></div>
<div class="scr si"><svg class=thumb viewBox="0 0 24 24"><path pathLength=1 d="M6 3h12a1 1 0 0 1 1 1v17l-7-4.5L5 21V4a1 1 0 0 1 1-1z"/></svg><h2 id=ih>Bring your bookmarks?</h2><p class=sub id=isub style="max-width:430px;margin:0">Libra Browser reads the bookmarks file once, on this PC only. Nothing is sent anywhere.</p><div class=btns id=ib></div></div>
<div class=pd aria-hidden=true><i class=on></i><i></i><i></i></div>
<div class=done id=done><svg class=thumb viewBox="0 0 24 24" pathLength=1><path pathLength=1 d="M7 10v12"/><path pathLength=1 d="M15 5.88 14 10h5.83a2 2 0 0 1 1.92 2.56l-2.33 8A2 2 0 0 1 17.5 22H4a2 2 0 0 1-2-2v-8a2 2 0 0 1 2-2h2.76a2 2 0 0 0 1.79-1.11L12 2a3.13 3.13 0 0 1 3 3.88Z"/></svg><h2>You're all set</h2></div>)HTML";
    std::wstring js = LR"HTML(
const ASKDEF=)HTML" + std::wstring(isDefault ? L"0" : L"1") + LR"HTML(;
const IMP=")HTML" + imp + LR"HTML(".split(',').filter(Boolean);
const RESUME=)HTML" + std::wstring(resume ? L"1" : L"0") + LR"HTML(;
function pd(n){document.querySelectorAll('.pd i').forEach((d,k)=>d.classList.toggle('on',k===n))}
function prot(){document.body.classList.remove('ask');document.body.classList.add('prot');pd(2)}
function fin(){document.body.classList.remove('ask','prot','imp');document.body.classList.add('fin');setTimeout(()=>P('welcome.done',''),1900)}
document.getElementById('p1').hidden=false;document.getElementById('p1').insertAdjacentHTML('beforebegin','<p class=cap2>AI assistants</p>');
document.querySelectorAll('.tile').forEach(t=>t.addEventListener('click',()=>{
  t.classList.add('sel');document.querySelector('.wel').classList.add('pick');setTimeout(()=>{if(ASKDEF){document.body.classList.add('ask');pd(1)}else prot()},700)}));
document.getElementById('dset').addEventListener('click',()=>{const d=document.querySelector('.dflt');d.classList.add('w');document.getElementById('dh').textContent='Choose Libra Browser in Windows';let lost=0;addEventListener('blur',()=>{lost=1});addEventListener('focus',()=>{if(lost){lost=0;P('default.chk','')}});setTimeout(()=>{const c=document.getElementById('dcont');c.style.display='';d.querySelector('.btns').style.display='flex'},4000)});
window.lbDef=ok=>{const d=document.querySelector('.dflt');d.classList.remove('w');d.querySelector('.btns').style.display='none';document.getElementById('dh').textContent=ok?'Libra Browser is your default':'Not set yet';d.querySelector('.sub').textContent=ok?'Links from other apps will open here.':'You can change this any time in Settings.';setTimeout(prot,1700)};
document.getElementById('dcont').addEventListener('click',prot);
document.getElementById('pgo').addEventListener('click',()=>{if(IMP.length){document.body.classList.remove('prot');document.body.classList.add('imp')}else fin()});
(function(){const nm={chrome:'Chrome',edge:'Edge',brave:'Brave',firefox:'Firefox'},ib=document.getElementById('ib');
IMP.forEach(k=>{if(!nm[k])return;const b=document.createElement('button');b.className='pri';b.textContent='Import from '+nm[k];b.setAttribute('data-c','onb.imp');b.setAttribute('data-a',k);ib.appendChild(b)});
const n=document.createElement('button');n.className='g';n.id='ino';n.textContent='Not now';n.addEventListener('click',fin);ib.appendChild(n);
window.lbImp=d=>{const sub=document.getElementById('isub'),hd=document.getElementById('ih');
 if(d.err||d.n===0){sub.textContent=d.err?'No bookmarks were found there. You can import later from the Bookmarks page.':'Everything in that file is already here.';document.getElementById('ino').textContent='Continue';return}
 if(d.done!==undefined){hd.textContent='Imported '+d.done+' bookmark'+(d.done==1?'':'s');sub.textContent='';ib.style.display='none';setTimeout(fin,1500)}};
ib.addEventListener('click',e=>{if(e.target.dataset&&e.target.dataset.c==='onb.imp'){document.getElementById('isub').textContent='Importing...'}})})();document.getElementById('dskip').addEventListener('click',prot);
function go3(){const h=document.getElementById('h3'),v=document.getElementById('h3v'),st=document.getElementById('h3s');let done=0,tx=0;
function fit(){const k=Math.min(innerWidth/1280,innerHeight/720);st.style.transform='scale('+k+')'}fit();addEventListener('resize',fit);
function txt(){if(!tx){tx=1;h.classList.add('txt')}}
function end(){if(done)return;done=1;try{v.pause();v.removeAttribute('src');v.load()}catch(e){}h.classList.add('out');setTimeout(()=>h.remove(),650);}
v.addEventListener('timeupdate',()=>{if(v.currentTime>=4.6)txt()});v.addEventListener('ended',()=>{txt();setTimeout(end,1200)});v.addEventListener('error',end);
v.__lbOk=true;if(v.dataset.s)v.src=v.dataset.s;const p=v.play();if(p&&p.catch)p.catch(end);setTimeout(end,16000)}
function go2(){const h=document.getElementById('h2'),v=document.getElementById('h2v'),st=document.getElementById('h2s');let done=0,tx=0;
function fit(){const k=Math.min(innerWidth/1344,innerHeight/768);st.style.transform='scale('+k+')'}fit();addEventListener('resize',fit);
function txt(){if(!tx){tx=1;h.classList.add('txt')}}
function end(){if(done)return;done=1;try{v.pause();v.removeAttribute('src');v.load()}catch(e){}h.classList.add('out');setTimeout(()=>h.remove(),650);go3()}
v.addEventListener('timeupdate',()=>{if(v.currentTime>=4.2)txt()});v.addEventListener('ended',()=>{txt();setTimeout(end,1200)});v.addEventListener('error',end);
v.__lbOk=true;if(v.dataset.s)v.src=v.dataset.s;const p=v.play();if(p&&p.catch)p.catch(end);setTimeout(end,14000)}
(function(){if(RESUME||(navigator.hardwareConcurrency||4)<2||(navigator.deviceMemory||8)<1||matchMedia('(prefers-reduced-motion:reduce)').matches||!document.getElementById('hv').canPlayType('video/mp4; codecs="avc1.4D4028"')){['hero','h2','h3'].forEach(i=>{const e=document.getElementById(i);e&&e.remove()});return}
const h=document.getElementById('hero'),v=document.getElementById('hv'),st=document.getElementById('hst');let done=0,tx=0;
function fit(){const k=Math.min(innerWidth/1344,innerHeight/768);st.style.transform='scale('+k+')'}fit();addEventListener('resize',fit);
function txt(){if(!tx){tx=1;h.classList.add('txt')}}
function end(){if(done)return;done=1;try{v.pause();v.removeAttribute('src');v.load()}catch(e){}h.classList.add('out');setTimeout(()=>h.remove(),650);go2()}
v.addEventListener('timeupdate',()=>{if(v.currentTime>=4.3)txt()});
v.addEventListener('ended',()=>{txt();setTimeout(end,1200)});v.addEventListener('error',end);

v.__lbOk=true;if(v.dataset.s)v.src=v.dataset.s;const p=v.play();if(p&&p.catch)p.catch(e=>{window.__he=e.name+':'+e.message;end()});
setTimeout(end,12000)})();
if(RESUME){if(ASKDEF){document.body.classList.add('ask');pd(1)}else prot()}
document.getElementById('done').addEventListener('click',()=>P('welcome.done',''));
)HTML";
    return PageShell(L"Welcome", b, grey, js, L"", false, false);
}

inline std::wstring TabsPage(const std::vector<TabInfo>& t, bool grey) {
    std::wstring b = L"<h1>Tabs</h1><div class=top><input type=text id=q placeholder=\"Search open tabs\"><button data-c=tabs.dups>Close duplicates</button><button data-c=tabs.others>Close others</button></div><div class=card>";
    for (int i = 0; i < (int)t.size(); ++i) {
        std::wstring a = L" data-a=" + std::to_wstring(i), url = t[i].url;
        bool internal = url.rfind(L"libra://", 0) == 0;
        std::wstring title = t[i].title.empty() ? (internal ? L"Libra" : url) : t[i].title;
        b += L"<div class=item><div class=t><a data-c=tab.go" + a + (t[i].current ? L" style=\"font-weight:700\"" : L"") + L">" + Esc(title) + (t[i].current ? L" &middot; this page" : L"") +
             L"</a><small>" + (t[i].loading ? L"Loading... &middot; " : L"") + (t[i].pinned ? L"Pinned &middot; " : L"") + (internal ? L"Libra page" : Esc(url)) + L"</small></div>" +
             L"<button data-c=tab.pin" + a + L">" + (t[i].pinned ? L"Unpin" : L"Pin") + L"</button><button data-c=tab.close" + a + L">Close</button></div>";
    }
    b += L"</div>";
    return PageShell(L"Tabs", b, grey, L"q.oninput=()=>{const v=q.value.toLowerCase();document.querySelectorAll('.item').forEach(i=>i.style.display=i.textContent.toLowerCase().includes(v)?'':'none')}", L"tabs");
}

#include "PopGame.h"
inline std::wstring ErrorPage(const std::wstring& kind, const std::wstring& host, const std::wstring& url, bool grey) {
    struct K { const wchar_t* k; const wchar_t* glyph; const wchar_t* title; const wchar_t* text; };
    static const K T[] = {
        { L"blocked", L"E711", L"Blocked by Libra",         L"This is an ad or tracker server, so cloud ad blocking refused to open it." },
        { L"dns",     L"E774", L"Can't find this site",     L"Libra couldn't find that address. Check the spelling, then try again." },
        { L"cert",    L"E7BA", L"Connection isn't private", L"This site's security certificate is invalid or has expired, so Libra stopped to protect you." },
        { L"timeout", L"E783", L"Taking too long",          L"The site didn't answer in time. It may be down or very busy." },
        { L"offline", L"E774", L"You're offline",           L"Check your internet connection and try again." },
        { L"https",   L"E72E", L"No secure connection",     L"This site doesn't offer HTTPS. Anything you type could be seen by others on the network." },
        { L"conn",    L"E783", L"Can't reach this site",    L"The connection was refused or dropped. The site may be down." },
        { L"family",  L"E72E", L"This page isn't available", L"Child Safety mode is on, so Libra didn't open this site." },
        { L"ip",      L"E72E", L"Opened by IP address",     L"In Max and Child Safety, Libra won't open a site by its number address, because that would skip the ad and safety filters. Open it by its name, or set this site to Balanced with the shield button." },
    };
    const K* k = &T[6];
    for (auto& x : T) if (kind == x.k) k = &x;
    std::wstring b = L"<div class=err>" + std::wstring((kind == L"blocked" || kind == L"family") ? L"<svg class=eic viewBox=\"0 0 24 24\"><path pathLength=1 d=\"M12 2.5 4.5 5.4v5.7c0 4.6 3.1 8.5 7.5 10.4 4.4-1.9 7.5-5.8 7.5-10.4V5.4L12 2.5z\"/><path pathLength=1 d=\"m9 9 6 6M15 9l-6 6\"/></svg>" : kind == L"cert" ? L"<svg class=eic viewBox=\"0 0 24 24\"><path pathLength=1 d=\"M7 11V7a5 5 0 0 1 10 0v4\"/><path pathLength=1 d=\"M6 11h12a2 2 0 0 1 2 2v7a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2v-7a2 2 0 0 1 2-2Z\"/><path pathLength=1 d=\"M12 15v2\"/></svg>" : L"<svg class=eic viewBox=\"0 0 24 24\"><path pathLength=1 d=\"m18.84 12.25 1.72-1.71a5 5 0 0 0-.12-7.07 5 5 0 0 0-6.95 0l-1.72 1.71\"/><path pathLength=1 d=\"m5.17 11.75-1.71 1.71a5 5 0 0 0 .12 7.07 5 5 0 0 0 6.95 0l1.71-1.71\"/><path pathLength=1 d=\"M8 2v3\"/><path pathLength=1 d=\"M2 8h3\"/><path pathLength=1 d=\"M16 19v3\"/><path pathLength=1 d=\"M19 16h3\"/></svg>") + L"<h1>" + k->title + L"</h1><p>" + k->text + L"</p><code>" + Esc(url) + L"</code><div class=btns>";
    if (kind != L"blocked" && kind != L"family") b += L"<button class=pri data-c=retry>Try again</button>";
    b += L"<button data-c=back>Go back</button>";
    if (kind == L"family") b += L"<button data-c=family.unlock>Ask a parent to unlock</button>";
    if (kind == L"cert") b += L"<button data-c=proceed>Proceed anyway (unsafe)</button>";
    if (kind == L"https") b += L"<button data-c=http.go>Continue with http</button>";
    b += L"</div>";
    if (kind == L"cert") b += L"<small>Only continue if you trust this site. Anything you type could be seen by others.</small>";
    b += L"</div>";
    bool net = kind == L"offline" || kind == L"dns" || kind == L"timeout" || kind == L"conn";   // Libra Pop shows only when the PC really is offline (the page checks navigator.onLine)
    if (net) b += kPopHtml;
    return PageShell(L"Can't open " + host, b, grey, net ? std::wstring(kPopJs) : std::wstring(), L"", false);
}
