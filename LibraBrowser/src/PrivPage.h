#pragma once
// libra://privacy : what Libra remembers, drawn as live bars; one button really deletes it.
#include "Pages.h"

inline std::wstring PrivacyPage(bool grey, bool isDefault, const Settings& st) {
    std::wstring b = LR"H(<style>
.hero{display:flex;gap:28px;align-items:center;padding:26px 28px;flex-wrap:wrap}
.big{font:800 56px 'Segoe UI Variable Display','Segoe UI',sans-serif;letter-spacing:-.03em;line-height:1}
.mini{display:flex;gap:26px;margin-left:auto}.mini div{text-align:center}.mini b{display:block;font-size:22px;font-weight:600}.mini small{color:var(--mute)}
.stack{display:flex;height:14px;border-radius:7px;overflow:hidden;background:#e6e6e6;margin:0 28px 22px;gap:2px}
.stack i{display:block;height:14px;transition:width .8s cubic-bezier(.2,.7,.2,1)}
.rg{display:inline-flex;background:rgba(255,255,255,.7);border-radius:999px;padding:3px}.rg button{background:none;padding:7px 16px;font-size:13px}.rg button.on{background:#111;color:#fff}
.pr{position:relative;display:grid;grid-template-columns:1fr auto 44px;gap:16px;align-items:center;padding:16px 18px;margin:8px 0;border-radius:16px;background:#f5f5f5;overflow:hidden}
.pr>*{position:relative}.pr .fill{position:absolute;inset:0 auto 0 0;width:0;transition:width .9s cubic-bezier(.2,.7,.2,1);border-radius:16px 0 0 16px}
.pr .v{text-align:right;font-weight:600;font-variant-numeric:tabular-nums}
.pr .t small{display:block;color:var(--mute);margin-top:2px;white-space:normal}
.go{display:flex;align-items:center;gap:16px;margin:0 0 18px;flex-wrap:wrap}.go button.pri{padding:13px 30px;font-size:15px;font-weight:600}
#msg{color:var(--mute);transition:opacity .4s}#msg b{color:var(--fg)}
.lg{display:flex;gap:16px;flex-wrap:wrap;align-items:center;margin-bottom:14px;font-size:13px}.lg i,.tr i{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:6px}.lg small{margin-left:auto;color:var(--mute)}
.bars{display:flex;gap:10px;align-items:flex-end;height:150px}.bars>div{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:flex-end;height:100%;font-size:12px;color:var(--mute)}
.bars .st{width:100%;max-width:56px;display:flex;flex-direction:column-reverse;border-radius:8px;overflow:hidden;gap:1px;min-height:3px;background:#eee;transition:height .8s cubic-bezier(.2,.7,.2,1)}.bars .st b{display:block}
.tr{display:grid;grid-template-columns:1fr 120px 52px;gap:12px;align-items:center;padding:7px 4px;font-size:13px}.tr .nm{overflow:hidden;text-overflow:ellipsis;white-space:nowrap}.tr .bar{height:5px;border-radius:3px}.tr .n{text-align:right;font-variant-numeric:tabular-nums;color:var(--mute)}
.snp{background:#f5f5f5;border-radius:14px;padding:14px 16px;font:12px/1.55 Consolas,"Cascadia Mono",monospace;white-space:pre-wrap;word-break:break-all;margin:8px 0;max-height:260px;overflow:auto}
.snl div{display:flex;justify-content:space-between;gap:12px;padding:5px 2px;font-size:13px;border-bottom:1px solid #f0f0f0}.snl .bad{color:#d6141c}
.card .row{padding:15px 0;gap:28px;min-height:62px;border-top:1px solid #f0f0f0;align-items:center}.card .row:first-child{border-top:0}.card .row>div:first-child{flex:1;min-width:0}.card .row small{max-width:none;margin-top:2px;font-size:13px;line-height:1.45}.lbl{font-size:15px;font-weight:500;display:block}.card:has(.lg){padding-top:16px;padding-bottom:14px}
.foot{color:var(--mute);font-size:13px;padding:0 0 14px;margin:-2px 0 0}
details.info{border-top:1px solid #f0f0f0}details.info summary{list-style:none;cursor:pointer;display:flex;justify-content:space-between;align-items:center;padding:15px 0;font-size:15px;font-weight:500}details.info summary::-webkit-details-marker{display:none}details.info summary::after{content:"";width:7px;height:7px;border-right:2px solid #aaa;border-bottom:2px solid #aaa;transform:rotate(45deg);margin-right:4px;transition:transform .2s}details.info[open] summary::after{transform:rotate(-135deg)}details.info .body{padding:0 0 16px;color:#777;font-size:13px;line-height:1.6}
.pulse{animation:pop .5s}@keyframes pop{40%{transform:scale(1.04)}}
</style>
<h1>Privacy</h1>
<div class=go><button class=pri id=go>Clear selected</button><div class=rg id=rg><button data-r=hour>Last hour</button><button data-r=day>24 hours</button><button data-r=week>7 days</button><button data-r=month>4 weeks</button><button class=on data-r=all>All time</button></div><a href="#" id=sa style="font-size:13px;color:#111">Select all / none</a><span id=msg></span></div>
<div class=card style="padding:0"><div class=hero><div><div class=big id=trk>0</div><div style="color:var(--mute);margin-top:6px">ads and trackers blocked</div></div>
<div class=mini><div><b id=blk>0</b><small>blocked servers</small></div><div><b id=vl>0</b><small>answered on this PC</small></div><div><b id=cdom>0</b><small>sites with cookies</small></div><div><b id=tot>0 B</b><small>stored on disk</small></div></div></div>
<div class=stack><i id=s1 style="background:#111"></i><i id=s2 style="background:#7c5cff"></i><i id=s3 style="background:#38bdf8"></i></div></div>
<p style="color:var(--mute);font-size:12.5px;margin:14px 2px 0;line-height:1.5">Sites cannot see your device name, model or system build, and your browser identity changes every day. Your IP address is still visible to the sites you visit; only a VPN or Tor can hide it.</p>
<h2>Protection report</h2>
<div class=card><div class=lg><span><i style="background:#E5484D"></i>Ads</span><span><i style="background:#F5A524"></i>Trackers</span><span><i style="background:#3E7BFA"></i>Analytics</span><span><i style="background:#8E4EC6"></i>Social</span><small>types are estimated from server names</small></div>
<div class=bars id=bars></div><h3 style="margin:22px 0 6px;font-size:14px">Top blocked servers <small style="color:var(--mute);font-weight:400">last 90 days</small></h3><div id=top></div></div>
<h2>What Libra sends</h2>
<div class=card>)H" + ToggleOn(st, L"sn_count", L"Anonymous counts", L"Shown and clicked counts for sponsored items, and rounded totals of blocked ads and trackers") + LR"H(
<p class=foot id=snoff style="display:none">Sponsored content is not active in this version, so Libra sends nothing.</p>
<details class=info id=sn2><summary>See the next report and what was sent</summary><div class=body><p id=snd></p><pre class=snp id=snp></pre>
<div class=go style="margin:10px 0 14px"><button id=snsend>Send now</button><button id=snclr>Discard these counts</button></div><div class=snl id=snl></div>
<p style="margin-top:12px">No id, cookie, address or history. Every report has the same size. Your IP address is visible to the network while it travels; it is not stored in the report.</p></div></details></div>
<h2>What Libra remembers</h2>
<div class=card>)H";
    struct R { const wchar_t *k, *t, *d, *col; };
    const R rows[] = {
        { L"hist",  L"Browsing history",       L"Pages you visited, in Libra and in the engine",       L"rgba(17,17,17,.10)" },
        { L"cook",  L"Cookies",                L"Logins, preferences and tracking cookies",            L"rgba(124,92,255,.18)" },
        { L"site",  L"Site data",              L"Local storage, databases, offline workers",           L"rgba(124,92,255,.18)" },
        { L"cache", L"Cache",                  L"Saved images and scripts",                            L"rgba(56,189,248,.22)" },
        { L"dns",   L"Lookup memory",             L"Names of the sites Libra looked up",                  L"rgba(17,17,17,.10)" },
        { L"rec",   L"Closed tabs",            L"The reopen list",                                     L"rgba(17,17,17,.10)" },
        { L"ctr",   L"Block counters",         L"The totals you see on the shield",                    L"rgba(17,17,17,.10)" },
        { L"dl",    L"Download list",          L"The list only; your files stay where they are",        L"rgba(17,17,17,.10)" },
        { L"agt",   L"Assistant data",         L"Memory notes, permissions, files. Your API key stays", L"rgba(17,17,17,.10)" } };
    for (auto& r : rows)
        b += std::wstring(L"<div class=pr><i class=fill id=m_") + r.k + L" style=\"background:" + r.col + L"\"></i><div class=t><b>" + r.t + L"</b><small>" + r.d +
             L"</small></div><div class=v id=v_" + r.k + L">0</div><input class=cb type=checkbox id=c_" + r.k + L" checked></div>";
    b += LR"H(</div>
<h2>Default browser</h2><div class=card><div class=row><div><span class=lbl>Make Libra your default browser</span><small id=dsub>)H" + std::wstring(isDefault ? L"Libra is your default browser." : L"Windows asks you to confirm the choice in Settings.") +
         L"</small></div><button class=pri data-c=default.set>" + (isDefault ? L"Open settings" : L"Set as default") + L"</button></div></div>"
         L"<p class=hint style=\"color:var(--mute);font-size:12px;margin:12px 6px\">Saved passwords, bookmarks, settings and your API key are never touched here. Everything stays on this PC.</p>";
    std::wstring js = LR"H(
const $=s=>document.querySelector(s),KEYS=['hist','cook','site','cache','dns','rec','ctr','dl','agt'],base={},cur={};let range='all';
const isB={site:1,cache:1};
function fmt(k,v){if(!isB[k])return v.toLocaleString();if(v===0)return 'Empty';if(v<1048576)return '< 1 MB';if(v<1024)return v+' B';if(v<1048576)return Math.round(v/1024)+' KB';if(v<1073741824)return (v/1048576).toFixed(1)+' MB';return (v/1073741824).toFixed(2)+' GB'}
function count(el,from,to,k,ms){const t0=performance.now();(function f(t){const p=Math.min(1,(t-t0)/ms),e=1-Math.pow(1-p,3);el.textContent=fmt(k,Math.round(from+(to-from)*e));if(p<1)requestAnimationFrame(f)})(t0)}
function set(k,v,first){const el=$('#v_'+k),from=cur[k]||0;cur[k]=v;if(base[k]===undefined||v>base[k])base[k]=v;
 count(el,from,v,k,first?900:700);const m=$('#m_'+k);m.style.width=base[k]?Math.round(100*v/base[k])+'%':'0'}
let first=true;
function lbStats(s){KEYS.forEach(k=>set(k,s[k]||0,first));
 const tr=cur.trk||0;count($('#trk'),+($('#trk').dataset.v||0),s.trk||0,'x',900);$('#trk').dataset.v=s.trk||0;
 $('#blk').textContent=(s.blk||0).toLocaleString();$('#vl').textContent=(s.vl||0).toLocaleString();$('#cdom').textContent=(s.cdom||0).toLocaleString();
 const tot=(s.site||0)+(s.cache||0),a=s.site||0,c=s.cache||0,h=(s.hist||0)*400;const sum=Math.max(1048576,tot+h);
 $('#tot').textContent=fmt('site',tot);$('#s1').style.width=100*h/sum+'%';$('#s2').style.width=100*a/sum+'%';$('#s3').style.width=100*c/sum+'%';first=false}
function lbCleared(s,txt){lbStats(s);const m=$('#msg');m.innerHTML=txt;m.style.opacity=1;document.querySelector('.hero').classList.remove('pulse');void document.querySelector('.hero').offsetWidth;document.querySelector('.hero').classList.add('pulse');$('#go').disabled=false;$('#go').textContent='Clear selected'}
$('#sa').onclick=e=>{e.preventDefault();const bs=KEYS.map(k=>$('#c_'+k)),all=bs.every(b=>b.checked);bs.forEach(b=>b.checked=!all)};
document.getElementById('rg').addEventListener('click',e=>{const b=e.target.closest('button');if(!b)return;range=b.dataset.r;document.querySelectorAll('#rg button').forEach(x=>x.classList.toggle('on',x===b))});
$('#go').onclick=()=>{const ks=KEYS.filter(k=>$('#c_'+k).checked);if(!ks.length){$('#msg').textContent='Pick something to clear.';return}
 $('#go').disabled=true;$('#go').textContent='Clearing...';$('#msg').style.opacity=.4;P('priv.clear',ks.join(','),range)};
const CC=['#E5484D','#F5A524','#3E7BFA','#8E4EC6'],CN=['Ads','Trackers','Analytics','Social'];
function lbReport(r){const mx=Math.max(1,...r.days.map(d=>d[1]+d[2]+d[3]+d[4])),B=$('#bars');B.textContent='';B.style.display=r.days.every(d=>!(d[1]+d[2]+d[3]+d[4]))?'none':'flex';
 r.days.forEach(d=>{const t=d[1]+d[2]+d[3]+d[4],c=document.createElement('div'),s=document.createElement('div');s.className='st';s.title=t+' blocked: '+CN.map((n,i)=>n+' '+d[i+1]).join(', ');
  s.style.height='0';requestAnimationFrame(()=>requestAnimationFrame(()=>s.style.height=(t?Math.max(3,Math.round(120*t/mx)):3)+'px'));
  for(let i=0;i<4;i++)if(d[i+1]){const b=document.createElement('b');b.style.background=CC[i];b.style.height=(100*d[i+1]/t)+'%';s.appendChild(b)}
  const l=document.createElement('span');const n=String(d[0]);l.textContent=n.slice(6)+'/'+n.slice(4,6);c.append(s,l);B.appendChild(c)});
 const T=$('#top');T.textContent='';if(!r.top.length){T.textContent='Nothing blocked yet.';T.style.color='var(--mute)';return}
 const m=r.top[0][2];r.top.forEach(x=>{const row=document.createElement('div');row.className='tr';const nm=document.createElement('span');nm.className='nm';const i=document.createElement('i');i.style.background=CC[x[1]];nm.append(i,x[0]);
  const w=document.createElement('div'),bar=document.createElement('div');bar.className='bar';bar.style.background=CC[x[1]];bar.style.width=Math.max(4,100*x[2]/m)+'%';w.appendChild(bar);
  const n=document.createElement('span');n.className='n';n.textContent=x[2].toLocaleString();row.append(nm,w,n);T.appendChild(row)})}
function lbSn(s){const on=s.cfg&&s.on;$('#sn2').style.display=s.cfg?'block':'none';$('#snoff').style.display=s.cfg?'none':'block';$('#snd').textContent=!s.cfg?'Sponsored content is not active in this version, so Libra sends nothing.':on?'The next report, exactly as it would be sent (once a day at a random moment, only if there is something to report):':'Counting is off. Nothing is sent.';
 $('#snp').style.display=on?'block':'none';$('#snp').textContent=on?s.next.replace(/,\{/g,',\n {').replace(/"b":/,'\n"b":').replace(/,"pad"/,'\n,"pad"'):'';
 $('#snsend').disabled=!on||s.busy;$('#snclr').disabled=!on;const L=$('#snl');L.textContent='';
 if(!s.log.length){L.textContent=s.cfg?'Nothing sent yet.':'';L.style.color='var(--mute)';return}
 s.log.slice().reverse().forEach(g=>{const r=document.createElement('div'),a=document.createElement('span'),b=document.createElement('span');a.textContent=new Date(g[0]*1000).toLocaleString()+' . '+(g[1]==='report'?'Count report':'Sponsored list');b.textContent=g[2]+' bytes . '+(g[3]?'sent':'failed');if(!g[3])b.className='bad';r.append(a,b);L.appendChild(r)})}
$('#snsend').onclick=()=>P('priv.snsend','');$('#snclr').onclick=()=>P('priv.snclear','');
document.addEventListener('change',e=>{if(e.target.dataset&&e.target.dataset.k==='sn_count')setTimeout(()=>P('priv.sn',''),250)});
P('priv.stats','');P('priv.sn','');
)H";
    return PageShell(L"Privacy", b, grey, js, L"privacy");
}
