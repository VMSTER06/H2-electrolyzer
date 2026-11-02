const $=id=>document.getElementById(id);
const canvas=$('canvas'),viewport=$('viewport'),steps=DATA.steps;
const SVG='http://www.w3.org/2000/svg';
const partMap=new Map(DATA.parts.map(p=>[p.id,p]));
const originalGroups=new Map([...canvas.querySelectorAll('[data-part]')].map(g=>[g.dataset.part,g]));
const originalLabels=[...canvas.querySelectorAll(':scope > text')];
const firstPorts=new Map();
for(const s of steps)if(s.connection)for(const id of [s.connection.a,s.connection.b]){const part=id.split('.')[0];if(!firstPorts.has(part))firstPorts.set(part,id)}
const STORAGE='irfb4110-assembly-2026-10-24-v1';
let saved={};try{saved=JSON.parse(localStorage.getItem(STORAGE)||'{}')}catch{}
let index=Math.max(0,Math.min(steps.length-1,Number(saved.index)||0)),mode='local',scale=1;
let completed=new Set(Array.isArray(saved.completed)?saved.completed.filter(id=>steps.some(s=>s.id===id)):[]);
let dimensions=[1300,720];
const layer=document.createElementNS(SVG,'g');layer.id='local-stage-layer';canvas.append(layer);
function store(){try{localStorage.setItem(STORAGE,JSON.stringify({index,completed:[...completed]}))}catch{}}
function el(name,attrs={},parent=layer){const n=document.createElementNS(SVG,name);for(const [k,v]of Object.entries(attrs))n.setAttribute(k,v);parent.append(n);return n}
function label(x,y,value,size=23,weight=400,anchor='start',parent=layer){let n=el('text',{x,y,style:`font-family:Arial,sans-serif;font-size:${size}px;font-weight:${weight};text-anchor:${anchor};fill:#193342`},parent);n.textContent=value;return n}
function lines(x,y,value,max=44,size=22,parent=layer){let words=String(value).split(/\s+/),row='',n=0;for(const word of words){if((row+' '+word).trim().length>max&&row){label(x,y+n*29,row,size,600,'start',parent);row=word;n++}else row=(row+' '+word).trim()}if(row)label(x,y+n*29,row,size,600,'start',parent)}
function bounds(id){const p=partMap.get(id);if(!p)return[0,0,10,10];let[x,y]=p.xy;
 if(id==='ESP')return[x,y,x+300,y+650];if(id==='U1')return[x,y,x+450,y+450];if(id==='U2')return[x,y,x+300,y+300];
 if(/^Q[1-4]$/.test(id))return[x,y,x+220,y+220*3350/1550];if(/^D[34]$/.test(id))return[x,y,x+180,y+270];if(id==='TV1')return[x,y,x+220,y+220];if(id==='T1')return[x,y,x+180,y+180];
 if(/^C\d+$/.test(id))return[x-38,y-43,x+38,y+72];
 let pp=p.ports.map(k=>DATA.nodes[k]),left=Math.min(...pp.map(p=>p.x)),right=Math.max(...pp.map(p=>p.x));
 if(/^(R\d+|D[12]|Z\d+|TV2)$/.test(id))return[left-3,y-47,right+3,y+47];
 if(id==='АКБ')return[x,y,x+315,y+245];if(id==='НАГРУЗКА')return[x,y,x+350,y+170];return[x,y,right,y+58];
}
function partTitle(id){let p=partMap.get(id);return p.name.startsWith(id+' · ')?p.name:id+' · '+p.name}
function markerName(nodeId){let [id,pin]=nodeId.split('.');if(id==='ESP')return pin==='32'?'32':'−';if(/^R\d+$/.test(id))return firstPorts.get(id)===nodeId?'1':'2';if(pin==='K'||pin==='A')return pin;if(/^C(3|5|7|9|10)$/.test(id))return firstPorts.get(id)===nodeId?'1':'2';if(pin==='+'||pin==='-')return pin==='+'?'+':'−';if(id==='АКБ')return pin==='P+'?'+':'−';return pin}
function markPin(x,y,id,selected){if(selected){let d=direction(id),bx=x+d[0]*28,by=y+(d[1]||-1)*28;el('circle',{cx:x,cy:y,r:11,fill:'none',stroke:'#d92f26','stroke-width':3});el('circle',{cx:x,cy:y,r:3,fill:'#d92f26'});el('circle',{cx:bx,cy:by,r:12,fill:'#ffe18a',stroke:'white','stroke-width':2});label(bx,by+5,markerName(id),16,700,'middle');}else{el('circle',{cx:x,cy:y,r:10,fill:'#486476',stroke:'white','stroke-width':2});label(x,y+5,markerName(id),14,700,'middle').setAttribute('style','font-family:Arial,sans-serif;font-size:14px;font-weight:700;text-anchor:middle;fill:white')}}
function detailBounds(id,selected){let b=bounds(id),[x,y]=partMap.get(id).xy;
 if(id==='U1'){let pins=selected.map(n=>+n.split('.')[1]);if(pins.every(n=>n<5))return[x,y+105,x+345,y+450];if(pins.every(n=>n>4))return[x+145,y,x+450,y+295]}
 if(/^Q[1-4]$/.test(id))return[x,y+170,b[2],b[3]];
 if(id==='U2')return[x+70,y+95,x+220,y+300];
 if(/^D[34]$/.test(id))return[x,y+70,b[2],b[3]];
 if(id==='ESP'){let p=DATA.nodes[selected[0]],top=Math.max(y,p.y-90),bottom=Math.min(y+650,p.y+90);return[x,top,x+180,bottom]}
 return b;
}
let clipNumber=0;
function place(id,slot,selected=[],showTitle=true,detail=false){let b=detail?detailBounds(id,selected):bounds(id),w=b[2]-b[0],h=b[3]-b[1],s=Math.min(slot.w/w,slot.h/h,3.4),x=slot.x+(slot.w-w*s)/2,y=slot.y+(slot.h-h*s)/2;
 let clipId='step-photo-clip-'+(++clipNumber);const defs=el('defs'),cp=el('clipPath',{id:clipId,clipPathUnits:'userSpaceOnUse'},defs);el('rect',{x,y,width:w*s,height:h*s},cp);const clipped=el('g',{'clip-path':'url(#'+clipId+')'});
 let g=originalGroups.get(id).cloneNode(true);g.classList.remove('step-part','part-active','related-part');g.removeAttribute('data-part');g.setAttribute('transform',`translate(${x-b[0]*s} ${y-b[1]*s}) scale(${s})`);clipped.append(g);
 if(showTitle)lines(slot.x,slot.y-43,partTitle(id),38,23);
 let fn=p=>({x:x+(p.x-b[0])*s,y:y+(p.y-b[1])*s});
 const pins=partMap.get(id).ports;if(/^(U1|U2|Q[1-4]|D[34]|ESP)$/.test(id)){for(const n of pins){let p=fn(DATA.nodes[n]);if(p.x>=x-1&&p.x<=x+w*s+1&&p.y>=y-1&&p.y<=y+h*s+1)markPin(p.x,p.y,n,selected.includes(n))}}
 else for(const n of selected){let p=fn(DATA.nodes[n]);markPin(p.x,p.y,n,true)}
 return{point:node=>fn(DATA.nodes[node]),box:{left:x,top:y,right:x+w*s,bottom:y+h*s},scale:s};
}
function direction(id){let[part,pin]=id.split('.');if(part==='U1')return +pin<5?[-1,1]:[1,-1];if(/^(U2|Q[1-4]|D[34]|C\d+)$/.test(part))return[0,1];if(part==='ESP')return[-1,0];if(part==='T1')return[-1,-1];if(part==='TV1')return pin==='K'?[1,-1]:[0,1];
 if(/^R\d+$/.test(part))return pin==='1'?[-1,0]:[1,0];if(part==='D1')return pin==='A'?[-1,0]:[1,0];if(/^(D2|Z\d+|TV2)$/.test(part))return pin==='K'?[-1,0]:[1,0];if(part==='НАГРУЗКА')return pin==='+'?[-1,0]:[1,0];return[1,0]}
