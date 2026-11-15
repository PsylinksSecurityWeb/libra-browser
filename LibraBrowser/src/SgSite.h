#pragma once
// Auto suggest page script (document start, top frame, https or localhost only; installed only while the switch is on).
// It tells Libra ONLY about the one empty text box you just clicked: its label, placeholder, name, id, type, size and position.
// It never sends what is typed, never reads other page text, and never receives anything back. Libra's own popup does the rest.
inline const wchar_t* kSgSite = LR"JS((function(){
if(window.top!==window)return;
var L=location;if(!(L.protocol=='https:'||(L.protocol=='http:'&&/^(localhost|127\.0\.0\.1|\[::1\])$/.test(L.hostname))))return;
var on=false,lastIn=0;
function C(s,n){s=String(s==null?'':s).replace(/[\t\r\n]+/g,' ').trim();return s.length>n?s.slice(0,n):s}
function post(a){try{chrome.webview.postMessage(a.join('\t'))}catch(e){}}
function vis(e){var r=e.getBoundingClientRect();if(r.width<12||r.height<10||r.bottom<=0||r.right<=0||r.top>=innerHeight||r.left>=innerWidth)return false;var s=getComputedStyle(e);return s.visibility!='hidden'&&s.display!='none'&&parseFloat(s.opacity)>0.05}
function label(e){var t=[];try{if(e.labels)for(var i=0;i<e.labels.length&&i<3;i++)t.push(e.labels[i].innerText)}catch(x){}
 var lb=e.getAttribute('aria-labelledby');if(lb)lb.split(/\s+/).slice(0,3).forEach(function(id){var n=document.getElementById(id);if(n)t.push(n.innerText)});
 return C(t.join(' '),160)}
function off(){if(!on)return;on=false;post(['sg.hide'])}
function mark(e){if(e.isTrusted)lastIn=Date.now()}
function near(e){var t='',p=e.previousSibling,n=0,br=0;   // text just before the box; never past another form control, never another box's label
 while(p&&n<6&&!C(t,160)){
  if(p.nodeType==3)t=p.textContent;
  else if(p.nodeType==1){var g=p.tagName;
   if(/^(INPUT|TEXTAREA|SELECT|BUTTON)$/.test(g))break;
   if(g=='BR'){if(++br>1)break}
   else if(g=='LABEL'&&p.htmlFor&&p.htmlFor!=e.id)break;
   else if(/^(SPAN|B|I|EM|STRONG|SMALL|LABEL|A|U|FONT)$/.test(g))t=p.innerText;
   else break}
  p=p.previousSibling;n++}
 if(!C(t,160)){var td=e.closest('td,th');if(td&&td.previousElementSibling)t=td.previousElementSibling.innerText}
 if(!C(t,160)){var fs=e.closest('fieldset');if(fs){var lg=fs.querySelector('legend');if(lg)t=lg.innerText}}
 return C(t,160)}
document.addEventListener('focusin',function(ev){
 var e=ev.target;if(!e||!e.tagName)return;var tg=e.tagName;if(tg!='INPUT'&&tg!='TEXTAREA')return;
 if(Date.now()-lastIn>1200){off();return}   // only focus that follows a real click or key press (not focus() calls, autofocus or synthetic events), like Chrome, Firefox and Safari
 var ty=(e.type||'text').toLowerCase();if(tg=='INPUT'&&!/^(text|email|tel|url|number)$/.test(ty)){off();return}
 if(e.value||e.readOnly||e.disabled||!vis(e)){off();return}
 var scope=e.form||document;if([].some.call(scope.querySelectorAll('input[type=password]'),vis)){off();return}
 if(e.getRootNode&&e.getRootNode()!==document){off();return}
 var r=e.getBoundingClientRect();on=true;
 post(['sg.focus',C(label(e)||e.title||near(e),160),C(e.placeholder,120),C(e.name,80),C(e.id,80),C(e.getAttribute('autocomplete'),40),ty,C(e.getAttribute('aria-label'),120),tg=='TEXTAREA'?1:0,e.maxLength>0?e.maxLength:0,Math.round(r.left),Math.round(r.top),Math.round(r.width),Math.round(r.height),window.devicePixelRatio||1]);
},true);
document.addEventListener('input',off,true);
document.addEventListener('focusout',function(){setTimeout(function(){var a=document.activeElement;if(!a||(a.tagName!='INPUT'&&a.tagName!='TEXTAREA'))off()},60)},true);
addEventListener('scroll',off,true);addEventListener('resize',off);
document.addEventListener('pointerdown',mark,true);document.addEventListener('mousedown',mark,true);document.addEventListener('touchstart',mark,true);
document.addEventListener('keydown',function(e){mark(e);if(e.key=='Escape')off()},true);
})();)JS";
