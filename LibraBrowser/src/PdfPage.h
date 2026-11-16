// libra://pdfsign : fill in and sign a PDF on this PC. pdf.js draws the page, pdf-lib writes a signed COPY; both are open source and are downloaded once on request
// (fixed SHA-256, see Pdf.inl). The page has its own CSP: it can load scripts only from the local tools folder and has no network at all, so the PDF can not be uploaded.
// A drawn or typed signature is a picture on the page, not a certified digital signature.
#pragma once
#include "Pages.h"

inline std::wstring PdfSignPage(bool grey) {
    std::wstring b = LR"H(<style>
#wrap{max-width:900px;margin:0 auto;padding-bottom:60px}.lead{color:var(--mute);font-size:15px;margin:-6px 0 24px}
.card{padding:2px 22px}.card .row{padding:15px 0;gap:28px;min-height:62px;align-items:center}.card .row>div:first-child{flex:1;min-width:0}.card .row small{max-width:none;margin-top:2px;font-size:13px;line-height:1.45}.lbl{font-size:15px;font-weight:500;display:block}.card .row button{flex-shrink:0}
#empty{display:flex;flex-direction:column;align-items:center;text-align:center;gap:10px;padding:60px 24px;border:1px dashed #d4d4d4;border-radius:22px}
#empty svg{width:42px;height:42px;fill:none;stroke:#111;stroke-width:1.3;stroke-linecap:round;stroke-linejoin:round;margin-bottom:6px}
#empty h2,#sd h3{font:800 24px 'Segoe UI Variable Display','Segoe UI',sans-serif;text-transform:none;letter-spacing:0;color:#111;margin:0}#sd h3{font-size:20px;margin:0 0 14px}#empty p{color:var(--mute);font-size:14px;max-width:430px;line-height:1.55;margin:0 0 12px}#empty small{color:var(--mute);font-size:12px;margin-top:4px}
.tb{display:none;flex-direction:column;gap:10px;position:sticky;top:0;z-index:5;background:#fff;border:1px solid #e6e6e6;border-radius:18px;padding:10px 14px;margin-bottom:6px}
.tr{display:flex;align-items:center;gap:8px;flex-wrap:wrap}.tr .sp{flex:1}
.tr .grp{display:flex;align-items:center;gap:4px;padding-right:12px;margin-right:4px;border-right:1px solid #ececec}
.tb button{padding:8px 15px;font-size:13px}.tb .ic{width:34px;height:34px;padding:0;display:grid;place-items:center;font-size:18px;line-height:1}.tb b{min-width:56px;text-align:center;font-size:13px;font-weight:600}
.tb #txt{min-width:0;width:130px;padding:8px 14px;font-size:13px}#fn{font-size:14px;font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;max-width:48%}
#msg{min-height:20px;margin:8px 4px 12px;font-size:13px;color:var(--mute)}#msg.err{color:#d6141c}
#view{display:none;background:#f3f3f3;border-radius:18px;padding:28px 20px;justify-content:center;overflow:auto}
#pgw{position:relative;line-height:0;background:#fff;border:1px solid #e0e0e0;flex:none}#cv{display:block}#ov{position:absolute;left:0;top:0;width:100%;height:100%}
.it{position:absolute;cursor:move;outline:1px dashed #9a9a9a;touch-action:none;user-select:none;line-height:1.1;font-family:Arial,Helvetica,sans-serif;color:#000;white-space:pre}.it:hover{outline:1px solid #111}.it img{width:100%;height:100%;display:block;pointer-events:none}
.it .x{display:none;position:absolute;top:-11px;right:-11px;width:22px;height:22px;border-radius:50%;border:0;background:#111;color:#fff;font:14px/1 sans-serif;padding:0;cursor:pointer}.it:hover .x{display:block}
#sd{display:none;position:fixed;inset:0;z-index:20;background:rgba(255,255,255,.8);align-items:center;justify-content:center}
#sd .box{width:min(560px,92vw);background:#fff;border:1px solid #e6e6e6;border-radius:22px;padding:24px}
#sd .tabs{display:inline-flex;background:#f0f0f0;border-radius:999px;padding:4px;margin-bottom:14px}#sd .tabs button{background:none;padding:7px 20px;font-size:13px;font-weight:600;color:#555}#sd .tabs button.on{background:#111;color:#fff}
#dc{display:block;width:100%;height:auto;border:1px solid #e6e6e6;border-radius:14px;touch-action:none;background:#fff linear-gradient(transparent 74%,#ddd 74%,#ddd calc(74% + 1px),transparent calc(74% + 1px))}
#ty{width:100%;font-size:18px}#tyv{font:64px/1.2 'Segoe Script','Brush Script MT',cursive;margin:10px 0;min-height:70px;color:#111;overflow:hidden;white-space:nowrap}
#sd .acts{display:flex;gap:10px;align-items:center;margin-top:16px}#sd .acts .sp{flex:1}
</style><div id=wrap><h1>Sign a PDF</h1><p class=lead>Fill in and sign a PDF on this PC. The original file is never changed.</p>
<div class=card id=need style="display:none"><div class=row><div><span class=lbl>PDF tools</span><small>Signing needs two small open-source tools (pdf.js and pdf-lib, 1.9 MB). They are downloaded once, checked against a fixed checksum and kept on this PC. Your PDF never leaves it.</small></div><button class=pri id=dl>Download (1.9 MB)</button></div></div>
<div id=app style="display:none">
<div id=empty><svg viewBox="0 0 24 24"><path d="M14 3H7a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8z"/><path d="M14 3v5h5"/><path d="M12 12v5"/><path d="M9.5 14.5h5"/></svg><h2>Choose a PDF to sign</h2><p>Add your signature or any text, then save a signed copy. Your PDF stays on this PC and is never uploaded.</p><button class=pri id=bopen>Choose a PDF...</button><small>PDF files up to 12 MB</small></div>
<div class=tb id=nav>
<div class=tr><span id=fn></span><span class=sp></span><button class=g id=bopen2>Change PDF</button><button class=pri id=bsave>Save signed copy</button></div>
<div class=tr><div class=grp><button class=ic id=bp aria-label="Previous page">&#8249;</button><b id=pn></b><button class=ic id=bn aria-label="Next page">&#8250;</button></div>
<div class=grp><button class=ic id=bz1 aria-label="Zoom out">&#8722;</button><b id=zl></b><button class=ic id=bz2 aria-label="Zoom in">+</button></div>
<div class=grp><button id=bsig>Signature</button></div>
<div class=grp><input type=text id=txt placeholder="Text to add" ><button id=btxt>Add text</button></div>
<span class=sp></span><button class=g id=bundo>Undo last</button></div></div>
<div id=msg></div>
<div id=view><div id=pgw><canvas id=cv></canvas><div id=ov></div></div></div></div>
<div id=sd><div class=box><h3>Create your signature</h3><div class=tabs><button data-m=draw class=on>Draw</button><button data-m=type>Type</button><button data-m=img>Image</button></div>
<div id=m_draw><canvas id=dc width=480 height=150></canvas></div>
<div id=m_type style="display:none"><input type=text id=ty placeholder="Your name"><div id=tyv></div></div>
<div id=m_img style="display:none"><input type=file id=im accept="image/png,image/jpeg"><small style="display:block;color:var(--mute);margin-top:6px">A picture of your signature on a plain background.</small></div>
<div class=acts><button class=g id=dclr>Clear</button><span class=sp></span><button id=scan>Cancel</button><button class=pri id=suse>Use this signature</button></div></div></div></div>)H";
    std::wstring js = LR"H(
const $=s=>document.querySelector(s),T='https://libra-pdf.local/';document.body.appendChild(document.querySelector('#sd'));   /* a fixed dialog must not live inside the animated main area */   /* P() comes from the page shell */
let pdf=null,bytes=null,fname='document.pdf',pg=1,N=0,SC=1.3,items=[],sig=null,mode=null,page1=null,loaded=false,busy=false;
const msg=(t,e)=>{const m=$('#msg');m.textContent=t;m.className=e?'err':''};
function ld(src){return new Promise((ok,no)=>{const s=document.createElement('script');s.src=T+src;s.onload=ok;s.onerror=()=>no(new Error(src));document.head.appendChild(s)})}
async function lbTools(ok){$('#need').style.display=ok?'none':'block';$('#app').style.display=ok?'block':'none';$('#dl').disabled=false;$('#dl').textContent='Download (1.9 MB)';
 if(!ok||loaded)return;try{await ld('pdf.min.js');await ld('pdf-lib.min.js');pdfjsLib.GlobalWorkerOptions.workerSrc=T+'pdf.worker.min.js';loaded=true}catch(e){msg('The PDF tools could not be loaded. Please download them again.',1)}}
function lbToolsErr(t){$('#dl').disabled=false;$('#dl').textContent='Download (1.9 MB)';msg(t,1);$('#need').style.display='block'}
$('#dl').onclick=()=>{$('#dl').disabled=true;$('#dl').textContent='Downloading...';P('pdf.tools','')};
$('#bopen').onclick=()=>P('pdf.open','');$('#bopen2').onclick=()=>P('pdf.open','');
function setMode(m){mode=m;$('#ov').style.cursor=m?'crosshair':''}
async function fitZoom(){const p=await pdf.getPage(1),v=p.getViewport({scale:1}),w=$('#view').clientWidth-60;SC=Math.max(.8,Math.min(2,w/v.width))}
$('#bz1').onclick=async()=>{if(pdf){SC=Math.max(.6,SC/1.15);await draw()}};$('#bz2').onclick=async()=>{if(pdf){SC=Math.min(3,SC*1.15);await draw()}};
async function lbOpen(name){try{fname=name;const r=await fetch(T+'work.pdf');if(!r.ok)throw 0;bytes=new Uint8Array(await r.arrayBuffer());P('pdf.loaded','');
 pdf=await pdfjsLib.getDocument({data:bytes.slice(),isEvalSupported:false,enableXfa:false}).promise;N=pdf.numPages;pg=1;items=[];$('#fn').textContent=name+'  .  '+N+' page'+(N>1?'s':'');$('#nav').style.display='flex';$('#view').style.display='flex';$('#empty').style.display='none';await fitZoom();await draw();msg('Create a signature or add text, then click on the page where it should go.')}
 catch(e){msg('This PDF could not be opened (it may be protected or damaged).',1);pdf=null}}
function lbOpenErr(t){msg(t,1)}
async function draw(){const p=await pdf.getPage(pg);const vp=p.getViewport({scale:SC}),cv=$('#cv');cv.width=Math.floor(vp.width);cv.height=Math.floor(vp.height);
 await p.render({canvasContext:cv.getContext('2d'),viewport:vp}).promise;$('#pn').textContent=pg+' / '+N;$('#zl').textContent=Math.round(SC*75)+'%';$('#bp').disabled=pg<=1;$('#bn').disabled=pg>=N;page1=p;ov();if(p.rotate){msg('This page is rotated. Signing rotated pages is not supported yet.',1)}}
function ov(){const o=$('#ov');o.textContent='';items.forEach((it,i)=>{if(it.page!==pg)return;const d=document.createElement('div');d.className='it';d.style.left=it.x*SC+'px';d.style.top=it.y*SC+'px';
 if(it.kind==='sig'){d.style.width=it.w*SC+'px';d.style.height=it.h*SC+'px';const im=document.createElement('img');im.src=it.data;d.appendChild(im)}else{d.style.fontSize=it.size*SC+'px';d.textContent=it.text}
 const xb=document.createElement('button');xb.className='x';xb.textContent='\u00d7';xb.setAttribute('aria-label','Remove');xb.onpointerdown=e=>e.stopPropagation();xb.onclick=e=>{e.stopPropagation();items.splice(i,1);ov()};d.appendChild(xb);
 d.onpointerdown=e=>{e.preventDefault();d.setPointerCapture(e.pointerId);const sx=e.clientX,sy=e.clientY,ox=it.x,oy=it.y;d.onpointermove=m=>{it.x=ox+(m.clientX-sx)/SC;it.y=oy+(m.clientY-sy)/SC;d.style.left=it.x*SC+'px';d.style.top=it.y*SC+'px'};d.onpointerup=()=>{d.onpointermove=null}};
 o.appendChild(d)})}
$('#bp').onclick=async()=>{if(pg>1){pg--;await draw()}};$('#bn').onclick=async()=>{if(pg<N){pg++;await draw()}};
$('#bundo').onclick=()=>{items.pop();ov()};
$('#btxt').onclick=()=>{const t=$('#txt').value.trim();if(!t){msg('Type the text first.',1);return}setMode('text');msg('Click on the page where the text goes.')};
$('#ov').addEventListener('click',e=>{if(!mode||!pdf)return;if(page1&&page1.rotate){msg('Signing rotated pages is not supported yet.',1);return}const r=$('#cv').getBoundingClientRect(),k=$('#cv').width/r.width,px=(e.clientX-r.left)*k/SC,py=(e.clientY-r.top)*k/SC;
 if(mode==='sig'&&sig){const w=170,h=Math.max(20,w*sig.h/sig.w);items.push({kind:'sig',page:pg,x:px-w/2,y:py-h/2,w,h,data:sig.url})}else if(mode==='text'){items.push({kind:'text',page:pg,x:px,y:py,size:12,text:$('#txt').value.trim().slice(0,200)})}
 setMode(null);ov();msg('Placed. Drag it to move it. "Undo last" removes it.')});
/* signature maker */
let sm='draw';const dc=$('#dc'),dx=dc.getContext('2d');dx.lineWidth=3;dx.lineCap='round';dx.strokeStyle='#111';let dr=false;
dc.onpointerdown=e=>{dr=true;dc.setPointerCapture(e.pointerId);const r=dc.getBoundingClientRect(),k=dc.width/r.width;dx.beginPath();dx.moveTo((e.clientX-r.left)*k,(e.clientY-r.top)*k)};
dc.onpointermove=e=>{if(!dr)return;const r=dc.getBoundingClientRect(),k=dc.width/r.width;dx.lineTo((e.clientX-r.left)*k,(e.clientY-r.top)*k);dx.stroke()};dc.onpointerup=()=>{dr=false};
$('#dclr').onclick=()=>dx.clearRect(0,0,dc.width,dc.height);
$('#bsig').onclick=()=>{$('#sd').style.display='flex'};$('#scan').onclick=()=>{$('#sd').style.display='none'};
document.querySelectorAll('#sd .tabs button').forEach(b=>b.onclick=()=>{sm=b.dataset.m;document.querySelectorAll('#sd .tabs button').forEach(x=>x.classList.toggle('on',x===b));['draw','type','img'].forEach(k=>$('#m_'+k).style.display=k===sm?'block':'none')});
$('#ty').oninput=()=>{$('#tyv').textContent=$('#ty').value.slice(0,40)};
function crop(c){const x=c.getContext('2d'),d=x.getImageData(0,0,c.width,c.height).data;let a=c.width,b=c.height,e=0,f=0;for(let y=0;y<c.height;y++)for(let q=0;q<c.width;q++)if(d[(y*c.width+q)*4+3]>8){if(q<a)a=q;if(q>e)e=q;if(y<b)b=y;if(y>f)f=y}
 if(e<a||f<b)return null;const o=document.createElement('canvas');o.width=e-a+9;o.height=f-b+9;o.getContext('2d').drawImage(c,a-4,b-4,o.width,o.height,0,0,o.width,o.height);return o}
function use(c){const o=c&&crop(c);if(!o){msg('Nothing to use yet.',1);return}sig={url:o.toDataURL('image/png'),w:o.width,h:o.height};setMode('sig');$('#sd').style.display='none';msg('Click on the page where the signature goes.')}
$('#suse').onclick=()=>{if(sm==='draw')use(dc);else if(sm==='type'){const t=$('#ty').value.trim().slice(0,40);if(!t){msg('Type your name first.',1);return}const c=document.createElement('canvas'),x=c.getContext('2d'),f="64px 'Segoe Script','Brush Script MT',cursive";x.font=f;c.width=Math.ceil(x.measureText(t).width)+30;c.height=110;x.font=f;x.fillStyle='#111';x.textBaseline='middle';x.fillText(t,10,55);use(c)}
 else{const f=$('#im').files[0];if(!f||f.size>2e6){msg('Choose a PNG or JPEG picture under 2 MB.',1);return}const r=new FileReader();r.onload=()=>{const im=new Image();im.onload=()=>{const k=Math.min(1,500/im.width),c=document.createElement('canvas');c.width=Math.max(1,Math.round(im.width*k));c.height=Math.max(1,Math.round(im.height*k));c.getContext('2d').drawImage(im,0,0,c.width,c.height);sig={url:c.toDataURL('image/png'),w:c.width,h:c.height};setMode('sig');$('#sd').style.display='none';msg('Click on the page where the signature goes.')};im.onerror=()=>msg('That picture could not be read.',1);im.src=r.result};r.readAsDataURL(f)}};
/* save a signed copy */
$('#bsave').onclick=async()=>{if(!pdf||busy)return;if(!items.length){msg('Place a signature or text first.',1);return}busy=true;
 try{const doc=await PDFLib.PDFDocument.load(bytes),font=await doc.embedFont(PDFLib.StandardFonts.Helvetica);
  for(const it of items){const p=doc.getPage(it.page-1),H=p.getSize().height;if(it.kind==='sig'){const im=await doc.embedPng(it.data);p.drawImage(im,{x:it.x,y:H-it.y-it.h,width:it.w,height:it.h})}else p.drawText(it.text,{x:it.x,y:H-it.y-it.size*0.9,size:it.size,font,color:PDFLib.rgb(0,0,0)})}
  const out=await doc.save();let s='';for(let i=0;i<out.length;i+=0x8000)s+=String.fromCharCode.apply(null,out.subarray(i,i+0x8000));P('pdf.save',fname,btoa(s));msg('Choose where to save the signed copy...')}
 catch(e){msg(/WinAnsi|encode/i.test(String(e))?'Typed text supports basic Latin letters only. Use a signature picture for other scripts.':'The signed copy could not be made.',1)}busy=false};
function lbSaved(ok,t){msg(ok?'Saved: '+t:(t||'Not saved.'),!ok&&!!t)}
P('pdf.status','');
)H";
    std::wstring html = PageShell(L"Sign PDF", b, grey, js, L"", false, false);
    const std::wstring oldc = L"default-src 'none'; img-src data: https://libra-hero.local; media-src https://libra-hero.local; style-src 'unsafe-inline'; script-src 'unsafe-inline'; form-action https:; base-uri 'none'";
    size_t p = html.find(oldc);
    if (p != std::wstring::npos) html.replace(p, oldc.size(), L"default-src 'none'; img-src data: blob:; style-src 'unsafe-inline'; script-src 'unsafe-inline' https://libra-pdf.local; worker-src blob:; connect-src https://libra-pdf.local; base-uri 'none'; form-action 'none'");
    return html;
}
