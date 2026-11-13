const fs=require('node:fs'),path=require('node:path');
const dir=__dirname,root=path.dirname(dir);
const W=4000,H=2550;
const col={BATT:'#c52225',FUSED:'#c52225',BP:'#c52225',D:'#92501b',GND:'#172a38',V12:'#bb5304',VPRE:'#b05c0b',AUX:'#c86b2c',D1OUT:'#b05c0b',OUTA:'#185bbd',OUTB:'#00849b',IN:'#7b34b7',ESP:'#7b34b7',EN:'#12805c',T1OUT:'#12805c',G1:'#185bbd',G2:'#185bbd',G3:'#00849b',G4:'#00849b'};
const asset={mos:['irfb4110.jpg',4000,4000],u1:['so8b.jpg',480,480],reg:['l7812-front.jpg',600,600],res:['resistor.jpg',480,480],zen:['zener-glass.jpg',480,480],d1:['1n4007.jpg',350,350],tv2:['tvs24.jpg',800,800],tv1:['sm8s33a-back.jpg',900,900],mbr:['mbr60100ct.jpg',800,800],t1:['thermostat.jpg',480,480],esp:['../H2_board_pinout.png',4480,4844]};
const used=new Set(),parts=[],nodes={},edges=[];
let drawing=[],badges=[],labels=[];
const e=s=>String(s).replaceAll('&','&amp;').replaceAll('<','&lt;').replaceAll('>','&gt;').replaceAll('"','&quot;');
const text=(x,y,s,cls='')=>`<text x="${x}" y="${y}" class="${cls}">${e(s)}</text>`;
function node(id,x,y,net,label,joint=false){if(nodes[id])throw Error('Duplicate node '+id);nodes[id]={id,x,y,net,label:label||id,joint};return id;}
function joint(id,x,y,net,label){return node(id,x,y,net,label,true);}
function edge(a,b,via=[],desc='',wide=0){let p=nodes[a],q=nodes[b];if(!p||!q)throw Error('Unknown '+a+' '+b);if(p.net!==q.net)throw Error('Net mismatch '+a+' '+b);const id='wire-'+(edges.length+1);edges.push({id,a,b,via,net:p.net,description:desc||`${p.label} → ${q.label}`,width:wide||(['BATT','FUSED','BP','D'].includes(p.net)?8:4)});return id;}
function image(key,x,y,w,h,view,transform=''){
 used.add(key);let a=asset[key];return `<svg x="${x}" y="${y}" width="${w}" height="${h}" viewBox="${view||`0 0 ${a[1]} ${a[2]}`}" overflow="hidden"><use href="#asset-${key}"${transform?` transform="${transform}"`:''}/></svg>`;
}
function component(id,name,html,ports,xy){parts.push({id,name,ports,xy});drawing.push(`<g data-part="${id}">${html}</g>`);}
function badge(id,label,dx=0,dy=0){let p=nodes[id],r=/^(U2|D3|D4)\./.test(id)?10:16;badges.push(`<g class="pin" data-node="${id}"><circle cx="${p.x}" cy="${p.y}" r="${r}" fill="${col[p.net]}" stroke="white" stroke-width="2"/><text x="${p.x+dx}" y="${p.y+(r===10?5:6)+dy}" class="pintext" style="font-size:${r===10?15:21}px">${e(label)}</text></g>`);}
function resistor(id,x,y,w,value,lnet,rnet){
 const a=node(id+'.1',x-10,y,lnet,`${id} ${value}: левый конец`),b=node(id+'.2',x+w+10,y,rnet,`${id} ${value}: правый конец`);
 component(id,`${id} · ${value}`,image('res',x,y-83.35*w/590,w,w*200/590,'-55 140 590 200','rotate(33.7 240 240)')+`<path d="M${x-10} ${y}h24m${w-28} 0h24" stroke="#65717c" stroke-width="2"/>`,[a,b],[x,y]);
 labels.push(text(x+w/2,y+(/^R([1-4]|11)$/.test(id)?36:-34),`${id} · ${value}`,'reslabel'));
 return [a,b];
}
function axial(id,key,x,y,w,value,knet,anet,kleft=true){
 let view,rot;if(key==='zen'){view='-110 160 700 160';rot=`rotate(${kleft?225:45} 240 240)`}else if(key==='d1'){view='-45 105 440 140';rot=`rotate(${kleft?225:45} 175 175)`}else{view='70 310 660 180';rot=`rotate(${kleft?30:210} 400 400)`}
 let left=node(id+(kleft?'.K':'.A'),x,y,kleft?knet:anet,`${id} ${value}: ${kleft?'конец с полоской K':'конец без полоски A'}`),right=node(id+(kleft?'.A':'.K'),x+w,y,kleft?anet:knet,`${id} ${value}: ${kleft?'конец без полоски A':'конец с полоской K'}`);
 component(id,`${id} · ${value}`,image(key,x,y-40,w,80,view,rot)+`<path d="M${x} ${y}h24m${w-48} 0h24" stroke="#65717c" stroke-width="2"/>`,[left,right],[x,y]);
 labels.push(text(x+w/2,y-46,`${id} · ${value}`,'reslabel')+text(kleft?x:x+w,y+40,'K · полоска','small center'));
 return [id+'.K',id+'.A'];
}
function capacitor(id,x,y,value,positive,negative='GND',polar=true){
 const a=node(id+'.+',x-13,y+56,positive,`${id} ${value}: ${polar?'плюс':'первый вывод'}`),b=node(id+'.-',x+13,y+56,negative,`${id} ${value}: ${polar?'минус, полоса «−»':'второй вывод'}`);
 let body=polar?`<ellipse cx="${x}" cy="${y-30}" rx="27" ry="9" fill="#78858e"/><path d="M${x-27} ${y-30}v63q27 13 54 0v-63" fill="#243d50"/><path d="M${x+8} ${y-27}v63h14v-63" fill="#d5dce2"/>${text(x+11,y+4,'−','small')}`:`<rect x="${x-23}" y="${y-25}" width="46" height="46" rx="7" fill="#e1ae62" stroke="#8c6228"/>`;
 let start=polar?33:21;body+=`<path d="M${x-13} ${y+start}v${56-start}m26 ${-(56-start)}v${56-start}" stroke="#65717c" stroke-width="3"/>`;
 if(polar)body+=text(x-27,y+65,'+','small');
 const vv=value.split('/').map(s=>s.trim());
 component(id,`${id} · ${value}`,body,[a,b],[x,y]);labels.push(text(x,y-90,id,'reslabel')+`<text x="${x}" y="${y-64}" style="font-size:18px;text-anchor:middle">${e(vv[0])}</text>`+`<text x="${x}" y="${y-42}" style="font-size:18px;text-anchor:middle">${e(vv[1]||'')}</text>`);return[a,b];
}
function block(id,x,y,w,h,title,sub,ports){
 let pp=ports.map(([n,dx,dy,net,label])=>node(id+'.'+n,x+dx,y+dy,net,label||id+' '+n));
 component(id,title,`<rect x="${x}" y="${y}" width="${w}" height="${h}" rx="14" fill="#f2f6f8" stroke="#6a8598" stroke-width="2"/>${text(x+20,y+45,title,'label')}${text(x+20,y+83,sub,'small')}`,pp,[x,y]);return pp;
}
const rotate=(p,c,a)=>{a*=Math.PI/180;const dx=p[0]-c[0],dy=p[1]-c[1];return[c[0]+Math.cos(a)*dx-Math.sin(a)*dy,c[1]+Math.sin(a)*dx+Math.cos(a)*dy]};

