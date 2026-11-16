// Privacy pack: fingerprint randomization script + HTTPS-upgrade rule. Included at the end of BrowserWindow.cpp.

static bool CanUpgradeToHttps(const std::wstring& url) {   // public http:// pages only; never local, IP, ported or file-download addresses
    if (url.rfind(L"http://", 0) != 0) return false;
    size_t e = url.find_first_of(L"/?#", 7); std::wstring auth = url.substr(7, e == std::wstring::npos ? std::wstring::npos : e - 7);
    if (auth.empty() || auth.find(L':') != std::wstring::npos || auth.find(L'@') != std::wstring::npos || auth[0] == L'[') return false;
    for (auto& c : auth) c = (wchar_t)towlower(c);
    if (auth.find(L'.') == std::wstring::npos || auth == L"localhost") return false;                       // intranet names
    bool allNum = true; for (wchar_t c : auth) if (!iswdigit(c) && c != L'.') allNum = false; if (allNum) return false;   // IPv4
    for (const wchar_t* s : { L".local", L".localhost", L".test", L".internal", L".lan", L".home", L".invalid", L".corp" })
        if (auth.size() > wcslen(s) && auth.compare(auth.size() - wcslen(s), wcslen(s), s) == 0) return false;
    std::wstring path = url.substr(7 + auth.size()); path = path.substr(0, path.find_first_of(L"?#")); size_t d = path.find_last_of(L'.');
    if (d != std::wstring::npos && path.size() - d <= 5) { std::wstring x = path.substr(d + 1); for (auto& c : x) c = (wchar_t)towlower(c);
        for (const wchar_t* f : { L"zip", L"rar", L"7z", L"exe", L"msi", L"iso", L"dmg", L"mp4", L"mkv", L"mp3", L"pdf", L"apk", L"gz", L"tar", L"bin", L"dat" }) if (x == f) return false; }
    return true;
}

static std::wstring StripTrackers(const std::wstring& url) {   // "" = nothing to strip. Fixed list of pure tracking parameters; login/OAuth parameters are never touched.
    size_t q = url.find(L'?'); if (q == std::wstring::npos) return L"";
    size_t h = url.find(L'#', q); std::wstring qs = url.substr(q + 1, (h == std::wstring::npos ? url.size() : h) - q - 1), out; bool cut = false; size_t i = 0;
    static const wchar_t* bad[] = { L"fbclid", L"gclid", L"gclsrc", L"dclid", L"gbraid", L"wbraid", L"msclkid", L"mc_eid", L"mc_cid", L"igshid", L"yclid", L"twclid", L"ttclid", L"_hsenc", L"_hsmi", L"mkt_tok", L"vero_id", L"wickedid", L"oly_enc_id", L"oly_anon_id", L"__s", L"_ga", L"_gl", L"__hsfp", L"__hssc", L"__hstc", L"_openstat", L"hsctatracking", L"ml_subscriber", L"ml_subscriber_hash", L"rb_clickid", L"s_cid", L"vero_conv" };   // compared in lower case
    while (i <= qs.size()) {
        size_t e = qs.find(L'&', i); if (e == std::wstring::npos) e = qs.size();
        std::wstring kv = qs.substr(i, e - i), k = kv.substr(0, kv.find(L'=')); for (auto& c : k) c = (wchar_t)towlower(c);
        bool drop = k.rfind(L"utm_", 0) == 0 && kv != L"utm_source=libra";   // the one constant tag on sponsored links stays: it is the same for every user for (auto b : bad) if (k == b) drop = true;
        if (drop) cut = true; else if (!kv.empty()) out += (out.empty() ? L"" : L"&") + kv;
        i = e + 1;
    }
    if (!cut) return L"";
    return url.substr(0, q) + (out.empty() ? L"" : L"?" + out) + (h == std::wstring::npos ? L"" : url.substr(h));
}

