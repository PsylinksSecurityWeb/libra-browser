#pragma once
// "Libra Pop": a small bubble shooter on the error page when the PC is offline (the browser's answer to the offline dinosaur).
// Pure page script + canvas: no network, no files. Aim with the mouse or finger (or arrow keys), click / Space to shoot, S swaps the next bubble.
static const wchar_t* kPopHtml = LR"HTML(<style>#pg{margin:22px auto 0;text-align:center}#pg canvas{height:min(480px,calc(100vh - 215px));width:auto;max-width:92vw;aspect-ratio:2/3;border:1px solid #e6e6e6;border-radius:18px;background:#fff;touch-action:none;cursor:crosshair;outline:none}
body.play .made{display:none}body.play .err{margin-top:3vh}body.play .eic,body.play .err p,body.play .err code{display:none}body.play .err h1{font-size:22px;margin-bottom:12px}
#pg .gt{font-size:14px;color:#666;margin-bottom:10px}#pg .gs{font-size:12px;color:#8a8a8a;margin-top:8px}#pg b{color:#111}</style>
<div id=pg hidden><div class=gt id=gt>You're offline. Pop some bubbles while you wait.</div><canvas id=pop width=320 height=480 tabindex=0 aria-label="Libra Pop, a bubble shooter game"></canvas>
<div class=gs>Aim with the mouse or finger, click to shoot, S swaps. Score <b id=sc>0</b> &middot; Best <b id=bs>0</b></div></div>)HTML";

static const wchar_t* kPopJs = LR"JS((()=>{
const cv=document.getElementById('pop'),g=cv.getContext('2d'),pg=document.getElementById('pg');
const W=320,H=480,R=16,D=32,RH=27.7,ROWS=14,SX=160,SY=444,COLORS=['#e5484d','#f5a524','#30a46c','#3e7bfa','#8e4ec6'];
const dpr=Math.min(2,window.devicePixelRatio||1);cv.width=W*dpr;cv.height=H*dpr;g.setTransform(dpr,0,0,dpr,0,0);
let grid,cur,nxt,fly,ang=-Math.PI/2,miss,score,best=0,over,phase='ready',pops=[],falls=[],txts=[],started=false,raf=0,tick=0;
try{best=+localStorage.getItem('lbpop')||0}catch(e){}
const cx=(r,c)=>(r.off?D:R)+c*D, cy=i=>R+i*RH, rnd=()=>COLORS[Math.random()*COLORS.length|0], clampA=a=>Math.max(-Math.PI+.17,Math.min(-.17,a));
const mk=off=>({off:off,c:new Array(off?9:10).fill(null)});
function pick(){const s=new Set();grid.forEach(r=>r.c.forEach(c=>c&&s.add(c)));const a=s.size?[...s]:COLORS;return a[Math.random()*a.length|0]}
function reset(ph){grid=[];for(let i=0;i<ROWS;i++)grid.push(mk(i%2));for(let i=0;i<5;i++)grid[i].c=grid[i].c.map(rnd);cur=pick();nxt=pick();fly=null;miss=0;score=0;over=false;phase=ph||'ready';pops=[];falls=[];txts=[];hud()}
function hud(){document.getElementById('sc').textContent=score;document.getElementById('bs').textContent=Math.max(best,score)}
function nb(i,j){const x=cx(grid[i],j),y=cy(i),o=[];for(let a=Math.max(0,i-1);a<=Math.min(ROWS-1,i+1);a++)grid[a].c.forEach((_,b)=>{if((a!==i||b!==j)&&Math.hypot(cx(grid[a],b)-x,cy(a)-y)<D*1.1)o.push([a,b])});return o}
function flood(i,j,same){const seen=new Set([i+','+j]),q=[[i,j]];while(q.length){const[a,b]=q.pop();nb(a,b).forEach(([p,r])=>{const k=p+','+r;if(!seen.has(k)&&grid[p].c[r]&&(!same||grid[p].c[r]===same)){seen.add(k);q.push([p,r])}})}return seen}
function resolve(i,j){   // match 3+, then let everything not hanging from the top row fall
  const col=grid[i].c[j],m=flood(i,j,col);let popped=0,px=cx(grid[i],j),py=cy(i);
  if(m.size>=3){m.forEach(k=>{const[a,b]=k.split(',').map(Number);pops.push({x:cx(grid[a],b),y:cy(a),c:grid[a].c[b],t:0});grid[a].c[b]=null});score+=m.size*10;popped=m.size;txts.push({x:px,y:py,s:'+'+m.size*10,t:0})}
  const keep=new Set();grid[0].c.forEach((v,b)=>{if(v)flood(0,b,null).forEach(k=>keep.add(k))});let dropped=0;
  grid.forEach((r,a)=>r.c.forEach((v,b)=>{if(v&&!keep.has(a+','+b)){falls.push({x:cx(r,b),y:cy(a),c:v,vy:-2-Math.random()*2,vx:(Math.random()-.5)*2});r.c[b]=null;dropped++}}));
  if(dropped){score+=dropped*15;popped+=dropped;txts.push({x:px,y:Math.max(30,py-24),s:'+'+dropped*15+' dropped',t:0})}
  return popped}