// Main current path. Only these thick conductors carry the electrolyzer current.
block('АКБ',45,155,315,245,'АКБ 24 В Li-ion','Выход после BMS', [['P+',315,65,'BATT','АКБ: плюс P+ после BMS'],['P-',315,175,'GND','АКБ: минус P− после BMS']]);
block('F1',500,195,150,50,'F1 · 80 А','', [['1',0,25,'BATT','F1: вход'],['2',150,25,'FUSED','F1: выход']]);
block('S1',760,195,150,50,'S1 · DC','', [['1',0,25,'FUSED','S1: вход'],['2',150,25,'BP','S1: выход']]);
block('НАГРУЗКА',3350,240,350,170,'ЭЛЕКТРОЛИЗЁР','Твои силовые клеммы', [['+',0,40,'BP','Электролизёр: плюс'],['-',350,110,'D','Электролизёр: минус']]);
joint('BPLUS',1100,220,'BP','Плюсовая шина B+ после F1/S1');joint('DMAIN',3780,2100,'D','Общая шина D: минус электролизёра');joint('SSTAR',1540,2300,'GND','Общая шина S / GND: минус АКБ');
joint('SLEFT',430,2300,'GND','Левый конец общей минусовой шины');joint('PGND',55,1080,'GND','Минус аккумулятора, ответвление питания');joint('SEND',3800,2300,'GND','Правый конец общей минусовой шины');
edge('АКБ.P+','F1.1',[],'Толстый провод: плюс АКБ → вход F1');edge('F1.2','S1.1');edge('S1.2','BPLUS');edge('BPLUS','НАГРУЗКА.+',[[3350,220]],'Толстый провод: B+ → плюс электролизёра');edge('НАГРУЗКА.-','DMAIN',[[3780,350]],'Толстый провод: минус электролизёра → общая шина D');
edge('АКБ.P-','PGND',[[430,330],[430,450],[55,450]],'Толстый провод: минус АКБ → общая шина S',9);edge('PGND','SLEFT',[[55,2300]],'Толстый провод минуса к шине S',9);edge('SLEFT','SSTAR',[],'Толстая медная шина S / GND',9);edge('SSTAR','SEND',[],'Толстая медная шина S / GND',9);

