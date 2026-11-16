// Agent.inl : included at the end of BrowserWindow.cpp. Libra Agent.
// Rules: only libra://agent may talk to this channel; fixed action list; key stays in DPAPI and is sent only to the saved endpoint;
// no redirects; page content is cut to visible elements; password/card/OTP fields refused; sensitive clicks and leaving the site need a native Yes.
// Event driven only: no timers, no threads except one short HTTPS call per step.

static std::wstring AgJs(const std::wstring& s) {   // safe JS string literal (ASCII only escapes for risky chars)
    std::wstring o = L"\"";
    for (wchar_t c : s) {
        if (c == L'"' || c == L'\\') { o += L'\\'; o += c; }
        else if (c < 32 || c == 0x2028 || c == 0x2029 || c == L'<' || c == L'>' || c == L'&') { wchar_t b[8]; swprintf_s(b, L"\\u%04x", (unsigned)c); o += b; }
        else o += c;
    }
    return o + L"\"";
}
static std::wstring AgDomOf(const std::wstring& url);
static const char* kAgEntropy = "LibraAgent-v1";
static std::string AgKeyGet() {
    std::string enc = ReadAll(DataDir() + L"\\agent.key");
    if (enc.empty()) return "";
    DATA_BLOB in{ (DWORD)enc.size(), (BYTE*)enc.data() }, ent{ (DWORD)strlen(kAgEntropy), (BYTE*)kAgEntropy }, out{};
    if (!CryptUnprotectData(&in, nullptr, &ent, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return "";
    std::string k((const char*)out.pbData, out.cbData); SecureZeroMemory(out.pbData, out.cbData); LocalFree(out.pbData); return k;
}
static void AgKeySet(const std::wstring& key) {
    if (key.empty()) { DeleteFileW((DataDir() + L"\\agent.key").c_str()); return; }
    std::string k = ToUtf8(key);
    DATA_BLOB in{ (DWORD)k.size(), (BYTE*)k.data() }, ent{ (DWORD)strlen(kAgEntropy), (BYTE*)kAgEntropy }, out{};
    if (CryptProtectData(&in, L"Libra agent", &ent, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) {
        WriteAll(DataDir() + L"\\agent.key", std::string((const char*)out.pbData, out.cbData)); LocalFree(out.pbData);
    }
    SecureZeroMemory(k.data(), k.size());
}
static bool AgIsLocal(const std::wstring& ep) { return ep.rfind(L"http://localhost", 0) == 0 || ep.rfind(L"http://127.0.0.1", 0) == 0; }
static bool AgEndpointOk(const std::wstring& e) {
    if (e.empty() || e.size() > 200) return false;
    if (e.rfind(L"https://", 0) != 0 && !AgIsLocal(e)) return false;
    for (wchar_t c : e) if (!(iswalnum(c) && c < 128) && !wcschr(L":/._-?=&%", c)) return false;   // no '@' (userinfo), no spaces
    return true;
}
static bool AgModelOk(const std::wstring& m) {
    if (m.empty() || m.size() > 80) return false;
    for (wchar_t c : m) if (!(iswalnum(c) && c < 128) && !wcschr(L"._:/-", c)) return false;
    return true;
}
static std::string AgPost(int port, const std::wstring& ep, const std::string& key, const std::string& body) {
    dl::Url u; if (!dl::Crack(ep, u)) return "{\"error\":\"bad endpoint\"}";
    const bool local = AgIsLocal(ep);   // loopback is refused by our DNS proxy (rebinding guard), so local models go direct
    HINTERNET ses = local ? WinHttpOpen(L"Libra", WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, 0) : dl::Session(port, 1);
    DWORD err = 0; std::string out;
    if (ses) {
        WinHttpSetTimeouts(ses, 10000, 10000, 60000, 60000);
        if (HINTERNET con = WinHttpConnect(ses, u.host.c_str(), u.port, 0)) {
            if (HINTERNET rq = WinHttpOpenRequest(con, L"POST", u.path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, u.https ? WINHTTP_FLAG_SECURE : 0)) {
                DWORD nc = WINHTTP_DISABLE_COOKIES; WinHttpSetOption(rq, WINHTTP_OPTION_DISABLE_FEATURE, &nc, sizeof nc);
                DWORD nr = WINHTTP_OPTION_REDIRECT_POLICY_NEVER; WinHttpSetOption(rq, WINHTTP_OPTION_REDIRECT_POLICY, &nr, sizeof nr);   // the key never follows a redirect
                std::wstring hdr = L"Content-Type: application/json\r\n";
                if (!key.empty()) hdr += u.path.find(L"/v1/messages") != std::wstring::npos
                    ? L"x-api-key: " + FromUtf8(key) + L"\r\nanthropic-version: 2023-06-01\r\n"   // Anthropic Messages API
                    : L"Authorization: Bearer " + FromUtf8(key) + L"\r\n";                       // OpenAI-compatible (all others)
                if (WinHttpSendRequest(rq, hdr.c_str(), (DWORD)-1, (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0) && WinHttpReceiveResponse(rq, nullptr)) {
                    DWORD code = 0, sz = sizeof code; WinHttpQueryHeaders(rq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &code, &sz, nullptr);
                    std::string r; char buf[4096]; DWORD n = 0;
                    while (r.size() < 1000000 && WinHttpReadData(rq, buf, sizeof buf, &n) && n) r.append(buf, n);
                    if (code == 200) out = r;
                    else {   // flag "wrong key" answers so the UI can say so plainly
                        std::string lo = r.substr(0, 600); for (char& ch : lo) ch = (char)tolower((unsigned char)ch);
                        bool bk = code == 401 || code == 403 || lo.find("api key") != std::string::npos || lo.find("api_key") != std::string::npos || lo.find("authentication") != std::string::npos || lo.find("unauthorized") != std::string::npos;
                        out = "{\"error\":\"HTTP " + std::to_string(code) + (bk ? " badkey" : "") + "\"}";
                    }
                } else err = GetLastError();
                if (!key.empty()) SecureZeroMemory((void*)hdr.data(), hdr.size() * sizeof(wchar_t));
                WinHttpCloseHandle(rq);
            }
            WinHttpCloseHandle(con);
        }
        WinHttpCloseHandle(ses);
    }
    if (out.empty()) {   // say WHICH host failed and why, in plain words
        std::string h = ToUtf8(u.host), why = err == 12007 || err == 12156 ? "the name could not be found (blocked or mistyped?)" : err == 12029 ? (local ? "nothing is listening there (is Ollama running?)" : "could not connect") : err == 12002 ? "it took too long" : err == 12175 || err == 12038 ? "its security certificate was rejected" : "code " + std::to_string(err);
        out = "{\"error\":\"Cannot reach " + h + ": " + why + "\"}";
    }
    return out;
}

// Runs inside the target tab: visible, interactive elements only (no hidden, off-screen or zero-size nodes).
static const wchar_t* kAgSnap = LR"H((function(){try{
var hc=function(){try{var f=document.querySelectorAll('iframe[src*="recaptcha"],iframe[src*="hcaptcha"],iframe[src*="turnstile"],iframe[src*="challenges.cloudflare"],iframe[src*="arkoselabs"],#px-captcha,.g-recaptcha,.h-captcha,.cf-turnstile');for(var i=0;i<f.length;i++){if(f[i].closest('.grecaptcha-badge'))continue;var r=f[i].getBoundingClientRect();if(r.width>40&&r.height>20)return 1}
if(/just a moment|attention required|access denied|are you a robot/i.test(document.title))return 1;var t=(document.body&&document.body.innerText||'').slice(0,3000);return /not a robot|are you a (person|human)|verify (that )?you are (a )?human|unusual traffic|complete the security check|checking if the site connection is secure|select all (images|squares)|press (&|and) hold|captcha challenge|bots use duckduckgo|pardon our interruption/i.test(t)?1:0}catch(x){return 0}};
if(hc())return 'HUMAN_CHECK';
var vis=function(e){var r=e.getBoundingClientRect();if(r.width<4||r.height<4||r.bottom<-200||r.top>innerHeight+400||r.right<-20||r.left>innerWidth+20)return 0;var s=getComputedStyle(e);if(s.visibility!='visible'||s.display=='none'||+s.opacity<.1)return 0;if(e.closest('[aria-hidden=true],[inert],[hidden]'))return 0;return 1};
var sel='a[href],button,input:not([type=hidden]),select,textarea,label,summary,[role=button],[role=link],[role=tab],[role=menuitem],[role=radio],[role=checkbox],[role=option],[role=combobox],[role=listbox],[role=switch],[role=textbox],[role=treeitem],[contenteditable=true],[aria-haspopup],[tabindex]:not([tabindex="-1"]),[onclick]';
var q=Array.prototype.slice.call(document.querySelectorAll(sel)),seen=new Set(q),ex=document.querySelectorAll('div,span,li,td'),k=0;
for(var j=0;j<ex.length&&k<700;j++){var x=ex[j];if(seen.has(x))continue;k++;var it=(x.innerText||'').trim();if(!it||it.length>60||x.children.length>3)continue;
 if(getComputedStyle(x).cursor=='pointer'&&!x.closest('a,button,[role=button],[onclick],label,[role=option]')){q.push(x);seen.add(x)}}
q.sort(function(a,b){return a===b?0:(a.compareDocumentPosition(b)&4?-1:1)});
var arr=[],out=[];
var qf=function(e){try{var s='';if(e.labels&&e.labels[0])s=e.labels[0].innerText;if(!s){var ids=e.getAttribute('aria-labelledby');if(ids)s=ids.split(' ').map(function(i){var n=document.getElementById(i);return n?n.innerText:''}).join(' ')}
if(!s){var n=e,k=0;while(n&&k<5&&!s){var p=n.previousElementSibling,g=0;while(p&&g<4&&!s){var tt=(p.innerText||'').trim();if(tt&&tt.length<200&&!p.querySelector('input,textarea,select'))s=tt;p=p.previousElementSibling;g++}n=n.parentElement;k++}}
return s.replace(/\s+/g,' ').trim().slice(0,110)}catch(z){return ''}};
for(var i=0;i<q.length&&arr.length<110;i++){var e=q[i];try{if(!vis(e))continue;
 if(e.tagName=='LABEL'){var inp=e.querySelector('input,select,textarea');if(inp&&vis(inp))continue}
 var t=String(e.getAttribute('aria-label')||e.innerText||e.value||e.placeholder||e.title||e.alt||'').replace(/\s+/g,' ').trim().slice(0,80);
 var kd=e.tagName.toLowerCase();if(kd=='input')kd+=':'+(e.type||'text');if(e.getAttribute('role'))kd+='/'+e.getAttribute('role');if(e.type=='password')t='[password: user only]';
 var x2=(e.checked||e.getAttribute('aria-checked')=='true'||e.getAttribute('aria-selected')=='true')?' [on]':'';
 if(e.tagName=='SELECT'){x2+=' value="'+(e.selectedIndex>=0?e.options[e.selectedIndex].text.trim().slice(0,40):'')+'" options: '+Array.prototype.slice.call(e.options,0,14).map(function(o){return o.text.trim().slice(0,30)}).join(' | ')}
 else if((e.tagName=='INPUT'||e.tagName=='TEXTAREA')&&e.type!='password'&&e.type!='file'&&e.type!='checkbox'&&e.type!='radio'&&e.value)x2+=' value="'+e.value.replace(/\s+/g,' ').slice(0,40)+'"';
 var qq='';if(/^(INPUT|TEXTAREA|SELECT)$/.test(e.tagName)||e.isContentEditable){var qv=qf(e);if(qv&&qv!=t)qq=' q="'+qv.replace(/"/g,"'")+'"'}
 out.push('['+arr.length+'] '+kd+' "'+t+'"'+x2+qq+(e.tagName=='A'?' -> '+(e.hostname||''):'')+(e.tagName=='A'&&/\.(pdf|zip|docx?|xlsx?|pptx?|csv|epub|rar|7z|mp3|mp4|apk)(\?|#|$)/i.test(e.href||'')?' file: '+String(e.href).slice(0,160):''));arr.push(e)}catch(z){}}
Object.defineProperty(window,'__lbA',{value:arr,configurable:true,writable:true});
var tx='',w=document.createTreeWalker(document.body||document,NodeFilter.SHOW_TEXT|NodeFilter.SHOW_ELEMENT),n,c=0;
while((n=w.nextNode())&&tx.length<2400&&c++<7000){if(n.nodeType==1){var cl=typeof n.className=='string'?n.className:'';if(/(^|[ _-])(star-)?rating/i.test(cl)&&vis(n)){var rt=cl.replace(/star-rating|rating/ig,'').trim();if(rt)tx+='[rating: '+rt+'] '}else if(n.tagName=='IMG'&&n.alt&&vis(n))tx+='[image: '+n.alt.slice(0,60)+'] ';continue}var s=n.nodeValue.trim(),p=n.parentElement;if(s&&p&&!/^(SCRIPT|STYLE|NOSCRIPT|TEMPLATE)$/.test(p.tagName)&&vis(p))tx+=s.replace(/\s+/g,' ')+' '}
tx=tx.slice(0,2400);
return 'URL: '+location.href.slice(0,200)+'\nTitle: '+document.title.slice(0,100)+'\nElements:\n'+out.join('\n')+'\nText: '+tx}catch(x){return 'error: '+String(x).slice(0,80)}})())H";

static const wchar_t* kAgFp = L"(function(){try{var a=document.activeElement;return location.href+'|'+(document.body?document.body.innerText.length:0)+'|'+document.querySelectorAll('*:not([data-lbfx]):not(#__lbcur):not(#__lbcur *)').length+'|'+document.querySelectorAll(':checked,[aria-checked=true],[aria-selected=true],[aria-expanded=true]').length+'|'+(a&&a.value?a.value.length:0)+'|'+scrollY}catch(x){return 'e'}})()";
// Runs an action in the target tab. Returns text; "NEEDCONFIRM:..." = Libra must ask the user first.
static const wchar_t* kAgAct = LR"H((function(op,id,tx,ok,fr){try{
var A=window.__lbA||[],e=A[+id];
var bad=/passw|card|cvv|cvc|cc-|credit|debit|iban|routing|account.?n(um|o)|otp|one-time|2fa|mfa|verif(y|ication).?code|ssn|social.?security|aadhaar|security code|secret|\bpin\b/i;
var sens=/\b(buy|pay|purchase|checkout|place order|order now|delete|remove|send|submit|confirm|transfer|withdraw|subscribe|log ?out|sign ?out|post|publish)\b/i;
var base=function(h){return h.split('.').slice(-2).join('.')};
var cur=function(e){try{e.scrollIntoView({block:'center',inline:'center'});var r=e.getBoundingClientRect(),c=document.getElementById('__lbcur');
 if(!c){c=document.createElement('div');c.id='__lbcur';var S='http://www.w3.org/2000/svg',v=document.createElementNS(S,'svg'),p=document.createElementNS(S,'path');v.setAttribute('width','22');v.setAttribute('height','22');v.setAttribute('viewBox','0 0 24 24');
  p.setAttribute('d','M4 2l16 9-7 2-3 7z');p.setAttribute('fill','#111');p.setAttribute('stroke','#fff');p.setAttribute('stroke-width','1.5');p.setAttribute('stroke-linejoin','round');v.appendChild(p);c.appendChild(v);
  c.style.cssText='position:fixed;left:0;top:0;z-index:2147483647;pointer-events:none;transition:transform .45s cubic-bezier(.3,.7,.2,1)';c.style.transform='translate('+innerWidth/2+'px,'+(innerHeight+10)+'px)';(document.body||document.documentElement).appendChild(c);c.getBoundingClientRect()}
 c.style.transform='translate('+(r.left+Math.min(r.width/2,60))+'px,'+(r.top+r.height/2)+'px)';
 clearTimeout(window.__lbT);window.__lbT=setTimeout(function(){c.remove()},4500)}catch(x){}};
var tap=function(e){try{var r=e.getBoundingClientRect(),x=r.left+Math.min(r.width/2,60),y=r.top+r.height/2,d=document.createElement('div');d.style.cssText='position:fixed;left:'+(x-14)+'px;top:'+(y-14)+'px;width:28px;height:28px;box-sizing:border-box;border-radius:50%;border:3px solid #e5322d;z-index:2147483647;pointer-events:none';d.setAttribute('data-lbfx','1');(document.body||document.documentElement).appendChild(d);d.animate([{transform:'scale(.35)',opacity:1},{transform:'scale(2.4)',opacity:0}],{duration:700,easing:'ease-out'}).onfinish=function(){d.remove()}}catch(x){}};
var go=function(f){cur(e);setTimeout(function(){if(e.isConnected)try{f()}catch(x){}},480)};
if(op=='scroll'){scrollBy(0,(tx=='up'?-1:1)*innerHeight*.8);return 'scrolled'}
if(op=='key'){if(!/^(Enter|Tab|Escape|Space|ArrowDown|ArrowUp)$/.test(tx))return 'REFUSED: key';var f=document.activeElement;
 if(tx=='Enter'&&f&&f.form){if(!ok)return 'NEEDCONFIRM:submit a form';f.form.requestSubmit();return 'submitted'}
 return 'KEYAT:'+tx;
 var o={key:tx,bubbles:true};if(f){f.dispatchEvent(new KeyboardEvent('keydown',o));f.dispatchEvent(new KeyboardEvent('keyup',o))}return 'key '+tx}
if(!e||!e.isConnected)return 'REFUSED: unknown element, look at the new page list';
var lab=(e.getAttribute('aria-label')||e.innerText||e.value||'').replace(/\s+/g,' ').slice(0,60);
if(op=='select'){if(e.tagName!='SELECT')return 'REFUSED: not a dropdown, click it instead';var w=tx.toLowerCase(),op2=Array.prototype.find.call(e.options,function(o){return o.text.trim().toLowerCase()==w||o.value.toLowerCase()==w})||Array.prototype.find.call(e.options,function(o){return o.text.toLowerCase().indexOf(w)>=0});
 if(!op2)return 'REFUSED: no such option';go(function(){e.value=op2.value;e.dispatchEvent(new Event('input',{bubbles:true}));e.dispatchEvent(new Event('change',{bubbles:true}))});return 'selected'}
if(op=='type'){if(e.type=='password'||bad.test([e.name,e.id,e.autocomplete,e.getAttribute('aria-label'),e.placeholder].join(' ')))return 'REFUSED: the user must type this field';if(/^\s*(?:\d[ -]*){13,19}\s*$/.test(String(tx||'')))return 'REFUSED: that looks like a card number; the user must type it';
 if(e.isContentEditable){go(function(){e.focus();document.execCommand('selectAll');document.execCommand('insertText',false,tx)});return 'typed'}
 var d=Object.getOwnPropertyDescriptor(Object.getPrototypeOf(e),'value');if(!d||!d.set||e.type=='file')return 'REFUSED: not a text field';
 go(function(){e.focus();d.set.call(e,tx);e.dispatchEvent(new Event('input',{bubbles:true}));e.dispatchEvent(new Event('change',{bubbles:true}))});return 'typed'}
if(op=='click'){var a=e.closest('a[href]');
 if(!ok&&/^(BUTTON|A)$/.test(e.tagName)&&document.querySelector('input[autocomplete^="cc-"],input[name*="card" i],input[id*="card" i]'))return 'NEEDCONFIRM:continue on a page that asks for payment details';
 if(!ok&&(sens.test(lab)||(e.type=='submit'&&e.form)))return 'NEEDCONFIRM:click "'+lab+'"';
 if(!ok&&!fr&&a&&a.hostname&&base(a.hostname)!=base(location.hostname))return 'NEEDCONFIRM:leave this site for '+a.hostname;
 cur(e);var q=e.getBoundingClientRect(),cx=Math.round(q.left+Math.min(q.width/2,60)),cy=Math.round(q.top+q.height/2),t=document.elementFromPoint(cx,cy);if(t&&(t===e||e.contains(t)||t.contains(e)))return 'CLICKAT:'+cx+','+cy;go(function(){tap(e);e.click()});return 'clicked'}
return 'REFUSED'}catch(x){return 'error: '+String(x).slice(0,80)}}))H";

// ── memory: the user's own notes/documents (Markdown), encrypted on disk, sent to the provider only inside a task ──
struct AgDoc { std::wstring name, text; bool on = true; };
static std::vector<AgDoc> AgMemLoad() {
    std::vector<AgDoc> d; std::string raw = ReadAll(DataDir() + L"\\memory.dat"); size_t s = 0;
    while (s < raw.size()) {
        size_t e = raw.find('\n', s); if (e == std::string::npos) e = raw.size(); std::string ln = raw.substr(s, e - s); s = e + 1;
        size_t a = ln.find('\t'), b = a == std::string::npos ? a : ln.find('\t', a + 1); if (b == std::string::npos) continue;
        AgDoc x; x.name = PwDec(ln.substr(0, a)); x.on = ln[a + 1] == '1'; x.text = PwDec(ln.substr(b + 1)); d.push_back(std::move(x));
    }
    return d;
}
static void AgMemSave(const std::vector<AgDoc>& d) { std::string o; for (auto& x : d) o += PwEnc(x.name) + "\t" + (x.on ? "1" : "0") + "\t" + PwEnc(x.text) + "\n"; WriteAll(DataDir() + L"\\memory.dat", o); }
static std::wstring AgUnpct(const std::wstring& s) { std::string n; for (wchar_t ch : s) n += ch < 128 ? (char)ch : '?'; return PwDec(n); }
static std::wstring AgMemCtx(const std::vector<AgDoc>& d, const std::wstring& q) {   // picks the notes that match the task, up to 6000 characters
    struct Ch { size_t di; int score; std::wstring t; };
    std::vector<Ch> cs; std::set<std::wstring> w; { std::wstring cur; for (wchar_t c : q + L" ") { if (iswalnum(c)) cur += (wchar_t)towlower(c); else { if (cur.size() >= 4) w.insert(cur); cur.clear(); } } }
    for (size_t i = 0; i < d.size(); ++i) {
        if (!d[i].on) continue; const bool facts = d[i].name == L"My facts"; std::wstring acc; size_t st = 0;
        auto flush = [&]() { if (acc.empty()) return; std::wstring lo = acc; for (auto& c : lo) c = (wchar_t)towlower(c); int sc = facts ? 1 : 0; for (auto& x : w) if (lo.find(x) != std::wstring::npos) ++sc; cs.push_back({ i, sc, acc }); acc.clear(); };
        while (st < d[i].text.size()) { size_t e = d[i].text.find(L'\n', st); if (e == std::wstring::npos) e = d[i].text.size(); std::wstring ln = d[i].text.substr(st, e - st) + L"\n"; st = e + 1; if (ln == L"\n") { flush(); continue; } if (facts || acc.size() + ln.size() > 500) flush(); acc += ln; }
        flush();
    }
    std::vector<size_t> idx(cs.size()); for (size_t k = 0; k < idx.size(); ++k) idx[k] = k;
    std::stable_sort(idx.begin(), idx.end(), [&](size_t x, size_t y) { return cs[x].score > cs[y].score; });
    size_t used = 0; std::vector<char> pick(cs.size(), 0);
    for (size_t k : idx) { if (used + cs[k].t.size() + 12 > 12000) continue; pick[k] = 1; used += cs[k].t.size() + 12; }
    std::wstring out; size_t last = (size_t)-1;
    for (size_t k = 0; k < cs.size(); ++k) if (pick[k]) { if (cs[k].di != last) { out += L"## " + d[cs[k].di].name + L"\n"; last = cs[k].di; } out += cs[k].t; }
    return out;
}
// Saves one memory document. Returns "" when saved, else an error text that starts with a number (E-1x) the user can quote.
static std::wstring AgMemPut(std::wstring nm, std::wstring tx) {
    for (wchar_t& ch : nm) if (ch < 32) ch = L' ';
    if (nm.empty() || nm.size() > 60) return L"E-12 The file name is empty or longer than 60 characters.";
    if (tx.empty()) return L"E-12 That file is empty or has no readable text.";
    if (tx.size() > 200000) return L"E-13 That file is too large (limit 200 KB of text per file).";
    auto d = AgMemLoad(); size_t tot = tx.size(); bool same = false; for (auto& x : d) { if (x.name == nm) same = true; else tot += x.text.size(); }
    if (tot > 600000 || (!same && d.size() >= 20)) return L"E-14 Memory is full (600 KB or 20 files). Delete a file first.";
    bool done = false; for (auto& x : d) if (x.name == nm) { x.text = tx; x.on = true; done = true; }
    if (!done) { AgDoc x; x.name = nm; x.text = tx; d.push_back(std::move(x)); }
    AgMemSave(d);
    bool ok = false; for (auto& x : AgMemLoad()) if (x.name == nm && x.text.size() == tx.size()) ok = true;
    return ok ? L"" : L"E-18 Could not write the memory file to disk. Check free space and folder access.";
}
static std::wstring AgMemJs(const std::wstring& q = L"") {
    auto d = AgMemLoad(); std::wstring docs = L"[";
    for (size_t i = 0; i < d.size(); ++i) docs += (i ? L"," : L"") + std::wstring(L"[") + AgJs(d[i].name) + L"," + std::to_wstring(d[i].text.size()) + L"," + (d[i].on ? L"1" : L"0") + L"]";
    return L"lbMem({docs:" + docs + L"],ctx:" + AgJs(AgMemCtx(d, q)) + L"})";
}

void BrowserWindow::AgSend(const std::wstring& js) {
    for (auto& t : m_tabs) if (t->url == L"libra://agent" && t->webView) {
        wil::unique_cotaskmem_string src; t->webView->get_Source(&src);
        if (!src.get() || IsHttp(src.get())) continue;   // never into a page that came from the web, whatever its tab says
        t->webView->ExecuteScript(js.c_str(), nullptr); return; }
}
void BrowserWindow::AgStop(const wchar_t* why) {
    m_agPaused = false;
    AgFileHook(false);
    if (m_agRes && m_agHome) {   // never leave a research tab behind
        int hi = IndexOf(m_agHome), ri = IndexOf(m_agRes);
        if (hi >= 0 && ri >= 0) { CloseTab(ri); hi = IndexOf(m_agHome); if (hi >= 0) SwitchTab(hi); m_agTarget = m_agHome; if (m_agDock) ApplyDock(); }
        m_agRes = nullptr; m_agHome = nullptr;
    }
    if (m_agRun) { int ti = AgTarget(); if (ti >= 0) m_tabs[ti]->webView->ExecuteScript(L"(function(){var c=document.getElementById('__lbcur');if(c)c.remove()})()", nullptr); }   // remove the pointer overlay
    m_agRun = false; ++m_agGen;
    if (why && *why) AgSend(L"lbStop(" + AgJs(why) + L")");
}
int BrowserWindow::AgTarget() const {   // target tab index, or -1 (also ends the run when it is gone)
    int i = m_agTarget ? IndexOf(m_agTarget) : -1;
    return (i >= 0 && m_tabs[i]->webView) ? i : -1;
}
static std::wstring AgOkStr(const wchar_t* r) {   // WebView2 gives a JSON string literal; accept only that, cap its size
    std::wstring s = r ? r : L"";
    if (s.size() < 2 || s.front() != L'"' || s.back() != L'"' || s.size() > 24000) return L"\"error\"";
    return s;
}
// ── embedded frames (investor forms are often embedded: Typeform, HubSpot, Airtable, JotForm...). Every frame, also other-site ones, is read by the same script, numbered after the page's own elements. ──
void BrowserWindow::AgTrackFrames(Tab* t) {
    wil::com_ptr<ICoreWebView2_4> w4; if (!t->webView || FAILED(t->webView->QueryInterface(IID_PPV_ARGS(&w4)))) return;
    auto track = std::make_shared<std::function<void(ICoreWebView2Frame*)>>(); std::weak_ptr<std::function<void(ICoreWebView2Frame*)>> wt = track;
    *track = [this, t, wt](ICoreWebView2Frame* f) {
        wil::com_ptr<ICoreWebView2Frame2> f2; if (!f || FAILED(f->QueryInterface(IID_PPV_ARGS(&f2))) || IndexOf(t) < 0) return;
        t->frames.push_back(f2); if (t->frames.size() > 40) t->frames.erase(t->frames.begin());
        wil::com_ptr<IUnknown> id; f->QueryInterface(IID_PPV_ARGS(&id)); EventRegistrationToken tk{};
        f->add_Destroyed(Microsoft::WRL::Callback<ICoreWebView2FrameDestroyedEventHandler>([this, t, id](ICoreWebView2Frame*, IUnknown*) -> HRESULT {
            if (IndexOf(t) < 0) return S_OK;
            t->frames.erase(std::remove_if(t->frames.begin(), t->frames.end(), [&](wil::com_ptr<ICoreWebView2Frame2>& x) { wil::com_ptr<IUnknown> u; x->QueryInterface(IID_PPV_ARGS(&u)); return u.get() == id.get(); }), t->frames.end()); return S_OK; }).Get(), &tk);
        wil::com_ptr<ICoreWebView2Frame7> f7;
        if (auto sp = wt.lock(); sp && SUCCEEDED(f->QueryInterface(IID_PPV_ARGS(&f7)))) f7->add_FrameCreated(Microsoft::WRL::Callback<ICoreWebView2FrameChildFrameCreatedEventHandler>(
            [sp](ICoreWebView2Frame*, ICoreWebView2FrameCreatedEventArgs* a) -> HRESULT { wil::com_ptr<ICoreWebView2Frame> c; if (a) a->get_Frame(&c); (*sp)(c.get()); return S_OK; }).Get(), &tk);
    };
    EventRegistrationToken tk{};
    w4->add_FrameCreated(Microsoft::WRL::Callback<ICoreWebView2FrameCreatedEventHandler>([track](ICoreWebView2*, ICoreWebView2FrameCreatedEventArgs* a) -> HRESULT {
        wil::com_ptr<ICoreWebView2Frame> f; if (a) a->get_Frame(&f); (*track)(f.get()); return S_OK; }).Get(), &tk);
}
static int AgCountEls(const std::wstring& lit) {   // how many "[n] ..." lines a snapshot literal holds (before its Text: part)
    size_t end = lit.find(L"\\nText: "); if (end == std::wstring::npos) end = lit.size(); int n = 0;
    for (size_t p = lit.find(L"\\n["); p != std::wstring::npos && p < end; p = lit.find(L"\\n[", p + 3)) if (p + 3 < lit.size() && iswdigit(lit[p + 3])) ++n;
    return n;
}
static std::wstring AgFrameSnap(int base) {   // the page snapshot script, numbered from `base`, headed "FRAME host"
    std::wstring s = kAgSnap;
    auto rp = [&](const std::wstring& a, const std::wstring& b) { size_t p = s.find(a); if (p != std::wstring::npos) s.replace(p, a.size(), b); };
    rp(LR"('['+arr.length+'] ')", L"'['+(" + std::to_wstring(base) + LR"(+arr.length)+'] ')");
    rp(LR"('URL: '+location.href.slice(0,200)+'\nTitle: '+document.title.slice(0,100)+'\nElements:\n')", LR"('FRAME '+location.hostname+'\n')");
    return s;
}
void BrowserWindow::AgSnapFrames(Tab* t, unsigned gen, std::wstring acc, std::vector<wil::com_ptr<ICoreWebView2Frame2>> fr, size_t i, int total, std::vector<AgFr> got) {
    if (gen != m_agGen || !m_agRun || IndexOf(t) < 0) return;
    if (i >= fr.size() || total > 240 || i >= 12) { m_agFr = got; AgSend(L"lbSnap(" + acc + L")"); return; }
    std::wstring js = AgFrameSnap(total); auto f = fr[i];
    HRESULT hr = f->ExecuteScript(js.c_str(), Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
        [this, t, gen, acc, fr, i, total, got, f](HRESULT, LPCWSTR r) mutable -> HRESULT {
            std::wstring s = AgOkStr(r); std::wstring a2 = acc; int tot2 = total;
            if (s.rfind(L"\"HUMAN_CHECK", 0) == 0) { a2 = s; got.clear(); m_agFr.clear(); AgSend(L"lbSnap(" + a2 + L")"); return S_OK; }   // a CAPTCHA frame: pause like on the main page
            int n = AgCountEls(s);
            if (n > 0 && s.rfind(L"\"FRAME ", 0) == 0) {
                std::string host; for (size_t k = 7; k < s.size() && s[k] != L'\\' && s[k] != L'"'; ++k) host += (char)(s[k] < 128 ? s[k] : '?');
                if (!DohProxy::AdLike(host)) { a2 = acc + L"+\"\\n\"+" + s; got.push_back({ f, total, n }); tot2 = total + n; }   // ad frames are skipped
            }
            AgSnapFrames(t, gen, a2, fr, i + 1, tot2, got); return S_OK; }).Get());
    if (FAILED(hr)) AgSnapFrames(t, gen, acc, fr, i + 1, total, got);
}

void BrowserWindow::AgRunAct(int ti, const std::wstring& op, const std::wstring& id, const std::wstring& tx, bool ok) {
    unsigned gen = m_agGen;
    bool fr = false;   // on a search-engine page, result links may be followed without asking
    { dl::Url cu; if (dl::Crack(m_tabs[ti]->url, cu)) for (const wchar_t* e : { L"google.", L"bing.", L"duckduckgo.", L"brave.", L"startpage.", L"ecosia.", L"perplexity." }) if (cu.host.find(e) == 0 || cu.host.find(std::wstring(L".") + e) != std::wstring::npos) fr = true; }
    std::wstring script = kAgAct; wil::com_ptr<ICoreWebView2Frame2> frame; int rid = _wtoi(id.c_str());
    if ((op == L"click" || op == L"type" || op == L"select") && rid >= m_agMainN) for (auto& g : m_agFr) if (rid >= g.base && rid < g.base + g.n) { frame = g.f; rid -= g.base; break; }
    if (frame) { size_t p = script.find(LR"(return 'CLICKAT:'+cx+','+cy;)"); if (p != std::wstring::npos) script.erase(p, wcslen(LR"(return 'CLICKAT:'+cx+','+cy;)")); }   // inside a frame the page element itself is clicked
    std::wstring js = script + L"(" + AgJs(op) + L"," + std::to_wstring(rid) + L"," + AgJs(tx) + L"," + (ok ? L"true" : L"false") + L"," + (fr ? L"true" : L"false") + L")";
    auto handler = Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
        [this, gen, op, id, tx, ok, rid, frame, t = m_tabs[ti].get()](HRESULT, LPCWSTR r) -> HRESULT {
            if (gen != m_agGen || !m_agRun || IndexOf(t) < 0) return S_OK;
            std::wstring s = AgOkStr(r);
            if (!ok && s.rfind(L"\"NEEDCONFIRM:", 0) == 0) {
                std::wstring what = s.substr(13, s.size() - 14);
                std::wstring q = L"The Libra agent wants to " + what + L".\n\nAllow this once?";
                const bool pay = what.find(L"payment") != std::wstring::npos; const std::wstring cat = pay ? L"payment" : what.rfind(L"leave this site", 0) == 0 ? L"leave_site" : L"submit";
                int a = AgAsk(cat, AgDomOf(t->url), L"Allow the assistant to " + what + L"?", L"This happens on " + AgDomOf(t->url) + L".", pay) ? IDYES : IDNO;
                if (gen != m_agGen || !m_agRun || IndexOf(t) < 0) return S_OK;
                if (a != IDYES) { AgSend(L"lbAct(\"The user said no.\")"); return S_OK; }
                AgRunAct(IndexOf(t), op, id, tx, true); return S_OK;
            }
            if (s.rfind(L"\"KEYAT:", 0) == 0) {   // a real key press through the browser engine
                std::wstring k = s.substr(7, s.size() - 8); const wchar_t* code = k == L"Enter" ? L"Enter" : k == L"Tab" ? L"Tab" : k == L"Escape" ? L"Escape" : k == L"Space" ? L"Space" : k == L"ArrowDown" ? L"ArrowDown" : L"ArrowUp";
                int vk = k == L"Enter" ? 13 : k == L"Tab" ? 9 : k == L"Escape" ? 27 : k == L"Space" ? 32 : k == L"ArrowDown" ? 40 : 38; std::wstring key = k == L"Space" ? L" " : k;
                for (const wchar_t* ty : { L"rawKeyDown", L"keyUp" }) { std::wstring j = std::wstring(L"{\"type\":\"") + ty + L"\",\"key\":\"" + key + L"\",\"code\":\"" + code + L"\",\"windowsVirtualKeyCode\":" + std::to_wstring(vk) + L"}"; if (t->webView) t->webView->CallDevToolsProtocolMethod(L"Input.dispatchKeyEvent", j.c_str(), nullptr); }
                AgSend(L"lbAct(\"key " + k + L" pressed\")"); return S_OK;
            }
            if (s.rfind(L"\"CLICKAT:", 0) == 0) {   // a real (trusted) mouse click through the browser engine, so sites and file choosers accept it
                m_agClickX = _wtoi(s.c_str() + 9); size_t cm = s.find(L','); m_agClickY = cm == std::wstring::npos ? 0 : _wtoi(s.c_str() + cm + 1);
                m_agClickTab = t; m_agClickId = id; m_agClickRid = rid; m_agClickFrame = nullptr; SetTimer(m_hWnd, 2021, 500, nullptr); return S_OK;   // the answer comes after we have checked that the click did something
            }
            if (op == L"click" && s == L"\"clicked\"") {   // a page-level click (covered element, or inside a frame): check it really did something
                m_agClickTab = t; m_agClickId = id; m_agClickRid = rid; m_agClickFrame = frame; m_agTry = 0; m_agClickX = m_agClickY = 0;
                auto fpH = Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>([this, gen](HRESULT, LPCWSTR r) -> HRESULT { if (gen == m_agGen) m_agFp = r ? r : L""; return S_OK; });
                AgExecT(kAgFp, fpH.Get()); SetTimer(m_hWnd, 2022, 1300, nullptr); return S_OK;
            }
            AgSend(L"lbAct(" + s + L")"); return S_OK;
        });
    if (frame) frame->ExecuteScript(js.c_str(), handler.Get()); else m_tabs[ti]->webView->ExecuteScript(js.c_str(), handler.Get());
}

#include <spellcheck.h>
static int AgLev(const std::wstring& a, const std::wstring& b) { std::vector<int> p(b.size() + 1), c(b.size() + 1); for (size_t j = 0; j <= b.size(); ++j) p[j] = (int)j; for (size_t i = 1; i <= a.size(); ++i) { c[0] = (int)i; for (size_t j = 1; j <= b.size(); ++j) c[j] = std::min({ p[j] + 1, c[j - 1] + 1, p[j - 1] + (a[i - 1] != b[j - 1]) }); p.swap(c); } return p[b.size()]; }
// Local polish for prose the assistant types: Windows' own spell checker (offline) + a few grammar rules. Words the user used themselves (task, memory) are never changed.
static std::wstring AgPolish(std::wstring t, const std::wstring& protect) {
    size_t sp = std::count(t.begin(), t.end(), L' '); if (sp < 2 || t.find(L'@') != std::wstring::npos || t.find(L"http") != std::wstring::npos || t.find(L"www.") != std::wstring::npos) return t;
    auto lower = [](std::wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; };
    wil::com_ptr<ISpellCheckerFactory> f;
    if (SUCCEEDED(CoCreateInstance(__uuidof(SpellCheckerFactory), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&f)))) {
        BOOL sup = FALSE; wil::com_ptr<ISpellChecker> sc; wil::com_ptr<IEnumSpellingError> en;
        if (SUCCEEDED(f->IsSupported(L"en-US", &sup)) && sup && SUCCEEDED(f->CreateSpellChecker(L"en-US", &sc)) && SUCCEEDED(sc->ComprehensiveCheck(t.c_str(), &en)) && en) {
            struct Fix { ULONG s, l; std::wstring r; }; std::vector<Fix> fx;
            for (;;) {
                wil::com_ptr<ISpellingError> e; if (en->Next(&e) != S_OK) break; ULONG s = 0, l = 0; CORRECTIVE_ACTION a = CORRECTIVE_ACTION_NONE; e->get_StartIndex(&s); e->get_Length(&l); e->get_CorrectiveAction(&a);
                if (s + l > t.size() || l == 0) continue; std::wstring w = t.substr(s, l), lw = lower(w);
                bool odd = iswupper(w[0]) && s > 0 && t[s - 1] != L'.' && t[s - 1] != L' ' ? true : false; (void)odd;
                if (protect.find(L" " + lw + L" ") != std::wstring::npos || std::any_of(w.begin(), w.end(), [](wchar_t c) { return iswdigit(c) || c == L'@' || c == L'_' || c == L'\''; })) continue;
                if (a == CORRECTIVE_ACTION_DELETE) { fx.push_back({ s, l + (s + l < t.size() && t[s + l] == L' ' ? 1u : 0u), L"" }); }
                else if (a == CORRECTIVE_ACTION_REPLACE) { PWSTR r = nullptr; if (SUCCEEDED(e->get_Replacement(&r)) && r) { fx.push_back({ s, l, r }); CoTaskMemFree(r); } }
                else if (a == CORRECTIVE_ACTION_GET_SUGGESTIONS && !iswupper(w[0])) {
                    wil::com_ptr<IEnumString> sug; LPOLESTR s1 = nullptr; ULONG got = 0;
                    if (SUCCEEDED(sc->Suggest(w.c_str(), &sug)) && sug && sug->Next(1, &s1, &got) == S_OK && got && s1) { std::wstring r = s1; CoTaskMemFree(s1); if (AgLev(lw, lower(r)) <= 2 && lower(r)[0] == lw[0] && r.find(L' ') == std::wstring::npos) fx.push_back({ s, l, r }); }
                }
            }
            for (size_t i = fx.size(); i-- > 0;) t.replace(fx[i].s, fx[i].l, fx[i].r);
        }
    }
    std::wstring o; for (size_t i = 0; i < t.size(); ++i) {   // grammar rules
        wchar_t c = t[i]; bool wordStart = i == 0 || !iswalnum(t[i - 1]) && t[i - 1] != L'\'';
        if (c == L'i' && wordStart && (i + 1 == t.size() || !iswalnum(t[i + 1]))) c = L'I';
        if (c == L' ' && i + 1 < t.size() && wcschr(L",.?!;:", t[i + 1]) && !(t[i + 1] == L'.' && i + 2 < t.size() && iswalnum(t[i + 2]))) continue;
        if (c == L' ' && !o.empty() && o.back() == L' ') continue;
        o += c;
    }
    t = o; while (!t.empty() && t.back() == L' ') t.pop_back(); while (!t.empty() && t.front() == L' ') t.erase(t.begin());
    bool cap = true; for (size_t i = 0; i < t.size(); ++i) { if (cap && iswalpha(t[i])) { t[i] = (wchar_t)towupper(t[i]); cap = false; } if (t[i] == L'.' || t[i] == L'?' || t[i] == L'!' || t[i] == L'\n') { if (i + 1 >= t.size() || t[i + 1] == L' ' || t[i + 1] == L'\n') cap = true; } else if (!iswspace(t[i])) cap = cap && !iswalnum(t[i]) ? cap : false; }
    for (size_t i = 0; i + 3 < t.size(); ++i) if ((i == 0 || t[i - 1] == L' ') && t[i] == L'a' && t[i + 1] == L' ' && wcschr(L"aeio", towlower(t[i + 2])) && !(towlower(t[i + 2]) == L'o' && towlower(t[i + 3]) == L'n')) t.replace(i, 1, L"an");
    if (sp >= 5 && !t.empty() && iswalnum(t.back())) t += L'.';
    return t;
}
// ── sandbox: the only folder the assistant may attach files from. The model sees names only, never paths. ──
static std::wstring AgSbx() { std::wstring d = DataDir() + L"\\sandbox"; CreateDirectoryW(d.c_str(), nullptr); return d; }
static bool AgSbxName(const std::wstring& n) { if (n.empty() || n.size() > 120 || n.find_first_of(L"\\/:*?\"<>|") != std::wstring::npos || n[0] == L'.') return false; return true; }
static std::vector<std::pair<std::wstring, unsigned long long>> AgSbxList() {
    std::vector<std::pair<std::wstring, unsigned long long>> v; WIN32_FIND_DATAW fd; HANDLE h = FindFirstFileW((AgSbx() + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) { do { if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && v.size() < 60) v.push_back({ fd.cFileName, ((unsigned long long)fd.nFileSizeHigh << 32) | fd.nFileSizeLow }); } while (FindNextFileW(h, &fd)); FindClose(h); }
    std::sort(v.begin(), v.end()); return v;
}
static std::wstring AgSbxJs() { auto v = AgSbxList(); std::wstring o = L"lbSbx(["; for (size_t i = 0; i < v.size(); ++i) o += (i ? L"," : L"") + std::wstring(L"[") + AgJs(v[i].first) + L"," + std::to_wstring(v[i].second) + L"]"; return o + L"])"; }
// ── permissions: one friendly native window; the choice can be remembered per site for a while ──
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")
#include <ctime>
struct AgGrant { std::wstring cat, dom; long long exp; };
static std::vector<AgGrant> AgGrantsLoad() {
    std::vector<AgGrant> v; const long long now = (long long)time(nullptr);
    for (auto& l : Lines(ReadAll(DataDir() + L"\\agent_perms.txt"))) { auto f = SplitTab(l); if (f.size() == 3) { long long e = _wtoi64(f[2].c_str()); if (e == 0 || e > now) v.push_back({ f[0], f[1], e }); } }
    return v;
}
static void AgGrantsSave(const std::vector<AgGrant>& v) { std::string o; for (auto& g : v) o += ToUtf8(g.cat + L"\t" + g.dom + L"\t" + std::to_wstring(g.exp)) + "\n"; WriteAll(DataDir() + L"\\agent_perms.txt", o); }
static const wchar_t* AgCatName(const std::wstring& c) { return c == L"leave_site" ? L"Open other sites" : c == L"type_site" ? L"Type on a site" : c == L"download" ? L"Download files" : L"Submit and send"; }
static std::wstring AgGrantsJs() {
    auto v = AgGrantsLoad(); std::wstring o = L"lbPerm(["; const long long now = (long long)time(nullptr);
    for (size_t i = 0; i < v.size(); ++i) { std::wstring when = v[i].exp == 0 ? L"always" : (v[i].exp - now > 86400 ? L"for " + std::to_wstring((v[i].exp - now + 86399) / 86400) + L" more days" : L"for less than a day");
        o += (i ? L"," : L"") + std::wstring(L"[") + AgJs(AgCatName(v[i].cat)) + L"," + AgJs(v[i].dom) + L"," + AgJs(when) + L"]"; }
    return o + L"])";
}
void BrowserWindow::AgWipe() {   // agent.key is deliberately left alone
    AgStop(nullptr);
    DeleteFileW((DataDir() + L"\\memory.dat").c_str()); DeleteFileW((DataDir() + L"\\agent_perms.txt").c_str());
    for (auto& f : AgSbxList()) DeleteFileW((AgSbx() + L"\\" + f.first).c_str());
    m_set.Set(L"agent_done", L"0");
    AgSend(AgMemJs()); AgSend(AgGrantsJs());
}
bool BrowserWindow::AgAsk(const std::wstring& cat, const std::wstring& dom, const std::wstring& title, const std::wstring& detail, bool once) {
    const long long now = (long long)time(nullptr);
    if (!once) for (auto& g : AgGrantsLoad()) if (g.cat == cat && g.dom == dom) return true;   // already allowed
    bool allow = false; int remember = 101;   // 101 only now, 102 day, 103 week, 104 month, 105 always
    wchar_t tf[MAX_PATH];
    if (TestEnv(L"LIBRA_TEST_ASK_FILE", tf, MAX_PATH) > 0) {   // automated tests: "y", "y:day|week|month|always" or "n"; every question is logged
        std::string d = ReadAllRaw(tf); allow = !d.empty() && d[0] == 'y'; if (d.find(":day") != std::string::npos) remember = 102; else if (d.find(":week") != std::string::npos) remember = 103; else if (d.find(":month") != std::string::npos) remember = 104; else if (d.find(":always") != std::string::npos) remember = 105;
        WriteAllRaw(std::wstring(tf) + L".log", ToUtf8(cat + L"|" + dom + L"|" + title + L"|" + (allow ? L"yes" : L"no") + L"\n"), true);
    } else {
        TASKDIALOG_BUTTON btns[] = { { 201, L"&Allow" }, { 202, L"Do&n't allow" } };
        TASKDIALOG_BUTTON rad[] = { { 101, L"Only this time" }, { 102, L"For 1 day" }, { 103, L"For 1 week" }, { 104, L"For 1 month" }, { 105, L"Always for this site" } };
        TASKDIALOGCONFIG tc{ sizeof tc }; tc.hwndParent = m_hWnd; tc.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW; tc.pszWindowTitle = L"Libra Assistant asks";
        tc.pszMainInstruction = title.c_str(); tc.pszContent = detail.c_str(); tc.pButtons = btns; tc.cButtons = 2; tc.nDefaultButton = 202; tc.pszMainIcon = TD_WARNING_ICON;
        tc.pRadioButtons = rad; tc.cRadioButtons = once ? 0 : 5; tc.nDefaultRadioButton = 101; tc.pszVerificationText = nullptr;
        tc.pszFooter = once ? L"Payments are always asked." : L"Remember my choice. You can review or revoke permissions in the assistant's settings."; tc.pszFooterIcon = TD_INFORMATION_ICON;
        int btn = 0, rd = 101; if (SUCCEEDED(TaskDialogIndirect(&tc, &btn, &rd, nullptr))) { allow = btn == 201; remember = rd; }
    }
    if (allow && !once && remember != 101) {
        long long span = remember == 102 ? 86400 : remember == 103 ? 7 * 86400 : remember == 104 ? 30 * 86400 : 0;
        auto v = AgGrantsLoad(); v.erase(std::remove_if(v.begin(), v.end(), [&](const AgGrant& g) { return g.cat == cat && g.dom == dom; }), v.end());
        v.push_back({ cat, dom, span ? now + span : 0 }); AgGrantsSave(v); AgSend(AgGrantsJs());
    }
    return allow;
}
static std::wstring AgDomOf(const std::wstring& url) { dl::Url u; return dl::Crack(url, u) ? RegDom(u.host) : L""; }
// ── going to a site directly (no search engines): an address, a domain, or a well-known site name ──
static std::wstring AgSiteUrl(std::wstring t) {
    while (!t.empty() && iswspace(t.back())) t.pop_back(); while (!t.empty() && iswspace(t.front())) t.erase(t.begin());
    if (t.empty() || t.size() > 300 || t.find(L' ') != std::wstring::npos) return L"";
    std::wstring l; for (wchar_t c : t) l += (wchar_t)towlower(c);
    if (l.rfind(L"http://", 0) == 0 || l.rfind(L"https://", 0) == 0) return t;
    if (l.find(L'.') != std::wstring::npos || l.rfind(L"localhost", 0) == 0) return (l.rfind(L"localhost", 0) == 0 ? L"http://" : L"https://") + t;
    static const struct { const wchar_t* k; const wchar_t* u; } M[] = { { L"google", L"https://www.google.com" }, { L"youtube", L"https://www.youtube.com" }, { L"wikipedia", L"https://en.wikipedia.org" }, { L"amazon", L"https://www.amazon.in" }, { L"flipkart", L"https://www.flipkart.com" },
        { L"makemytrip", L"https://www.makemytrip.com" }, { L"ixigo", L"https://www.ixigo.com" }, { L"cleartrip", L"https://www.cleartrip.com" }, { L"goibibo", L"https://www.goibibo.com" }, { L"irctc", L"https://www.irctc.co.in" }, { L"github", L"https://github.com" },
        { L"linkedin", L"https://www.linkedin.com" }, { L"twitter", L"https://x.com" }, { L"x", L"https://x.com" }, { L"facebook", L"https://www.facebook.com" }, { L"instagram", L"https://www.instagram.com" }, { L"reddit", L"https://www.reddit.com" }, { L"gmail", L"https://mail.google.com" },
        { L"maps", L"https://www.google.com/maps" }, { L"flights", L"https://www.google.com/travel/flights" }, { L"news", L"https://news.google.com" }, { L"bbc", L"https://www.bbc.com" }, { L"ilovepdf", L"https://www.ilovepdf.com" }, { L"typeform", L"https://www.typeform.com" },
        { L"swiggy", L"https://www.swiggy.com" }, { L"zomato", L"https://www.zomato.com" }, { L"myntra", L"https://www.myntra.com" }, { L"naukri", L"https://www.naukri.com" }, { L"indeed", L"https://in.indeed.com" }, { L"stackoverflow", L"https://stackoverflow.com" }, { L"quora", L"https://www.quora.com" } };
    for (auto& m : M) if (l == m.k) return m.u;
    bool ok = l.size() >= 3 && l.size() <= 40; for (wchar_t c : l) if (!(iswalnum(c) && c < 128) && c != L'-') ok = false;
    return ok ? L"https://www." + l + L".com" : L"";   // a single brand word: try its .com
}
static bool AgTestLocal() { return TestEnv(L"LIBRA_TEST_LOCAL", nullptr, 0) > 0; }
static bool AgPrivateHost(const std::wstring& h0, const std::wstring& key) {   // LAN, loopback and router addresses are off limits unless the user named them
    std::wstring h; for (wchar_t c : h0) h += (wchar_t)towlower(c); if (h.empty()) return true;
    std::wstring flat; for (wchar_t c : h) if (iswalnum(c)) flat += c;
    if (flat.size() >= 6 && key.find(flat) != std::wstring::npos) return false;
    auto ends = [&](const wchar_t* x) { size_t l = wcslen(x); return h.size() > l && h.compare(h.size() - l, l, x) == 0; };
    if (h == L"localhost" || h.rfind(L"127.", 0) == 0) return !AgTestLocal();
    if (ends(L".localhost") || ends(L".local") || ends(L".internal") || ends(L".lan") || ends(L".home") || h.find(L':') != std::wstring::npos || h[0] == L'[') return true;
    int a = 0, b = 0, c = 0, d = 0; if (swscanf_s(h.c_str(), L"%d.%d.%d.%d", &a, &b, &c, &d) == 4) return a == 10 || a == 0 || (a == 192 && b == 168) || (a == 172 && b >= 16 && b <= 31) || (a == 169 && b == 254);
    return false;
}
static bool AgLeaks(const std::wstring& url) {   // never put the user's saved details into a web address
    auto norm = [](const std::wstring& s) { std::wstring o; for (size_t i = 0; i < s.size(); ++i) { wchar_t c = s[i]; if (c == L'%' && i + 2 < s.size() && iswxdigit(s[i + 1]) && iswxdigit(s[i + 2])) { c = (wchar_t)wcstol(s.substr(i + 1, 2).c_str(), nullptr, 16); i += 2; } if (iswalnum(c)) o += (wchar_t)towlower(c); } return o; };
    const std::wstring u = norm(url);
    for (auto& d : AgMemLoad()) {
        if (!d.on) continue; size_t s = 0;
        while (s < d.text.size()) {
            size_t e = d.text.find(L'\n', s); if (e == std::wstring::npos) e = d.text.size(); std::wstring ln = d.text.substr(s, e - s); s = e + 1;
            size_t colon = ln.find(L':'); std::wstring val = colon == std::wstring::npos ? L"" : ln.substr(colon + 1);
            for (const std::wstring& cand : { norm(ln), norm(val) }) if (cand.size() >= 12 && u.find(cand) != std::wstring::npos) return true;
        }
    }
    return false;
}

