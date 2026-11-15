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
               L"<a data-c=nav data-a=agent><i class=ico><svg viewBox=\"0 0 24 24\" width=18 height=18 fill=none stroke=currentColor stroke-width=1.7 stroke-linejoin=round><path d=\"M9 3l1.6 4.4L15 9l-4.4 1.6L9 15l-1.6-4.4L3 9l4.4-1.6z\"/><path d=\"M17 13l.9 2.5 2.6.9-2.6.9L17 20l-.9-2.7-2.6-.9 2.6-.9z\"/></svg></i>Assistant</a>" +
               NavItem(L"downloads", L"E896", L"Downloads", active) + NavItem(L"privacy", L"E7B3", L"Privacy", active) + NavItem(L"settings", L"E713", L"Settings", active) + L"</nav><div class=made>Built by Psylinks Security Private Limited</div></aside>";
    if (!sidebar && footer) side = L"<div class=made style=\"position:fixed;bottom:16px;left:0;right:0;text-align:center\">Built by Psylinks Security Private Limited</div>";
    return std::wstring(LR"HTML(<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta http-equiv="Content-Security-Policy" content="default-src 'none'; img-src data: https://libra-hero.local; media-src https://libra-hero.local; style-src 'unsafe-inline'; script-src 'unsafe-inline'; form-action https:; base-uri 'none'"><title>)HTML") + Esc(title) + LR"HTML(</title><style>
:root{--bg:)HTML" + L"#fff" + LR"HTML(;--fg:#111;--mute:#8a8a8a;--card:#fff}
*{box-sizing:border-box;margin:0;padding:0}
body{font:14px 'Segoe UI Variable Text','Segoe UI',system-ui,sans-serif;background:var(--bg);color:var(--fg);display:flex;min-height:100vh;-webkit-font-smoothing:antialiased}
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
input[type=text],select{font:inherit;color:var(--fg);background:#f2f2f2;border:0;outline:0;border-radius:12px;padding:10px 14px;min-width:250px;transition:box-shadow .12s,background .12s}
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
</style></head><body>)HTML" + side + L"<main>" + body + LR"HTML(</main><script>
const P=(...a)=>chrome.webview.postMessage(a.join('\t'));
document.addEventListener('click',e=>{const b=e.target.closest('[data-c]');if(b)P(b.dataset.c,b.dataset.a||'')});
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
    return std::wstring(L"<div class=row><div>") + label + (*sub ? std::wstring(L"<small>") + sub + L"</small>" : L"") +
           L"</div><input type=checkbox data-k=" + k + Chk(s, k) + L"></div>";
}

inline std::wstring ToggleOn(const Settings& s, const wchar_t* k, const wchar_t* label, const wchar_t* sub, const wchar_t* def = L"1") {   // on unless set to 0
    return std::wstring(L"<div class=row><div>") + label + L"<small>" + sub + L"</small></div><input type=checkbox data-k=" + k + (s.Get(k, def) != L"0" ? L" checked" : L"") + L"></div>";
}
inline std::wstring SettingsPage(const Settings& s, bool grey, const std::wstring& cloud = L"On", long long blocked = 0) {
    std::wstring eng = s.Get(L"engine", L"Google"), st = s.Get(L"startup", L"home");
    bool cs = s.Get(L"child_safe", L"0") == L"1";
    std::wstring b = L"<h1>Settings</h1><p class=bnote>Beta: Libra Browser is still being built. Small errors can happen, and we improve it every day.</p><h2>Family</h2><div class=card><div class=row><div>Child Safety mode<small>" + std::wstring(cs
        ? L"On. Adult, malware and scam sites are blocked and ads stay blocked. Turning it off needs your Windows password."
        : L"Blocks adult, malware and scam sites. Turning it on or off needs your Windows password.")
        + (s.Get(L"child_nolock", L"0") == L"1" ? L" Your Windows account has no password, so anyone can turn this off. Set a Windows password to lock it." : L"")

        + L"</small></div><button class=pri data-c=family.toggle>" + (cs ? L"Turn off" : L"Turn on") + L"</button></div></div><h2>Search</h2><div class=card><div class=row><div>Search engine<small>Used by the address bar and the new tab page</small></div><select data-k=engine>";
    for (auto& e : kEngines) if (!e.ai) b += std::wstring(L"<option") + (eng == e.name ? L" selected" : L"") + L">" + e.name + L"</option>";
    b += L"<optgroup label=\"AI search\">";
    for (auto& e : kEngines) if (e.ai) b += std::wstring(L"<option") + (eng == e.name ? L" selected" : L"") + L">" + e.name + L"</option>";
    b += L"</optgroup>";
    b += std::wstring(L"<option value=Custom") + (eng == L"Custom" ? L" selected" : L"") + L">Custom...</option></select></div>";
    { const std::wstring key = L"<div class=row><div>Search engine<small>"; size_t at = b.find(key);
      if (at != std::wstring::npos) b.replace(at, key.size(), L"<div class=row><div>" + LogoHtml(eng == L"Custom" ? L"Custom" : eng) + L"Search engine<small>"); }
    b += L"<div class=row><div>Custom search URL<small>Starts with https:// and ends where the query goes, e.g. https://searx.example/search?q=</small></div><input type=text data-k=engine_url value=\"" + Esc(s.Get(L"engine_url")) + L"\"></div></div>";
    b += L"<h2>Startup</h2><div class=card><div class=row><div>When Libra opens</div><select data-k=startup>";
    b += std::wstring(L"<option value=home") + (st == L"home" ? L" selected" : L"") + L">New tab page</option>";
    b += std::wstring(L"<option value=last") + (st == L"last" ? L" selected" : L"") + L">Continue where I left off</option>";
    b += std::wstring(L"<option value=url") + (st == L"url" ? L" selected" : L"") + L">A specific page</option></select></div>";
    b += L"<div class=row><div>Home page<small>Used when 'A specific page' is chosen</small></div><input type=text data-k=homepage value=\"" + Esc(s.Get(L"homepage")) + L"\"></div></div>";
    b += L"<h2>Downloads</h2><div class=card><div class=row><div>Save to<small>Empty means your Downloads folder</small></div><input type=text data-k=dl_dir value=\"" + Esc(s.Get(L"dl_dir")) + L"\"></div>";
    b += std::wstring(L"<div class=row><div>Ask where to save<small>Libra always asks where to save each file. Nothing is downloaded until you press Save.</small></div></div>") + L"</div>";
    b += L"<h2>Privacy</h2><div class=card><div class=row><div>Cloud ad blocking<small>Ads and trackers are stopped in the cloud before they load. Runs on the Psylinks private cloud (dns.psylinks-beta.com). Always on.</small></div><b>" + cloud + L"</b></div>"
      L"<div class=row><div>Ads and trackers blocked<small>Since you started using Libra</small></div><b>" + std::to_wstring(blocked) + L"</b></div>" + ToggleOn(s, L"idle_prompt", L"Ask to close unused tabs", L"After a long time unused, Libra asks whether to close a tab") + ToggleOn(s, L"sponsored", L"Show sponsored content", L"Clearly labelled sponsored links and a sponsored picture on the new tab page. We do not track you: nothing about you is sent, and the choice of what to show happens on your PC. Hide any item with one click.") + ToggleOn(s, L"web_suggest", L"Search suggestions", L"Your search engine suggests words while you type (Perplexity uses DuckDuckGo)") + ToggleOn(s, L"https_upgrade", L"Upgrade to HTTPS", L"Opens the secure version of a site when there is one. Asks before using an insecure page.") + ToggleOn(s, L"fingerprint", L"Fingerprint protection", L"Adds tiny random changes to what sites can read from your device, different on every site and every launch. Follows the shield level.") + ToggleOn(s, L"block_3p", L"Block third-party cookies", L"Stops ad and tracking sites from following you between websites. Off by default: it can stop some CAPTCHAs and embedded sign-ins from loading. Restart to apply.", L"0") + ToggleOn(s, L"fp_a11y", L"Hide accessibility preferences", L"Websites cannot read your contrast, reduced-motion and transparency settings (Max level only). Turn off if a site ignores your accessibility needs.") + Toggle(s, L"tz_utc", L"Hide my time zone", L"Sites see UTC instead of your time zone. Off by default: times on booking and calendar sites will show in UTC.");
    b += Toggle(s, L"perm_allow", L"Let secure sites use camera, microphone and location without asking", L"Off (recommended): Libra asks every time. Motion sensors are always blocked.");
    b += Toggle(s, L"smartscreen_off", L"Turn off Microsoft SmartScreen", L"SmartScreen warns about phishing and malware but sends site addresses to Microsoft. Off = more private, less protection.");
    { std::wstring bd = s.Get(L"block_default", L"0");
      b += L"<div class=row><div>Default blocking level<small>Used for sites you have not set yourself. Change a site with the shield button.</small></div><select data-k=block_default>";
      b += std::wstring(L"<option value=0") + (bd == L"0" ? L" selected" : L"") + L">Max (block all ads and trackers)</option>";
      b += std::wstring(L"<option value=1") + (bd == L"1" ? L" selected" : L"") + L">Balanced (block trackers, some ads)</option>";
      b += std::wstring(L"<option value=2") + (bd == L"2" ? L" selected" : L"") + L">Off (no blocking)</option></select></div>"; }
    { std::wstring pf = s.Get(L"perf", L"auto");
      b += L"<div class=row><div>Performance<small>Light mode uses less memory on older PCs (sleeps background tabs sooner). Restart to apply.</small></div><select data-k=perf>";
      b += std::wstring(L"<option value=auto") + (pf == L"auto" ? L" selected" : L"") + L">Automatic</option>";
      b += std::wstring(L"<option value=light") + (pf == L"light" ? L" selected" : L"") + L">Light (older PCs)</option>";
      b += std::wstring(L"<option value=normal") + (pf == L"normal" ? L" selected" : L"") + L">Normal</option></select></div>"; }
    b += Toggle(s, L"clear_start", L"Clear cookies, site data and cache at startup", L"A fresh start every time: sites cannot recognise you between sessions. Sign-ins are lost except for the sites listed below.") + L"<div class=row><div>Stay signed in on<small>Sites whose cookies are kept, separated by commas (example: google.com, github.com). Their other site data is still cleared.</small></div><input type=text data-k=keep_signin placeholder=\"google.com, github.com\" value=\"" + Esc(s.Get(L"keep_signin")) + L"\"></div>" +Toggle(s, L"block_images", L"Data saver", L"Don't load images");
    b += Toggle(s, L"local_block", L"Local ad blocker", L"Extra layer, off by default. Open a new tab after changing.");
    b += L"<div class=row><div>Browsing data<small>Choose what to clear: history, cookies, sign-ins, cache and more</small></div><button class=pri data-c=nav data-a=privacy>Clear data...</button></div><div class=row><div>Privacy dashboard<small>See what Libra remembers and clear it piece by piece</small></div><button data-c=nav data-a=privacy>Open</button></div></div>";
    b += L"<h2>Media</h2><div class=card>" + Toggle(s, L"save_video", L"Save videos", L"Shows a quiet icon in the address bar when a page has a video you can save. Off by default.") + L"</div>";
    b += L"<h2>AI Agent</h2><div class=card><div class=row><div>Beta<small>The AI Agent is a beta feature. It can make mistakes, so check its work before you submit anything important.</small></div></div>"
        + Toggle(s, L"agent_on", L"Agent",L"Lets you give Libra Browser a job, such as filling a form or reading a page for you. Off by default. While it is off, none of it runs.")
        + ToggleOn(s, L"agent_btn", L"Show the Agent button in the toolbar", L"Turn this off to hide the Agent from the toolbar. Ctrl+Shift+Space still opens it.")
        + L"<div class=row><div>Connection<small>" + (s.Get(L"agent_model").empty() ? std::wstring(L"Not connected yet. Open the Agent and choose a provider and add your API key.") : L"Model: " + Esc(s.Get(L"agent_model")) + L". Your API key is stored encrypted on this PC and is never shown.") + L"</small></div><button class=pri data-c=nav data-a=agent>Open the Agent</button></div>"
        + Toggle(s, L"autosuggest", L"Auto suggest", L"When you click an empty box for your name, e-mail, phone, company and similar, Libra shows what is in the notes you saved in the Assistant's Memory, and types it when you click it. Never for passwords, user names, cards or ID numbers, and never on sign-in forms. It works on this PC only: no AI is used and nothing is sent anywhere. Off by default.")
        + ToggleOn(s, L"agent_opt", L"Shorten my requests", L"Removes filler words before sending, so each job uses fewer tokens. Your meaning is kept.")
        + ToggleOn(s, L"agent_vauto", L"Send voice requests automatically", L"After you speak, the text is sent right away. Turn off to review it first.")
        + L"<div class=row><div>What the Agent can do<small>Open a site you name, read the page, click, type, choose from lists, fill forms from the notes you gave it, read and summarise pages, work in up to four research tabs, upload files you placed in its Files folder, and download files only after you say yes. It remembers details you tell it, and you can remove them any time.</small></div></div>"
        L"<div class=row><div>What it never does<small>It never types passwords, card numbers or one-time codes, never solves CAPTCHAs or human checks (it stops and waits for you), never searches the web by itself, never opens pages inside Libra Browser (settings, vault), and never sees your saved passwords or files outside its Files folder.</small></div></div>"
        L"<div class=row><div>You stay in control<small>Anything risky (sending or submitting a form, paying, leaving for another site, downloading, typing on a site you did not name) opens a Windows question first, and you choose how long to remember your answer. Review every answer before a form is sent. Press Stop at any time.</small></div></div>"
        L"<div class=row><div>Your data<small>Your key and notes are encrypted with Windows on this PC. Page text goes only to the provider you chose, nothing goes to Psylinks. The Agent is disabled in private windows and in Child Safety mode.</small></div></div>"
        L"<div class=row><div>Honest limits<small>An AI can make mistakes. Websites may try to trick it with hidden text, so it treats page text as data, not as orders. Check the review card before you submit anything important.</small></div></div></div>";
    b += L"<h2>About</h2><div class=card><div class=row><div>Libra Browser<small>Version 3.9.0 (beta). Errors can happen; we are improving it. Built by Psylinks Security Private Limited. Secure, private and lightweight.</small></div></div>"
        L"<div class=row><div>What Libra does<small>Blocks ads and trackers before they load. Upgrades sites to secure https. Blocks popups and downloads you did not ask for. Hides your browser fingerprint. Keeps a password vault and a password generator. Fast multi-connection downloads. Search suggestions from your own engine. Private windows that leave nothing behind. Light on memory and CPU. We collect nothing about you. The new-tab page may show labelled sponsored items; switch them off in Settings > Privacy.</small></div></div>"
        L"<div class=row><div>License<small>Source available: you can read the code. You may not change, share or sell it without written permission from Psylinks Security Private Limited. See the LICENSE file.</small></div></div>"
        L"<div class=row><div>Source code<small><a href=\"https://github.com/PsylinksSecurityWeb/libra-browser\" style=\"color:inherit\">github.com/PsylinksSecurityWeb/libra-browser</a></small></div></div></div>";
    return PageShell(L"Settings", b, grey, LR"JS((function(){const m=document.querySelector('main'),hs=[...m.querySelectorAll('h2')];const f=hs.find(h=>h.textContent==='Family'),a=hs.find(h=>h.textContent==='About');if(!f||!a)return;const els=[f];let n=f.nextElementSibling;while(n&&n.tagName!=='H2'){els.push(n);n=n.nextElementSibling}els.forEach(e=>m.insertBefore(e,a))})();)JS", L"settings");
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
<div class=bt><button id=cx>Cancel</button><button class=pri id=cgo>Clear data</button></div></div></div><div id=tst></div><div class=card>)H";
    int shown = 0;
    for (int i = (int)h.size() - 1; i >= 0 && shown < 500; --i, ++shown) {
        wchar_t tm[32] = L""; struct tm lt; time_t t = (time_t)h[i].t;
        if (!localtime_s(&lt, &t)) wcsftime(tm, 32, L"%d %b, %H:%M", &lt);
        b += L"<div class=item data-t=" + std::to_wstring(h[i].t) + L"><div class=t><a href=\"" + Esc(h[i].url) + L"\">" + Esc(h[i].title.empty() ? h[i].url : h[i].title) + L"</a><small>" + Esc(h[i].url) + L" &middot; " + tm + L"</small></div><button data-c=hist.del data-a=" + std::to_wstring(i) + L">Remove</button></div>";
    }
    if (!shown) b += L"<div class=empty>Nothing here yet.</div>";
    b += L"</div>";
    return PageShell(L"History", b, grey, LR"JS(q.oninput=()=>{const v=q.value.toLowerCase();document.querySelectorAll('.item').forEach(i=>i.style.display=i.textContent.toLowerCase().includes(v)?'':'none')};
const cm=document.getElementById('cm'),cgo=document.getElementById('cgo');
document.getElementById('clr').onclick=()=>{cm.style.display='flex'};document.getElementById('cx').onclick=()=>{cm.style.display='none'};
cm.onclick=e=>{if(e.target===cm)cm.style.display='none'};document.addEventListener('keydown',e=>{if(e.key==='Escape')cm.style.display='none'});
cgo.onclick=()=>{const ks=[...cm.querySelectorAll('input:checked')].map(i=>i.value);if(!ks.length){return}cgo.disabled=true;cgo.textContent='Clearing...';P('priv.clear',ks.join(','),document.getElementById('cr').value)};
document.getElementById('sall').onclick=e=>{e.preventDefault();cm.querySelectorAll('input[type=checkbox]').forEach(i=>i.checked=true)};document.getElementById('snone').onclick=e=>{e.preventDefault();cm.querySelectorAll('input[type=checkbox]').forEach(i=>i.checked=false)};
window.lbDone=(since,h,msg)=>{if(h){document.querySelectorAll('.item').forEach(i=>{if(!since||+i.dataset.t>=since)i.remove()});if(!document.querySelector('.item')){const c=document.querySelector('main .card:last-of-type');if(c)c.innerHTML='<div class=empty>Nothing here yet.</div>'}}
cm.style.display='none';cgo.disabled=false;cgo.textContent='Clear data';const t=document.getElementById('tst');t.textContent=msg||'Cleared';t.style.opacity=1;setTimeout(()=>t.style.opacity=0,msg&&msg.length>40?6000:2200)})JS", L"history");
}