// Auxiliary 12 V regulator and its input/output parts.
block('F2',140,625,170,50,'F2 · 250 мА','', [['1',0,25,'BP','F2: вход от B+'],['2',170,25,'AUX','F2: выход']]);
axial('D1','d1',350,650,160,'1N4007','D1OUT','AUX',false);
resistor('R13',610,650,160,'100 Ом / 1 Вт','D1OUT','VPRE');
const rp=[[246,568],[291,568],[340,568]];const regPins=rp.map((p,i)=>node('U2.'+(i+1),850+p[0]/2,430+p[1]/2,['VPRE','GND','V12'][i],`U2 L7812: ножка ${i+1} (${['IN — вход','GND — земля','OUT — 12 В'][i]})`));
component('U2','L7812CV',image('reg',850,430,300,300),regPins,[850,430]);regPins.forEach((p,i)=>badge(p,String(i+1)));
joint('VPRE',800,714,'VPRE','Узел VPRE после R13');joint('VPRECAP',800,790,'VPRE','VPRE для C4/C5/TV2');joint('V12',1450,714,'V12','Выход стабилизатора +12 В');joint('AUXGND',430,1080,'GND','Общая земля цепи L7812');
edge('BPLUS','F2.1',[[1100,430],[140,430]],'B+ → F2: маломощное питание драйвера');edge('F2.2','D1.A');edge('D1.K','R13.1');edge('R13.2','VPRE',[[800,650]]);edge('VPRE','U2.1');edge('U2.3','V12');edge('U2.2','AUXGND',[[995.5,1080]]);edge('AUXGND','PGND',[],'Земля вспомогательного питания → минус аккумулятора');
axial('D2','d1',1110,390,210,'1N4007','VPRE','V12',true);edge('D2.K','VPRE',[[800,390]]);edge('D2.A','V12',[[1450,390]]);edge('VPRE','VPRECAP');
joint('VPREC4',545,790,'VPRE');joint('VPREC5',725,790,'VPRE');joint('VPRETV2',250,790,'VPRE');edge('VPRECAP','VPREC5');edge('VPREC5','VPREC4');edge('VPREC4','VPRETV2');
capacitor('C4',600,870,'10 мкФ / 50 В','VPRE');capacitor('C5',780,870,'330 нФ / 50 В','VPRE','GND',false);edge('C4.+','VPREC4',[[545,926]]);edge('C5.+','VPREC5',[[725,926]]);edge('C4.-','AUXGND',[[655,926],[655,1080]]);edge('C5.-','AUXGND',[[835,926],[835,1080]]);
axial('TV2','tv2',250,910,220,'1.5KE24A','VPRE','GND',true);edge('TV2.K','VPRETV2');edge('TV2.A','AUXGND',[[500,910],[500,1080]]);
capacitor('C6',1200,875,'10 мкФ / 25 В','V12');capacitor('C7',1390,875,'100 нФ / 50 В','V12','GND',false);edge('C6.+','V12',[[1145,931],[1145,790],[1450,790]]);edge('C7.+','V12',[[1335,931],[1335,790],[1450,790]]);edge('C6.-','AUXGND',[[1255,931],[1255,1080]]);edge('C7.-','AUXGND',[[1445,931],[1445,1080]]);