function land(){
  let bi=-1,bj=-1,bd=1e9;grid.forEach((r,i)=>r.c.forEach((v,j)=>{if(!v){const d=Math.hypot(cx(r,j)-fly.x,cy(i)-fly.y);if(d<bd){bd=d;bi=i;bj=j}}}));
  if(bi<0){fly=null;over=true;return}
  grid[bi].c[bj]=fly.c;fly=null;const p=resolve(bi,bj);
  if(!p){miss++;if(miss>=8){miss=0;if(grid[ROWS-1].c.some(v=>v))over=true;grid.pop();const t=mk(1-grid[0].off);t.c=t.c.map(rnd);grid.unshift(t)}}
  if(grid[ROWS-2].c.some(v=>v))over=true;
  if(!grid.some(r=>r.c.some(v=>v))){score+=200;txts.push({x:W/2,y:200,s:'Board cleared +200',t:0});for(let i=0;i<5;i++)grid[i].c=grid[i].c.map(rnd)}
  cur=nxt;nxt=pick();if(over){try{if(score>best)localStorage.setItem('lbpop',score)}catch(e){}best=Math.max(best,score)}hud()}
function shoot(){if(fly||over||phase!=='play')return;fly={x:SX,y:SY,vx:Math.cos(ang)*9,vy:Math.sin(ang)*9,c:cur}}
function step(){
  if(fly){fly.x+=fly.vx;fly.y+=fly.vy;if(fly.x<R){fly.x=R;fly.vx*=-1}if(fly.x>W-R){fly.x=W-R;fly.vx*=-1}
    let hit=fly.y<=R;if(!hit)for(let i=0;i<ROWS&&!hit;i++)grid[i].c.forEach((v,j)=>{if(v&&Math.hypot(cx(grid[i],j)-fly.x,cy(i)-fly.y)<D*.9)hit=true});if(hit)land()}
  pops.forEach(p=>p.t++);pops=pops.filter(p=>p.t<14);txts.forEach(t=>t.t++);txts=txts.filter(t=>t.t<42);
  falls.forEach(f=>{f.vy+=.6;f.y+=f.vy;f.x+=f.vx});falls=falls.filter(f=>f.y<H+R);tick++}
function trace(){   // dotted aim guide that follows the wall bounces and stops where the shot would stick
  const pts=[];let x=SX,y=SY,vx=Math.cos(ang)*6,vy=Math.sin(ang)*6,b=0;
  for(let i=0;i<150;i++){x+=vx;y+=vy;if(x<R){x=R;vx*=-1;b++}if(x>W-R){x=W-R;vx*=-1;b++}if(b>2||y<=R)break;let hit=false;for(let a=0;a<ROWS&&!hit;a++)for(let c=0;c<grid[a].c.length;c++)if(grid[a].c[c]&&Math.hypot(cx(grid[a],c)-x,cy(a)-y)<D*.9){hit=true;break}if(hit)break;if(i%3===2)pts.push([x,y])}
  return pts}
