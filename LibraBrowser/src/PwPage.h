#pragma once
// libra://passwords : saved logins list (never contains a password in the HTML) + generator tab.
#include "Pages.h"
#include "Vault.h"
#include "PwJs.h"

inline std::wstring PasswordsPage(const std::vector<Cred>& v, bool grey) {
    std::wstring b = LR"H(<style>.seg{display:inline-flex;background:rgba(255,255,255,.7);border-radius:999px;padding:3px;margin-bottom:18px}.seg button{background:none;padding:8px 20px}.seg button.on{background:#111;color:#fff}
.pv{font:12px Consolas,monospace;max-width:190px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;color:#111}.gen{font:15px Consolas,monospace;word-break:break-all;padding:16px 0 6px;user-select:all}
input[type=range]{accent-color:#111;width:180px}input.n{min-width:0;width:64px}.hint{color:var(--mute);font-size:12px;margin:12px 6px 0;line-height:1.5}</style>
<h1>Passwords</h1><div class=seg><button id=t1 class=on>Saved</button><button id=t2>Generator</button></div>
<section id=s1><div class=top><input type=text id=q placeholder="Search saved logins"></div><div class=card>)H";
    if (v.empty()) b += L"<div class=empty>No saved logins yet. When you sign in on a site, Libra asks before saving anything.</div>";
    for (auto it = v.rbegin(); it != v.rend(); ++it) {
        std::wstring id = std::to_wstring(it->created);
        b += L"<div class=item data-s=\"" + Esc(it->origin + L" " + it->user) + L"\"><div class=t><a>" + Esc(it->origin) + L"</a><small>" +
             (it->user.empty() ? L"(no username)" : Esc(it->user)) + L"</small></div><code class=pv id=v" + id + L"></code>" +
             L"<button data-c=pw.show data-a=" + id + L">Show</button><button data-c=pw.copy data-a=" + id + L">Copy</button><button data-d=" + id + L">Delete</button></div>";
    }
    b += LR"H(</div><p class=hint>Saved on this PC only, encrypted with your Windows account. Nothing is sent anywhere. Libra fills a login only after you click it, and only on the exact same site.</p></section>
<section id=s2 hidden><div class=card><div class=row><div style="flex:1;min-width:0"><div class=gen id=out></div><div class=bar><i id=bar></i></div><small id=lab></small></div></div>
<div class=row><div>Length (1 to 286)</div><div><input type=range id=rng min=1 max=286 value=20> <input type=text class=n id=num value=20></div></div>
<div class=row><div>Lowercase a-z</div><input type=checkbox id=c_lower checked></div>
<div class=row><div>Uppercase A-Z</div><input type=checkbox id=c_upper checked></div>
<div class=row><div>Numbers 0-9</div><input type=checkbox id=c_digit checked></div>
<div class=row><div>Symbols<small>Edit the list if a site rejects some</small></div><div><input type=text id=sym value="!@#$%^&*()-_=+[]{};:,.?/" style="min-width:0;width:230px"> <input type=checkbox id=c_symbol checked></div></div>
<div class=row><div>Avoid look-alikes<small>Leaves out 0 O 1 l I |</small></div><input type=checkbox id=c_noAmb></div>
<div class=row><div>No repeated characters</div><input type=checkbox id=c_noRep></div>
<div class=row><div>At least one of each chosen type</div><input type=checkbox id=c_each checked></div>
<div class=row><div></div><div><button id=again>New</button> <button class=pri id=copy>Copy</button></div></div></div>
<p class=hint>Made on this PC with your system's secure random generator. A copied password is removed from the clipboard after 30 seconds. Most sites accept 64 to 128 characters only.</p></section>)H";
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
function lbShown(id,p){const e=document.getElementById('v'+id);if(!e)return;e.textContent=p;setTimeout(()=>{e.textContent=''},8000)}
)H";
    return PageShell(L"Passwords", b, grey, js, L"passwords");
}