void BrowserWindow::OnAgentPage(int idx, const std::vector<std::wstring>& v) {
    if (idx < 0 || idx >= (int)m_tabs.size() || m_tabs[idx]->url != L"libra://agent" || m_tabs[idx]->incognito) return;
    const std::wstring& c = v[0]; const std::wstring a = v.size() > 1 ? v[1] : L"";
    if (c == L"agent.cfg") {
        AgSend(L"lbCfg({on:" + std::wstring(m_set.Get(L"agent_on", L"0") == L"1" ? L"1" : L"0") + L",ep:" + AgJs(m_set.Get(L"agent_ep")) + L",md:" + AgJs(m_set.Get(L"agent_model")) +
               L",opt:" + std::wstring(m_set.Get(L"agent_opt", L"1") == L"0" ? L"0" : L"1") + L",vauto:" + std::wstring(m_set.Get(L"agent_vauto", L"1") == L"0" ? L"0" : L"1") + L",vl:" + AgJs(m_set.Get(L"agent_vlang", L"auto")) + L",done:" + std::to_wstring(_wtoi(m_set.Get(L"agent_done", L"0").c_str())) + L",se:" + AgJs(m_set.Get(L"agent_search", L"ddg")) + L",key:" + std::wstring(ReadAll(DataDir() + L"\\agent.key").empty() ? L"0" : L"1") + L"})");
    } else if (c == L"agent.on") { m_set.Set(L"agent_on", a == L"1" ? L"1" : L"0"); if (a != L"1") AgStop(L"Agent is off."); }
    else if (c == L"agent.endpoint") { if (AgEndpointOk(a)) m_set.Set(L"agent_ep", a); else if (!a.empty()) AgSend(L"lbMsg('Endpoint must start with https:// (or http://localhost)')"); }
    else if (c == L"agent.model") { if (AgModelOk(a)) m_set.Set(L"agent_model", a); }
    else if (c == L"agent.key") {   // one key at a time: remove the old one before adding another; a new key is tested at once
        bool ok = a.size() <= 512 && a.size() >= 8; for (wchar_t ch : a) if (ch < 33 || ch > 126) ok = false;
        const bool have = !ReadAll(DataDir() + L"\\agent.key").empty();
        if (a.empty()) { AgKeySet(L""); AgStop(nullptr); AgSend(L"lbKeyRes('none','')"); }
        else if (have) AgSend(L"lbKeyRes('bad','An API key is already saved. Remove it first to add a different one.')");
        else if (!ok) AgSend(L"lbKeyRes('bad','That does not look like an API key. Please check it.')");
        else {
            const std::wstring ep = m_set.Get(L"agent_ep"), md = m_set.Get(L"agent_model");
            if (!AgEndpointOk(ep) || !AgModelOk(md)) AgSend(L"lbKeyRes('warn','Key saved. Choose a provider and model so I can test it.')");
            else {
                AgKeySet(a);
                std::string mt = ep.find(L"/v1/messages") != std::wstring::npos ? "\"max_tokens\":1," : "", key = ToUtf8(a), body = "{\"model\":\"" + ToUtf8(md) + "\"," + mt + "\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}"; int port = DohProxy::Get().Port(); HWND hw = m_hWnd;
                std::thread([hw, port, ep, key, body]() mutable {
                    std::string r = AgPost(port, ep, key, body); SecureZeroMemory(key.data(), key.size());
                    PostMessage(hw, WM_APP + 35, 0, (LPARAM) new std::wstring(FromUtf8(r)));
                }).detach();
            }
        }
    }
    else if (c == L"agent.stop") AgStop(nullptr);
    else if (c == L"agent.close") PostMessage(m_hWnd, WM_APP + 33, 0, 0);   // closing destroys this very view: do it after the handler returns
    else if (c == L"agent.run") {
        const std::wstring ep = m_set.Get(L"agent_ep");
        const wchar_t* why = nullptr;
        if (m_child) why = L"Not available in Child Safety mode.";
        else if (m_set.Get(L"agent_on", L"0") != L"1") why = L"Turn the agent On in Setup first.";
        else if (!AgEndpointOk(ep) || !AgModelOk(m_set.Get(L"agent_model"))) why = L"Set the endpoint and model first.";
        else if (!AgIsLocal(ep) && ReadAll(DataDir() + L"\\agent.key").empty()) why = L"Add your API key first.";
        int ti = -1; std::wstring host;
        if (!why) {   // docked: the tab the user is looking at; else the web tab nearest to this page
            if (m_agDock && m_activeTab != idx && m_activeTab >= 0 && m_activeTab < (int)m_tabs.size()) { Tab* at = m_tabs[m_activeTab].get(); if (!at->incognito && at->webView && (IsHttp(at->url) || at->url == L"libra://home")) ti = m_activeTab; }
            for (int d = 1; ti < 0 && d < (int)m_tabs.size() && ti < 0; ++d)
                for (int k : { idx - d, idx + d }) if (k >= 0 && k < (int)m_tabs.size() && !m_tabs[k]->incognito && m_tabs[k]->webView && (IsHttp(m_tabs[k]->url) || m_tabs[k]->url == L"libra://home")) { ti = k; break; }
            if (ti < 0) why = L"Open a normal web page or a new tab first."; else { dl::Url u; host = dl::Crack(m_tabs[ti]->url, u) ? u.host : L"a new tab"; }
        }
        if (why) { AgSend(L"lbStop(" + AgJs(why) + L")"); return; }
        m_agTarget = m_tabs[ti].get(); m_agScope = RegDom(host); m_agSteps = 0; m_agStart = GetTickCount(); m_agRun = true; ++m_agGen;
        m_agAttach.clear(); m_agTabs = 0; m_agHome = nullptr; m_agRes = nullptr; m_agTypeOk.clear(); m_agFcN = 0;
        m_agTaskRaw = a; m_agTaskKey.clear(); for (wchar_t ch : a) if (iswalnum(ch)) m_agTaskKey += (wchar_t)towlower(ch);
        AgFileHook(true);
        AgSend(L"lbRunOk(" + AgJs(host) + L")");
    } else if (c == L"agent.pause") { m_agPaused = true; int pi = m_agTarget ? IndexOf(m_agTarget) : -1; if (pi >= 0 && m_tabs[pi]->webView) m_tabs[pi]->webView->ExecuteScript(L"(function(){var c=document.getElementById('__lbcur');if(c)c.remove()})()", nullptr); }
    else if (c == L"agent.resume") m_agPaused = false;
    else if (c == L"agent.opt") m_set.Set(L"agent_opt", a == L"0" ? L"0" : L"1");
    else if (c == L"agent.vauto") m_set.Set(L"agent_vauto", a == L"0" ? L"0" : L"1");
    else if (c == L"agent.vlang") { static const wchar_t* ok[] = { L"auto", L"en", L"hi", L"bn", L"ta", L"te", L"mr", L"gu", L"kn", L"ml", L"pa", L"ur", L"es", L"fr", L"de", L"ar", L"zh", L"ja", L"ru", L"pt" }; for (auto o : ok) if (a == o) m_set.Set(L"agent_vlang", a); }
    else if (c == L"agent.voice") VoiceCmd(a, v.size() > 2 ? v[2] : L"");
    else if (c == L"agent.voicetest") VoiceCmd(L"test", a);
    else if (c == L"agent.sbxlist") AgSend(AgSbxJs());
    else if (c == L"agent.sbxdel") { auto v = AgSbxList(); size_t i = (size_t)_wtoi(a.c_str()); if (i < v.size()) DeleteFileW((AgSbx() + L"\\" + v[i].first).c_str()); AgSend(AgSbxJs()); }
    else if (c == L"agent.sbxadd") {   // the USER picks files; they are copied into the sandbox (25 MB each, 60 files)
        wil::com_ptr<IFileOpenDialog> d;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) {
            DWORD o = 0; d->GetOptions(&o); d->SetOptions(o | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_ALLOWMULTISELECT); d->SetTitle(L"Add files to the assistant's sandbox");
            if (SUCCEEDED(d->Show(m_hWnd))) {
                wil::com_ptr<IShellItemArray> arr; DWORD n = 0;
                if (SUCCEEDED(d->GetResults(&arr)) && SUCCEEDED(arr->GetCount(&n)))
                    for (DWORD i = 0; i < n && i < 20; ++i) { wil::com_ptr<IShellItem> it; PWSTR ps = nullptr; if (SUCCEEDED(arr->GetItemAt(i, &it)) && SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &ps))) {
                        std::wstring src = ps; CoTaskMemFree(ps); size_t sl = src.find_last_of(L"\\/"); std::wstring nm = sl == std::wstring::npos ? src : src.substr(sl + 1);
                        WIN32_FILE_ATTRIBUTE_DATA fa; if (AgSbxName(nm) && GetFileAttributesExW(src.c_str(), GetFileExInfoStandard, &fa) && ((((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow) <= 25ull * 1024 * 1024) && AgSbxList().size() < 60) CopyFileW(src.c_str(), (AgSbx() + L"\\" + nm).c_str(), FALSE); } }
            }
        }
        AgSend(AgSbxJs());
    }
    else if (c == L"agent.permlist") AgSend(AgGrantsJs());
    else if (c == L"agent.permdel") { auto v = AgGrantsLoad(); size_t i = (size_t)_wtoi(a.c_str()); if (i < v.size()) v.erase(v.begin() + i); AgGrantsSave(v); AgSend(AgGrantsJs()); }
    else if (c == L"agent.permclear") { AgGrantsSave({}); AgSend(AgGrantsJs()); }
    else if (c == L"agent.done") m_set.Set(L"agent_done", std::to_wstring(_wtoi(m_set.Get(L"agent_done", L"0").c_str()) + 1));
    else if (c == L"agent.search") { if (a == L"ddg" || a == L"brave" || a == L"mojeek" || a == L"google") m_set.Set(L"agent_search", a); }
    else if (c == L"agent.memlist") AgSend(AgMemJs());
    else if (c == L"agent.memq") AgSend(AgMemJs(a));
    else if (c == L"agent.fact" && v.size() == 3) {   // facts the user tells the assistant: remembered across chats, forgotten on request
        std::wstring tx = AgUnpct(v[2]); for (wchar_t& ch : tx) if (ch < 32) ch = L' ';
        if (tx.empty() || tx.size() > 300) return;
        auto d = AgMemLoad(); size_t k = 0; while (k < d.size() && d[k].name != L"My facts") ++k;
        auto low = [](std::wstring s) { for (wchar_t& ch : s) ch = (wchar_t)towlower(ch); return s; };
        if (a == L"add") {
            if (k == d.size()) { AgDoc x; x.name = L"My facts"; d.insert(d.begin(), x); k = 0; }
            if (d[k].text.size() + tx.size() < 20000 && low(d[k].text).find(low(L"- " + tx + L"\n")) == std::wstring::npos) d[k].text += L"- " + tx + L"\n";
            d[k].on = true;
            while (d[k].text.size() > 8000) { size_t e = d[k].text.find(L'\n'); if (e == std::wstring::npos) break; d[k].text.erase(0, e + 1); }   // compaction: drop the oldest lines
        } else if (a == L"del" && k < d.size()) {
            std::wstring out, ln, key = low(tx); size_t s = 0, e;
            while (s < d[k].text.size()) { e = d[k].text.find(L'\n', s); if (e == std::wstring::npos) e = d[k].text.size(); ln = d[k].text.substr(s, e - s); s = e + 1; if (low(ln).find(key) == std::wstring::npos) out += ln + L"\n"; }
            d[k].text = out; if (out.empty()) d.erase(d.begin() + k);
        }
        AgMemSave(d); AgSend(AgMemJs());
    }
    else if (c == L"agent.memadd" && v.size() == 3) {
        std::wstring er = AgMemPut(AgUnpct(a), AgUnpct(v[2]));
        AgSend(AgMemJs()); AgSend(L"lbMemRes(" + AgJs(er.empty() ? L"Saved." : er) + L")");
    }
    else if (c == L"agent.memfile") {   // the USER picks a notes file in the Windows dialog; read here, no browser file input involved
        std::wstring er;
        wil::com_ptr<IFileOpenDialog> d;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) {
            DWORD o = 0; d->GetOptions(&o); d->SetOptions(o | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST); d->SetTitle(L"Add a notes file to the assistant's memory");
            COMDLG_FILTERSPEC fs[] = { { L"Notes (.md .markdown .txt .csv .json)", L"*.md;*.markdown;*.txt;*.csv;*.json" } }; d->SetFileTypes(1, fs);
            wchar_t tf[520] = L""; bool hook = TestEnv(L"LIBRA_TEST_MEMFILE", tf, 520) > 0;   // test hook: skip the dialog
            if (hook || SUCCEEDED(d->Show(m_hWnd))) {
                wil::com_ptr<IShellItem> it; PWSTR ps = nullptr;
                if (hook || (SUCCEEDED(d->GetResult(&it)) && SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &ps)))) {
                    std::wstring src = hook ? std::wstring(tf) : std::wstring(ps); if (!hook) CoTaskMemFree(ps); size_t sl = src.find_last_of(L"\\/"); std::wstring nm = sl == std::wstring::npos ? src : src.substr(sl + 1);
                    std::wstring ex = nm.find_last_of(L'.') == std::wstring::npos ? L"" : nm.substr(nm.find_last_of(L'.') + 1); for (auto& ch : ex) ch = (wchar_t)towlower(ch);
                    WIN32_FILE_ATTRIBUTE_DATA fa;
                    if (ex != L"md" && ex != L"markdown" && ex != L"txt" && ex != L"csv" && ex != L"json") er = L"E-11 File type not allowed. Use .md, .markdown, .txt, .csv or .json.";
                    else if (!GetFileAttributesExW(src.c_str(), GetFileExInfoStandard, &fa)) er = L"E-12 Could not open that file.";
                    else if (fa.nFileSizeHigh || fa.nFileSizeLow > 800000) er = L"E-13 That file is too large (limit 200 KB of text per file).";
                    else {
                        std::string raw = ReadAllRaw(src); std::wstring tx;
                        if (raw.size() >= 2 && (unsigned char)raw[0] == 0xFF && (unsigned char)raw[1] == 0xFE) tx.assign((const wchar_t*)(raw.data() + 2), (raw.size() - 2) / 2);
                        else { if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF) raw.erase(0, 3); tx = FromUtf8(raw); }
                        std::wstring cl; for (wchar_t ch : tx) if (ch != 0 && ch != L'\r') cl += ch;
                        size_t b = cl.find_first_not_of(L" \t\n"); cl = b == std::wstring::npos ? L"" : cl.substr(b);
                        er = AgMemPut(nm.substr(0, 60), cl);
                        if (er.empty()) { AgSend(AgMemJs()); AgSend(L"lbMemRes(" + AgJs(L"Saved: " + nm + L" (" + std::to_wstring(cl.size()) + L" characters read).") + L")"); return; }
                    }
                }
            } else return;   // cancelled
        } else er = L"E-19 Could not open the Windows file dialog.";
        AgSend(AgMemJs()); AgSend(L"lbMemRes(" + AgJs(er) + L")");
    }
    else if (c == L"agent.memdel") { auto d = AgMemLoad(); size_t i = (size_t)_wtoi(a.c_str()); if (i < d.size()) d.erase(d.begin() + i); AgMemSave(d); AgSend(AgMemJs()); }
    else if (c == L"agent.memon" && v.size() == 3) { auto d = AgMemLoad(); size_t i = (size_t)_wtoi(a.c_str()); if (i < d.size()) d[i].on = v[2] == L"1"; AgMemSave(d); AgSend(AgMemJs()); }
    else if (c == L"agent.conv" && v.size() == 2) {   // PDF -> Markdown through the user's own Claude key
        std::wstring ep = m_set.Get(L"agent_ep"); std::string key = AgKeyGet();
        if (a.size() > 6000000 || a.empty() || a[0] != L'{' || key.empty() || ep.find(L"/v1/messages") == std::wstring::npos) { AgSend(L"lbConv('{\"error\":\"PDF conversion needs an Anthropic key\"}')"); return; }
        m_agConv = true; std::string body = ToUtf8(a); int port = DohProxy::Get().Port(); HWND hw = m_hWnd;
        std::thread([hw, port, ep, key, body]() mutable {
            std::string r = AgPost(port, ep, key, body); SecureZeroMemory(key.data(), key.size());
            PostMessage(hw, WM_APP + 31, 0, (LPARAM) new std::wstring(FromUtf8(r)));
        }).detach();
    }
    else if (!m_agRun) return;   // everything below reads or acts on a page: only inside a run the user started from the open panel
    else if (c == L"agent.snap") {
        if (m_agPaused) return;   // a human check is on screen: do nothing at all
        if (GetTickCount() - m_agStart > 360000 || ++m_agSteps > 60) { AgStop(L"Step or time limit reached."); return; }
        int ti = m_agTarget ? IndexOf(m_agTarget) : -1;
        if (ti < 0 && m_agHome && IndexOf(m_agHome) >= 0) { m_agTarget = m_agHome; ti = IndexOf(m_agTarget); }   // research tab closed by hand: back to the original
        if (ti < 0) { AgStop(L"The tab was closed."); return; }
        if (!m_tabs[ti]->webView) { AgSend(L"lbSnap(\"URL: loading. Use wait.\")"); return; }
        if (m_tabs[ti]->url.rfind(L"libra://", 0) == 0) { AgSend(L"lbSnap(\"URL: new tab page. No elements. Use the search or navigate action to go somewhere.\")"); return; }   // never read Libra's own pages
        unsigned gen = m_agGen;
        m_tabs[ti]->webView->ExecuteScript(kAgSnap, Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>(
            [this, gen, t = m_tabs[ti].get()](HRESULT, LPCWSTR r) -> HRESULT {
                if (gen != m_agGen || !m_agRun) return S_OK;
                std::wstring s = AgOkStr(r); m_agFr.clear(); m_agMainN = AgCountEls(s);
                if (t->frames.empty() || s.rfind(L"\"HUMAN_CHECK", 0) == 0 || IndexOf(t) < 0) { AgSend(L"lbSnap(" + s + L")"); return S_OK; }
                AgSnapFrames(t, gen, s, t->frames, 0, m_agMainN, {}); return S_OK; }).Get());
    } else if (c == L"agent.act" && v.size() == 4) {
        if (m_agPaused) return;
        const std::wstring &op = v[1], &id = v[2]; std::wstring tx = v[3];
        int ti = AgTarget(); if (ti < 0) { AgStop(L"The tab was closed."); return; }
        if (tx.size() > 500) tx.resize(500);
        if (op == L"type") {   // protect every word the user themselves wrote (task + memory)
            std::wstring prot = L" "; { std::wstring cur; auto flush = [&]() { if (!cur.empty()) { prot += cur + L" "; cur.clear(); } };
                std::wstring src = m_agTaskRaw + L" "; for (auto& d : AgMemLoad()) if (d.on) src += d.text + L" ";
                for (wchar_t ch : src) { if (iswalnum(ch) || ch == L'\'') cur += (wchar_t)towlower(ch); else flush(); } flush(); }
            { auto lo = [](std::wstring z) { for (auto& c : z) c = (wchar_t)towlower(c); return z; };
              if (lo(m_agTaskRaw).find(lo(tx)) == std::wstring::npos) tx = AgPolish(tx, prot); }   // text the user gave verbatim is typed exactly as given
            AgSend(L"lbPol(" + AgJs(tx) + L")");   // so the review shows the corrected text
        }
        if ((op == L"type" || op == L"select" || (op == L"key" && tx == L"Enter")) && m_agRes && m_tabs[ti].get() == m_agRes) { AgSend(L"lbAct(\"REFUSED: research tabs are read-only\")"); return; }
        if (op == L"type" || op == L"select") {   // typing on a site the user never named needs a yes
            dl::Url cu;
            if (dl::Crack(m_tabs[ti]->url, cu)) {
                std::wstring dom = RegDom(cu.host), lab = dom.substr(0, dom.find(L'.')); bool named = lab.size() >= 4 && m_agTaskKey.find(lab) != std::wstring::npos;
                if (dom != m_agScope && !named && !m_agTypeOk.count(dom)) {
                    std::wstring q = L"The Libra assistant wants to type into " + cu.host + L", a site you did not ask for.\n\nAllow typing there for this task?";
                    if (!AgAsk(L"type_site", dom, L"Allow typing on " + cu.host + L"?", L"The assistant wants to type into a site you did not ask for.", false)) { AgSend(L"lbAct(\"The user said no.\")"); return; }
                    m_agTypeOk.insert(dom); ti = AgTarget(); if (ti < 0 || !m_agRun) return;
                }
            }
        }
        if (m_tabs[ti]->url.rfind(L"libra://", 0) == 0 && (op == L"click" || op == L"type" || op == L"select" || op == L"key")) { AgSend(L"lbAct(\"REFUSED: use search or navigate here\")"); return; }   // Libra pages and settings are off limits
        for (wchar_t& ch : tx) if (ch < 32 || ch == 127) ch = L' ';
        bool needId = op == L"click" || op == L"type" || op == L"select";
        if (needId && (id.empty() || id.size() > 3 || id.find_first_not_of(L"0123456789") != std::wstring::npos)) { AgSend(L"lbAct(\"REFUSED: bad element\")"); return; }
        if (op == L"click" || op == L"type" || op == L"select" || op == L"key" || op == L"scroll") AgRunAct(ti, op, needId ? id : L"0", tx, false);
        else if (op == L"back") { m_tabs[ti]->webView->GoBack(); AgSend(L"lbAct(\"went back\")"); }
        else if (op == L"wait") AgSend(L"lbAct(\"waited\")");
        else if (op == L"navigate" || op == L"search") {   // typed into the address bar, like a person would
            if (op == L"search") { AgSend(L"lbAct(\"REFUSED: searching the web is turned off. Go straight to a site with navigate and a web address (you may use a site's own search page).\")"); return; }
            std::wstring url = AgSiteUrl(tx); dl::Url u;
            if (url.empty()) { AgSend(L"lbAct(\"REFUSED: give a full web address such as https://example.com or a site name\")"); return; }
            if (!IsHttp(url) || !dl::Crack(url, u)) { AgSend(L"lbAct(\"REFUSED: only web addresses\")"); return; }
            if (AgPrivateHost(u.host, m_agTaskKey)) { AgSend(L"lbAct(\"REFUSED: private or local addresses are off limits\")"); return; }
            if (AgLeaks(url)) { AgSend(L"lbAct(\"REFUSED: I will not put your saved details into a web address\")"); return; }
            std::wstring dom = RegDom(u.host), label = dom.substr(0, dom.find(L'.'));
            bool named = label.size() >= 4 && m_agTaskKey.find(label) != std::wstring::npos;   // the user's own words named this site
            if (op == L"navigate" && dom != m_agScope && !named) {
                std::wstring q = L"The Libra assistant wants to leave " + m_agScope + L" and open " + u.host + L".\n\nAllow this once?";
                if (!AgAsk(L"leave_site", dom, L"Open " + u.host + L"?", L"The assistant wants to leave " + m_agScope + L" and go to " + u.host + L".", false)) { AgSend(L"lbAct(\"The user said no.\")"); return; }
                ti = AgTarget(); if (ti < 0 || !m_agRun) return;
            }
            SetWindowTextW(m_hOmnibox, tx.c_str());   // the text appears in the address bar, then the page opens
            m_agGoUrl = url; m_agGoTab = m_tabs[ti].get(); m_agScope = dom; SetTimer(m_hWnd, 2020, 450, nullptr);
            AgSend(L"lbAct(\"opening\")");
        }else if (op == L"attach") {   // choose which sandbox file the next upload dialog gets
            if (!AgSbxName(tx) || GetFileAttributesW((AgSbx() + L"\\" + tx).c_str()) == INVALID_FILE_ATTRIBUTES) { AgSend(L"lbAct(\"REFUSED: no such file in the sandbox. Ask the user to add it under Files.\")"); return; }
            m_agAttach = tx; AgSend(L"lbAct(\"ready to attach the file: now click the site upload button\")");
        }
        else if (op == L"download") {   // downloads always need the user's yes
            dl::Url du; std::wstring url = tx;
            if (!IsHttp(url) || !dl::Crack(url, du) || AgPrivateHost(du.host, m_agTaskKey)) { AgSend(L"lbAct(\"REFUSED: only public web addresses\")"); return; }
            if (AgLeaks(url)) { AgSend(L"lbAct(\"REFUSED: I will not put your saved details into a web address\")"); return; }
            std::wstring q = L"The Libra assistant wants to download this file:\n\n" + url.substr(0, 150) + L"\n\nAllow the download?";
            if (!AgAsk(L"download", RegDom(du.host), L"Download this file?", url.substr(0, 150) + L"\n\nfrom " + du.host, false)) { AgSend(L"lbAct(\"The user said no.\")"); return; }
            ti = AgTarget(); if (ti < 0 || !m_agRun) return; StartSave(m_tabs[ti].get(), url, L"", L"", false); AgSend(L"lbAct(\"download started; it appears on the Downloads page\")");
        }
        else if (op == L"open_tab") {   // research in a NEW tab; it becomes the working tab until close_tab
            if (++m_agTabs > 4) { AgSend(L"lbAct(\"REFUSED: too many tabs, finish with what you have\")"); return; }
            std::wstring url = AgSiteUrl(tx); dl::Url u;
            if (url.empty() || !IsHttp(url) || !dl::Crack(url, u) || m_tabs[ti]->incognito) { AgSend(L"lbAct(\"REFUSED: only web addresses\")"); return; }
            if (AgPrivateHost(u.host, m_agTaskKey)) { AgSend(L"lbAct(\"REFUSED: private or local addresses are off limits\")"); return; }
            if (AgLeaks(url)) { AgSend(L"lbAct(\"REFUSED: I will not put your saved details into a web address\")"); return; }
            if (!m_agHome) m_agHome = m_tabs[ti].get();
            AgFileHook(false); OpenTab(url);
            m_agTarget = m_tabs.back().get(); m_agRes = m_agTarget; m_agScope = RegDom(u.host);
            if (m_agDock) ApplyDock();
            AgSend(L"lbAct(\"opening a new tab\")");
        }
        else if (op == L"close_tab") {
            int hi = m_agHome ? IndexOf(m_agHome) : -1, ri = m_agRes ? IndexOf(m_agRes) : -1;
            if (hi < 0 || ri < 0) { AgSend(L"lbAct(\"REFUSED: no research tab is open\")"); return; }
            AgFileHook(false); CloseTab(ri); hi = IndexOf(m_agHome); if (hi >= 0) SwitchTab(hi);
            m_agTarget = m_agHome; m_agRes = nullptr; m_agHome = nullptr;
            { dl::Url u; int h2 = IndexOf(m_agTarget); if (h2 >= 0 && dl::Crack(m_tabs[h2]->url, u)) m_agScope = RegDom(u.host); }
            AgFileHook(true); if (m_agDock) ApplyDock();
            AgSend(L"lbAct(\"back on the original tab\")");
        }
        else AgSend(L"lbAct(\"REFUSED: unknown action\")");
    } else if (c == L"agent.llm" && v.size() == 2) {
        if (a.size() > 200000 || a.empty() || a[0] != L'{') { AgStop(L"Request too large."); return; }
        std::wstring ep = m_set.Get(L"agent_ep"); std::string key = AgKeyGet(), body = ToUtf8(a); int port = DohProxy::Get().Port(); unsigned gen = m_agGen; HWND hw = m_hWnd;
        std::thread([hw, gen, port, ep, key, body]() mutable {
            std::string r = AgPost(port, ep, key, body);
            if (!key.empty()) SecureZeroMemory(key.data(), key.size());
            PostMessage(hw, WM_APP + 31, gen, (LPARAM) new std::wstring(FromUtf8(r)));
        }).detach();
    }
}
void BrowserWindow::AgentOnLlm(unsigned gen, const std::wstring& text) {
    if (m_agConv) { m_agConv = false; AgSend(L"lbConv(" + AgJs(text.size() > 6000000 ? L"" : text) + L")"); return; }
    if (gen != m_agGen || !m_agRun) return;
    AgSend(L"lbLlm(" + AgJs(text.size() > 1000000 ? L"" : text) + L")");
}