// Main bus capacitors. The earth return is a wire, not a net-label shortcut.
joint('BPCAPS',1600,220,'BP');edge('BPLUS','BPCAPS');joint('MAINCAPGND',2100,700,'GND');edge('MAINCAPGND','PGND',[[2100,760],[80,760],[80,1080]]);
[['C1',1600,'1000 мкФ / 63 В',true],['C2',1810,'1000 мкФ / 63 В',true],['C3',2020,'1 мкФ / 100 В',false]].forEach(([id,x,v,pol])=>{capacitor(id,x,500,v,'BP','GND',pol);edge(id+'.+','BPCAPS',[[x-55,556],[x-55,280],[1600,280]]);edge(id+'.-','MAINCAPGND',[[x+55,556],[x+55,700]])});

// Two flyback dual diodes, all six physical legs wired.
joint('DFLY',3780,700,'D');edge('DFLY','DMAIN');
for(let i=0;i<2;i++){
 const id='D'+(i+3),x=2470+i*420,y=350,width=180,sc=width/400;
 const pp=[[579,590],[620,550],[657,512]].map(p=>rotate(p,[400,400],45)).map(p=>[x+(p[0]-230)*sc,y+(p[1]-100)*sc]);
 const ports=pp.map((p,j)=>node(id+'.'+(j+1),...p,j===1?'BP':'D',`${id} MBR60100CT: ножка ${j+1}`));component(id,'MBR60100CT',image('mbr',x,y,width,270,'230 100 400 600','rotate(45 400 400)'),ports,[x,y]);ports.forEach((p,j)=>badge(p,String(j+1)));
 edge(id+'.1','DFLY',[[pp[0][0],700]]);edge(id+'.3','DFLY',[[pp[2][0],700]]);edge(id+'.2','BPCAPS',[[pp[1][0],640],[x+270,640],[x+270,220]]);
 labels.push(text(x+90,328,`${id} · MBR60100CT`,'label center'));
}