inline std::wstring BookmarksPage(const std::vector<BookmarkItem>& m, bool grey) {
    std::wstring b = L"<h1>Bookmarks</h1><div class=card>";
    for (int i = 0; i < (int)m.size(); ++i)
        b += L"<div class=item><div class=t><a href=\"" + Esc(m[i].url) + L"\">" + Esc(m[i].title.empty() ? m[i].url : m[i].title) + L"</a><small>" + Esc(m[i].url) + L"</small></div><button data-c=bm.del data-a=" + std::to_wstring(i) + L">Remove</button></div>";
    if (m.empty()) b += L"<div class=empty>No bookmarks yet. Press Ctrl+D or the star in the address bar.</div>";
    return PageShell(L"Bookmarks", b + L"</div>", grey, L"", L"bookmarks");
}

inline std::wstring DownloadsPage(const std::vector<DownloadItem>& d, bool grey) {
    std::wstring b = LR"H(<style>
.dh{display:none;align-items:baseline;gap:12px;margin:0 0 20px}.dh #tsp{font:800 56px 'Segoe UI Variable Display','Segoe UI',sans-serif;letter-spacing:-.03em;font-variant-numeric:tabular-nums;line-height:1}.dh small{color:var(--mute);font-size:15px}
.dr{position:relative;display:flex;align-items:center;gap:16px;padding:17px 20px;margin:9px 0;border-radius:18px;background:#fff;overflow:hidden;box-shadow:none}
.dr .fill{position:absolute;inset:0 auto 0 0;width:0;background:linear-gradient(90deg,rgba(17,17,17,.06),rgba(17,17,17,.13));transition:width .26s linear}
.dr>*{position:relative}.dr .t{flex:1;min-width:0}.dr .t b{display:block;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.dr .t small{color:var(--mute);font-variant-numeric:tabular-nums}
.dr .v{font:700 22px 'Segoe UI Variable Display','Segoe UI',sans-serif;min-width:118px;text-align:right;font-variant-numeric:tabular-nums}
.dr button{padding:8px 16px}.dr button.g{background:none;color:var(--mute)}
</style><h1>Downloads</h1><div class=dh id=dh><span id=tsp>0</span><small id=tsm></small></div>)H";
    bool active = false, opActive = false;
    for (int i = (int)d.size() - 1; i >= 0; --i) {
        auto& x = d[i]; std::wstring a = L" data-a=" + std::to_wstring(i), id = std::to_wstring(i);
        if (x.state == 0) { active = true; if (!x.job) opActive = true; }
        int pc = x.state == 1 ? 100 : (x.total > 0 ? (int)(x.got * 100 / x.total) : 0);
        std::wstring info = x.state == 0 ? Fmt(x.got) + (x.total > 0 ? L" of " + Fmt(x.total) : L"") : x.state == 1 ? Fmt(x.total > 0 ? x.total : x.got) :
                            x.hold == 3 ? L"Removed by Windows Security" : x.canRetry ? L"Interrupted at " + Fmt(x.got) + (x.total > 0 ? L" of " + Fmt(x.total) : L"") : L"Stopped";
        if (x.hold == 1) info = L"Checking with Windows Security...";
        else if (x.hold == 2) info = L"Held for your safety. " + x.scanNote;
        b += L"<div class=\"dr" + std::wstring(x.state == 1 ? L" ok" : x.state == 2 ? L" bad" : L"") + L"\" data-i=" + id + L" data-st=" + std::to_wstring(x.state) + L"><i class=fill style=\"width:" + std::to_wstring(x.state == 0 ? pc : 0) + L"%\"></i>" +
             L"<div class=t><b>" + Esc(x.name.empty() ? L"(starting)" : x.name) + L"</b><small class=inf>" + info + L"</small></div>";
        if (x.state == 0) b += L"<div class=v><span class=sp>&middot;&middot;&middot;</span></div><button class=g data-c=dl.cancel" + a + L">Cancel</button>";
        else if (x.state == 1 && x.hold == 1) b += L"<div class=v><span class=sp>&middot;&middot;&middot;</span></div>";
        else if (x.state == 1 && x.hold == 2) b += L"<button class=pri data-c=dl.release" + a + L">Release</button>" + std::wstring(WinSandboxExe().empty() ? L"" : L"<button data-c=dl.sandbox" + a + L">Open in Sandbox</button>") + L"<button data-c=dl.show" + a + L">Folder</button><button class=g data-c=dl.delhold" + a + L">Delete</button>";
        else if (x.state == 1) b += L"<button class=pri data-c=dl.open" + a + L">Open</button><button data-c=dl.show" + a + L">Folder</button><button class=g data-c=dl.remove" + a + L">Remove</button>";
        else b += (x.canRetry ? L"<button class=pri data-c=dl.retry" + a + L">Retry</button>" : std::wstring(L"")) + L"<button class=g data-c=dl.remove" + a + L">Remove</button>";
        b += L"</div>";
    }
    if (d.empty()) b += L"<div class=empty>No downloads yet.</div>";
    std::wstring js = LR"H(
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

inline std::wstring WelcomePage(bool grey, bool isDefault = false) {
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
.seg{position:relative;display:inline-flex;background:rgba(255,255,255,.7);border-radius:999px;padding:4px;box-shadow:none}
.seg .ind{position:absolute;top:4px;left:4px;width:calc(50% - 4px);height:calc(100% - 8px);background:#111;border-radius:999px;transition:transform .32s cubic-bezier(.3,.7,.2,1)}
.seg button{position:relative;z-index:1;background:none;border-radius:999px;padding:9px 26px;min-width:132px;font-weight:600;color:#555;transition:color .25s}
.seg button:hover{background:none}.seg button.on{color:#fff}
.cap{color:var(--mute);font-size:12.5px;margin:14px 0 22px;height:16px}
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
.sl.on .thumb path.st{stroke-dasharray:3.6 3.4;stroke-dashoffset:0;animation:flow .4s linear infinite;opacity:.6}@keyframes flow{to{stroke-dashoffset:-7}}
.dflt h2,.dflt p,.dflt .btns{opacity:0;animation:fade .7s .7s both}.dflt p{animation-delay:.9s}.dflt .btns{animation-delay:1.1s}.dflt .btns button.g{background:none;color:var(--mute)}
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
#hskip{position:absolute;right:24px;bottom:22px;background:none;color:#888;font-size:12px;border:0}
.intro{display:none!important;position:fixed;inset:0;z-index:40;background:var(--bg);display:flex;flex-direction:column;align-items:center;justify-content:center;transition:opacity .5s;cursor:pointer}.intro.out{opacity:0;pointer-events:none}
.sl{position:absolute;left:0;right:0;top:30%;text-align:center;opacity:0;transition:opacity .45s;pointer-events:none}.sl.on{opacity:1}.sl.on .thumb path{animation:draw .9s .1s cubic-bezier(.4,.1,.2,1) forwards}
.sl .thumb{margin:0 auto 22px}.sl .thumb path{stroke-dashoffset:1;animation:none}.sl h2{font:300 26px 'Segoe UI Variable Text','Segoe UI',sans-serif;text-transform:none;letter-spacing:.02em;color:var(--fg);margin:0 0 8px}.sl p{color:var(--mute);font-size:14px}
.dots{position:absolute;bottom:92px;display:flex;gap:8px}.dots i{width:6px;height:6px;border-radius:3px;background:#c9c9c9;transition:all .3s}.dots i.on{width:22px;background:#111}#skip{position:absolute;bottom:30px;background:none;color:var(--mute)}
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
.sub,.seg,.cap,.fine{display:none}
.panel{grid-template-columns:repeat(6,1fr);gap:6px;animation:none}.panel#p1{grid-template-columns:repeat(2,minmax(0,150px));justify-content:center;margin-top:6px}
.cap2{color:var(--mute);font-size:12px;letter-spacing:.14em;text-transform:uppercase;margin:34px 0 10px;animation:rise 1s .5s both}
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
<div class=intro id=intro>
<section class="sl on"><svg class=thumb viewBox="0 0 24 24"><path pathLength=1 d="M13 2 3 14h9l-1 8 10-12h-9l1-8z"/></svg><h2>Fast browsing</h2><p>Pages open in a blink.</p></section>
<section class=sl><svg class=thumb viewBox="0 0 24 24"><path class=st d="M4.5 1v11"/><path class=st d="M19.5 3v11"/><path class=st d="M8.5 0.5v7"/><path class=st d="M15.5 2v7"/><path pathLength=1 d="M12 4v11"/><path pathLength=1 d="m7.5 10.5 4.5 4.5 4.5-4.5"/><path pathLength=1 d="M5 20h14"/></svg><h2>Fast downloads</h2><p>Big files use several connections at once.</p></section>
<section class=sl><svg class=thumb viewBox="0 0 24 24"><path pathLength=1 d="M12 2.5 4.5 5.4v5.7c0 4.6 3.1 8.5 7.5 10.4 4.4-1.9 7.5-5.8 7.5-10.4V5.4L12 2.5z"/><path pathLength=1 d="M8.8 14.5a2 2 0 0 1-.2-4 3.2 3.2 0 0 1 6.2.5 1.8 1.8 0 0 1 .2 3.5H8.8z"/></svg><h2>Cloud ad blocking</h2><p>Ads and trackers stop in the cloud, before they load.</p></section>
<section class=sl><svg class=thumb viewBox="0 0 24 24"><path pathLength=1 d="M7 11V7a5 5 0 0 1 10 0v4"/><path pathLength=1 d="M6 11h12a2 2 0 0 1 2 2v7a2 2 0 0 1-2 2H6a2 2 0 0 1-2-2v-7a2 2 0 0 1 2-2Z"/></svg><h2>Private by design</h2><p>We collect nothing. Ever.</p></section>
<div class=dots><i class=on></i><i></i><i></i><i></i></div></div>
<div class=wel><h1>Choose your search.</h1><p class=sub>Choose how you search</p>
<div class=seg><i class=ind></i><button class=on data-t=0>Search</button><button data-t=1>AI search</button></div>
<p class=cap id=cap>Classic search engines</p>
<div class=panel id=p0>)HTML" + tiles(false) + L"</div><div class=panel id=p1 hidden>" + tiles(true) + LR"HTML(</div>
<p class=fine>You can change this any time in Settings.</p></div>
<div class=dflt><svg class=thumb viewBox="0 0 24 24"><path pathLength=1 d="M5 4h14a3 3 0 0 1 3 3v10a3 3 0 0 1-3 3H5a3 3 0 0 1-3-3V7a3 3 0 0 1 3-3z"/><path pathLength=1 d="M2 9.5h20"/><path pathLength=1 d="M5.2 6.8h.01M7.6 6.8h.01"/><path pathLength=1 d="M11 6.8h8"/><path pathLength=1 d="M6 13.5h7"/><path pathLength=1 d="M6 16.5h4.5"/></svg><h2>Make Libra your default browser?</h2><p class=sub style="max-width:430px;margin:0">Links from other apps will open here. Windows will ask you to confirm the choice.</p><div class=btns><button class=pri data-c=default.set id=dset>Set as default</button><button id=dskip class=g>Not now</button></div></div>
<div class=done id=done><svg class=thumb viewBox="0 0 24 24" pathLength=1><path pathLength=1 d="M7 10v12"/><path pathLength=1 d="M15 5.88 14 10h5.83a2 2 0 0 1 1.92 2.56l-2.33 8A2 2 0 0 1 17.5 22H4a2 2 0 0 1-2-2v-8a2 2 0 0 1 2-2h2.76a2 2 0 0 0 1.79-1.11L12 2a3.13 3.13 0 0 1 3 3.88Z"/></svg><h2>You're all set</h2></div>)HTML";
    std::wstring js = LR"HTML(
const bt=[...document.querySelectorAll('.seg button')];
bt.forEach(x=>x.addEventListener('click',()=>{const i=+x.dataset.t;bt.forEach(y=>y.classList.toggle('on',y===x));
 document.querySelector('.ind').style.transform='translateX('+(i*100)+'%)';
 ['p0','p1'].forEach((id,k)=>document.getElementById(id).hidden=k!==i);
 document.getElementById('cap').textContent=i?'Answers from AI assistants':'Classic search engines';}));
const ASKDEF=)HTML" + std::wstring(isDefault ? L"0" : L"1") + LR"HTML(;
function fin(){document.body.classList.remove('ask');document.body.classList.add('fin');setTimeout(()=>P('welcome.done',''),1900)}
document.getElementById('p1').hidden=false;document.getElementById('p1').insertAdjacentHTML('beforebegin','<p class=cap2>AI assistants</p>');
document.querySelectorAll('.tile').forEach(t=>t.addEventListener('click',()=>{
  t.classList.add('sel');document.querySelector('.wel').classList.add('pick');setTimeout(()=>{if(ASKDEF)document.body.classList.add('ask');else fin()},700)}));
document.getElementById('dset').addEventListener('click',()=>setTimeout(fin,500));document.getElementById('dskip').addEventListener('click',fin);
function go3(){const h=document.getElementById('h3'),v=document.getElementById('h3v'),st=document.getElementById('h3s');let done=0,tx=0;
function fit(){const k=Math.min(innerWidth/1280,innerHeight/720);st.style.transform='scale('+k+')'}fit();addEventListener('resize',fit);
function txt(){if(!tx){tx=1;h.classList.add('txt')}}
function end(){if(done)return;done=1;try{v.pause();v.removeAttribute('src');v.load()}catch(e){}h.classList.add('out');setTimeout(()=>h.remove(),650);window.introGo&&window.introGo()}
v.addEventListener('timeupdate',()=>{if(v.currentTime>=4.6)txt()});v.addEventListener('ended',()=>{txt();setTimeout(end,2400)});v.addEventListener('error',end);
v.__lbOk=true;if(v.dataset.s)v.src=v.dataset.s;const p=v.play();if(p&&p.catch)p.catch(end);setTimeout(end,16000)}
function go2(){const h=document.getElementById('h2'),v=document.getElementById('h2v'),st=document.getElementById('h2s');let done=0,tx=0;
function fit(){const k=Math.min(innerWidth/1344,innerHeight/768);st.style.transform='scale('+k+')'}fit();addEventListener('resize',fit);
function txt(){if(!tx){tx=1;h.classList.add('txt')}}
function end(){if(done)return;done=1;try{v.pause();v.removeAttribute('src');v.load()}catch(e){}h.classList.add('out');setTimeout(()=>h.remove(),650);go3()}
v.addEventListener('timeupdate',()=>{if(v.currentTime>=4.2)txt()});v.addEventListener('ended',()=>{txt();setTimeout(end,2400)});v.addEventListener('error',end);
v.__lbOk=true;if(v.dataset.s)v.src=v.dataset.s;const p=v.play();if(p&&p.catch)p.catch(end);setTimeout(end,14000)}
(function(){if((navigator.hardwareConcurrency||4)<2||(navigator.deviceMemory||8)<1||matchMedia('(prefers-reduced-motion:reduce)').matches||!document.getElementById('hv').canPlayType('video/mp4; codecs="avc1.4D4028"')){['hero','h2','h3'].forEach(i=>{const e=document.getElementById(i);e&&e.remove()});return}
const h=document.getElementById('hero'),v=document.getElementById('hv'),st=document.getElementById('hst');let done=0,tx=0;
function fit(){const k=Math.min(innerWidth/1344,innerHeight/768);st.style.transform='scale('+k+')'}fit();addEventListener('resize',fit);
function txt(){if(!tx){tx=1;h.classList.add('txt')}}
function end(){if(done)return;done=1;try{v.pause();v.removeAttribute('src');v.load()}catch(e){}h.classList.add('out');setTimeout(()=>h.remove(),650);go2()}
v.addEventListener('timeupdate',()=>{if(v.currentTime>=4.3)txt()});
v.addEventListener('ended',()=>{txt();setTimeout(end,2200)});v.addEventListener('error',end);

v.__lbOk=true;if(v.dataset.s)v.src=v.dataset.s;const p=v.play();if(p&&p.catch)p.catch(e=>{window.__he=e.name+':'+e.message;end()});
setTimeout(end,12000)})();
(function(){const ss=[...document.querySelectorAll('.sl')],ds=[...document.querySelectorAll('.dots i')],el=document.getElementById('intro');let i=0,t;
function show(n){ss.forEach((s,k)=>s.classList.toggle('on',k===n));ds.forEach((d,k)=>d.classList.toggle('on',k===n))}
function end(){clearInterval(t);el.classList.add('out');setTimeout(()=>el.remove(),500)}
function next(){if(i>=ss.length-1){end();return}show(++i)}
window.introGo=()=>{clearInterval(t);t=setInterval(next,2700)};if(!document.getElementById('hero'))window.introGo()})();
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
    return PageShell(L"Can't open " + host, b, grey, L"", L"", false);
}