// ── docked sidebar: the agent tab shows as a right-hand panel while another tab is active ──
int BrowserWindow::AgDockIdx() const {
    if (!m_agDock) return -1;
    for (int i = 0; i < (int)m_tabs.size(); ++i) if (m_tabs[i]->url == L"libra://agent") return i;
    return -1;
}
bool BrowserWindow::AgAllowedHere() const {   // the Assistant lives only beside the home screen or a web page, never beside Settings, History, Downloads, Privacy... (Libra's own pages)
    if (m_activeTab < 0 || m_activeTab >= (int)m_tabs.size()) return false;
    const std::wstring& u = m_tabs[m_activeTab]->url; return u.empty() || u == L"libra://home" || IsHttp(u);
}
void BrowserWindow::ApplyDock() {
    int ai = AgDockIdx(); ResizeWebViews();
    if (ai < 0 || ai == m_activeTab) return;
    Tab* a = m_tabs[ai].get();
    if (!AgAllowedHere()) { if (m_agRun) AgStop(L"Stopped: the Assistant works only on the home screen and web pages."); if (a->controller) a->controller->put_IsVisible(FALSE); return; }   // on one of Libra's own pages the panel is hidden (back on a web page it returns)
    a->hiddenSince = 0; ResumeTab(a);
    if (a->controller) a->controller->put_IsVisible(TRUE);
}
void BrowserWindow::ToggleAgent() {
    if (m_child) return;   // Child Safety: no agent
    int ai = -1; for (int i = 0; i < (int)m_tabs.size(); ++i) if (m_tabs[i]->url == L"libra://agent") ai = i;
    if (ai >= 0 && m_agDock) { AgStop(nullptr); m_agDock = false; CloseTab(ai); ResizeWebViews(); if (m_hBtnAgent) InvalidateRect(m_hBtnAgent, nullptr, FALSE); return; }   // second click closes it
    if (!AgAllowedHere() && ai != m_activeTab) return;   // not from Settings or any other page of Libra: only from the home screen or a web page
    if (ai < 0) { AgStop(nullptr); ShowInternal(L"libra://agent", false); }   // keeps the current tab active
    else if (ai == m_activeTab) for (int i = (int)m_tabs.size() - 1; i >= 0; --i) if (IsHttp(m_tabs[i]->url)) { SwitchTab(i); break; }
    m_agDock = true; ApplyDock();
    if (m_hBtnAgent) InvalidateRect(m_hBtnAgent, nullptr, FALSE);
}