// ESP32 and UCC27524 with all eight pins individually anchored to the photo.
const ep=node('ESP.32',105.8,1490.1,'ESP','ESP32: контакт с надписью 32 / GPIO32'),ez=node('ESP.GND',105.8,1668.4,'GND','ESP32: контакт GND');
component('ESP','ESP32',image('esp',80,1240,300,650,'1380 1150 1700 3350'),[ep,ez],[80,1240]);badge(ep,'32');badge(ez,'−');
const up={1:[33,274],2:[85,319],3:[138,369],4:[193,405],5:[444,224],6:[387,183],7:[334,140],8:[275,100]},un={1:'EN',2:'IN',3:'GND',4:'IN',5:'OUTB',6:'V12',7:'OUTA',8:'EN'};
const uports=Object.entries(up).map(([n,p])=>node('U1.'+n,900+p[0]*450/480,1200+p[1]*450/480,un[n],`U1 UCC27524: ножка ${n} (${({1:'ENA',2:'INA',3:'GND',4:'INB',5:'OUTB',6:'12 В VDD',7:'OUTA',8:'ENB'})[n]})`));
component('U1','UCC27524DR',image('u1',900,1200,450,450),uports,[900,1200]);uports.forEach(p=>badge(p,p.split('.')[1]));
resistor('R9',500,1520,160,'100 Ом','ESP','IN');joint('INPUT',730,1520,'IN','Вход IN: ножки 2/4 после R9');edge('ESP.32','R9.1',[[450,1490.1],[450,1520]]);edge('R9.2','INPUT');edge('INPUT','U1.2',[[870,1520],[870,nodes['U1.2'].y]]);edge('INPUT','U1.4',[[840,1520],[840,1640],[nodes['U1.4'].x,1640]]);
resistor('R10',540,1770,180,'10 кОм','GND','IN');edge('R10.2','INPUT',[[730,1770]]);joint('SIGNALGND',460,2370,'GND','Отдельный тонкий провод земли управления');edge('R10.1','SIGNALGND',[[460,1770]]);edge('ESP.GND','SIGNALGND',[[400,1668.4],[400,2370]]);edge('SIGNALGND','SSTAR',[[1540,2370]],'Отдельная земля ESP32 → общая точка S',4);
joint('DRVGND',1540,1940,'GND','Земля драйвера и C8–C10');edge('U1.3','DRVGND',[[nodes['U1.3'].x,1940]]);edge('DRVGND','SSTAR');
joint('VDDDRV',1450,1150,'V12','Питание драйвера 12 В');edge('V12','VDDDRV');edge('VDDDRV','U1.6',[[nodes['U1.6'].x,1150]]);
joint('VDDCAPS',1450,1680,'V12','Плюс C8–C10');edge('VDDDRV','VDDCAPS');
[['C8',1120,'47 мкФ / 25 В',true],['C9',1260,'1 мкФ / 25 В',false],['C10',1385,'100 нФ / 50 В',false]].forEach(([id,x,v,pol])=>{capacitor(id,x,1790,v,'V12','GND',pol);edge(id+'.+','VDDCAPS',[[x-55,1846],[x-55,1680]]);edge(id+'.-','DRVGND',[[x+55,1846],[x+55,1940]])});

// Thermostat and EN divider, drawn continuously back to both EN pins.
let t1a=node('T1.1',589.75,1939.6,'V12','T1 NC 70 °C: первый провод (любой)'),t1b=node('T1.2',553.7,1954.3,'T1OUT','T1 NC 70 °C: второй провод');
component('T1','NC 70 °C',image('t1',550,1930,180,180),[t1a,t1b],[550,1930]);resistor('R11',820,2050,180,'2,2 кОм','T1OUT','EN');resistor('R12',1130,2180,160,'1 кОм','EN','GND');joint('ENABLE',1050,2050,'EN','Узел EN после R11');
edge('VDDCAPS','T1.1',[[1450,1900],[589.75,1900]]);edge('T1.2','R11.1',[[480,1954.3],[480,2260],[780,2260],[780,2050]]);edge('R11.2','ENABLE');edge('ENABLE','R12.1',[[1050,2180]]);edge('R12.2','SSTAR',[[1300,2180],[1300,2300]]);edge('ENABLE','U1.1',[[1050,2010],[760,2010],[760,nodes['U1.1'].y]]);edge('ENABLE','U1.8',[[1500,2050],[1500,1100],[nodes['U1.8'].x,1100]]);

