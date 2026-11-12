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

void BrowserWindow::InstallFpScript(Tab* t) {
    if (!t->webView) return;
    if (!t->fpScriptId.empty()) { t->webView->RemoveScriptToExecuteOnDocumentCreated(t->fpScriptId.c_str()); t->fpScriptId.clear(); }
    if (m_set.Get(L"fingerprint", L"1") == L"0") return;
    std::wstring allow = L"[", ball = L"[", maxl = L"[";
    for (auto& kv : m_modes) (kv.second == 2 ? allow : kv.second == 1 ? ball : maxl) += L"\"" + Esc(kv.first) + L"\",";
    allow += L"]"; ball += L"]"; maxl += L"]";
    std::wstring defs = std::to_wstring(_wtoi(m_set.Get(L"block_default", L"0").c_str()));
    std::wstring js = L"(function(){var AL=" + allow + L",BL=" + ball + L",ML=" + maxl + L",DEF=" + defs + L",SEED=" + std::to_wstring(m_fpSeed) + L";" + LR"js(
var th=location.hostname;if(location.protocol!='http:'&&location.protocol!='https:'){if(location.protocol!='about:'||window.top===window)return;try{th=window.top.location.hostname}catch(e){try{th=window.parent.location.hostname}catch(e2){return}}if(!th)return}
try{if(window.top!==window&&location.ancestorOrigins&&location.ancestorOrigins.length)th=new URL(location.ancestorOrigins[location.ancestorOrigins.length-1]).hostname}catch(e){}
function mt(L){for(var i=0;i<L.length;i++)if(th===L[i]||th.endsWith('.'+L[i]))return true;return false}
var MODE=mt(ML)?0:mt(BL)?1:mt(AL)?2:DEF;if(MODE===2)return;var FULL=(MODE===0);
if(/recaptcha|hcaptcha|turnstile|challenges\.cloudflare|accounts\.google\.com/.test(location.hostname+location.pathname))return;
var site=th.split('.').slice(-2).join('.');
function hs(s){var x=2166136261>>>0;for(var i=0;i<s.length;i++){x^=s.charCodeAt(i);x=Math.imul(x,16777619)>>>0}return x}
var S=(hs(site)^SEED)>>>0;
function pert(d){var n=d.length;for(var i=0;i<n;i+=4){if(d[i+3]&&(Math.imul((i>>2)+S,2654435761)>>>0)%5===0){var k=i+(S%3);d[k]=d[k]^1}}}
try{
 var gc=HTMLCanvasElement.prototype.getContext;HTMLCanvasElement.prototype.getContext=function(t){if(t&&String(t).indexOf('2d')>=0)this.__lb=2;return gc.apply(this,arguments)};
 var gid=CanvasRenderingContext2D.prototype.getImageData;
 CanvasRenderingContext2D.prototype.getImageData=function(){var r=gid.apply(this,arguments);if(r.data.length<16000000)pert(r.data);return r};
 function clone(c){if(c.__lb!==2)return null;var w=c.width,h=c.height;if(!w||!h||w*h>4000000)return null;var o=document.createElement('canvas');o.width=w;o.height=h;var x=o.getContext('2d');x.drawImage(c,0,0);var im=gid.call(x,0,0,w,h);pert(im.data);x.putImageData(im,0,0);return o}
 var tdu=HTMLCanvasElement.prototype.toDataURL;HTMLCanvasElement.prototype.toDataURL=function(){var o=clone(this);return tdu.apply(o||this,arguments)};
 var tb=HTMLCanvasElement.prototype.toBlob;HTMLCanvasElement.prototype.toBlob=function(){var o=clone(this);return tb.apply(o||this,arguments)};
}catch(e){}
function wgl(P){var gp=P.getParameter;P.getParameter=function(p){if(FULL&&p===37445)return 'Google Inc. (Libra)';if(FULL&&p===37446)return 'ANGLE (Libra, Generic Renderer)';return gp.apply(this,arguments)};
 var rp=P.readPixels;P.readPixels=function(x,y,w,h,f,t,px){var r=rp.apply(this,arguments);try{if(px&&px.length<16000000&&px instanceof Uint8Array)pert(px)}catch(e){}return r}}
try{wgl(WebGLRenderingContext.prototype);if(window.WebGL2RenderingContext)wgl(WebGL2RenderingContext.prototype)}catch(e){}
try{var gcd=AudioBuffer.prototype.getChannelData,done=new WeakSet();AudioBuffer.prototype.getChannelData=function(){var d=gcd.apply(this,arguments);if(!done.has(d)){done.add(d);for(var i=0;i<d.length;i+=100)d[i]+=((Math.imul(i+S,2654435761)>>>0)%7-3)*1e-7}return d};
 var gf=AnalyserNode.prototype.getFloatFrequencyData;AnalyserNode.prototype.getFloatFrequencyData=function(a){gf.apply(this,arguments);for(var i=0;i<a.length;i+=7)a[i]+=((Math.imul(i+S,2654435761)>>>0)%5-2)*1e-4}}catch(e){}
if(FULL){try{var hc=Object.getOwnPropertyDescriptor(Navigator.prototype,'hardwareConcurrency');Object.defineProperty(Navigator.prototype,'hardwareConcurrency',{get:function(){return Math.min(4,hc.get.call(this))},configurable:true});
 function ov(o,k,f){var d=Object.getOwnPropertyDescriptor(o,k);if(d&&d.get)Object.defineProperty(o,k,{get:function(){return f(d.get.call(this))},configurable:true})}
 ov(Navigator.prototype,'plugins',function(){return []});ov(Navigator.prototype,'mimeTypes',function(){return []});
 ov(Navigator.prototype,'languages',function(l){return Object.freeze([l[0]])});
 ['width','height','availWidth','availHeight'].forEach(function(k){ov(Screen.prototype,k,function(v){return Math.round(v/100)*100})});
 if(window.NavigatorUAData){var bf=function(b){return b.filter(function(x){return !/WebView2|Edge/.test(x.brand)})};
  ov(NavigatorUAData.prototype,'brands',bf);var hev=NavigatorUAData.prototype.getHighEntropyValues;
  NavigatorUAData.prototype.getHighEntropyValues=function(){return hev.apply(this,arguments).then(function(r){if(r.brands)r.brands=bf(r.brands);if(r.fullVersionList)r.fullVersionList=bf(r.fullVersionList);return r})}}
 if(window.OffscreenCanvasRenderingContext2D){var og=OffscreenCanvasRenderingContext2D.prototype.getImageData;OffscreenCanvasRenderingContext2D.prototype.getImageData=function(){var r=og.apply(this,arguments);if(r.data.length<16000000)pert(r.data);return r}}
 var dm=Object.getOwnPropertyDescriptor(Navigator.prototype,'deviceMemory');if(dm)Object.defineProperty(Navigator.prototype,'deviceMemory',{get:function(){return Math.min(8,dm.get.call(this))},configurable:true})}catch(e){}}
})();)js";
    t->webView->AddScriptToExecuteOnDocumentCreated(js.c_str(), Microsoft::WRL::Callback<ICoreWebView2AddScriptToExecuteOnDocumentCreatedCompletedHandler>(
        [this, t](HRESULT, LPCWSTR id) -> HRESULT { if (id && IndexOf(t) >= 0) t->fpScriptId = id; return S_OK; }).Get());
}