// ── file uploads: the model never touches the file system. When a page opens a file chooser, Libra asks the USER to pick the file. ──
struct AgFc { Tab* t; int node; bool multi; };
void BrowserWindow::AgGoNow() {
    int i = m_agGoTab ? IndexOf(m_agGoTab) : -1;
    if (i >= 0 && m_agRun && m_tabs[i]->webView && !m_agGoUrl.empty()) m_tabs[i]->webView->Navigate(m_agGoUrl.c_str());
    m_agGoUrl.clear();
}
void BrowserWindow::AgFileHook(bool on) {
    if (on) {
        int ti = AgTarget(); if (ti < 0 || m_agFcTab) return; Tab* t = m_tabs[ti].get();
        t->webView->CallDevToolsProtocolMethod(L"Page.enable", L"{}", nullptr);
        t->webView->CallDevToolsProtocolMethod(L"DOM.enable", L"{}", nullptr);
        t->webView->CallDevToolsProtocolMethod(L"Page.setInterceptFileChooserDialog", L"{\"enabled\":true}", nullptr);
        wil::com_ptr<ICoreWebView2DevToolsProtocolEventReceiver> rc;
        if (SUCCEEDED(t->webView->GetDevToolsProtocolEventReceiver(L"Page.fileChooserOpened", &rc)))
            rc->add_DevToolsProtocolEventReceived(Microsoft::WRL::Callback<ICoreWebView2DevToolsProtocolEventReceivedEventHandler>(
                [this, t](ICoreWebView2*, ICoreWebView2DevToolsProtocolEventReceivedEventArgs* a) -> HRESULT {
                    wil::unique_cotaskmem_string j; if (FAILED(a->get_ParameterObjectAsJson(&j)) || !j) return S_OK;
                    std::wstring s = j.get(); auto* f = new AgFc{ t, 0, s.find(L"selectMultiple") != std::wstring::npos };
                    size_t p = s.find(L"\"backendNodeId\":"); if (p != std::wstring::npos) f->node = _wtoi(s.c_str() + p + 16);
                    PostMessage(m_hWnd, WM_APP + 34, 0, (LPARAM)f); return S_OK;
                }).Get(), &m_agFcTok);
        m_agFcTab = t;
    } else if (m_agFcTab) {
        int i = IndexOf(m_agFcTab);
        if (i >= 0 && m_tabs[i]->webView) {
            wil::com_ptr<ICoreWebView2DevToolsProtocolEventReceiver> rc;
            if (SUCCEEDED(m_tabs[i]->webView->GetDevToolsProtocolEventReceiver(L"Page.fileChooserOpened", &rc))) rc->remove_DevToolsProtocolEventReceived(m_agFcTok);
            m_tabs[i]->webView->CallDevToolsProtocolMethod(L"Page.setInterceptFileChooserDialog", L"{\"enabled\":false}", nullptr);
        }
        m_agFcTab = nullptr;
    }
}
void BrowserWindow::AgFileChosen(void* p) {
    std::unique_ptr<AgFc> f((AgFc*)p);
    if (++m_agFcN > 3) { AgSend(L"lbMsg('This page keeps asking for files, so I stopped. Please attach files yourself.')"); return; }
    if (!m_agRun || IndexOf(f->t) < 0 || !f->t->webView) return;
    std::vector<std::wstring> paths; wchar_t tf[1024];
    if (!m_agAttach.empty() && AgSbxName(m_agAttach) && GetFileAttributesW((AgSbx() + L"\\" + m_agAttach).c_str()) != INVALID_FILE_ATTRIBUTES) { paths.push_back(AgSbx() + L"\\" + m_agAttach); m_agAttach.clear(); }   // from the sandbox: no dialog
    else if (TestEnv(L"LIBRA_TEST_FILE", tf, 1024) > 0) paths.push_back(tf);   // automated tests only
    else {
        AgSend(L"lbMsg('Choose the file in the window that opened.')");
        wil::com_ptr<IFileOpenDialog> d;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d)))) {
            DWORD o = 0; d->GetOptions(&o); d->SetOptions(o | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | (f->multi ? FOS_ALLOWMULTISELECT : 0));
            d->SetTitle(L"Choose the file for the assistant"); { wil::com_ptr<IShellItem> sf; if (SUCCEEDED(SHCreateItemFromParsingName(AgSbx().c_str(), nullptr, IID_PPV_ARGS(&sf)))) d->SetFolder(sf.get()); }
            if (SUCCEEDED(d->Show(m_hWnd))) {
                wil::com_ptr<IShellItemArray> arr; DWORD n = 0;
                if (SUCCEEDED(d->GetResults(&arr)) && SUCCEEDED(arr->GetCount(&n)))
                    for (DWORD i = 0; i < n && i < 20; ++i) { wil::com_ptr<IShellItem> it; PWSTR ps = nullptr; if (SUCCEEDED(arr->GetItemAt(i, &it)) && SUCCEEDED(it->GetDisplayName(SIGDN_FILESYSPATH, &ps))) { paths.push_back(ps); CoTaskMemFree(ps); } }
            }
        }
    }
    if (paths.empty() || !m_agRun || IndexOf(f->t) < 0) { AgSend(L"lbMsg('No file was chosen.')"); return; }
    std::wstring js = L"{\"files\":["; for (size_t i = 0; i < paths.size(); ++i) js += (i ? L"," : L"") + AgJs(paths[i]);
    js += L"],\"backendNodeId\":" + std::to_wstring(f->node) + L"}";
    f->t->webView->CallDevToolsProtocolMethod(L"DOM.setFileInputFiles", js.c_str(), nullptr);
    AgSend(L"lbMsg('File attached.')");
}