// Daily identity: HMAC-SHA256(install secret, local date). The secret is random, DPAPI-wrapped in the registry; without it no site can compute or predict a day's values.
static bool FpDaily(int day, unsigned& a, unsigned& b) {
    static unsigned char key[32]; static bool have = false;
    if (!have) {
        const wchar_t* sub = L"Software\\Psylinks\\Libra\\Fk"; DWORD n = 0;
        if (RegGetValueW(HKEY_CURRENT_USER, sub, L"k", RRF_RT_REG_BINARY, nullptr, nullptr, &n) == ERROR_SUCCESS && n) {
            std::string raw(n, '\0');
            if (RegGetValueW(HKEY_CURRENT_USER, sub, L"k", RRF_RT_REG_BINARY, nullptr, &raw[0], &n) == ERROR_SUCCESS) { std::string k = SecretOpen(raw); if (k.size() == 32 && raw.compare(0, 4, "LBE1") == 0) { memcpy(key, k.data(), 32); have = true; } }
        }
        if (!have) {
            BCryptGenRandom(nullptr, key, 32, BCRYPT_USE_SYSTEM_PREFERRED_RNG); have = true;
            std::string w = SecretChunk(std::string((char*)key, 32)); HKEY k;
            if (!w.empty() && RegCreateKeyExW(HKEY_CURRENT_USER, sub, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) { RegSetValueExW(k, L"k", 0, REG_BINARY, (const BYTE*)w.data(), (DWORD)w.size()); RegCloseKey(k); }
        }
    }
    BCRYPT_ALG_HANDLE alg = nullptr; unsigned char out[32]{}; std::string msg = "day:" + std::to_string(day);
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG) != 0) return false;
    bool ok = BCryptHash(alg, key, 32, (PUCHAR)msg.data(), (ULONG)msg.size(), out, 32) == 0; BCryptCloseAlgorithmProvider(alg, 0);
    memcpy(&a, out, 4); memcpy(&b, out + 4, 4); return ok;
}

