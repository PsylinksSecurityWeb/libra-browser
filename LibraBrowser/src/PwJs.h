#pragma once
// Password generator in JavaScript (one copy, used by the Passwords page and by the in-page sign-up card).
// Secure random numbers (crypto.getRandomValues) with rejection sampling, so every character is equally likely.
#include "pch.h"

inline const wchar_t* kGenJs = LR"JS(
var LB_AMB=/[0O1lI|]/g;
function lbRnd(n){var lim=Math.floor(4294967296/n)*n,b=new Uint32Array(1);do{crypto.getRandomValues(b)}while(b[0]>=lim);return b[0]%n}
function lbPools(o){var p=[];
 if(o.lower)p.push('abcdefghijklmnopqrstuvwxyz');
 if(o.upper)p.push('ABCDEFGHIJKLMNOPQRSTUVWXYZ');
 if(o.digit)p.push('0123456789');
 if(o.symbol)p.push(o.symbols||'!@#$%^&*()-_=+[]{};:,.?/');
 if(o.noAmb)p=p.map(function(x){return x.replace(LB_AMB,'')});
 return p.filter(function(x){return x.length})}
function lbAll(pools){var seen={},a='';pools.forEach(function(p){p.split('').forEach(function(c){if(!seen[c]){seen[c]=1;a+=c}})});return a}
function lbGen(len,o){
 len=Math.max(1,Math.min(286,len|0));
 var pools=lbPools(o);if(!pools.length)return '';
 var all=lbAll(pools),avail=all.split(''),out=[];
 function pick(src){
  if(o.noRep&&avail.length){var cand=src.split('').filter(function(x){return avail.indexOf(x)>=0});
   if(cand.length){var c=cand[lbRnd(cand.length)];avail.splice(avail.indexOf(c),1);return c}}
  return src[lbRnd(src.length)]}
 if(o.each&&len>=pools.length)pools.forEach(function(p){out.push(pick(p))});
 while(out.length<len)out.push(pick(all));
 for(var i=out.length-1;i>0;i--){var j=lbRnd(i+1),t=out[i];out[i]=out[j];out[j]=t}
 return out.join('')}
function lbBits(len,o){var a=lbAll(lbPools(o));return a.length>1?Math.round(len*Math.log(a.length)/Math.LN2):0}
function lbLabel(b){return b<28?'Very weak':b<40?'Weak':b<60?'Fair':b<80?'Good':b<110?'Strong':'Excellent'}
)JS";