// Red tap ring around the pointer for every tap the Agent makes; a red "No reaction" tag when the page did not react.
static void AgTapFx(Tab* t, int x, int y, bool fail) {
    if (!t || !t->webView) return;
    std::wstring js = LR"((function(x,y,f){try{var h=document.body||document.documentElement;if(!h)return;var d=document.createElement('div');
d.style.cssText='position:fixed;left:'+(x-14)+'px;top:'+(y-14)+'px;width:28px;height:28px;box-sizing:border-box;border-radius:50%;border:3px solid #e5322d;z-index:2147483647;pointer-events:none';d.setAttribute('data-lbfx','1');h.appendChild(d);
d.animate([{transform:'scale(.35)',opacity:1},{transform:'scale(2.4)',opacity:0}],{duration:700,easing:'ease-out'}).onfinish=function(){d.remove()};
if(f){var b=document.createElement('div');b.textContent='\u00d7 No reaction';b.style.cssText='position:fixed;left:'+Math.max(4,Math.min(x+16,innerWidth-110))+'px;top:'+Math.max(4,y-38)+'px;background:#e5322d;color:#fff;font:600 12px system-ui,sans-serif;padding:4px 9px;border-radius:11px;z-index:2147483647;pointer-events:none';b.setAttribute('data-lbfx','1');h.appendChild(b);setTimeout(function(){b.remove()},2200)}}catch(e){}})LB)";
    js = js.substr(0, js.size() - 2) + L"(" + std::to_wstring(x) + L"," + std::to_wstring(y) + L"," + (fail ? L"true" : L"false") + L")";
    t->webView->ExecuteScript(js.c_str(), nullptr);
}