function miniature(id,slot,selected){let b=bounds(id),s=Math.min(86/(b[2]-b[0]),90/(b[3]-b[1])),x=slot.x+slot.w-90,y=45;let g=originalGroups.get(id).cloneNode(true);g.removeAttribute('data-part');g.classList.remove('step-part','related-part');g.setAttribute('transform',`translate(${x-b[0]*s} ${y-b[1]*s}) scale(${s})`);layer.append(g);label(x,35,'Целиком',13,400);let c=detailBounds(id,selected);el('rect',{x:x+(c[0]-b[0])*s,y:y+(c[1]-b[1])*s,width:(c[2]-c[0])*s,height:(c[3]-c[1])*s,fill:'none',stroke:'#d92f26','stroke-width':1.5});}
function detailPart(id,slot,selected){let b=bounds(id),c=detailBounds(id,selected),changed=b.some((v,i)=>Math.abs(v-c[i])>.5);let placed=place(id,slot,selected,true,true);if(changed)miniature(id,slot,selected);return placed}
function portInfo(id){let[part,pin]=id.split('.'),first=firstPorts.get(part)===id,n=+pin;
 if(part==='U1'){let names={1:'ENA',2:'INA',3:'GND',4:'INB',5:'OUTB',6:'VDD, 12 В',7:'OUTA',8:'ENB'},rank=n<5?n:9-n;return{title:`U1: ножка ${pin} (${names[n]})`,hint:n<5?`Сторона с точкой. Считай от края с точкой: ножка №${rank}.`:`Сторона напротив точки. Считай от края с точкой: ${rank}-я ножка.`}}
 if(/^Q[1-4]$/.test(part))return{title:`${part}: ${['','ЛЕВАЯ ножка 1 G','СРЕДНЯЯ ножка 2 D','ПРАВАЯ ножка 3 S'][n]}`,hint:'Надпись IRFB4110 к себе, металлическое ушко сверху, ножки вниз.'};
 if(part==='U2')return{title:`U2 L7812: ${['','ЛЕВАЯ ножка 1 IN','СРЕДНЯЯ ножка 2 GND','ПРАВАЯ ножка 3 OUT'][n]}`,hint:'Надпись L7812 к себе, ножки вниз. Не путай с IRFB4110.'};
 if(/^R\d+$/.test(part))return{title:`${part}: ${first?'первый конец — любой вывод':'второй конец — оставшийся вывод'}`,hint:first?'У резистора нет полярности. Любой его вывод назначь первым.':'Это другой конец того же резистора. Не припаивай сюда уже подключённый конец.'};
 if(/^Z\d+$/.test(part)||part==='D1'||part==='D2'||part==='TV2')return{title:`${part}: конец ${pin==='K'?'С ПОЛОСКОЙ (K)':'БЕЗ ПОЛОСКИ (A)'}`,hint:'Полоска находится на корпусе возле нужного конца, а не на проволочной ножке.'};
 if(/^C\d+$/.test(part)){let pol=/^C(1|2|4|6|8)$/.test(part);return pol?{title:`${part}: ${pin==='+'?'ПЛЮС, ножка без полосы «−»':'МИНУС, ножка со стороны полосы «−»'}`,hint:'Полоса «−» на корпусе указывает на минусовую ножку.'}:{title:`${part}: ${first?'любой первый вывод':'другой, второй вывод'}`,hint:'Это неполярный конденсатор; его выводы равнозначны.'}}
 if(part==='ESP')return{title:pin==='32'?'ESP32: контакт с надписью 32 / GPIO32':'ESP32: контакт с надписью GND',hint:'Ищи именно надпись на плате, а не отсчитывай контакты от края.'};
 if(/^D[34]$/.test(part))return{title:`${part}: ${['','ЛЕВАЯ ножка 1','СРЕДНЯЯ ножка 2','ПРАВАЯ ножка 3'][n]}`,hint:'Надпись MBR60100CT к себе, металлическое ушко сверху.'};
 if(part==='TV1')return{title:pin==='A'?'TV1: БОЛЬШАЯ нижняя металлическая площадка A':'TV1: МАЛЕНЬКИЙ отдельный контакт K',hint:'На картинке DOWO SM8S33A показан снизу.'};
 if(part==='T1')return{title:first?'T1: любой первый провод':'T1: второй, оставшийся провод',hint:'У проводов термостата нет полярности.'};
 return{title:DATA.nodes[id].label,hint:part==='АКБ'?'Подготовь кабель. Аккумулятор пока не подключай.':'Смотри отмеченную точку на рисунке.'};
}
function stepAction(s){if(!s.connection)return s.note;let q=s.connection,p=s.primary[0],a=q.a.split('.')[0],b=q.b.split('.')[0];
 if(a==='АКБ'||b==='АКБ')return'Подготовь кабель между указанными точками. К аккумулятору его пока НЕ подключай.';
 if(/^R\d+$/.test(p)){let own=a===p?q.a:q.b,other=a===p?q.b:q.a;return`Припаяй ${firstPorts.get(p)===own?'первый (любой)':'свободный второй'} вывод ${partTitle(p)} сюда: ${portInfo(other).title}.`}
 if(a===b&&a==='U1')return`Сделай перемычку между ножками ${q.a.split('.')[1]} и ${q.b.split('.')[1]} драйвера U1.`;
 if(a===b&&/^D[34]$/.test(a))return`Соедини левую ножку 1 и правую ножку 3 диода ${a}.`;
 return'Припаяй провод между двумя красными точками. Точные выводы подписаны ниже.';
}
function localWire(q,a,b,same){const p=a.point(q.a),r=b.point(q.b),da=direction(q.a),db=direction(q.b),ea={x:p.x+da[0]*38,y:p.y+da[1]*38},eb={x:r.x+db[0]*38,y:r.y+db[1]*38};let pts=[];
 if(same){if(da[1]>0&&db[1]>0&&da[0]===0&&db[0]===0)pts=[p,ea,{x:ea.x,y:624},{x:eb.x,y:624},eb,r];else{let left=a.box.left-45,right=a.box.right+45;if(da[0]===db[0]){let side=da[0]<0?left:right;pts=[p,ea,{x:side,y:ea.y},{x:side,y:eb.y},eb,r]}else pts=[p,ea,{x:da[0]<0?left:right,y:ea.y},{x:da[0]<0?left:right,y:624},{x:db[0]<0?left:right,y:624},{x:db[0]<0?left:right,y:eb.y},eb,r]}}
 else{let leftPath=da[0]>0?[p,ea,{x:650,y:ea.y}]:da[0]<0?[p,ea,{x:a.box.left-35,y:ea.y},{x:a.box.left-35,y:624},{x:650,y:624}]:[p,ea,{x:ea.x,y:624},{x:650,y:624}];let rightPath=db[0]<0?[{x:650,y:eb.y},eb,r]:db[0]>0?[{x:650,y:624},{x:b.box.right+35,y:624},{x:b.box.right+35,y:eb.y},eb,r]:[{x:650,y:624},{x:eb.x,y:624},eb,r];pts=[...leftPath,...rightPath]}
 let color=canvas.querySelector('#'+q.edges[0]+' .wire-line')?.getAttribute('stroke')||'#1764ba',pstr=pts.map(p=>`${p.x},${p.y}`).join(' ');
 el('polyline',{points:pstr,fill:'none',stroke:'white','stroke-width':13,'stroke-linejoin':'round'});el('polyline',{points:pstr,fill:'none',stroke:color,'stroke-width':6,'stroke-linecap':'round','stroke-linejoin':'round'});markPin(p.x,p.y,q.a,true);markPin(r.x,r.y,q.b,true);
 label(650,688,'Красный кружок — точное место пайки. В жёлтой метке — номер вывода.',19,400,'middle');
}
function localStep(step){layer.replaceChildren();let q=step.connection;
 if(q){let aId=q.a.split('.')[0],bId=q.b.split('.')[0],same=aId===bId;label(same?430:100,56,same?'Оба места пайки — на одной детали':'Место пайки 1 · крупно',22,400);if(!same)label(770,56,'Место пайки 2 · крупно',22,400);let a=detailPart(aId,same?{x:430,y:155,w:440,h:410}:{x:100,y:155,w:430,h:410},[q.a,...same?[q.b]:[]]);let b=same?a:detailPart(bId,{x:770,y:155,w:430,h:410},[q.b]);localWire(q,a,b,same);}
 else if(step.kind==='adapter'){place('U1',{x:100,y:150,w:430,h:420},['U1.1']);label(760,100,'Переходник SOIC-8 → DIP-8',23,700);el('image',{href:DATA.adapter,x:735,y:170,width:470,height:314});lines(750,540,'На фото пример платы: установленная микросхема другая.',43,19);label(100,640,'Точка на корпусе U1 — возле ножки 1.',22);}
 else if(step.kind==='mount'){step.primary.forEach((id,i)=>place(id,{x:45+i*320,y:165,w:265,h:410},partMap.get(id).ports));label(650,650,'Сзади общий радиатор. Все металлические ушки = D.',24,600,'middle');}
 else{place('U2',{x:100,y:160,w:430,h:400},partMap.get('U2').ports);place('U1',{x:770,y:160,w:430,h:400},partMap.get('U1').ports);label(650,650,'Здесь проверка измерением, а не добавление нового провода.',22,400,'middle')}
}
function clearScene(){canvas.querySelectorAll('.step-wire,.step-node,.step-part,.active,.part-active,.related-part,.related-label').forEach(n=>n.classList.remove('step-wire','step-node','step-part','active','part-active','related-part','related-label'));canvas.classList.remove('has-selection')}
function related(step){let ids=new Set(step.primary),connections=DATA.instructions.filter(q=>ids.has(q.a.split('.')[0])||ids.has(q.b.split('.')[0]));let parts=new Set(step.primary),edges=new Set();for(const q of connections){parts.add(q.a.split('.')[0]);parts.add(q.b.split('.')[0]);q.edges.forEach(id=>edges.add(id))}return{connections,parts,edges}}
function highlight(step){const scope=related(step);for(const id of scope.parts){originalGroups.get(id)?.classList.add('related-part');if(step.primary.includes(id))originalGroups.get(id)?.classList.add('step-part');for(const p of partMap.get(id)?.ports||[])canvas.querySelectorAll(`[data-node="${p}"]`).forEach(n=>n.classList.add('step-node'));for(const t of originalLabels){let s=t.textContent;if(s===id||s.startsWith(id+' ·')||(id==='ESP'&&s.startsWith('ESP32'))){t.classList.add('related-label');if(/^C\d+$/.test(id)&&s===id){t.nextElementSibling?.classList.add('related-label');t.nextElementSibling?.nextElementSibling?.classList.add('related-label')}}}}
 for(const id of scope.edges){let w=DATA.edges.find(w=>w.id===id);canvas.querySelector('#'+id).classList.add('step-wire');for(const n of [w.a,w.b])canvas.querySelectorAll(`[data-node="${n}"]`).forEach(p=>p.classList.add('step-node'))}
 const nets=new Set([...scope.edges].map(id=>DATA.edges.find(w=>w.id===id).net));for(const t of originalLabels)if((nets.has('D')&&t.textContent.startsWith('D ·'))||(nets.has('GND')&&t.textContent.startsWith('S / GND')))t.classList.add('related-label');
 $('relations-list').replaceChildren();for(const q of scope.connections){let li=document.createElement('li');li.textContent=portInfo(q.a).title+' ↔ '+portInfo(q.b).title;$('relations-list').append(li)}$('relations-title').textContent='Все соединения '+step.primary.join(', ')+': '+scope.connections.length;$('relations').open=scope.connections.length<=4;return scope;
}
function contextBounds(){const scope=related(steps[index]),pts=[];for(const id of scope.parts){let b=bounds(id);pts.push([b[0],b[1]-60],[b[2],b[3]+60])}for(const id of scope.edges){let w=DATA.edges.find(w=>w.id===id);pts.push([DATA.nodes[w.a].x,DATA.nodes[w.a].y],...w.via,[DATA.nodes[w.b].x,DATA.nodes[w.b].y])}return[Math.min(...pts.map(p=>p[0]))-65,Math.min(...pts.map(p=>p[1]))-65,Math.max(...pts.map(p=>p[0]))+65,Math.max(...pts.map(p=>p[1]))+65]}
function zoom(z){scale=Math.max(.08,Math.min(3,z));canvas.style.width=(dimensions[0]*scale)+'px';canvas.style.height=(dimensions[1]*scale)+'px'}
function fit(){let top=viewport.getBoundingClientRect().top+scrollY;viewport.style.height=Math.max(350,Math.min(780,innerHeight-top-45))+'px';if(mode==='context'){let b=contextBounds();zoom(Math.min(1.8,viewport.clientWidth/(b[2]-b[0]),viewport.clientHeight/(b[3]-b[1])));viewport.scrollTo(Math.max(0,b[0]*scale),Math.max(0,b[1]*scale));}else{zoom(Math.min(viewport.clientWidth/dimensions[0],viewport.clientHeight/dimensions[1]));viewport.scrollTo(0,0)}}
function updateView(){const s=steps[index];clearScene();document.body.classList.toggle('full-overview',mode==='all');canvas.classList.toggle('local-view',mode==='local');canvas.classList.toggle('context-view',mode==='context');
 if(mode==='local'){dimensions=[1300,720];canvas.setAttribute('viewBox','0 0 1300 720');localStep(s);$('view-caption').textContent='Крупный шаг: только нужные детали и одно соединение.';}
 else{dimensions=[4000,2550];canvas.setAttribute('viewBox','0 0 4000 2550');if(mode==='context')highlight(s);$('view-caption').textContent=mode==='context'?'Только '+s.primary.join(', ')+', связанные детали и ВСЕ провода этого элемента.':'Абсолютно вся схема: все детали и все соединения, без скрытия.'}
 $('relations').hidden=mode!=='context';layer.style.display=mode==='local'?'':'none';$('mode-local').setAttribute('aria-pressed',String(mode==='local'));$('mode-context').setAttribute('aria-pressed',String(mode==='context'));$('mode-all').setAttribute('aria-pressed',String(mode==='all'));$('center-current').hidden=mode!=='context';$('diagram-note').textContent=mode==='local'?'Красный кружок отмечает сам металл, к которому паять. Для резистора «1» — первый выбранный конец, «2» — другой. Маршрут провода условный.':'Положение деталей сохранено как на полной схеме. Точка — соединение; пересечение без точки не паять.';fit();
}
function render(){const s=steps[index],q=s.connection;$('section').textContent=s.section;$('step-count').textContent='Шаг '+(index+1)+' из '+steps.length;$('step-title').textContent=s.title;$('step-select').value=index;$('action').textContent=stepAction(s);
 $('ends').replaceChildren();if(q){for(const [i,id]of[q.a,q.b].entries()){let info=portInfo(id),card=document.createElement('div');card.className='end';let small=document.createElement('span');small.textContent='Место пайки '+(i+1);let b=document.createElement('strong');b.textContent=info.title;let hint=document.createElement('small');hint.textContent=info.hint;card.append(small,b,hint);$('ends').append(card)}}$('ends').hidden=!q;$('note').hidden=!q;$('note').textContent=q?s.note+(/^R\d+$/.test(s.primary[0])?' Фото корпуса общее: номинал по подписи, проверь мультиметром.':''):'';$('done').checked=completed.has(s.id);$('completion').textContent='Отмечено: '+completed.size+' из '+steps.length;$('progress').value=completed.size;$('previous').disabled=index===0;$('next').disabled=index===steps.length-1;updateView();store()}
