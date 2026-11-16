// Reader mode (F9): a clean, text-only view of the article on the current page.
// The extractor runs in the page and returns plain data (heading/paragraph/list/quote/code texts). The reader page builds its DOM with textContent only,
// loads no pictures and no scripts of the site, and has its own strict CSP, so nothing from the site can run or phone home in it.
// shortcut: simple text-density heuristic, no pictures or inline links; upgrade to Mozilla Readability if articles are missed.
#pragma once
#include "Pages.h"

static const wchar_t* kReaderJs = LR"JS((function(){try{
var BAD='script,style,noscript,nav,header,footer,aside,form,iframe,svg,canvas,button,select,input,textarea,dialog,[role=navigation],[role=banner],[role=complementary],[aria-hidden=true],.sidebar,.comments,#comments,.advert,.ads';
function sc(e){var s=0,ps=e.querySelectorAll('p');for(var i=0;i<ps.length;i++){var n=(ps[i].innerText||'').length;if(n>60)s+=n}return s}
var best=document.querySelector('article')||document.querySelector('main,[role=main]'),bs=best?sc(best):0,cs=document.querySelectorAll('article,main,[role=main],section,div');
for(var k=0;k<cs.length&&k<4000;k++){var s=sc(cs[k]);if(s>bs*1.25||(!best&&s>bs)){best=cs[k];bs=s}}
var out=[],cnt=0;
function tx(e){return(e.innerText||'').replace(/\s+/g,' ').trim()}
var INL=/^(B|I|EM|STRONG|A|SPAN|FONT|U|SUP|SUB|SMALL|CODE)$/;
var INS=/^(B|I|EM|STRONG|A|U|SUP|SUB|SMALL|CODE)$/;
function own(e){var s=0,c=e.childNodes;for(var i=0;i<c.length;i++){var n=c[i];if(n.nodeType===3)s+=n.nodeValue.trim().length;else if(n.nodeType===1&&INS.test(n.tagName))s+=(n.innerText||'').length}return s}
function oldStyle(){var fb=null,fs=0,all=document.querySelectorAll('td,div,font,body,section,center');for(var k=0;k<all.length&&k<4000;k++){var e=all[k];if(e.matches(BAD))continue;var s=own(e);if(s>fs){fs=s;fb=e}}
 if(!fb||fs<1500)return false;var buf='',brs=0;
 function fl(){var t=buf.replace(/\s+/g,' ').trim();if(t.length>1&&cnt<1500){out.push(['p',t.slice(0,6000)]);cnt++}buf='';brs=0}
 var c=fb.childNodes;for(var i=0;i<c.length;i++){var n=c[i];if(n.nodeType===3){buf+=n.nodeValue;brs=0}else if(n.nodeType===1){var t=n.tagName;
  if(t==='BR'){brs++;if(brs>=2)fl();else buf+=' '}else if(INL.test(t)){buf+=n.innerText||'';brs=0}
  else if(!n.matches(BAD)){fl();var x=tx(n);if(/^H[1-4]$/.test(t)){if(x&&x.length<200){out.push(['h2',x]);cnt++}}else if(t==='P'||t==='BLOCKQUOTE'||t==='LI'){if(x){out.push([t==='P'?'p':t==='LI'?'li':'q',x.slice(0,4000)]);cnt++}}}}}
 fl();return true}
function walk(n){var ch=n.children;for(var i=0;i<ch.length&&cnt<1500;i++){var e=ch[i],t=e.tagName;if(e.matches(BAD))continue;var x;
 if(/^H[1-4]$/.test(t)){x=tx(e);if(x&&x.length<200){out.push([t==='H1'?'h1':t==='H2'?'h2':'h3',x]);cnt++}}
 else if(t==='P'){x=tx(e);if(x.length>1){out.push(['p',x.slice(0,6000)]);cnt++}}
 else if(t==='LI'){x=tx(e);if(x){out.push(['li',x.slice(0,2000)]);cnt++}}
 else if(t==='BLOCKQUOTE'){x=tx(e);if(x){out.push(['q',x.slice(0,4000)]);cnt++}}
 else if(t==='PRE'){x=(e.innerText||'').slice(0,4000);if(x.trim()){out.push(['pre',x]);cnt++}}
 else walk(e)}}