void BrowserWindow::AgClickNow() {
    if (m_agPaused) return;   // move, press, release, then (900 ms later) check the page really reacted
    int i = m_agClickTab ? IndexOf(m_agClickTab) : -1; if (i < 0 || !m_agRun || !m_tabs[i]->webView) return;
    unsigned gen = m_agGen; m_agTry = 0;
    m_tabs[i]->webView->ExecuteScript(kAgFp, Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>([this, gen](HRESULT, LPCWSTR r) -> HRESULT {
        int j = m_agClickTab ? IndexOf(m_agClickTab) : -1; if (gen != m_agGen || !m_agRun || j < 0 || !m_tabs[j]->webView) return S_OK;
        m_agFp = r ? r : L"";
        auto ev = [&](const wchar_t* type, int buttons, int cc) {
            std::wstring js = std::wstring(L"{\"type\":\"") + type + L"\",\"x\":" + std::to_wstring(m_agClickX) + L",\"y\":" + std::to_wstring(m_agClickY) +
                              L",\"button\":\"" + (buttons || cc ? L"left" : L"none") + L"\",\"buttons\":" + std::to_wstring(buttons) + L",\"clickCount\":" + std::to_wstring(cc) + L"}";
            m_tabs[j]->webView->CallDevToolsProtocolMethod(L"Input.dispatchMouseEvent", js.c_str(), nullptr); };
        ev(L"mouseMoved", 0, 0); ev(L"mousePressed", 1, 1); ev(L"mouseReleased", 0, 1);
        AgTapFx(m_tabs[j].get(), m_agClickX, m_agClickY, false);
        SetTimer(m_hWnd, 2022, 900, nullptr); return S_OK; }).Get());
}
// Shared by the retry scripts: the red ring (and the red "No reaction" tag when f) at the element.
static const wchar_t* kAgTapFn = LR"JS(var __tap=function(e,f){try{var r=e.getBoundingClientRect(),x=r.left+Math.min(r.width/2,60),y=r.top+r.height/2,h=document.body||document.documentElement,d=document.createElement('div');
d.style.cssText='position:fixed;left:'+(x-14)+'px;top:'+(y-14)+'px;width:28px;height:28px;box-sizing:border-box;border-radius:50%;border:3px solid #e5322d;z-index:2147483647;pointer-events:none';d.setAttribute('data-lbfx','1');h.appendChild(d);
d.animate([{transform:'scale(.35)',opacity:1},{transform:'scale(2.4)',opacity:0}],{duration:700,easing:'ease-out'}).onfinish=function(){d.remove()};
if(f){var b=document.createElement('div');b.textContent='\u00d7 No reaction';b.style.cssText='position:fixed;left:'+Math.max(4,Math.min(x+16,innerWidth-110))+'px;top:'+Math.max(4,y-38)+'px;background:#e5322d;color:#fff;font:600 12px system-ui,sans-serif;padding:4px 9px;border-radius:11px;z-index:2147483647;pointer-events:none';b.setAttribute('data-lbfx','1');h.appendChild(b);setTimeout(function(){b.remove()},2400)}}catch(z){}};)JS";