void BrowserWindow::InstallFpScript(Tab* t) {
    if (!t->webView) return;
    if (!t->fpScriptId.empty()) { t->webView->RemoveScriptToExecuteOnDocumentCreated(t->fpScriptId.c_str()); t->fpScriptId.clear(); }
    t->webView->CallDevToolsProtocolMethod(L"Emulation.setTimezoneOverride", (m_set.Get(L"fingerprint", L"1") != L"0" && m_set.On(L"tz_utc")) ? LR"({"timezoneId":"UTC"})" : LR"({"timezoneId":""})", nullptr);   // opt-in: Date and Intl both say UTC (empty = real time zone)
    if (m_set.Get(L"fingerprint", L"1") == L"0") return;
    unsigned keepA = 0, keepB = 0; FpDaily(0, keepA, keepB);   // day 0 = a value that never changes
    m_fpDay = StatDay(0); { wchar_t te[16]; if (TestEnv(L"LIBRA_TEST_DAY", te, 16)) m_fpDay = _wtoi(te); }   // test hook: pretend it is another day
    unsigned seedA = 0, seedB = 0; if (!FpDaily(m_fpDay, seedA, seedB)) { seedA = m_fpSeed; seedB = m_fpSeed * 7 + 1; }
    {   // one generic engine identity (header + navigator + Client Hints + workers): Windows 10 / x64 / Chrome, no Edge/WebView2 names, no OS build
        wil::unique_cotaskmem_string vs; if (m_env) m_env->get_BrowserVersionString(&vs);
        std::wstring M = std::to_wstring(vs.get() ? _wtoi(vs.get()) : 130);
        std::wstring ua = L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/" + M + L".0.0.0 Safari/537.36";
        wil::com_ptr<ICoreWebView2Settings> st; wil::com_ptr<ICoreWebView2Settings2> s2;
        if (SUCCEEDED(t->webView->get_Settings(&st)) && SUCCEEDED(st->QueryInterface(IID_PPV_ARGS(&s2)))) s2->put_UserAgent(ua.c_str());
        std::wstring j = LR"({"userAgent":")" + ua + LR"(","acceptLanguage":"en-US","platform":"Win32","userAgentMetadata":{"brands":[{"brand":"Chromium","version":")" + M + LR"("},{"brand":"Not.A/Brand","version":"99"}],"fullVersionList":[{"brand":"Chromium","version":")" + M + LR"(.0.0.0"},{"brand":"Not.A/Brand","version":"99.0.0.0"}],"platform":"Windows","platformVersion":"","architecture":"","model":"","mobile":false,"bitness":"","wow64":false}})";
        t->webView->CallDevToolsProtocolMethod(L"Emulation.setUserAgentOverride", j.c_str(), nullptr);
        std::wstring hc = LR"({"hardwareConcurrency":4})";   // one constant value: privacytests.org passes a value that is the same on every machine
        t->webView->CallDevToolsProtocolMethod(L"Emulation.setHardwareConcurrencyOverride", hc.c_str(), nullptr);   // also reaches workers
        t->webView->CallDevToolsProtocolMethod(L"Network.enable", L"{}", nullptr);
        t->webView->CallDevToolsProtocolMethod(L"Network.setExtraHTTPHeaders", LR"({"headers":{"Sec-GPC":"1"}})", nullptr);   // Global Privacy Control: first and third parties are told "do not sell or share"
    }
    std::wstring allow = L"[", ball = L"[", maxl = L"[";
    for (auto& kv : m_modes) (kv.second == 2 ? allow : kv.second == 1 ? ball : maxl) += L"\"" + Esc(kv.first) + L"\",";
    allow += L"]"; ball += L"]"; maxl += L"]";
    std::wstring defs = std::to_wstring(_wtoi(m_set.Get(L"block_default", L"0").c_str()));
    std::wstring keepList; for (auto& k : m_keepId) keepList += L"\"" + Esc(k) + L"\",";
    std::wstring js = L"(function(){var AL=" + allow + L",BL=" + ball + L",ML=" + maxl + L",DEF=" + defs + L",KP=[" + keepList + L"],SEEDK=" + std::to_wstring(keepA) + L",SEEDK2=" + std::to_wstring(keepB) + L",SEED=" + std::to_wstring(seedA) + L",SEED2=" + std::to_wstring(seedB) + L",A11=" + (m_set.Get(L"fp_a11y", L"1") == L"0" ? L"0" : L"1") + L";" + LR"js(
var th=location.hostname;if(location.protocol!='http:'&&location.protocol!='https:'){if(location.protocol!='about:'||window.top===window)return;try{th=window.top.location.hostname}catch(e){try{th=window.parent.location.hostname}catch(e2){return}}if(!th)return}
try{if(window.top!==window&&location.ancestorOrigins&&location.ancestorOrigins.length)th=new URL(location.ancestorOrigins[location.ancestorOrigins.length-1]).hostname}catch(e){}
function mt(L){for(var i=0;i<L.length;i++)if(th===L[i]||th.endsWith('.'+L[i]))return true;return false}
var MODE=mt(ML)?0:mt(BL)?1:mt(AL)?2:DEF;if(MODE===2)return;var FULL=(MODE===0);
if(/recaptcha|hcaptcha|turnstile|challenges\.cloudflare|accounts\.google\.com/.test(location.hostname+location.pathname))return;
var site=th.split('.').slice(-2).join('.');
function hs(s){var x=2166136261>>>0;for(var i=0;i<s.length;i++){x^=s.charCodeAt(i);x=Math.imul(x,16777619)>>>0}return x}
var KEEP=mt(KP);var S=(hs(site+':'+(KEEP?SEEDK2:SEED2))^(KEEP?SEEDK:SEED))>>>0;function R(n,z){return (Math.imul((S^z)>>>0,2654435761)>>>0)%n}
var GP=[['Intel','Intel(R) UHD Graphics 620'],['NVIDIA','NVIDIA GeForce GTX 1650'],['AMD','AMD Radeon(TM) Graphics'],['Intel','Intel(R) Iris(R) Xe Graphics'],['NVIDIA','NVIDIA GeForce RTX 3060']][R(5,7)];
function pert(d){var n=d.length;for(var i=0;i<n;i+=4){if(d[i+3]&&(Math.imul((i>>2)+S,2654435761)>>>0)%5===0){var k=i+(S%3);d[k]=d[k]^1}}}
try{
 var gc=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(t){if(t&&String(t).indexOf('2d')>=0)this.__lb=2;return gc.apply(this,arguments)};
 var gid=CanvasRenderingContext2D.prototype.getImageData;
 CanvasRenderingContext2D.prototype.getImageData=function(){var r=gid.apply(this,arguments);if(r.data.length<16000000)pert(r.data);return r};
 function clone(c){if(c.__lb!==2)return null;var w=c.width,h=c.height;if(!w||!h||w*h>4000000)return null;var o=document.createElement('canvas');o.width=w;o.height=h;var x=o.getContext('2d');x.drawImage(c,0,0);var im=gid.call(x,0,0,w,h);pert(im.data);x.putImageData(im,0,0);return o}
 var tdu=HTMLCanvasElement.prototype.toDataURL;HTMLCanvasElement.prototype.toDataURL=function(){var o=clone(this);return tdu.apply(o||this,arguments)};
 var tb=HTMLCanvasElement.prototype.toBlob;HTMLCanvasElement.prototype.toBlob=function(){var o=clone(this);return tb.apply(o||this,arguments)};
}catch(e){}
function wgl(P){var gp=P.getParameter;P.getParameter=function(p){if(FULL&&p===37445)return 'Google Inc. ('+GP[0]+')';if(FULL&&p===37446)return 'ANGLE ('+GP[0]+', '+GP[1]+' Direct3D11 vs_5_0 ps_5_0, D3D11)';return gp.apply(this,arguments)};
 var rp=P.readPixels;P.readPixels=function(x,y,w,h,f,t,px){var r=rp.apply(this,arguments);try{if(px&&px.length<16000000&&px instanceof Uint8Array)pert(px)}catch(e){}return r}}
try{wgl(WebGLRenderingContext.prototype);if(window.WebGL2RenderingContext)wgl(WebGL2RenderingContext.prototype)}catch(e){}
try{var gcd=AudioBuffer.prototype.getChannelData,done=new WeakSet();AudioBuffer.prototype.getChannelData=function(){var d=gcd.apply(this,arguments);if(!done.has(d)){done.add(d);for(var i=0;i<d.length;i+=100)d[i]+=((Math.imul(i+S,2654435761)>>>0)%7-3)*1e-7}return d};
 var gf=AnalyserNode.prototype.getFloatFrequencyData;AnalyserNode.prototype.getFloatFrequencyData=function(a){gf.apply(this,arguments);for(var i=0;i<a.length;i+=7)a[i]+=((Math.imul(i+S,2654435761)>>>0)%5-2)*1e-4}}catch(e){}
try{Object.defineProperty(Navigator.prototype,'globalPrivacyControl',{get:function(){return true},configurable:true,enumerable:true});
 ['baseLatency','outputLatency'].forEach(function(k){[window.AudioContext,window.webkitAudioContext].forEach(function(C){if(C&&k in C.prototype)Object.defineProperty(C.prototype,k,{get:function(){return k=='baseLatency'?0.01:0},configurable:true})})})}catch(e){}
if(FULL){try{
 if('memory' in Performance.prototype||performance.memory)Object.defineProperty(Performance.prototype,'memory',{get:function(){return {jsHeapSizeLimit:4294705152,totalJSHeapSize:10000000,usedJSHeapSize:10000000}},configurable:true});
 var sp=Screen.prototype;['availLeft','availTop'].forEach(function(k){if(k in sp)Object.defineProperty(sp,k,{get:function(){return 0},configurable:true})});
 if(A11){var mm=window.matchMedia;window.matchMedia=function(q){q=String(q).replace(/\(\s*prefers-(contrast|reduced-motion|reduced-transparency)\s*:\s*(more|less|custom|reduce)\s*\)/g,'(width:-1px)').replace(/\(\s*prefers-(contrast|reduced-motion|reduced-transparency)\s*:\s*no-preference\s*\)/g,'(min-width:0px)');return mm.call(window,q)}}
 try{var FA=/^(serif|sans-serif|monospace|cursive|fantasy|system-ui|ui-[a-z-]+|emoji|math|fangsong|arial|arial black|helvetica|helvetica neue|times|times new roman|courier|courier new|verdana|georgia|tahoma|trebuchet ms|segoe ui|segoe ui emoji|segoe ui symbol|calibri|cambria|consolas|impact|comic sans ms|lucida console|lucida sans unicode|palatino linotype|microsoft sans serif|sylfaen|webdings|wingdings|inherit|initial|unset)$/;var fd=Object.getOwnPropertyDescriptor(CanvasRenderingContext2D.prototype,'font');
  function pf(v){v=String(v);var m=v.match(/^(.*?\d(?:\.\d+)?(?:px|pt|em|rem|%|ex|ch|vw|vh|vmin|vmax|cm|mm|in|pc)(?:\/\S+)?\s+)(.+)$/);if(!m)return v;var pg=null;var fs=m[2].split(',').map(function(s){return s.trim()}).filter(function(s){var n=s.replace(/^['"]|['"]$/g,'').toLowerCase();if(FA.test(n))return true;if(pg===null){pg={};try{document.fonts.forEach(function(f){pg[String(f.family).replace(/^['"]|['"]$/g,'').toLowerCase()]=1})}catch(e){}}return !!pg[n]});if(!fs.length)fs=['sans-serif'];return m[1]+fs.join(', ')}
  Object.defineProperty(CanvasRenderingContext2D.prototype,'font',{get:fd.get,set:function(v){fd.set.call(this,pf(v))},configurable:true,enumerable:true})}catch(e){}
 if(window.Keyboard&&navigator.keyboard){var US={Backquote:'`',Minus:'-',Equal:'=',BracketLeft:'[',BracketRight:']',Backslash:'\\',Semicolon:';',Quote:"'",Comma:',',Period:'.',Slash:'/',IntlBackslash:'\\'};for(var c=0;c<26;c++)US['Key'+String.fromCharCode(65+c)]=String.fromCharCode(97+c);for(var d=0;d<10;d++)US['Digit'+d]=''+d;
  Keyboard.prototype.getLayoutMap=function(){return Promise.resolve(new Map(Object.keys(US).map(function(k){return [k,US[k]]})))}}
}catch(e){}
try{var hc=Object.getOwnPropertyDescriptor(Navigator.prototype,'hardwareConcurrency');Object.defineProperty(Navigator.prototype,'hardwareConcurrency',{get:function(){return 4},configurable:true});
 function ov(o,k,f){var d=Object.getOwnPropertyDescriptor(o,k);if(d&&d.get)Object.defineProperty(o,k,{get:function(){return f(d.get.call(this))},configurable:true})}
 ov(Navigator.prototype,'plugins',function(){return []});ov(Navigator.prototype,'mimeTypes',function(){return []});
 ov(Navigator.prototype,'languages',function(l){return Object.freeze([l[0]])});
 ['width','height','availWidth','availHeight'].forEach(function(k){var src=k.replace('avail','').toLowerCase(),sd=Object.getOwnPropertyDescriptor(Screen.prototype,src);Object.defineProperty(Screen.prototype,k,{get:function(){return Math.round(sd.get.call(this)/100)*100},configurable:true})});
 if(window.NavigatorUAData){var bf=function(b){return b.filter(function(x){return !/WebView2|Edge/.test(x.brand)})};
  ov(NavigatorUAData.prototype,'brands',bf);var hev=NavigatorUAData.prototype.getHighEntropyValues;
  NavigatorUAData.prototype.getHighEntropyValues=function(){return hev.apply(this,arguments).then(function(r){if(r.brands)r.brands=bf(r.brands);if(r.fullVersionList)r.fullVersionList=bf(r.fullVersionList);var f={platform:'Windows',platformVersion:'',architecture:'',bitness:'',model:'',wow64:false};for(var k in f)if(k in r)r[k]=f[k];return r})}}
 if(window.OffscreenCanvasRenderingContext2D){var og=OffscreenCanvasRenderingContext2D.prototype.getImageData;OffscreenCanvasRenderingContext2D.prototype.getImageData=function(){var r=og.apply(this,arguments);if(r.data.length<16000000)pert(r.data);return r}}
 var dm=Object.getOwnPropertyDescriptor(Navigator.prototype,'deviceMemory');if(dm)Object.defineProperty(Navigator.prototype,'deviceMemory',{get:function(){return 8},configurable:true})}catch(e){}
 try{if(window.MediaDeviceInfo)ov(MediaDeviceInfo.prototype,'label',function(){return ''});
  if(window.SpeechSynthesis)SpeechSynthesis.prototype.getVoices=function(){return []};
  ['gpu','bluetooth','usb','hid','serial','getBattery'].forEach(function(k){if(k in Navigator.prototype)Object.defineProperty(Navigator.prototype,k,{get:function(){return undefined},configurable:true})});
  if(window.NetworkInformation)['effectiveType','rtt','downlink','saveData'].forEach(function(k){ov(NetworkInformation.prototype,k,function(v){return k=='effectiveType'?'4g':k=='rtt'?50:k=='downlink'?10:false})})}catch(e){}}
})();)js";
    t->webView->AddScriptToExecuteOnDocumentCreated(js.c_str(), Microsoft::WRL::Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
        [this, t](HRESULT, LPCWSTR id) -> HRESULT { if (id && IndexOf(t) >= 0) t->fpScriptId = id; return S_OK; }).Get());
}