function ball(x,y,c,a,s){const r=(R-1)*(s||1);g.globalAlpha=a==null?1:a;g.fillStyle=c;g.beginPath();g.arc(x,y,r,0,6.3);g.fill();g.strokeStyle='rgba(0,0,0,.14)';g.lineWidth=1;g.stroke();g.fillStyle='rgba(255,255,255,.4)';g.beginPath();g.arc(x-r*.32,y-r*.34,r*.26,0,6.3);g.fill();g.globalAlpha=1}
function card(y,h){g.fillStyle='rgba(255,255,255,.92)';g.beginPath();if(g.roundRect)g.roundRect(24,y,W-48,h,16);else g.rect(24,y,W-48,h);g.fill();g.strokeStyle='#e6e6e6';g.stroke()}
function draw(){
  g.clearRect(0,0,W,H);grid.forEach((r,i)=>r.c.forEach((v,j)=>v&&ball(cx(r,j),cy(i),v)));
  g.strokeStyle='#e6e6e6';g.setLineDash([4,4]);g.beginPath();g.moveTo(0,cy(ROWS-2)+R);g.lineTo(W,cy(ROWS-2)+R);g.stroke();g.setLineDash([]);
  pops.forEach(p=>{const k=p.t/14;ball(p.x,p.y,p.c,1-k,1-k*.6);g.strokeStyle=p.c;g.globalAlpha=1-k;g.lineWidth=2;g.beginPath();g.arc(p.x,p.y,R+p.t*1.8,0,6.3);g.stroke();g.globalAlpha=1;g.lineWidth=1});
  falls.forEach(f=>ball(f.x,f.y,f.c,.85));
  if(phase==='play'&&!fly&&!over){trace().forEach((p,i,a)=>{g.fillStyle=cur;g.globalAlpha=Math.max(.15,.7-i/a.length*.6);g.beginPath();g.arc(p[0],p[1],2.2,0,6.3);g.fill()});g.globalAlpha=1}
  g.strokeStyle='#d6d6d6';g.lineWidth=1.5;g.beginPath();g.arc(SX,SY,R+7,0,6.3);g.stroke();g.lineWidth=1;   // the shooter ring
  if(fly)ball(fly.x,fly.y,fly.c);else if(!over)ball(SX,SY,cur);
  ball(SX-96,SY+10,nxt,.6,.75);g.fillStyle='#8a8a8a';g.font='11px "Segoe UI",sans-serif';g.textAlign='center';g.fillText('next',SX-96,SY+34);
  for(let i=0;i<8;i++){g.fillStyle=i<miss?'#f5a524':'#e6e6e6';g.beginPath();g.arc(SX+62+i*10,SY+8,3,0,6.3);g.fill()}g.fillStyle='#8a8a8a';g.fillText('next row',SX+97,SY+34);   // eight dots: misses until a new row arrives
  txts.forEach(t=>{g.globalAlpha=Math.min(1,(42-t.t)/16);g.fillStyle='#111';g.font='bold 14px "Segoe UI",sans-serif';g.fillText(t.s,t.x,t.y-t.t*.9);g.globalAlpha=1});
  if(phase==='ready'){card(150,150);g.fillStyle='#111';g.font='bold 26px "Segoe UI",sans-serif';g.fillText('Libra Pop',SX,203);g.font='13px "Segoe UI",sans-serif';g.fillStyle='#666';g.fillText('Match 3 or more of a colour to pop them.',SX,232);g.fillText('Tap or press Space to start',SX,256)}
  if(over){card(150,160);g.fillStyle='#111';g.font='bold 26px "Segoe UI",sans-serif';g.fillText('Game over',SX,200);g.font='14px "Segoe UI",sans-serif';g.fillStyle='#444';g.fillText('Score '+score+'   ·   Best '+Math.max(best,score),SX,232);g.fillStyle='#666';g.fillText('Tap or press Space to play again',SX,262)}
  g.textAlign='left'}