void BrowserWindow::AgExecT(const wchar_t* js, ICoreWebView2ExecuteScriptCompletedHandler* h) {   // in the clicked frame, or the page
    int i = m_agClickTab ? IndexOf(m_agClickTab) : -1; if (i < 0) return;
    if (m_agClickFrame) m_agClickFrame->ExecuteScript(js, h); else if (m_tabs[i]->webView) m_tabs[i]->webView->ExecuteScript(js, h);
}
void BrowserWindow::AgClickCheck() {   // did the tap do anything? If not, the next way
    int i = m_agClickTab ? IndexOf(m_agClickTab) : -1; if (i < 0 || !m_agRun || !m_tabs[i]->webView) return;
    unsigned gen = m_agGen;
    auto h = Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>([this, gen](HRESULT, LPCWSTR r) -> HRESULT {
        int j = m_agClickTab ? IndexOf(m_agClickTab) : -1; if (gen != m_agGen || !m_agRun || j < 0) return S_OK;
        const bool changed = !r || std::wstring(r) != m_agFp;   // a navigation (no result) counts as a change
        static const wchar_t* how[] = { L"", L" (the first tap did nothing; it worked with pointer events)", L" (the first taps did nothing; it worked on the nearest real control)", L" (the first taps did nothing; it worked with the keyboard)", L" (the first taps did nothing; it worked at another spot)" };
        if (changed) { AgSend(std::wstring(L"lbAct(\"clicked; the page changed") + how[std::min(m_agTry, 4)] + L"\")"); return S_OK; }
        AgNextStrategy(); return S_OK; });
    AgExecT(kAgFp, h.Get());
}
void BrowserWindow::AgNextStrategy() {   // 1 pointer events, 2 nearest real control, 3 keyboard, 4 another spot (real mouse); then give up and say why
    int j = m_agClickTab ? IndexOf(m_agClickTab) : -1; if (j < 0 || !m_agRun || !m_tabs[j]->webView) return;
    unsigned gen = m_agGen; Tab* t = m_tabs[j].get(); const bool fr = (bool)m_agClickFrame; int n = ++m_agTry; if (fr && n >= 3) n = 99;
    const std::wstring id = std::to_wstring(fr ? m_agClickRid : _wtoi(m_agClickId.c_str()));
    const std::wstring pre = std::wstring(L"(function(i){") + kAgTapFn + L"var e=window.__lbA&&window.__lbA[i];if(!e||!e.isConnected)return ";
    auto mouse = [this, t](int x, int y) { if (!t->webView) return; for (int k = 0; k < 3; ++k) { const wchar_t* ty = k == 0 ? L"mouseMoved" : k == 1 ? L"mousePressed" : L"mouseReleased"; int b = k == 1 ? 1 : 0, cc = k ? 1 : 0;
        std::wstring js = std::wstring(L"{\"type\":\"") + ty + L"\",\"x\":" + std::to_wstring(x) + L",\"y\":" + std::to_wstring(y) + L",\"button\":\"" + (b || cc ? L"left" : L"none") + L"\",\"buttons\":" + std::to_wstring(b) + L",\"clickCount\":" + std::to_wstring(cc) + L"}";
        t->webView->CallDevToolsProtocolMethod(L"Input.dispatchMouseEvent", js.c_str(), nullptr); } };
    auto after = [this, gen](bool gone) { if (gone) return; SetTimer(m_hWnd, 2022, 900, nullptr); };   // look again in a moment
    auto give_up = [this, gen, id]() {   // red tag at the element + the reason, for the user and for the AI
        std::wstring js = std::wstring(L"(function(i){") + kAgTapFn + LR"JS(var e=window.__lbA&&window.__lbA[i];if(!e||!e.isConnected)return 'the element is gone from the page';__tap(e,true);
if(e.disabled||e.getAttribute('aria-disabled')=='true')return 'the control is disabled (a required field before it may be empty)';var r=e.getBoundingClientRect(),t=document.elementFromPoint(r.left+r.width/2,r.top+r.height/2);
if(t&&t!==e&&!e.contains(t)&&!t.contains(e))return 'another element covers it ('+t.tagName.toLowerCase()+' '+String(t.innerText||t.getAttribute('aria-label')||'').replace(/\s+/g,' ').slice(0,40)+'): close that pop-up or scroll';return 'it may not be the real control'}))JS" + L"(" + id + L")";
        AgExecT(js.c_str(), Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>([this, gen](HRESULT, LPCWSTR r) -> HRESULT {
            if (gen != m_agGen || !m_agRun) return S_OK;
            AgSend(L"lbAct(\"clicked, but NOTHING changed on the page, even after trying several different ways (real mouse, pointer events, the nearest real control, the keyboard, another spot). Why: \"+" + AgOkStr(r) + L"+\". Try something else: another element, scroll first, or close a pop-up.\")"); return S_OK; }).Get()); };
    if (n == 1 || n == 2) {
        std::wstring body = n == 1
            ? LR"JS(0;e.scrollIntoView({block:'center'});__tap(e);var r=e.getBoundingClientRect(),o={bubbles:true,cancelable:true,composed:true,clientX:r.left+r.width/2,clientY:r.top+r.height/2,button:0,view:window};
['pointerover','mouseover','pointerdown','mousedown','pointerup','mouseup','click'].forEach(function(t){var C=/^pointer/.test(t)&&window.PointerEvent?PointerEvent:MouseEvent;e.dispatchEvent(new C(t,Object.assign({pointerId:1,isPrimary:true,pointerType:'mouse'},o)))});return 1}))JS"
            : LR"JS(0;var t=e.closest('a[href],button,[role=button],[role=link],[role=radio],[role=checkbox],[role=option],[role=tab],[role=menuitem],summary,label,[onclick]')||e;