$('previous').onclick=()=>{if(index>0){index--;mode='local';render()}};$('next').onclick=()=>{if(index<steps.length-1){index++;mode='local';render()}};$('step-select').onchange=e=>{index=+e.target.value;mode='local';render()};$('done').onchange=e=>{if(e.target.checked)completed.add(steps[index].id);else completed.delete(steps[index].id);$('completion').textContent='Отмечено: '+completed.size+' из '+steps.length;$('progress').value=completed.size;store()};
for(const m of ['local','context','all'])$('mode-'+m).onclick=()=>{mode=m;updateView()};$('fit').onclick=fit;$('plus').onclick=()=>zoom(scale*1.3);$('minus').onclick=()=>zoom(scale/1.3);
$('center-current').onclick=()=>{const bb=steps[index].primary.map(bounds),minX=Math.min(...bb.map(b=>b[0]))-100,maxX=Math.max(...bb.map(b=>b[2]))+100,minY=Math.min(...bb.map(b=>b[1]))-100,maxY=Math.max(...bb.map(b=>b[3]))+100;zoom(Math.min(1.5,viewport.clientWidth/(maxX-minX),viewport.clientHeight/(maxY-minY)));viewport.scrollTo(Math.max(0,minX*scale),Math.max(0,minY*scale))};
// Clicking a wire in the overview selects the corresponding assembly step.
canvas.querySelectorAll('.wire').forEach(w=>w.addEventListener('click',()=>{const i=steps.findIndex(s=>s.connection?.edges.includes(w.id));if(i>=0){index=i;mode='local';render()}}));
function fullSource(){let c=canvas.cloneNode(true);c.querySelector('#local-stage-layer')?.remove();c.removeAttribute('style');c.classList.remove('local-view','context-view','has-selection');c.setAttribute('viewBox','0 0 4000 2550');c.setAttribute('width','4000');c.setAttribute('height','2550');c.querySelectorAll('.step-wire,.step-node,.step-part,.active,.part-active,.related-part,.related-label').forEach(n=>n.classList.remove('step-wire','step-node','step-part','active','part-active','related-part','related-label'));return new XMLSerializer().serializeToString(c)}
function download(blob,ext){let a=document.createElement('a');a.href=URL.createObjectURL(blob);a.download='IRFB4110_полная_схема.'+ext;a.click();setTimeout(()=>URL.revokeObjectURL(a.href),1000)}
$('download-svg').onclick=()=>download(new Blob([fullSource()],{type:'image/svg+xml;charset=utf-8'}),'svg');$('download-png').onclick=()=>{let u=URL.createObjectURL(new Blob([fullSource()],{type:'image/svg+xml;charset=utf-8'})),im=new Image;im.onload=()=>{let c=document.createElement('canvas');c.width=4000;c.height=2550;let ctx=c.getContext('2d');ctx.fillStyle='white';ctx.fillRect(0,0,4000,2550);ctx.drawImage(im,0,0);c.toBlob(b=>download(b,'png'));URL.revokeObjectURL(u)};im.src=u};
const fullLink=document.createElement('a');fullLink.id='open-full';fullLink.href='IRFB4110_общая_схема_пайки.svg';fullLink.target='_blank';fullLink.rel='noopener';fullLink.textContent='Открыть всю схему отдельно ↗';document.querySelector('.view-controls').append(fullLink);
const relations=document.createElement('details');relations.id='relations';relations.hidden=true;relations.innerHTML='<summary id="relations-title"></summary><ul id="relations-list"></ul>';document.querySelector('.view-controls').before(relations);$('mode-context').textContent='2. Все связи этого элемента';
let resizing;addEventListener('resize',()=>{clearTimeout(resizing);resizing=setTimeout(fit,100)});render();