// Gate distribution. Separate colors keep OUTA and OUTB distinct at crossings.
joint('OUTA.BUS',2165,900,'OUTA','Разветвление OUTA к R1/R2');joint('OUTA.Q1',1715,900,'OUTA');joint('OUTB.BUS',3065,820,'OUTB','Разветвление OUTB к R3/R4');joint('OUTB.Q3',2615,820,'OUTB');
edge('U1.7','OUTA.Q1',[[nodes['U1.7'].x,900]]);edge('OUTA.Q1','OUTA.BUS');edge('U1.5','OUTB.Q3',[[1410,nodes['U1.5'].y],[1410,820]]);edge('OUTB.Q3','OUTB.BUS');
joint('DLEFT',1650,2100,'D');edge('DLEFT','DMAIN',[],'Общая толстая шина D: все средние ножки MOSFET',9);
for(let i=0;i<4;i++){
 const base=1800+450*i,x=base+115,y=1080,width=220,id='Q'+(i+1),gn='G'+(i+1),on=i<2?'OUTA':'OUTB';
 const pp=[[550,2900],[775,3100],[1000,3295]].map(p=>rotate(p,[2000,2000],-45)).map(p=>[x+(p[0]-1180)*width/1550,y+(p[1]-450)*width/1550]);
 const ports=pp.map((p,j)=>node(id+'.'+(j+1),...p,[gn,'D','GND'][j],`${id} IRFB4110: ножка ${j+1} (${['G — левая','D — средняя','S — правая'][j]})`));
 component(id,'IRFB4110PBF',image('mos',x,y,width,width*3350/1550,'1180 450 1550 3350','rotate(-45 2000 2000)'),ports,[x,y]);ports.forEach((p,j)=>badge(p,String(j+1)));
 const g=joint(id+'.GATE',base-10,1730,gn,`${id}: узел G после своего резистора 10 Ом`),gb=joint(id+'.GLOW',base-10,1940,gn),st=joint(id+'.SOURCE',base+150,1620,'GND',`${id}: ветвь к ножке 3 S`),sb=joint(id+'.SLOW',base+150,1940,'GND'),dp=joint(id+'.DRAIN',pp[1][0],2100,'D'),sp=joint(id+'.SRETURN',base+150,2300,'GND');
 resistor('R'+(i+1),base-190,1730,160,'10 Ом',on,gn);resistor('R'+(i+5),base+5,1840,125,'10 кОм',gn,'GND');axial('Z'+(i+1),'zen',base-5,1940,155,'BZX55C15',gn,'GND',true);
 edge('R'+(i+1)+'.2',g);edge(ports[0],g,[[base-10,pp[0][1]]]);edge(g,gb);edge(g,'R'+(i+5)+'.1',[[base-10,1840]]);edge('R'+(i+5)+'.2',sb,[[base+150,1840]]);edge('Z'+(i+1)+'.K',gb);edge('Z'+(i+1)+'.A',sb);edge(ports[2],st,[[pp[2][0],1620]]);edge(st,sb);edge(sb,sp);edge(sp,'SSTAR');edge(ports[1],dp);edge(dp,'DLEFT');
 const outBus=i===0?'OUTA.Q1':i===1?'OUTA.BUS':i===2?'OUTB.Q3':'OUTB.BUS';edge(outBus,'R'+(i+1)+'.1',[[base-85,1660],[base-200,1660]]);
 labels.push(text(x+110,1040,id+' · IRFB4110PBF','label center')+text(base+65,2015,'Z: полоска к G','small center'));
}

// Clamp: exact DOWO backside photo, large pad anode and the single tongue cathode.
const tvk=node('TV1.K',3500+650*220/900,1900+220*220/900,'D','TV1 DOWO SM8S33A: маленький контакт K'),tva=node('TV1.A',3500+450*220/900,1900+380*220/900,'GND','TV1 DOWO SM8S33A: большая нижняя площадка A');
component('TV1','DOWO SM8S33A',image('tv1',3500,1900,220,220),[tvk,tva],[3500,1900]);edge(tvk,'DMAIN',[[3760,nodes[tvk].y],[3760,2100]]);edge(tva,'SEND',[[nodes[tva].x,2300]]);badge(tvk,'K');badge(tva,'A');labels.push(text(3610,1875,'TV1 · SM8S33A','label center')+text(3610,2150,'Фото снизу · DOWO','small center'));

