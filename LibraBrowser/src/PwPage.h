#pragma once
// libra://passwords : saved logins list (never contains a password in the HTML) + generator tab.
#include "Pages.h"
#include "Vault.h"
#include "PwJs.h"

inline std::wstring PasswordsPage(const std::vector<Cred>& v, bool grey) {
    std::wstring b = LR"H(<style>.seg{display:inline-flex;background:#f3f3f3;border-radius:999px;padding:3px;margin-bottom:18px}.seg button{background:none;padding:8px 20px}.seg button.on{background:#111;color:#fff}
.card .row{padding:15px 0;gap:28px;min-height:62px;border-top:1px solid #f0f0f0;align-items:center}.card .row:first-child{border-top:0}.card .row>div:first-child{flex:1;min-width:0}.card .row small{max-width:none;margin-top:2px;font-size:13px;line-height:1.45}.lbl{font-size:15px;font-weight:500;display:block}
.item .av{flex:0 0 40px;width:40px;height:40px;border-radius:50%;display:flex;align-items:center;justify-content:center;font:700 15px/1 'Segoe UI',sans-serif;font-style:normal}.item .pv{flex:0 1 150px}
.genbox{background:#fafafa;border:1px solid #eee;border-radius:14px;padding:6px 18px 16px;margin:14px 0 4px}.genbox .gen{padding:12px 0 8px}.acts{display:flex;gap:8px;padding:12px 0 14px}
.pv{font:12px Consolas,monospace;max-width:190px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;color:#111}.gen{font:15px Consolas,monospace;word-break:break-all;padding:16px 0 6px;user-select:all}
input[type=range]{accent-color:#111;width:180px}input.n{min-width:0;width:64px}.hint{color:var(--mute);font-size:12px;margin:12px 6px 0;line-height:1.5}</style>
<h1>Passwords</h1><div class=seg><button id=t1 class=on>Saved</button><button id=t2>Generator</button></div>
<section id=s1><div class=top><input type=text id=q placeholder="Search saved logins"></div><div class=card>)H";
    if (v.empty()) b += L"<div class=empty><b>No saved logins yet</b>When you sign in on a site, Libra asks before saving anything.</div>";
    for (auto it = v.rbegin(); it != v.rend(); ++it) {
        std::wstring id = std::to_wstring(it->created);
        b += L"<div class=item data-s=\"" + Esc(it->origin + L" " + it->user) + L"\"><div class=t><a>" + Esc(it->origin) + L"</a><small>" +
             (it->user.empty() ? L"(no username)" : Esc(it->user)) + L"</small></div><code class=pv id=v" + id + L"></code>" +
             L"<button data-c=pw.show data-a=" + id + L">Show</button><button data-c=pw.copy data-a=" + id + L">Copy</button><button data-d=" + id + L">Delete</button></div>";
    }
    b += LR"H(</div><p class=hint>Saved on this PC only, encrypted with your Windows account. Libra fills a login only after you click it, and only on the exact same site.</p></section>
<section id=s2 hidden><div class=card><div class=genbox><div class=gen id=out></div><div class=bar><i id=bar></i></div><small id=lab style="display:block;margin-top:8px;color:var(--mute);font-size:13px"></small></div><div class=acts><button class=pri id=copy>Copy</button><button id=again>New password</button></div>
<div class=row><div><span class=lbl>Length</span><small>1 to 286 characters</small></div><div><input type=range id=rng min=1 max=286 value=20> <input type=text class=n id=num value=20></div></div>
<div class=row><div><span class=lbl>Lowercase</span><small>a to z</small></div><input type=checkbox id=c_lower checked></div>
<div class=row><div><span class=lbl>Uppercase</span><small>A to Z</small></div><input type=checkbox id=c_upper checked></div>
<div class=row><div><span class=lbl>Numbers</span><small>0 to 9</small></div><input type=checkbox id=c_digit checked></div>
<div class=row><div><span class=lbl>Symbols</span><small>Edit the list if a site rejects some</small></div><div><input type=text id=sym value="!@#$%^&*()-_=+[]{};:,.?/" style="min-width:0;width:230px"> <input type=checkbox id=c_symbol checked></div></div>
<div class=row><div><span class=lbl>Avoid look-alikes</span><small>Leaves out 0 O 1 l I |</small></div><input type=checkbox id=c_noAmb></div>
<div class=row><div><span class=lbl>No repeated characters</span></div><input type=checkbox id=c_noRep></div>
<div class=row><div><span class=lbl>At least one of each type</span></div><input type=checkbox id=c_each checked></div>
</div>
<p class=hint>Made on this PC with your system's secure random generator. A copied password is removed from the clipboard after 30 seconds.</p></section>)H";
    std::wstring js = std::wstring(kGenJs) + LR"H(
const $=s=>document.querySelector(s);
function tab(n){$('#s1').hidden=n!=1;$('#s2').hidden=n!=2;$('#t1').className=n==1?'on':'';$('#t2').className=n==2?'on':'';if(n==2)gen()}
$('#t1').onclick=()=>tab(1);$('#t2').onclick=()=>tab(2);
$('#q').oninput=e=>{const q=e.target.value.toLowerCase();document.querySelectorAll('#s1 .item').forEach(r=>r.style.display=r.dataset.s.toLowerCase().includes(q)?'':'none')};
function opts(){const g=k=>$('#c_'+k).checked;return{lower:g('lower'),upper:g('upper'),digit:g('digit'),symbol:g('symbol'),noAmb:g('noAmb'),noRep:g('noRep'),each:g('each'),symbols:$('#sym').value.replace(/\s/g,'')||undefined}}
let cur='';
function gen(){let n=Math.max(1,Math.min(286,parseInt($('#num').value)||1));const o=opts();cur=lbGen(n,o);$('#out').textContent=cur||'Pick at least one type';const b=lbBits(n,o);$('#bar').style.width=Math.min(100,b/1.28)+'%';$('#lab').textContent=cur?lbLabel(b)+' ('+b+' bits)':''}
$('#rng').oninput=e=>{$('#num').value=e.target.value;gen()};
$('#num').oninput=e=>{$('#rng').value=Math.max(1,Math.min(286,parseInt(e.target.value)||1));gen()};
['lower','upper','digit','symbol','noAmb','noRep','each'].forEach(k=>$('#c_'+k).onchange=gen);$('#sym').oninput=gen;$('#again').onclick=gen;
$('#copy').onclick=()=>{if(cur){P('pw.clip',cur);$('#copy').textContent='Copied';setTimeout(()=>$('#copy').textContent='Copy',1500)}};
let armed=null;
document.addEventListener('click',e=>{const d=e.target.closest('[data-d]');if(!d)return;if(armed===d){P('pw.del',d.dataset.d);return}
 if(armed)armed.textContent='Delete';armed=d;d.textContent='Confirm?';setTimeout(()=>{if(armed===d){d.textContent='Delete';armed=null}},3000)});
document.querySelectorAll('#s1 .item').forEach(r=>{const a=r.querySelector('.t a');let h=a.textContent;try{h=new URL(a.textContent).host}catch(e){}a.textContent=h;let n=0;for(const c of h)n=(n*31+c.charCodeAt(0))%360;const v=document.createElement('i');v.className='av';v.textContent=(h.replace(/^www\./,'')[0]||'?').toUpperCase();v.style.background='hsl('+n+',70%,93%)';v.style.color='hsl('+n+',55%,32%)';r.insertBefore(v,r.firstChild)});
function lbShown(id,p){const e=document.getElementById('v'+id);if(!e)return;e.textContent=p;setTimeout(()=>{e.textContent=''},8000)}
)H";
    return PageShell(L"Passwords", b, grey, js, L"passwords");
}
