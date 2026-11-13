// Single page web UI. Self contained (no CDN) so it also works in AP / captive portal mode.
#pragma once
#include <Arduino.h>

static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>SDM Gateway</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--fg:#1d2330;--mut:#6b7280;--line:#e3e6eb;--acc:#2563eb;--ok:#16a34a;--bad:#dc2626;--warn:#d97706}
@media (prefers-color-scheme:dark){:root{--bg:#111418;--card:#1b2027;--fg:#e6e9ee;--mut:#9aa3af;--line:#2b323c;--acc:#5b8def;--ok:#22c55e;--bad:#ef4444;--warn:#f59e0b}}
*{box-sizing:border-box}body{margin:0;font:14px/1.45 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;background:var(--bg);color:var(--fg)}
header{display:flex;align-items:center;gap:12px;padding:12px 16px;background:var(--card);border-bottom:1px solid var(--line);flex-wrap:wrap}
header h1{font-size:17px;margin:0;font-weight:650}header .sub{color:var(--mut);font-size:12px}
nav{display:flex;gap:4px;padding:8px 16px;overflow-x:auto;background:var(--card);border-bottom:1px solid var(--line)}
nav button{border:0;background:none;color:var(--mut);padding:7px 12px;border-radius:7px;cursor:pointer;font:inherit;white-space:nowrap}
nav button.on{background:var(--acc);color:#fff}
main{max-width:1000px;margin:0 auto;padding:16px}
.card{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px 16px;margin-bottom:14px}
.card h2{font-size:15px;margin:0 0 10px}
.tiles{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:10px;margin-bottom:14px}
.tile{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:10px 12px}
.tile .k{color:var(--mut);font-size:12px}.tile .v{font-size:15px;font-weight:600;word-break:break-all}
.dot{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:6px;background:var(--mut)}
.dot.ok{background:var(--ok)}.dot.bad{background:var(--bad)}.dot.warn{background:var(--warn)}
.chips{display:flex;gap:6px;flex-wrap:wrap;margin-bottom:10px}
.chip{border:1px solid var(--line);background:var(--card);color:var(--fg);border-radius:20px;padding:6px 12px;cursor:pointer;font:inherit}
.chip.on{border-color:var(--acc);box-shadow:0 0 0 1px var(--acc)}
table{width:100%;border-collapse:collapse}td,th{padding:6px 8px;border-bottom:1px solid var(--line);text-align:left}
th{color:var(--mut);font-weight:500;font-size:12px}td.v{text-align:right;font-variant-numeric:tabular-nums;font-weight:600}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(230px,1fr));gap:10px 16px}
label.f{display:flex;flex-direction:column;gap:4px;font-size:12px;color:var(--mut)}
label.c{display:flex;align-items:center;gap:8px;font-size:13px;padding-top:18px}
input,select{font:inherit;color:var(--fg);background:var(--bg);border:1px solid var(--line);border-radius:7px;padding:7px 9px;width:100%}
input[type=checkbox]{width:auto}
.hint{font-size:12px;color:var(--mut);margin:6px 0 0}
.btn{border:0;border-radius:8px;padding:9px 16px;font:inherit;font-weight:600;cursor:pointer;background:var(--acc);color:#fff}
.btn.sec{background:var(--bg);color:var(--fg);border:1px solid var(--line)}.btn.red{background:var(--bad)}
.row{display:flex;gap:8px;flex-wrap:wrap;align-items:center}
.bar{position:sticky;bottom:0;background:var(--card);border-top:1px solid var(--line);padding:10px 16px;display:flex;gap:10px;justify-content:flex-end;align-items:center}
.regs{display:grid;grid-template-columns:repeat(auto-fill,minmax(210px,1fr));gap:2px 12px;margin-top:8px;max-height:320px;overflow:auto}
.regs label{display:flex;gap:6px;align-items:center;font-size:12px}
pre{white-space:pre-wrap;font-size:12px;background:var(--bg);padding:10px;border-radius:8px;max-height:420px;overflow:auto}
.msg{font-size:13px}.msg.err{color:var(--bad)}.msg.ok{color:var(--ok)}
.hidden{display:none}
details summary{cursor:pointer;color:var(--acc);margin-top:8px}
</style></head><body>
<header><div><h1 id="title">SDM Gateway</h1><div class="sub" id="sub"></div></div></header>
<nav id="nav"></nav>
<main id="main"></main>
<div class="bar hidden" id="bar"><span class="msg" id="msg"></span><button class="btn" onclick="save()">Save &amp; reboot</button></div>
<script>
const $=s=>document.querySelector(s),h=(t,a={},...c)=>{const e=document.createElement(t);for(const k in a){if(k.startsWith('on'))e[k]=a[k];else if(k==='html')e.innerHTML=a[k];else e.setAttribute(k,a[k])}c.flat().forEach(x=>x!=null&&e.append(x));return e};
let C=null,P=[],S=null,tab='live',sel=0;
const TABS=[['live','Live values'],['meters','Meters'],['mqtt','MQTT / HA'],['wifi','WiFi'],['modbus','Modbus'],['system','System']];
const F={
wifi:[['ssid','WiFi SSID','text'],['wifiPass','WiFi password','pw'],['staticIp','Static IP','chk'],['ip','IP address','text'],['gateway','Gateway','text'],['subnet','Subnet mask','text'],['dns','DNS server','text'],
 ['apPass','Fallback AP password (empty = open, min 8)','pw'],['apFallbackSec','Start AP after WiFi down (s)','num'],['staRetrySec','Retry WiFi while in AP every (s)','num']],
mqtt:[['mqttEnabled','MQTT enabled','chk'],['mqttHost','Broker host / IP','text'],['mqttPort','Port','num'],['mqttUser','Username','text'],['mqttPass','Password','pw'],['mqttClientId','Client ID (empty = hardware id)','text'],
 ['baseTopic','Base topic ({id} = hardware id, {host} = hostname)','text'],['format','Payload format','sel',[[0,'One topic per value (old default)'],[1,'One JSON per meter'],[2,'Both']]],
 ['publishSec','Publish interval to MQTT / Home Assistant (s, 0 = every poll)','num'],
 ['decimals','Decimals','num'],['retain','Retain values','chk'],['publishSystem','Publish Uptime/RSSI/VCC/...','chk'],['systemIntervalSec','System values interval (s, 0 = with every publish)','num'],
 ['haEnabled','Home Assistant discovery','chk'],['haPrefix','Discovery prefix','text']],
modbus:[['baud','Baud rate','sel',[[1200,1200],[2400,2400],[4800,4800],[9600,9600],[19200,19200],[38400,38400]]],['serialFmt','Format','sel',[['8N1','8N1'],['8E1','8E1'],['8O1','8O1'],['8N2','8N2']]],
 ['timeoutMs','Response timeout (ms)','num'],['pollMs','Poll interval (ms)','num'],['holdingSec','Holding registers interval (s)','num'],['gapMs','Pause between requests (ms)','num'],
 ['swapSerial','ESP8266: use D7(RX)/D8(TX) instead of RX/TX pins','chk','esp8266'],['rxPin','RX pin (GPIO)','num','esp32'],['txPin','TX pin (GPIO)','num','esp32'],['dePin','RS485 DE/RE pin (-1 = auto direction module)','num']],
system:[['hostname','Hostname (empty = sdm-gateway-<id>)','text'],['adminPass','Web UI password, user "admin" (empty = none)','pw'],['ntpServer','NTP server','text'],['tz','Time zone (POSIX, e.g. CET-1CEST,M3.5.0,M10.5.0/3)','text'],
 ['wdNoWifiMin','Reboot if WiFi down for (min, 0 = off)','num'],['wdNoMqttMin','Reboot if MQTT down for (min, 0 = off)','num'],['wdNoModbusMin','Reboot if no meter answers for (min, 0 = off)','num']]};
const HINT={wifi:'If the WiFi is gone for longer than the fallback time, the access point "'+'SDM-Gateway-…" starts (http://192.168.4.1). The gateway keeps retrying the WiFi in the background and switches the AP off again when it is back.',
mqtt:'Default topics are the same as the old firmware: <base><meter topic>/<value>, e.g. 1A2B3C/PowerMeter/1/Voltage/1. Availability: <base>status (online/offline). Commands to <base>control: reboot, discovery. The meters are still read at the Modbus poll interval (live values in this UI); MQTT / Home Assistant get the latest values at the publish interval.',
modbus:'D1 mini: Modbus uses the hardware UART (RX/TX pins, or D7/D8 when swapped). Logs go to D4 (TX only). ESP32-S3: any free GPIOs.'};
function nav(){const n=$('#nav');n.innerHTML='';TABS.forEach(([k,l])=>n.append(h('button',{class:k===tab?'on':'',onclick:()=>{tab=k;nav();render()}},l)))}
function field(f){const[k,l,t,o,only]=f,b=(C._board||'').toLowerCase();const plat=t==='sel'?only:o;if(plat&&!b.includes(plat))return null;
 if(t==='chk'){const i=h('input',{type:'checkbox'});i.checked=!!C[k];i.onchange=()=>{C[k]=i.checked;dirty()};return h('label',{class:'c'},i,l)}
 let i;if(t==='sel'){i=h('select');o.forEach(([v,lab])=>{const op=h('option',{value:v},String(lab));if(String(C[k])===String(v))op.selected=true;i.append(op)})}
 else{i=h('input',{type:t==='pw'?'password':t==='num'?'number':'text',value:C[k]??''});if(t==='pw')i.placeholder='(not set)'}
 i.oninput=i.onchange=()=>{C[k]=(t==='num'||(t==='sel'&&typeof C[k]==='number'))?Number(i.value):i.value;dirty()};return h('label',{class:'f'},l,i)}
function form(key,title){return h('div',{class:'card'},h('h2',{},title),h('div',{class:'grid'},F[key].map(field)),HINT[key]?h('p',{class:'hint'},HINT[key]):null)}
function dirty(){$('#bar').classList.remove('hidden');msg('Unsaved changes','')}
function msg(t,c){const m=$('#msg');m.textContent=t;m.className='msg '+(c||'')}
function fmtUp(s){const d=Math.floor(s/86400),hh=Math.floor(s/3600)%24,m=Math.floor(s/60)%60;return(d?d+'d ':'')+hh+'h '+m+'m'}
function tile(k,v,cls){return h('div',{class:'tile'},h('div',{class:'k'},k),h('div',{class:'v'},cls!=null?h('span',{class:'dot '+cls}):null,v))}
async function api(u,o){const r=await fetch(u,o);if(!r.ok){let e;try{e=(await r.json()).error}catch(x){}throw new Error(e||r.statusText)}return r.json()}
function render(){const m=$('#main');m.innerHTML='';
 if(tab==='live')return renderLive(m);
 if(tab==='meters')return renderMeters(m);
 if(tab==='mqtt')m.append(form('mqtt','MQTT & Home Assistant'));
 if(tab==='wifi'){m.append(form('wifi','WiFi'));m.append(scanCard())}
 if(tab==='modbus')m.append(form('modbus','Modbus RTU (RS485)'));
 if(tab==='system')renderSystem(m)}
function renderLive(m){
 m.append(h('div',{class:'tiles',id:'tiles'}));m.append(h('div',{class:'chips',id:'chips'}));
 m.append(h('div',{class:'card'},h('h2',{id:'vt'},'Values'),h('div',{id:'mi',class:'hint'}),h('table',{},h('thead',{},h('tr',{},h('th',{},'Value'),h('th',{style:'text-align:right'},'Reading'),h('th',{},'Unit'))),h('tbody',{id:'vals'}))));
 updLive()}
function updLive(){if(tab!=='live'||!S)return;const t=$('#tiles');t.innerHTML='';const w=S.wifi,q=S.mqtt;
 t.append(tile('WiFi',w.connected?w.ssid+' ('+w.rssi+' dBm)':w.state,w.connected?'ok':'bad'),tile('IP',w.ip||'-'),tile('MQTT',q.state,q.connected?'ok':(C.mqttEnabled?'bad':null)),
  tile('Access point',w.ap?w.apSsid+' ('+w.apClients+' clients)':'off',w.ap?'warn':null),tile('Uptime',fmtUp(S.uptime)),tile('Free heap',S.heap+' B'));
 if(S.scanning)t.append(tile('Modbus','bus scan running','warn'));
 const c=$('#chips');c.innerHTML='';if(!S.meters.length)c.append(h('span',{class:'hint'},'No meters configured - add one in the Meters tab.'));
 S.meters.forEach((x,i)=>c.append(h('button',{class:'chip'+(i===sel?' on':''),onclick:()=>{sel=i;updLive();loadVals()}},h('span',{class:'dot '+(!x.enabled?'':x.online?'ok':'bad')}),x.name+' #'+x.addr)));
 const x=S.meters[sel];if(x){$('#vt').textContent=x.name+' - '+x.type;$('#mi').textContent=(x.enabled?(x.online?'online':'offline'):'disabled')+' | ok '+x.ok+' / errors '+x.err+(x.lastErr?' (last: '+x.lastErr+')':'')+' | last answer '+(x.age<0?'never':x.age+' s ago')+' | poll '+x.pollMs+' ms | topic '+x.topic+'/…'}}
async function loadVals(){if(tab!=='live'||!S||!S.meters[sel])return;try{const d=await api('/api/values?m='+sel);const tb=$('#vals');if(!tb)return;tb.innerHTML='';
 d.values.forEach(v=>tb.append(h('tr',{},h('td',{},v.n),h('td',{class:'v'},v.v==null?'–':v.v),h('td',{},v.u))))}catch(e){}}
let scanTimer=null;
function scanMetersCard(){
 const from=h('input',{type:'number',min:1,max:247,value:1}),to=h('input',{type:'number',min:1,max:247,value:32});
 const prog=h('div',{class:'hint'}),res=h('div');
 const btn=h('button',{class:'btn',onclick:start},'Scan bus'),stop=h('button',{class:'btn sec hidden',onclick:()=>api('/api/meter-scan/stop',{method:'POST'}).then(show).catch(()=>{})},'Stop');
 const isNew=f=>!C.meters.some(x=>x.addr===f.addr);
 const addFound=f=>{if(!isNew(f)||C.meters.length>=(C._maxMeters||10))return;C.meters.push({enabled:true,addr:f.addr,type:f.type,name:'',topic:'',mask:''});dirty()};
 async function start(){try{show(await api('/api/meter-scan?from='+from.value+'&to='+to.value,{method:'POST'}));poll()}catch(e){prog.textContent=e.message}}
 function poll(){clearTimeout(scanTimer);scanTimer=setTimeout(async()=>{if(tab!=='meters')return;try{const d=await api('/api/meter-scan');show(d);if(d.running)poll()}catch(e){poll()}},1000)}
 function show(d){btn.classList.toggle('hidden',d.running);stop.classList.toggle('hidden',!d.running);
  const n=d.to-d.from+1,done=Math.max(0,Math.min(n,d.current-d.from));
  prog.textContent=d.running?'Scanning address '+d.current+' ('+d.from+'–'+d.to+', '+Math.round(done*100/n)+'%) · '+d.found.length+' found · normal polling is paused'
   :(done?'Scan finished: '+d.found.length+' meter(s) found on addresses '+d.from+'–'+d.to+'.':'');
  res.innerHTML='';
  d.found.forEach(f=>{const lab=(P.find(p=>p.id===f.type)||{}).label||f.type;const b=h('button',{class:'btn sec',onclick:()=>{addFound(f);render()}},'Add');if(!isNew(f)){b.disabled=true;b.textContent='Already configured'}
   res.append(h('div',{class:'row',style:'padding:5px 0;border-bottom:1px solid var(--line)'},h('b',{style:'min-width:44px'},'#'+f.addr),h('span',{style:'flex:1'},lab),h('span',{class:'hint'},f.threePhase?f.v1+' / '+f.v2+' / '+f.v3+' V':f.v1+' V'),b))});
  const nw=d.found.filter(isNew);if(!d.running&&nw.length>1)res.append(h('button',{class:'btn',style:'margin-top:8px',onclick:()=>{nw.forEach(addFound);render()}},'Add all '+nw.length+' new meters'))}
 api('/api/meter-scan').then(d=>{show(d);if(d.running)poll()}).catch(()=>{});
 return h('div',{class:'card'},h('h2',{},'Find meters (bus scan)'),
  h('div',{class:'row'},h('label',{class:'f',style:'width:110px'},'From address',from),h('label',{class:'f',style:'width:110px'},'To address',to),h('div',{style:'padding-top:18px'},btn,stop)),
  prog,res,h('p',{class:'hint'},'Asks every address for its voltages and picks the type: 3-phase → SDM630 full, single phase → SDM230 (full or small). Check the type afterwards. About 0.3 s per empty address (1–247 ≈ 1 min). Added meters are saved with "Save & reboot".'))}
function renderMeters(m){const prof=id=>P.find(p=>p.id===id);m.append(scanMetersCard());
 C.meters.forEach((mt,i)=>{const p=prof(mt.type)||P[0];const regs=h('div',{class:'regs'});
  const mask=()=>mt.mask&&mt.mask.length===p.regs.length?mt.mask:'1'.repeat(p.regs.length);
  const cnt=h('span');const upd=()=>{const s=mask();cnt.textContent=' ('+[...s].filter(c=>c==='1').length+'/'+s.length+' enabled)'};
  p.regs.forEach((r,j)=>{const cb=h('input',{type:'checkbox'});cb.checked=mask()[j]==='1';cb.onchange=()=>{const a=[...mask()];a[j]=cb.checked?'1':'0';mt.mask=a.join('');upd();dirty()};
   regs.append(h('label',{},cb,r[0]+(r[1]?' ['+r[1]+']':'')+(r[2]==3?' *':'')))});upd();
  const setAll=v=>{mt.mask=(v?'1':'0').repeat(p.regs.length);dirty();render()};
  const inp=(k,l,t)=>{const e=h('input',{type:t||'text',value:mt[k]??''});e.oninput=()=>{mt[k]=t==='number'?Number(e.value):e.value;dirty()};return h('label',{class:'f'},l,e)};
  const ty=h('select');P.forEach(pp=>{const o=h('option',{value:pp.id},pp.label);if(pp.id===mt.type)o.selected=true;ty.append(o)});ty.onchange=()=>{mt.type=ty.value;mt.mask='';dirty();render()};
  const en=h('input',{type:'checkbox'});en.checked=mt.enabled;en.onchange=()=>{mt.enabled=en.checked;dirty()};
  m.append(h('div',{class:'card'},h('div',{class:'row'},h('h2',{style:'flex:1;margin:0'},(mt.name||'Meter '+mt.addr)),h('button',{class:'btn sec',onclick:()=>{C.meters.splice(i,1);dirty();render()}},'Remove')),
   h('div',{class:'grid'},h('label',{class:'c'},en,'Enabled'),inp('name','Name (Home Assistant device name)'),inp('addr','Modbus address (1-247)','number'),h('label',{class:'f'},'Meter type',ty),inp('topic','MQTT sub topic (empty = address, old default)')),
   h('details',{},h('summary',{},'Published values',cnt),h('div',{class:'row',style:'margin-top:6px'},h('button',{class:'btn sec',onclick:()=>setAll(1)},'All'),h('button',{class:'btn sec',onclick:()=>setAll(0)},'None'),h('span',{class:'hint'},'* = holding register, read every holding interval')),regs)))});
 if(C.meters.length<(C._maxMeters||10))m.append(h('button',{class:'btn',onclick:()=>{const used=C.meters.map(x=>x.addr);let a=1;while(used.includes(a))a++;C.meters.push({enabled:true,addr:a,type:'630',name:'',topic:'',mask:''});dirty();render()}},'+ Add meter'));
 m.append(h('p',{class:'hint'},'All meters share one RS485 bus and need different Modbus addresses (set on the meter display). The old firmware polled addresses 1-4 with topics 1-4.'))}
function scanCard(){const list=h('div');const b=h('button',{class:'btn sec',onclick:scan},'Scan networks');
 async function scan(){b.disabled=true;list.textContent='Scanning…';for(let i=0;i<15;i++){const d=await api('/api/scan');if(!d.running){list.innerHTML='';(d.networks||[]).sort((a,b)=>b.rssi-a.rssi).forEach(n=>list.append(h('div',{class:'row',style:'padding:4px 0'},h('button',{class:'chip',onclick:()=>{C.ssid=n.ssid;dirty();render()}},n.ssid||'(hidden)'),h('span',{class:'hint'},n.rssi+' dBm'+(n.open?' open':'')))));b.disabled=false;return}await new Promise(r=>setTimeout(r,1000))}b.disabled=false}
 return h('div',{class:'card'},h('h2',{},'Available networks'),b,list)}
function renderSystem(m){m.append(form('system','System & watchdog'));
 const info=h('div',{class:'grid'});if(S)[['Firmware',S.fw],['Board',S.board],['Hardware ID',S.id],['MAC',S.wifi.mac],['Last reset reason',S.reset],['VCC',S.vcc?S.vcc+' mV':'-']].forEach(([k,v])=>info.append(tile(k,v)));
 m.append(h('div',{class:'card'},h('h2',{},'Device'),info));
 const up=h('form',{method:'POST',action:'/update',enctype:'multipart/form-data'},h('div',{class:'row'},h('input',{type:'file',name:'update',accept:'.bin',style:'flex:1'}),h('button',{class:'btn'},'Upload firmware')));
 m.append(h('div',{class:'card'},h('h2',{},'Firmware update'),up,h('p',{class:'hint'},'Use .pio/build/<env>/firmware.bin. The device reboots when done.')));
 const lg=h('pre',{},'…');const ld=async()=>{try{lg.textContent=await (await fetch('/api/log')).text();lg.scrollTop=1e9}catch(e){}};ld();
 m.append(h('div',{class:'card'},h('div',{class:'row'},h('h2',{style:'flex:1;margin:0'},'Log'),h('button',{class:'btn sec',onclick:ld},'Refresh')),lg));
 m.append(h('div',{class:'card'},h('h2',{},'Maintenance'),h('div',{class:'row'},h('button',{class:'btn sec',onclick:()=>act('/api/reboot','Reboot now?')},'Reboot'),
  h('button',{class:'btn red',onclick:()=>act('/api/factory-reset','Erase ALL settings (WiFi, MQTT, meters)?')},'Factory reset'))))}
async function act(u,q){if(!confirm(q))return;await fetch(u,{method:'POST'});waitReboot()}
function waitReboot(){$('#main').innerHTML='';$('#main').append(h('div',{class:'card'},h('h2',{},'Rebooting…'),h('p',{class:'hint'},'The page reloads automatically. If you changed WiFi settings, reconnect to the new network and open the new address.')));
 $('#bar').classList.add('hidden');setTimeout(function t(){fetch('/api/status').then(()=>location.reload()).catch(()=>setTimeout(t,2000))},6000)}
async function save(){msg('Saving…','');try{await api('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(C)});waitReboot()}catch(e){msg(e.message,'err')}}
async function poll(){try{S=await api('/api/status');$('#title').textContent=S.host;$('#sub').textContent=S.board+' · fw '+S.fw+' · '+(S.wifi.ip||'192.168.4.1');updLive();loadVals()}catch(e){}}
(async()=>{nav();try{[C,P]=await Promise.all([api('/api/config'),api('/api/profiles')])}catch(e){$('#main').textContent='Failed to load: '+e.message;return}
 if(!C.ssid)tab='wifi';nav();await poll();render();setInterval(poll,2500)})();
</script></body></html>)HTML";