if(t.tagName=='LABEL'&&t.control)t=t.control;else if(t===e&&!/^(A|BUTTON|INPUT|SELECT|SUMMARY)$/.test(e.tagName)){var d=e.querySelector('a[href],button,input,[role=button],[role=radio],[role=checkbox]');if(d)t=d}
t.scrollIntoView({block:'center'});__tap(t);t.click();return 1}))JS";
        std::wstring js = pre + body + L"(" + id + L")";
        AgExecT(js.c_str(), Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>([after, give_up](HRESULT, LPCWSTR r) mutable -> HRESULT { if (!r || std::wstring(r) == L"0") { give_up(); return S_OK; } after(false); return S_OK; }).Get());
    } else if (n == 3) {
        std::wstring js = pre + LR"JS('';var t=e.closest('a[href],button,[role=button],[role=link],[role=radio],[role=checkbox],[role=option],[role=tab],summary,label')||e;if(t.tagName=='LABEL'&&t.control)t=t.control;
t.scrollIntoView({block:'center'});if(!t.hasAttribute('tabindex')&&!/^(A|BUTTON|INPUT|SELECT|SUMMARY|TEXTAREA)$/.test(t.tagName))t.setAttribute('tabindex','-1');t.focus();__tap(t);
return ((t.tagName=='INPUT'&&/^(checkbox|radio)$/.test(t.type))||/^(checkbox|radio)$/.test(t.getAttribute('role')||''))?'Space':'Enter'}))JS" + L"(" + id + L")";
        AgExecT(js.c_str(), Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>([this, t, after, give_up](HRESULT, LPCWSTR r) mutable -> HRESULT {
            std::wstring k = r ? r : L""; bool sp = k == L"\"Space\""; if (!sp && k != L"\"Enter\"") { give_up(); return S_OK; }
            const wchar_t* key = sp ? L" " : L"Enter"; const wchar_t* code = sp ? L"Space" : L"Enter"; int vk = sp ? 32 : 13;
            for (const wchar_t* ty : { L"rawKeyDown", L"keyUp" }) { std::wstring j2 = std::wstring(L"{\"type\":\"") + ty + L"\",\"key\":\"" + key + L"\",\"code\":\"" + code + L"\",\"windowsVirtualKeyCode\":" + std::to_wstring(vk) + L"}"; if (t->webView) t->webView->CallDevToolsProtocolMethod(L"Input.dispatchKeyEvent", j2.c_str(), nullptr); }
            after(false); return S_OK; }).Get());
    } else if (n == 4) {
        std::wstring js = pre + LR"JS('';e.scrollIntoView({block:'center'});var r=e.getBoundingClientRect();__tap(e);return Math.round(r.left+Math.max(4,Math.min(10,r.width/4)))+','+Math.round(r.top+r.height/2)}))JS" + L"(" + id + L")";
        AgExecT(js.c_str(), Microsoft::WRL::Callback<ICoreWebView2ExecuteScriptCompletedHandler>([this, mouse, after, give_up](HRESULT, LPCWSTR r) mutable -> HRESULT {
            std::wstring s = r ? r : L""; size_t cm = s.find(L','); if (s.size() < 5 || cm == std::wstring::npos) { give_up(); return S_OK; }
            mouse(_wtoi(s.c_str() + 1), _wtoi(s.c_str() + cm + 1)); after(false); return S_OK; }).Get());
    } else give_up();
}

void BrowserWindow::AgKeyChecked(const std::wstring& r) {   // answer to the test request made right after a key was added
    auto has = [&](const wchar_t* x) { return r.find(x) != std::wstring::npos; };
    if (has(L"badkey")) { AgKeySet(L""); AgSend(L"lbKeyRes('bad','Invalid API key. Please change it.')"); }
    else if (has(L"HTTP 429")) AgSend(L"lbKeyRes('ok','API key accepted (the provider is limiting requests right now).')");
    else if (has(L"HTTP 400") || has(L"HTTP 404") || has(L"HTTP 422")) AgSend(L"lbKeyRes('warn','Key saved. The provider did not accept the test request: check the model name under Advanced.')");
    else if (has(L"Cannot reach")) AgSend(L"lbKeyRes('warn','Key saved, but I could not reach the provider to test it. Check your internet.')");
    else if (has(L"\"error\"")) AgSend(L"lbKeyRes('warn','Key saved, but the provider gave an unexpected answer. Try a message to test it.')");
    else AgSend(L"lbKeyRes('ok','API key verified.')");
}