if(!best||bs<300){if(!oldStyle())return JSON.stringify({err:1})}else walk(best);if(out.length<3)return JSON.stringify({err:1});
var hs=document.querySelectorAll('h1'),h=null,dt=(document.title||'').toLowerCase();   // the heading that the tab title contains (a site banner h1 is not the article title)
for(var q=0;q<hs.length&&q<20;q++){var ht=(hs[q].innerText||'').trim();if(ht&&dt.indexOf(ht.toLowerCase())>=0&&(!h||ht.length>(h.innerText||'').trim().length))h=hs[q]}
if(!h)h=(best&&best.querySelector('h1'))||hs[0];
return JSON.stringify({title:((h&&h.innerText)||document.title||'').slice(0,300),site:location.hostname,url:location.href,blocks:out})
}catch(e){return JSON.stringify({err:1})}})())JS";

// raw = the JavaScript string literal that ExecuteScript returned for kReaderJs (a quoted JSON text). '<' is escaped so it can never close the script tag.
inline std::wstring ReaderPage(const std::wstring& raw, bool grey) {
    std::wstring lit; lit.reserve(raw.size() + 16);
    for (wchar_t c : raw) { if (c == L'<') lit += L"\\u003c"; else lit += c; }
    if (lit.empty() || lit[0] != L'"') lit = L"\"{\\\"err\\\":1}\"";
    std::wstring b = LR"H(<style>
body{justify-content:center}
#bar{position:sticky;top:0;display:flex;gap:8px;justify-content:flex-end;padding:10px 0;background:var(--bg);z-index:2}
#bar button{padding:6px 14px;font-size:13px}
#rd{max-width:680px;margin:0 auto;padding-bottom:80px;font:var(--fs,19px)/1.7 Georgia,'Times New Roman',serif}
#rd h1{font:700 2em/1.2 'Segoe UI Variable Display','Segoe UI',sans-serif;margin:10px 0 6px}
#rd h2,#rd h3{font-family:'Segoe UI',sans-serif;margin:1.4em 0 .4em;line-height:1.3}#rd h2{font-size:1.35em}#rd h3{font-size:1.1em}
#rd p{margin:0 0 1em}#rd ul{margin:0 0 1em 1.4em}#rd li{margin:.2em 0}
#rd blockquote{margin:0 0 1em;padding-left:16px;border-left:3px solid #ccc;color:#555}
#rd pre{white-space:pre-wrap;word-break:break-word;background:#f4f4f4;border-radius:10px;padding:12px 14px;font:13px/1.5 Consolas,monospace;margin:0 0 1em}
#meta{font:13px 'Segoe UI',sans-serif;color:var(--mute);margin-bottom:26px}#meta a{color:inherit}
body.sepia{--bg:#f4ecd8;--fg:#3b2f1e}body.dark{--bg:#1b1b1b;--fg:#e6e6e6;--mute:#9a9a9a}body.dark #rd pre{background:#2a2a2a}body.dark #rd blockquote{color:#bbb;border-color:#555}body.dark button{background:#333;color:#eee}
</style><div style="width:100%"><div id=bar><button id=sm>A-</button><button id=bg>A+</button><button id=th>Theme</button></div><article id=rd></article></div>)H";
    std::wstring js = L"const RAW=" + lit + LR"H(;
let d;try{d=JSON.parse(RAW)}catch(e){d={err:1}}
const rd=document.getElementById('rd'),mk=(t,x)=>{const e=document.createElement(t);e.textContent=x;return e};
if(!d||d.err||!Array.isArray(d.blocks)){rd.appendChild(mk('p','This page does not look like an article, so there is nothing to show in reader mode.'))}
else{rd.appendChild(mk('h1',String(d.title||'')));const m=document.createElement('div');m.id='meta';m.appendChild(document.createTextNode(String(d.site||'')+'  .  '));
 if(/^https?:\/\//i.test(d.url||'')){const a=mk('a','Open the original page');a.href=d.url;a.rel='noopener noreferrer';m.appendChild(a)}rd.appendChild(m);
 let ul=null;for(const b of d.blocks){if(!Array.isArray(b)||typeof b[1]!=='string')continue;const k=b[0];
  if(k==='li'){if(!ul){ul=document.createElement('ul');rd.appendChild(ul)}ul.appendChild(mk('li',b[1]));continue}ul=null;
  const tag={h1:'h2',h2:'h2',h3:'h3',p:'p',q:'blockquote',pre:'pre'}[k];if(tag)rd.appendChild(mk(tag,b[1]))}}
let fs=19;const sz=n=>{fs=Math.max(14,Math.min(30,fs+n));document.documentElement.style.setProperty('--fs',fs+'px')};
document.getElementById('sm').onclick=()=>sz(-2);document.getElementById('bg').onclick=()=>sz(2);
const T=['','sepia','dark'];let ti=0;document.getElementById('th').onclick=()=>{ti=(ti+1)%3;document.body.className=T[ti]};
)H";
    return PageShell(L"Reader", b, grey, js, L"", false, false);
}