// Validate topology against the intended circuit, independent of drawn crossings.
const adj=Object.fromEntries(Object.keys(nodes).map(k=>[k,[]]));for(const x of edges){adj[x.a].push(x.b);adj[x.b].push(x.a)};
const netGroups={};for(const n of Object.values(nodes))(netGroups[n.net]??=[]).push(n.id);
for(const [net,ids]of Object.entries(netGroups)){let seen=new Set([ids[0]]),todo=[ids[0]];while(todo.length){for(const q of adj[todo.pop()])if(!seen.has(q)){seen.add(q);todo.push(q)}}if(ids.some(q=>!seen.has(q)))throw Error('Disconnected '+net+': '+ids.filter(q=>!seen.has(q)).join(','))}
const inventory={Q:4,R:13,Z:4,C:10,D:4,TV:2,U:2,T:1,F:2};for(const [family,n]of Object.entries(inventory)){let re=new RegExp('^'+family+'\\d+$');let count=parts.filter(p=>re.test(p.id)).length;if(count!==n)throw Error(`Inventory ${family}: ${count} != ${n}`)};

const defs=[...used].map(key=>{let a=asset[key],mime=a[0].endsWith('.png')?'image/png':'image/jpeg';return `<image id="asset-${key}" width="${a[1]}" height="${a[2]}" href="data:${mime};base64,${fs.readFileSync(path.join(dir,a[0])).toString('base64')}"/>`}).join('');
const baseStyle=`text{font:24px Arial,sans-serif;fill:#183342}.title{font-size:48px;font-weight:700}.label{font-size:28px;font-weight:700}.small{font-size:22px}.tiny{font-size:19px}.center{text-anchor:middle}.reslabel{font-size:24px;font-weight:700;text-anchor:middle}.pintext{font-size:21px;fill:white;text-anchor:middle;font-weight:700}.wire-hit{stroke:transparent;stroke-width:24;fill:none;cursor:pointer}.wire-line{fill:none;stroke-linecap:round;stroke-linejoin:round;transition:opacity .1s}.wire-halo{fill:none;stroke:white;stroke-linecap:round;stroke-linejoin:round}.wire.active .wire-line{filter:drop-shadow(0 0 5px #f6b908);stroke-width:9px}.has-selection .wire:not(.active){opacity:.1}.has-selection .pin:not(.active){opacity:.32}.part-active{filter:drop-shadow(0 0 9px #f6b908)}`;
const wires=edges.map(x=>{let a=nodes[x.a],b=nodes[x.b],pts=[[a.x,a.y],...x.via,[b.x,b.y]].map(p=>p.join(',')).join(' ');return `<g class="wire" id="${x.id}" data-a="${x.a}" data-b="${x.b}"><title>${e(x.description)}</title><polyline class="wire-halo" points="${pts}" stroke-width="${x.width+7}"/><polyline class="wire-line" points="${pts}" stroke="${col[x.net]}" stroke-width="${x.width}"/><polyline class="wire-hit" points="${pts}"/></g>`}).join('');
const dots=Object.values(nodes).filter(n=>n.joint).map(n=>`<circle class="joint" data-node="${n.id}" cx="${n.x}" cy="${n.y}" r="6" fill="${col[n.net]}" stroke="white" stroke-width="2"/>`).join('');
const title=text(45,65,'ОБЩАЯ МОНТАЖНАЯ СХЕМА · 4 × IRFB4110PBF + ESP32','title')+text(45,110,'Все детали соединены проводами. Смотри на надписи корпусов. Точка на пересечении = пайка; пересечение без точки = не соединять.','small');
const notes=text(60,2440,'Синий / бирюзовый: управление затворами. Коричневый: D. Чёрный: S / GND. Красный: АКБ+. Оранжевый: питание драйвера.','small')+text(60,2485,'Длины проводов условные. Резисторы G–S и керамику C9/C10 ставить у ножек. Радиатор = D; изолировать от корпуса и GND.','small');
labels.push(text(80,1200,'ESP32 · взять только GPIO32 и GND','label')+text(980,1175,'U1 · UCC27524DR','label center')+text(980,1210,'Фото SOIC-8; паять через адаптер','small center')+text(1200,480,'U2 · L7812CV','label')+text(1200,520,'1 IN / 2 GND / 3 OUT','small')+text(520,2130,'T1 · NC 70 °C','label')+text(1650,2350,'S / GND · минус аккумулятора','label')+text(1650,2070,'D · общая шина','label'));
let svg=`<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${W} ${H}" width="${W}" height="${H}" role="img" aria-label="Общая схема пайки IRFB4110PBF"><title>Все соединения четырёх IRFB4110PBF с ESP32</title><desc>Интернет-фотографии деталей с непрерывными проводами до каждого вывода. Не разводка платы; длины условные.</desc><defs>${defs}<style>${baseStyle}</style></defs><rect width="${W}" height="${H}" fill="white"/>${title}${drawing.join('')}${wires}${dots}${badges.join('')}${labels.join('')}${notes}</svg>`;
fs.writeFileSync(path.join(root,'IRFB4110_общая_схема_пайки.svg'),svg);
// User-facing instructions connect real component leads, not internal routing joints.
const instructions=[];
function instruction(a,b){if(nodes[a].net!==nodes[b].net)throw Error('Instruction mismatch '+a+' '+b);let todo=[a],prev={[a]:null};while(todo.length&&!Object.hasOwn(prev,b)){let k=todo.shift();for(const q of adj[k])if(!Object.hasOwn(prev,q)){prev[q]=k;todo.push(q)}}let ids=[],v=b;while(prev[v]!==null){let p=prev[v];let found=edges.find(x=>(x.a===p&&x.b===v)||(x.a===v&&x.b===p));ids.unshift(found.id);v=p}instructions.push({id:'join-'+(instructions.length+1),a,b,edges:ids,description:nodes[a].label+' ↔ '+nodes[b].label});}
[['АКБ.P+','F1.1'],['F1.2','S1.1'],['S1.2','НАГРУЗКА.+'],['АКБ.P-','Q1.3'],['Q1.3','Q2.3'],['Q1.3','Q3.3'],['Q1.3','Q4.3'],['Q1.2','НАГРУЗКА.-'],['Q1.2','Q2.2'],['Q1.2','Q3.2'],['Q1.2','Q4.2'],['ESP.32','R9.1'],['R9.2','U1.2'],['U1.2','U1.4'],['U1.2','R10.2'],['R10.1','U1.3'],['ESP.GND','U1.3'],['U1.3','Q1.3'],['U2.3','U1.6'],['U2.2','U1.3'],['S1.2','F2.1'],['F2.2','D1.A'],['D1.K','R13.1'],['R13.2','U2.1'],['U2.1','TV2.K'],['TV2.A','U2.2'],['D2.K','U2.1'],['D2.A','U2.3'],['U1.6','T1.1'],['T1.2','R11.1'],['R11.2','U1.1'],['U1.1','U1.8'],['U1.1','R12.1'],['R12.2','U1.3'],['TV1.K','Q1.2'],['TV1.A','Q1.3']].forEach(p=>instruction(...p));
for(let i=1;i<=4;i++){instruction(i<3?'U1.7':'U1.5','R'+i+'.1');instruction('R'+i+'.2','Q'+i+'.1');instruction('Q'+i+'.1','R'+(i+4)+'.1');instruction('Q'+i+'.3','R'+(i+4)+'.2');instruction('Q'+i+'.1','Z'+i+'.K');instruction('Q'+i+'.3','Z'+i+'.A')}
for(let i=1;i<=10;i++){let p=i<4?'НАГРУЗКА.+':i<6?'U2.1':i<8?'U2.3':'U1.6',n=i<4?'Q1.3':i<8?'U2.2':'U1.3';instruction(p,'C'+i+'.+');instruction(n,'C'+i+'.-')}
for(let i=3;i<=4;i++){instruction('D'+i+'.1','D'+i+'.3');instruction('D'+i+'.1','Q1.2');instruction('D'+i+'.2','НАГРУЗКА.+')}
const payload={nodes,edges,parts,inventory,instructions};fs.writeFileSync(path.join(dir,'overall-connections.json'),JSON.stringify(payload,null,2));
console.log(JSON.stringify({parts:parts.length,nodes:Object.keys(nodes).length,wires:edges.length,nets:Object.keys(netGroups).length,inventory,allNetsConnected:true,svgBytes:Buffer.byteLength(svg)}));
// Keep the final UI in the current staged form after rebuilding the drawing.
require('./make-staged-guide.cjs');
