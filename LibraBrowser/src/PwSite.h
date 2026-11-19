#pragma once
// Script injected at document start into every https (or localhost) top-level page.
// 1) New-password field -> small generator card (closed shadow DOM). 2) Submit with a filled password -> "pw.save" to Libra.
// 3) Login field focus -> "pw.focus": Libra (native) decides and shows its OWN popup; the page never receives a saved password.
#include "PwJs.h"

inline const wchar_t* kPwSiteA = LR"JS((function(){
if(window.top!==window)return;
var L=location;if(!(L.protocol=='https:'||(L.protocol=='http:'&&/^(localhost|127\.0\.0\.1|\[::1\])$/.test(L.hostname))))return;
var PRIV=@PRIV@;
)JS";

inline const wchar_t* kPwSiteB = LR"JS(
var enc=encodeURIComponent;
function post(){try{chrome.webview.postMessage(Array.prototype.join.call(arguments,'\t'))}catch(e){}}
function vis(e){var r=e.getBoundingClientRect();return r.width>0&&r.height>0}
function pws(f){return [].filter.call((f||document).querySelectorAll('input[type=password]'),vis)}
function isNew(el){var a=(el.autocomplete||'').toLowerCase();if(a=='new-password')return true;if(a=='current-password')return false;
 var p=pws(el.form||document);if(p.length>=3)return p.indexOf(el)>0;return p.length==2}
function setv(e,v){Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(e,v);e.dispatchEvent(new Event('input',{bubbles:true}));e.dispatchEvent(new Event('change',{bubbles:true}))}
function E(t,a,k){var e=document.createElement(t);for(var x in a)e[x]=a[x];(k||[]).forEach(function(c){e.appendChild(typeof c=='string'?document.createTextNode(c):c)});return e}
var CSS='*{box-sizing:border-box;font:13px "Segoe UI",system-ui,sans-serif;color:#111}.c{width:300px;background:#fff;border-radius:16px;box-shadow:none;padding:14px}'+
'.t{font-weight:600;font-size:14px;margin-bottom:8px}.pw{font:13px Consolas,monospace;background:#f2f2f2;border-radius:10px;padding:9px 11px;word-break:break-all;max-height:86px;overflow:auto;user-select:all}'+
'.b{height:4px;background:#e6e6e6;border-radius:2px;margin:8px 0 4px;overflow:hidden}.b i{display:block;height:4px;background:#111;width:0}.s{color:#8a8a8a;font-size:11px;min-height:14px}'+
'.r{display:flex;align-items:center;gap:8px;margin:9px 0}.r input[type=range]{flex:1;accent-color:#111}.r input[type=number]{width:58px;border:0;background:#f2f2f2;border-radius:8px;padding:5px 7px}'+
'.o{display:grid;grid-template-columns:1fr 1fr;gap:4px 8px}.o label{display:flex;gap:6px;align-items:center;cursor:pointer;color:#444;font-size:12px}.o input{accent-color:#111}'+
'.k{display:flex;gap:8px;margin-top:11px}button{border:0;border-radius:999px;padding:8px 14px;background:#efefef;cursor:pointer}button.p{background:#111;color:#fff;flex:1}';
var host=null,cur='',tgt=null;
function close(){if(host){host.remove();host=null}}
function fill(p){var f=tgt.form||document;pws(f).forEach(function(x){if(x===tgt||isNew(x))setv(x,p)})}
function open(el){
 close();tgt=el;var ml=parseInt(el.maxLength)>0?Math.min(286,parseInt(el.maxLength)):286,len=Math.min(20,ml);
 var o={lower:1,upper:1,digit:1,symbol:1,noAmb:0,noRep:0,each:1};
 host=document.createElement('div');host.style.cssText='all:initial;position:fixed;z-index:2147483647';
 var root=host.attachShadow({mode:'closed'});
 try{var sh=new CSSStyleSheet();sh.replaceSync(CSS);root.adoptedStyleSheets=[sh]}catch(e){root.appendChild(E('style',{textContent:CSS}))}
 var out=E('div',{className:'pw'}),bar=E('i'),lab=E('div',{className:'s'}),rng=E('input',{type:'range',min:1,max:ml,value:len}),num=E('input',{type:'number',min:1,max:ml,value:len});
 function gen(){len=Math.max(1,Math.min(ml,parseInt(num.value)||1));cur=lbGen(len,o);out.textContent=cur||'Pick at least one type';var b=lbBits(len,o);bar.style.width=Math.min(100,b/1.28)+'%';lab.textContent=cur?lbLabel(b)+' ('+b+' bits)':''}
 function cb(k,t){var c=E('input',{type:'checkbox',checked:!!o[k]});c.onchange=function(){o[k]=c.checked?1:0;gen()};return E('label',{},[c,t])}
 rng.oninput=function(){num.value=rng.value;gen()};num.oninput=function(){rng.value=Math.max(1,Math.min(ml,parseInt(num.value)||1));gen()};
 var again=E('button',{textContent:'New'}),use=E('button',{textContent:'Use this password',className:'p'});
 again.onclick=gen;use.onclick=function(){if(cur)fill(cur);close()};
 root.appendChild(E('div',{className:'c'},[E('div',{className:'t',textContent:'Strong password'}),out,E('div',{className:'b'},[bar]),lab,
  E('div',{className:'r'},[rng,num]),E('div',{className:'o'},[cb('lower','a-z'),cb('upper','A-Z'),cb('digit','0-9'),cb('symbol','Symbols'),cb('noAmb','No look-alikes'),cb('noRep','No repeats'),cb('each','One of each')]),
  E('div',{className:'k'},[again,use])]));
 document.documentElement.appendChild(host);gen();
 var r=el.getBoundingClientRect();host.style.left=Math.max(8,Math.min(innerWidth-310,r.left))+'px';host.style.top=Math.max(8,Math.min(r.bottom+6,innerHeight-350))+'px'}