function loop(){step();draw();raf=requestAnimationFrame(loop)}
function aim(e){const r=cv.getBoundingClientRect(),x=(e.clientX-r.left)*W/r.width,y=(e.clientY-r.top)*H/r.height;ang=clampA(Math.atan2(y-SY,x-SX))}
function go(){if(over){reset('play')}else if(phase==='ready'){phase='play'}else shoot()}
cv.addEventListener('pointermove',aim);cv.addEventListener('pointerdown',e=>{aim(e);cv.focus({preventScroll:true});go()});
addEventListener('keydown',e=>{if(pg.hidden)return;if(e.key==='ArrowLeft'){ang=clampA(ang-.05);e.preventDefault()}else if(e.key==='ArrowRight'){ang=clampA(ang+.05);e.preventDefault()}else if(e.key===' '||e.key==='ArrowUp'){e.preventDefault();go()}else if(e.key==='s'||e.key==='S'){const t=cur;cur=nxt;nxt=t}});
function show(){document.body.classList.add('play');if(started){pg.hidden=false;return}started=true;pg.hidden=false;reset();raf=requestAnimationFrame(loop)}
function hide(){document.body.classList.remove('play');pg.hidden=true;cancelAnimationFrame(raf);started=false}
function back(){const t=document.getElementById('gt');t.textContent='Back online. Reloading...';setTimeout(()=>{const b=document.querySelector('[data-c=retry]');if(b)b.click()},500)}
if(navigator.onLine===false)show();addEventListener('offline',show);addEventListener('online',back);
window.__pop={selfTest(){   // checks the rules on a hand-made board
  const r=[];reset('play');grid.forEach(x=>x.c.fill(null));
  grid[0].c[0]=COLORS[0];grid[0].c[1]=COLORS[0];grid[1].c[0]=COLORS[1];grid[5].c[3]=COLORS[2];
  grid[0].c[2]=COLORS[0];const p=resolve(0,2);r.push(['three in a row pop',p>=4&&!grid[0].c[0]&&!grid[0].c[1]&&!grid[0].c[2]]);
  r.push(['bubble hanging from nothing falls',falls.length>=2&&!grid[5].c[3]&&!grid[1].c[0]]);
  reset('play');grid.forEach(x=>x.c.fill(null));grid[0].c[0]=COLORS[0];grid[0].c[1]=COLORS[1];const q=resolve(0,1);r.push(['two of a colour do not pop',q===0&&!!grid[0].c[0]]);
  reset('play');r.push(['five rows to start',grid.slice(0,5).every(x=>x.c.every(v=>v))&&grid[5].c.every(v=>!v)]);
  reset('play');ang=-Math.PI/2;shoot();let n=0;while(fly&&n++<400)step();r.push(['a shot lands',!fly&&n<400]);
  reset('play');r.push(['neighbours are 2..6',nb(2,3).length>=4&&nb(2,3).length<=6]);
  reset();ang=-Math.PI/2;shoot();r.push(['no shot before the start screen is dismissed',!fly]);
  reset('play');grid.forEach(x=>x.c.fill(null));grid[0].c[0]=COLORS[0];grid[0].c[1]=COLORS[1];r.push(['aim guide stays inside the board',trace().every(p=>p[0]>=R-1&&p[0]<=W-R+1&&p[1]>=0)]);
  reset();return r},
  auto(n,keep){if(!keep)reset("play");for(let i=0;i<n&&!over;i++){ang=clampA(-Math.PI/2+(Math.random()-.5)*2.4);shoot();let k=0;while(fly&&k++<600)step()}return {score,over}},
  state:()=>({score,over,phase,flying:!!fly,visible:!pg.hidden,miss}),hide,show,go};
})();)JS";