document.addEventListener('mousedown',function(e){if(host&&e.target!==host&&e.target!==tgt)close()},true);
document.addEventListener('keydown',function(e){if(e.key=='Escape')close()},true);
document.addEventListener('focusin',function(e){var el=e.target;if(!el||el.tagName!='INPUT')return;
 if(el.type=='password'){if(isNew(el))open(el);else if(!PRIV)post('pw.focus')}
 else if(!PRIV&&/^(text|email|tel)$/.test(el.type)&&el.form){var p=pws(el.form);if(p.length&&!isNew(p[0]))post('pw.focus')}},true);
document.addEventListener('focusout',function(e){if(!PRIV&&e.target&&e.target.tagName=='INPUT')post('pw.blur')},true);
var last='';
function snap(f){var p=pws(f).filter(function(x){return x.value});if(!p.length)return;
 var pw=p[p.length-1],ins=[].filter.call((pw.form||document).querySelectorAll('input'),function(x){return vis(x)&&/^(text|email|tel)$/.test(x.type)&&x.value&&(pw.compareDocumentPosition(x)&2)});
 var u=ins.length?ins[ins.length-1].value:'';if(pw.value.length>1024||u.length>256)return;
 var k=u+'\n'+pw.value;if(k===last)return;last=k;setTimeout(function(){last=''},5000);post('pw.save',enc(u),enc(pw.value))}
document.addEventListener('submit',function(e){if(!PRIV)snap(e.target)},true);
document.addEventListener('click',function(e){if(PRIV||!e.isTrusted)return;var b=e.target.closest&&e.target.closest('button,input[type=submit]');if(!b)return;
 var t=(b.getAttribute('type')||'').toLowerCase();if(t=='submit'||(b.tagName=='BUTTON'&&!t))snap(b.form||document)},true);
})();
)JS";
