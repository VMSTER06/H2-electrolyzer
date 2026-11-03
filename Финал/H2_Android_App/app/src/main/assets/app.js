'use strict';
const byId=id=>document.getElementById(id);
let requestId=0,pending={},state=null,online=false,pollBusy=false,actionBusy=false,pollFails=0;
let fanMode=3,fanDirty=false,pressureDirty=false,soundOut=2,soundDirty=false,soundCfg=null,calDirty=false;
let timerDirty=false,gasDirty=false,maintDirty=false,logSeen=-1,timeSentFor=-1;
let toastTimer=0,lastAction=0,pollTimer=0,currentTab='home',previousTab='home',lastUpdate=0;
const scrollPositions={},safetyBusy={},safetyLast={};
const systemDark=matchMedia('(prefers-color-scheme: dark)');
let preferences={theme:'light',awake:false,haptics:true,refresh:2200,digits:2,debug:false};
try{Object.assign(preferences,JSON.parse(localStorage.getItem('h2.preferences')||'{}'))}catch(e){}
if(!['light','dark','auto'].includes(preferences.theme))preferences.theme='light';
if(![1000,2200,5000].includes(preferences.refresh))preferences.refresh=2200;
if(![1,2].includes(preferences.digits))preferences.digits=2;
preferences.awake=preferences.awake===true;preferences.haptics=preferences.haptics===true;

// v1.3.0: мост — один метод Brain.request(id, method, path, type, bodyBase64); белый список путей и диапазоны — в Java.
// Таймаут JS больше, чем очередь + соединение + чтение в Java: опрос 12 с (4 + 2,5 + 4,5), команды 8 с (1 + 2,5 + 3).
const NO_ANSWER='Мозг не ответил. Проверь Wi-Fi H2_PRO и повтори';
function bridge(timeout,start,onProgress){return new Promise((resolve,reject)=>{const id=++requestId;const timer=timeout?setTimeout(()=>{delete pending[id];reject(new Error(NO_ANSWER))},timeout):0;pending[id]={resolve,reject,timer,onProgress};try{start(id)}catch(e){clearTimeout(timer);delete pending[id];reject(new Error('Подключи телефон к Wi-Fi H2_PRO'))}})}
const READ_PATH=/^\/(status|log|snd\/cfg|snd\/get|rmel\/cfg|settime)(\?|$)/;
function brain(path,o){o=o||{};if(/^\/cmd\?id=\d+$/.test(path))path+='&src=app';return bridge(o.timeout!==undefined?o.timeout:READ_PATH.test(path)?12000:8000,id=>Brain.request(id,o.method||'GET',path,o.type||'',o.body||''),o.progress)}
// v1.3.0: ошибка говорит, что случилось и что делать. Русские ответы Java показываем как есть,
// служебные коды мозга («bad vol», «Not found», HTML 403) — переводим по коду ответа.
function errText(code,body){body=String(body||'');if(code===503&&body==='busy')return 'Мозг занят. Повтори через секунду';if(/[А-Яа-яЁё]/.test(body)&&body.length<200&&!/</.test(body))return body;if(code===400)return 'Мозг не принял значение. Обнови экран и повтори';if(code===404)return body==='no melody'?'Мелодии нет в мозге. Обнови список':'Мозг не знает эту команду. Обнови прошивку мозга';if(code===403)return 'Мозг отказал: выключи систему и дождись конца продувки';return code>0?'Ошибка мозга (код '+code+'). Повтори':NO_ANSWER}
window.h2Response=function(id,code,body){const p=pending[id];if(!p)return;clearTimeout(p.timer);delete pending[id];if(code>=200&&code<300){p.resolve(body);return}const e=new Error(errText(code,body));e.code=code;p.reject(e)};
window.h2Progress=function(id,sent,total){const p=pending[id];if(p&&p.onProgress)p.onProgress(sent,total)};
// Пишем только изменения: живые области (role=alert/status) не объявляются заново на каждом опросе
function text(id,value){const e=byId(id);if(e.textContent!==value)e.textContent=value}
// v1.3.0: ошибка не исчезает по таймеру — держится до касания или до следующего сообщения
function toast(message,error=false){const el=byId('toast');byId('toastText').textContent=message;el.className='toast show'+(error?' error':'');byId('toastClose').hidden=!error;clearTimeout(toastTimer);if(!error)toastTimer=setTimeout(hideToast,3200)}
function hideToast(){clearTimeout(toastTimer);const el=byId('toast');el.className='toast';byId('toastClose').hidden=true;if(el.contains(document.activeElement))document.activeElement.blur()}
byId('toast').addEventListener('click',()=>{if(byId('toast').classList.contains('error'))hideToast()});
function haptic(){try{if(preferences.haptics&&window.Brain&&Brain.haptic)Brain.haptic()}catch(e){}}
// v1.3.0: своё окно подтверждения (confirm() в WebView без WebChromeClient молча возвращал false).
// Нативный <dialog> + showModal(): фон недоступен, Escape — «Отмена», фокус возвращается на кнопку.
let sheetResolve=null,sheetReturn=null;
function ask(title,message,ok,danger){return new Promise(resolve=>{if(sheetResolve)sheetResolve(false);sheetResolve=resolve;text('sheetTitle',title);text('sheetText',message);const b=byId('sheetOk');b.textContent=ok||'Подтвердить';b.classList.toggle('danger',danger!==false);const s=byId('sheet');if(!s.open){sheetReturn=document.activeElement;if(typeof s.showModal==='function')s.showModal();else s.setAttribute('open','')}setTimeout(()=>byId('sheetCancel').focus(),50)})}
function closeSheet(value){const r=sheetResolve,s=byId('sheet');sheetResolve=null;if(s.open){if(typeof s.close==='function')s.close();else s.removeAttribute('open')}const back=sheetReturn;sheetReturn=null;if(back&&back.focus&&document.contains(back))try{back.focus({preventScroll:true})}catch(e){}if(r)r(value)}
byId('sheetOk').onclick=()=>closeSheet(true);byId('sheetCancel').onclick=()=>closeSheet(false);byId('sheet').addEventListener('cancel',e=>{e.preventDefault();closeSheet(false)});byId('sheet').addEventListener('click',e=>{const s=byId('sheet');if(e.target!==s)return;const r=s.getBoundingClientRect();if(e.clientX<r.left||e.clientX>r.right||e.clientY<r.top||e.clientY>r.bottom)closeSheet(false)});
function tab(name){if(!byId('page-'+name)||currentTab===name)return;parkedFocus=null;scrollPositions[currentTab]=window.scrollY;if(name==='settings')previousTab=currentTab;currentTab=name;document.querySelectorAll('.page').forEach(x=>x.classList.toggle('active',x.id==='page-'+name));document.querySelectorAll('.nav').forEach(x=>{const active=x.dataset.tab===name;x.classList.toggle('active',active);if(active)x.setAttribute('aria-current','page');else x.removeAttribute('aria-current')});byId('settingsButton').classList.toggle('selected',name==='settings');requestAnimationFrame(()=>window.scrollTo({top:scrollPositions[name]||0,behavior:'instant'}));if(name==='sound'&&online){loadSound();loadRemote(true)}if(name==='diag'&&online)loadLog();if(name==='diag')debugPaint();if(name==='history')requestAnimationFrame(drawCharts);if(name==='home')requestAnimationFrame(fitPressure)}
// v1.3.0: «Назад» на «Обзоре» закрывает приложение — во время записи прошивки не выходим (оборвалась бы загрузка)
window.handleBack=function(){if(byId('sheet').open){closeSheet(false);return true}if(currentTab==='settings'){tab(previousTab);return true}if(currentTab!=='home'){tab('home');return true}if(otaBusy){toast('Идёт обновление прошивки — дождись окончания',true);return true}return false};
function statusPill(id,ok,good='OK',bad='НЕТ'){const e=byId(id);e.textContent=ok?good:bad;e.className='diag-value '+(ok?'good':'bad')}
// Команды 3/4/5/7 мозг выполняет только при включённой, не заблокированной системе без продувки и наклона
function runBlocked(){const d=state||{};return !online||!d.pwr||!!d.prg||!!d.lck||!!d.tlk}
// v1.3.0: аварийные 99/2/12 (data-safety) не ждут других действий; 99 доступна и без связи — ответит Java
function updateControls(){const d=state||{},blocked=runBlocked();document.querySelectorAll('[data-control]').forEach(x=>{let off=!online||actionBusy;const run=x.dataset.run;if(run&&blocked&&!(run==='7'&&d.mgs))off=true;setOff(x,off)});document.querySelectorAll('[data-safety]').forEach(x=>{const c=Number(x.dataset.safety);setOff(x,!!safetyBusy[c]||(c!==99&&!online))});byId('runHint').hidden=!online||!blocked;otaPaint();paintDirty();restoreFocus()}
// v1.3.0: disabled сбрасывает фокус на body — клавиатура и TalkBack теряли место после каждой команды
// (и после окна подтверждения). Кнопку в фокусе запоминаем и возвращаем ей фокус, когда она снова доступна.
let parkedFocus=null;
function setOff(x,off){if(off&&!x.disabled&&document.activeElement===x)parkedFocus=x;x.disabled=off}
function restoreFocus(){const p=parkedFocus,a=document.activeElement;if(!p||p.disabled)return;parkedFocus=null;if((!a||a===document.body)&&document.contains(p)&&p.offsetParent!==null)try{p.focus({preventScroll:true})}catch(e){}}
// v1.3.0: кнопка сохранения залита, только когда в форме есть несохранённые изменения
function paintDirty(){[['pressureSave',pressureDirty],['fanSave',fanDirty],['timerSave',timerDirty],['gasSave',gasDirty],['maintSave',maintDirty],['soundSave',soundDirty],['calSave',calDirty]].forEach(x=>byId(x[0]).classList.toggle('secondary',!x[1]))}
// v1.3.0: пропущенный опрос — янтарная точка и «Нет ответа» (не зелёный «на связи»), показания не тускнеют
function setStale(value){document.body.classList.toggle('stale',value);const c=byId('connection');c.classList.toggle('online',online&&!value);c.classList.toggle('stale',online&&value);if(!online)return;text('connText',value?'Нет ответа, повтор…':'На связи');text('networkText',value?'192.168.4.1 · нет ответа, повтор…':'192.168.4.1 · на связи');if(value){text('stateBadge','Нет ответа');byId('stateBadge').className='state-badge'}}
function setOnline(value){const recovered=value&&!online;online=value;byId('connection').classList.toggle('online',value);text('connText',value?'На связи':lastUpdate||pollFails?'Нет связи':'Поиск сети…');byId('networkCard').classList.toggle('offline',!value);text('networkText',value?'192.168.4.1 · на связи':lastUpdate||pollFails?'192.168.4.1 · нет связи':'192.168.4.1 · поиск сети…');byId('networkDot').style.background=value?'var(--green)':'var(--muted)';text('outputsFresh',value?'Прямой эфир':'Нет данных');statusPill('dgBrain',value,'В СЕТИ','НЕТ СВЯЗИ');if(recovered&&currentTab==='sound')loadSound();if(!value){setStale(false);['pressure','targetHome','tempHome','tiltHome','radioHome','lockHome','runtimeHome','dgUptime','dgChannel'].forEach(id=>text(id,'—'));byId('pressure').classList.add('empty');byId('pressure').style.removeProperty('--fit');text('modulesState','—');text('stateBadge','Нет связи');byId('stateBadge').className='state-badge';byId('pressurePanel').classList.remove('warn','danger');byId('tempHome').classList.remove('hot');byId('tiltHome').className='';byId('processLine').hidden=true;byId('alarmNotice').hidden=true;byId('checkCard').hidden=true;showPlaying(false);byId('gaugeFill').style.setProperty('--progress','0%');byId('targetMarker').style.visibility='hidden';['dgDisplay','dgTemp','dgMpu','dgNrf','dgRemote','dgFs'].forEach(id=>{text(id,'—');byId(id).className='diag-value'});document.querySelectorAll('.output').forEach(el=>{el.classList.remove('on');el.querySelector('.output-state').textContent='—'});['cmdPurge','cmdGas','cmdRep','cmdPiezo'].forEach(id=>byId(id).classList.remove('active'));byId('powerButton').classList.remove('is-running','is-busy','is-quiet');byId('powerButton').removeAttribute('aria-disabled');byId('lockButton').classList.remove('active');byId('purgeButton').classList.remove('active');text('powerName','Включить систему');text('powerHint','Нет подключения');text('lockName','Блокировка');text('gasName','Ручной газ');calPaint()}updateControls()}
function fmtTime(s){s=Number(s)||0;const h=Math.floor(s/3600),m=Math.floor(s%3600/60);return h?`${h} ч ${m} м`:`${m} мин`}
function fmt(value,digits=1){return Number(value).toFixed(digits).replace('.',',')}
// «1 нота», «3 ноты», «12 нот»
function plural(n,one,few,many){n=Math.abs(n)%100;const k=n%10;return n>10&&n<20?many:k>1&&k<5?few:k===1?one:many}
function fmtBytes(b){b=Number(b)||0;return b>=1048576?fmt(b/1048576,2)+' МБ':Math.round(b/1024)+' КБ'}
function rangePaint(el){el.style.setProperty('--range',((Number(el.value)-Number(el.min))/(Number(el.max)-Number(el.min))*100)+'%')}
function rangeOutput(el,id,digits,unit){text(id,fmt(el.value,digits)+' '+unit);rangePaint(el)}
// v1.3.0: тревоги и предупреждения в порядке веб-интерфейса мозга; первая активная — в баннер «Обзора».
// badge — подпись плашки состояния (аварии — «Авария · …», предупреждения — «Внимание»), без badge — только баннер.
// В тексте баннера нет живых чисел: role=alert объявляет его только при смене тревоги, давление видно в панели.
// act — аварийное действие прямо в баннере (та же команда, что у кнопок ниже; защиты не меняются).
const ACT_UNLOCK={code:2,label:'Разблокировать',icon:'i-lock'},ACT_PURGE={code:99,label:'Сброс давления',icon:'i-gauge'};
const ALARMS=[
{k:'tlk',level:'danger',badge:'Авария · наклон',title:'Авария: наклон.',msg:()=>'Электролизёр и клапан обесточены. Выровняй установку и нажми «Разблокировать».',act:()=>ACT_UNLOCK},
// мозг снимает защёлку давления командой 2 только ниже 6,0 бар — до этого нужен сброс давления
{k:'ovl',level:'danger',badge:'Авария · давление',title:'Авария по давлению.',msg:d=>d.prg?'Идёт сброс давления до нуля.':Number(d.p)>=6?'Электролизёр выключен. Нажми «Сброс давления».':'Давление снижено. Нажми «Разблокировать», затем включи систему.',act:d=>d.prg?null:Number(d.p)>=6?ACT_PURGE:ACT_UNLOCK},
{k:'psf',level:'danger',badge:'Авария · датчик',title:'Датчик давления неисправен.',msg:()=>'Электролизёр заблокирован. Что проверить — на вкладке «Статус».'},
{k:'mpuf',level:'danger',badge:'Авария · датчик наклона',title:'Датчик наклона не отвечает.',msg:()=>'Электролизёр заблокирован до восстановления. Что проверить — на вкладке «Статус».'},
{k:'lkl',level:'danger',badge:'Авария · утечка',title:'Утечка.',msg:()=>'Система выключена. Проверь клапан и магистраль, затем нажми «Разблокировать».',act:()=>ACT_UNLOCK},
{k:'oh',level:'danger',badge:'Авария · перегрев',title:'Перегрев.',msg:()=>'Дай установке остыть.'},
{k:'ovw',level:'warn',badge:'Внимание',title:'Давление выше 6,8 бар.',msg:()=>'Авария — при 7,2 бар.'},
{k:'wd',level:'warn',badge:'Внимание',title:'Бак не набирает давление.',msg:()=>'Проверь электролизёр и герметичность.'},
{k:'lkw',level:'warn',title:'Возможна утечка.',msg:()=>'Давление падает при закрытом клапане.'},
{k:'mntd',level:'warn',title:'Пора обслужить.',msg:()=>'Долей воду или электролит, затем нажми «Обслужено» (Управление → Обслуживание).'},
{k:'relw',level:'warn',title:'Ресурс реле на исходе.',msg:()=>'Проверь контакты (Статус → Ресурс).'},
{k:'tme',level:'warn',title:'Система выключена таймером.',msg:()=>'',on:d=>d.tme&&!d.pwr}];
function alarmOn(a,d){return a.on?!!a.on(d):!!d[a.k]}
function renderTilt(d){const e=byId('tiltHome');let v='—',c='';if(d.ta!==undefined){if(d.tcal)v='Калибровка…';else if(d.mpuf){v='Нет ответа';c='danger'}else if(d.mpuok!==undefined&&!d.mpuok){v='Нет датчика';c='warn'}else if(d.tlk){v=d.ta+'° · авария';c='danger'}else{v=d.ta+'°';c=d.ta>=20?'warn':''}}e.textContent=v;e.className=c}
function render(d){if(!d||!Number.isFinite(Number(d.p))||!Number.isFinite(Number(d.tgt)))throw new Error('Не удалось прочитать показания');state=d;pollFails=0;lastUpdate=Date.now();renderRemoteInfo(d);renderDebugState(d);setOnline(true);setStale(false);const p=Math.max(0,Number(d.p)),t=Number(d.tgt);byId('gaugeFill').style.setProperty('--progress',Math.min(100,p/6.5*100)+'%');byId('targetMarker').style.setProperty('--target',Math.min(100,Math.max(0,t/6.5*100))+'%');byId('targetMarker').style.visibility='visible';const panel=byId('pressurePanel');panel.classList.toggle('danger',!!d.ovl);panel.classList.toggle('warn',!!d.ovw&&!d.ovl);byId('pressure').classList.remove('empty');text('pressure',fmt(p,preferences.digits));fitPressure();text('targetHome',fmt(t)+' бар');const temp=byId('tempHome');temp.textContent=d.tok?fmt(d.t)+' °C':'Нет данных';temp.classList.toggle('hot',!!d.oh);renderTilt(d);text('lockHome',d.lck?'Вкл':'Выкл');text('radioHome',d.rf?'На связи':'Нет связи');text('runtimeHome',fmtTime(d.rt));
const first=ALARMS.find(a=>a.badge&&alarmOn(a,d)),alarm=!!first&&first.level==='danger',warnOnly=!!first&&!alarm;const badge=byId('stateBadge');badge.textContent=alarm?first.badge:d.prg?'Продувка':warnOnly?first.badge:d.lck?'Блокировка':d.pwr?'Работает':'Ожидание';badge.className='state-badge'+(alarm?' alarm':warnOnly&&!d.prg?' warn':d.pwr&&!d.lck?' live':'');
// v1.3.0: во время продувки кнопка питания приглушена (команда всё равно не пройдёт — sendCmd подскажет),
// при аварии — вторична: самое заметное на экране — действие в баннере. Логика команд не меняется.
const busy=!!d.prg,pb=byId('powerButton');pb.classList.toggle('is-running',!!d.pwr&&!busy);pb.classList.toggle('is-busy',busy);pb.classList.toggle('is-quiet',alarm&&!busy);if(busy)pb.setAttribute('aria-disabled','true');else pb.removeAttribute('aria-disabled');text('powerName',busy?'Идёт продувка':d.pwr?'Выключить систему':'Включить систему');text('powerHint',busy?'Дождись конца продувки':d.pwr?'Остановить установку':'Запуск установки');
// команда 2 снимает любую защёлку (блокировка, наклон, давление, утечка) — подпись говорит, что кнопка сделает
const latched=!!(d.lck||d.tlk||d.ovl||d.lkl);text('lockName',latched?'Разблокировать':'Заблокировать');byId('lockButton').classList.toggle('active',latched);byId('purgeButton').classList.toggle('active',busy);text('gasName',d.mgs?'Остановить газ':'Ручной газ');
['Electro','Valve','Piezo','Fan'].forEach((x,i)=>{const on=!!d[['ele','val','pzo','f2'][i]];byId('out'+x).classList.toggle('on',on);byId('out'+x).querySelector('.output-state').textContent=on?'ВКЛ':'ВЫКЛ'});
[['cmdPurge','psh'],['cmdGas','mgs'],['cmdRep','rep'],['cmdPiezo','pzo']].forEach(x=>byId(x[0]).classList.toggle('active',!!d[x[1]]));
// в строке процесса — только процессы; что делать при аварии, говорит баннер
const proc=d.prg?'Сброс давления до нуля — '+fmt(p,2)+' бар':d.psh?'Короткая продувка · клапан 200 мс':d.rep?'Репульсор — выстрел':'';const pl=byId('processLine');pl.textContent=proc;pl.hidden=!proc;
if(!pressureDirty&&document.activeElement!==byId('targetSlider')){byId('targetSlider').value=t;targetPaint()}
renderExtras(d);
if(!fanDirty){if(document.activeElement!==byId('fanOn'))byId('fanOn').value=d.f2on;if(document.activeElement!==byId('fanOff'))byId('fanOff').value=d.f2off;selectFan(Number(d.f2m),false)}
statusPill('dgDisplay',d.lcdok||d.oledok,d.lcdok?'LCD2004':'OLED','НЕ НАЙДЕН');statusPill('dgTemp',d.tok,'OK','НЕТ ДАННЫХ');statusPill('dgMpu',d.mpuok&&!d.mpuf,'OK',d.mpuf?'НЕТ ОТВЕТА':'НЕТ');statusPill('dgNrf',d.nrfok);statusPill('dgRemote',d.rf,'В СЕТИ','НЕТ СВЯЗИ');statusPill('dgFs',d.fsok);text('dgUptime',fmtTime(d.up));text('dgChannel',d.rfc!==undefined?'к'+d.rfc+' · '+(d.rfh||0):'—');
// v1.3.0: в норме плашку не показываем — «всё в порядке» видно по модулям
const dn=d.psf?['Датчик давления вне диапазона ('+fmt(d.pv,2)+' В): обрыв или замыкание. Электролизёр заблокирован. Проверь провод к GPIO36 и делитель.','danger']:d.lkl?['Сильная утечка: система выключена. Проверь клапан и магистраль, затем сними блокировку.','danger']:!d.tok?['Нет показаний DS18B20. Вентилятор может включаться в защитном режиме. Проверь DATA → GPIO15 и подтяжку 4,7 кОм к 3,3 В.','warn']:d.tlk?['Активна защита по наклону. Проверь положение установки.','danger']:d.mpuf?['MPU6050 перестал отвечать: угла наклона нет, электролизёр заблокирован до восстановления. Проверь шину I²C (SDA21/SCL22).','danger']:d.oh?['Температура выше допустимой. Дай установке остыть.','danger']:d.ovl?['Сработала защита по давлению. Проверь состояние установки.','danger']:d.tcal?['Идёт калибровка датчика наклона — не двигай установку.','']:null;showDiag(dn?dn[0]:'',dn?dn[1]:'');renderChecks(d);renderService(d);renderHistory(d);updateControls();updateLastSeen()}
function updateLastSeen(){text('lastUpdate',lastUpdate?(online?'Обновлено в ':'Последний ответ в ')+new Date(lastUpdate).toLocaleTimeString('ru-RU',{hour:'2-digit',minute:'2-digit',second:'2-digit'}):'Ожидание первого ответа мозга')}
// Плашка на «Статусе»: пустой текст — скрыть; level — '' (инфо), 'warn' или 'danger'
function showDiag(msg,level){const e=byId('diagnosticNotice');e.hidden=!msg;e.className='notice'+(level?' '+level:'');text('diagnosticNotice',msg)}
// Баннер «Обзора»: заголовок, пояснение, значок по уровню (форма значка различает аварию и предупреждение не только цветом)
function showBanner(level,title,msg,icon){const an=byId('alarmNotice');an.className='notice alarm-notice '+level;byId('alarmIcon').setAttribute('href','#'+icon);text('alarmTitle',title);text('alarmText',msg);an.hidden=false}
function alarmAction(){return sendCmd(Number(byId('alarmAct').dataset.safety))}
// Крупная цифра давления не должна вылезать за панель при системном шрифте 150–200 %: ужимаем через --fit
function fitPressure(){const el=byId('pressure'),box=el.parentElement;el.style.removeProperty('--fit');if(el.classList.contains('empty')||!box.offsetWidth)return;const avail=box.clientWidth-box.lastElementChild.offsetWidth-13,w=el.offsetWidth;if(w>avail&&avail>0)el.style.setProperty('--fit',Math.max(.4,avail/w).toFixed(3))}
// v1.3.0: «Нет связи» — после двух неудач подряд или когда данные старше 2 × интервал + 3 с;
// одиночный таймаут лишь помечает показания устаревшими и не сбрасывает экран
function staleLimit(){return 2*preferences.refresh+3000}
// v1.3.0: без связи — что случилось и что сделать: баннер с кнопкой «Открыть Wi-Fi» на «Обзоре» и плашка на «Статусе»
function goOffline(message){setOnline(false);const wifi=/^Подключи телефон/.test(message||'');showDiag(wifi?'Телефон не в сети H2_PRO. Подключи его к Wi-Fi H2_PRO — интернет для управления не нужен.':'Мозг не отвечает. Проверь, что телефон в Wi-Fi H2_PRO, а мозг включён.','warn');showBanner('info','Нет связи с H2_PRO.',wifi?'Подключи телефон к Wi-Fi устройства.':'Проверь Wi-Fi H2_PRO и питание мозга.','i-wifi');byId('alarmAct').hidden=true;byId('alarmMute').hidden=true;byId('alarmWifi').hidden=false;byId('alarmActions').hidden=false;updateLastSeen()}
function checkStale(){if(online&&Date.now()-lastUpdate>staleLimit())goOffline('Нет свежих данных от мозга')}
function pollFailed(e){pollFails++;if(online&&pollFails<2&&Date.now()-lastUpdate<=staleLimit()){setStale(true);showDiag('Мозг не ответил на последний запрос — повторяю…','warn');return}goOffline(e.message)}
async function poll(){checkStale();if(pollBusy||document.hidden)return;pollBusy=true;try{render(JSON.parse(await brain('/status')))}catch(e){pollFailed(e)}finally{pollBusy=false}}
// Свежее состояние сразу, мимо pollBusy: после ошибки команды видно, дошла ли она
async function refreshNow(){try{render(JSON.parse(await brain('/status')))}catch(e){}}
// v1.3.0: ошибка команды — тост сразу. Переключатели 1/2/7 держат кнопки, пока не придёт свежий статус
// (иначе повтор переключит дважды), но не дольше 2,5 с; остальные команды — просто внеочередной опрос.
const TOGGLE_CMD=/^\/cmd\?id=(1|2|7)$/;
function wait(ms){return new Promise(r=>setTimeout(r,ms))}
async function commandFailed(path,e){toast(e.message,true);if(TOGGLE_CMD.test(path))await Promise.race([refreshNow(),wait(2500)]);else setTimeout(poll,0)}
async function action(path,success,o){if(!online){toast('Нет связи с мозгом. Подключись к Wi-Fi H2_PRO',true);return false}if(actionBusy||Date.now()-lastAction<500)return false;lastAction=Date.now();actionBusy=true;updateControls();haptic();try{const r=await brain(path,o);if(success)toast(success);setTimeout(poll,250);return r||true}catch(e){await commandFailed(path,e);return false}finally{actionBusy=false;updateControls()}}
async function safetyCmd(code){if(code!==99&&!online){toast('Нет связи с мозгом. Подключись к Wi-Fi H2_PRO',true);return false}if(safetyBusy[code]||Date.now()-(safetyLast[code]||0)<500)return false;safetyLast[code]=Date.now();safetyBusy[code]=true;updateControls();haptic();try{await brain('/cmd?id='+code);toast(CMD_OK[code]);setTimeout(poll,250);return true}catch(e){await commandFailed('/cmd?id='+code,e);return false}finally{safetyBusy[code]=false;updateControls()}}
const CMD_OK={1:'Команда системе отправлена',12:'Тревога заглушена',2:'Блокировка переключена',3:'Импульс репульсора отправлен',4:'Команда поджига отправлена',5:'Команда продувки отправлена',7:'Подача газа переключена',99:'Команда сброса давления отправлена'};
function sendCmd(code){if(online&&state&&state.prg&&code!==2&&code!==12){toast('Идёт продувка. Сейчас работают только блокировка и «Заглушить» — дождись конца продувки',true);return Promise.resolve(false)}return [2,12,99].includes(code)?safetyCmd(code):action('/cmd?id='+code,CMD_OK[code])}
function targetPaint(){const el=byId('targetSlider'),v=Number(el.value);rangeOutput(el,'targetValue',1,'бар');document.querySelectorAll('[data-q]').forEach(b=>{const s=Number(b.dataset.q)===v;b.classList.toggle('selected',s);b.setAttribute('aria-pressed',String(s))})}
function targetInput(){pressureDirty=true;targetPaint();paintDirty()}
function setTargetValue(v){byId('targetSlider').value=Math.max(0.5,Math.min(6.5,v));pressureDirty=true;targetPaint();paintDirty()}
function stepTarget(delta){setTargetValue(Math.round((Number(byId('targetSlider').value)+delta)*2)/2)}
function setQuick(v){setTargetValue(v)}
async function setPressure(){const v=Number(byId('targetSlider').value);if(!(v>=0.5&&v<=6.5)){toast('Цель: 0,5–6,5 бар',true);return}if(await action('/setpressure?val='+v.toFixed(1),'Целевое давление сохранено')){pressureDirty=false;paintDirty()}}
function fmtLeft(s){s=Math.max(0,s|0);const h=Math.floor(s/3600),m=Math.floor(s%3600/60),c=s%60;return (h?h+':'+String(m).padStart(2,'0'):m)+':'+String(c).padStart(2,'0')}
function fmtNL(v){v=Number(v);return v<0||!Number.isFinite(v)?'—':(v>=100?v.toFixed(0):v>=10?v.toFixed(1):v.toFixed(2)).replace('.',',')+' нл'}
function timerPaint(){byId('timerCustomFld').hidden=Number(byId('timerSelect').value)>=0;paintDirty()}
function renderExtras(d){
const banner=ALARMS.find(a=>alarmOn(a,d));let act=null;if(banner){showBanner(banner.level,banner.title,banner.msg(d),banner.level==='danger'?'i-alert':'i-warn');act=banner.act?banner.act(d):null}else if(d.tcal)showBanner('info','Калибровка датчика наклона.','Не двигай установку.','i-info');else byId('alarmNotice').hidden=true;
// «Заглушить» — в том же баннере: он виден при любой аварии (аварии стоят в ALARMS первыми)
const alarm=d.ovl||d.tlk||d.oh||d.psf||d.lkl||d.mpuf,mute=!!alarm&&!d.mute,ab=byId('alarmAct');ab.hidden=!act;if(act){ab.dataset.safety=String(act.code);text('alarmActName',act.label);byId('alarmActIcon').setAttribute('href','#'+act.icon)}byId('alarmMute').hidden=!mute;byId('alarmWifi').hidden=true;byId('alarmActions').hidden=!act&&!mute;
text('timerLeft',d.tml>=0?'осталось '+fmtLeft(d.tml):d.tmr?d.tmr+' мин':'без ограничения');
if(!timerDirty&&document.activeElement!==byId('timerSelect')&&document.activeElement!==byId('timerCustom')){const sel=byId('timerSelect');const has=[...sel.options].some(o=>Number(o.value)===Number(d.tmr));sel.value=has?String(d.tmr):'-1';if(!has)byId('timerCustom').value=d.tmr;timerPaint()}
text('gasRate',fmt(d.grate||0)+' нл/ч');text('gasDuty',Math.round(d.duty||0)+'%');text('gasSession',fmtNL(d.gses||0));text('gasTotal',fmtNL(d.gtot||0));text('gasDay',d.tsync?fmtNL(d.gday):'—');text('gasWeek',d.tsync?fmtNL(d.gwk):'—');
if(!gasDirty&&document.activeElement!==byId('gasTank'))byId('gasTank').value=Number(d.gtank||1).toFixed(1);
text('maintLeft',d.mntd?'пора':'осталось '+d.mnt+' ч');byId('maintLeft').classList.toggle('warn-text',!!d.mntd);if(!maintDirty&&document.activeElement!==byId('maintHours'))byId('maintHours').value=d.mnti;
statusPill('dgPress',!d.psf,'OK','НЕИСПРАВЕН');text('dgPv',fmt(d.pv||0,2)+' В');
if(d.sout!==undefined&&Number(d.sout)!==soundOut)selectSound(Number(d.sout),false);if(d.sout!==undefined&&!soundDirty&&document.activeElement!==byId('volumeSlider')){if(String(d.svol)!==byId('volumeSlider').value&&d.svol>=1){byId('volumeSlider').value=d.svol;rangeOutput(byId('volumeSlider'),'volumeValue',0,'/ 10')}}showPlaying(!!d.sp);
if(d.boot!==timeSentFor){timeSentFor=d.boot;brain('/settime?t='+Math.floor(Date.now()/1000)).catch(()=>{timeSentFor=-1})}
if(d.evs!==logSeen){logSeen=d.evs;if(currentTab==='diag')loadLog()}}
// ⚡ v1.3.0: пустое «Своё значение» больше не отправляет 0 (не выключает таймер)
async function setTimer(){let v=Number(byId('timerSelect').value);if(v<0){const raw=byId('timerCustom').value.trim();v=Math.round(Number(raw));if(!raw||!(v>=1&&v<=600)){toast('Таймер: от 1 до 600 минут',true);return}}if(!(v>=0&&v<=600)){toast('Таймер: от 1 до 600 минут',true);return}if(await action('/settimer?min='+v,v?'Таймер: '+v+' мин':'Таймер выключен')){timerDirty=false;paintDirty()}}
async function setGasTank(){const v=Number(byId('gasTank').value);if(!(v>=0.1&&v<=100)){toast('Объём бака: 0,1–100 л',true);return}if(await action('/setgas?tank='+v.toFixed(2),'Объём бака сохранён')){gasDirty=false;paintDirty()}}
async function gasReset(){if(await ask('Обнулить счётчик газа?','Сбросятся «Всего», «За сессию» и суточные значения. Вернуть их нельзя.','Обнулить'))action('/setgas?reset=1','Счётчик газа обнулён')}
async function setMaint(){const h=Math.round(Number(byId('maintHours').value));if(!(h>=1&&h<=1000)){toast('Интервал: 1–1000 ч',true);return}if(await action('/maint?h='+h,'Интервал сохранён')){maintDirty=false;paintDirty()}}
// v1.3.0: «Обслужено» обнуляет отсчёт до доливки — как и другие сбросы, только с подтверждением
async function maintDone(){if(await ask('Отметить доливку?','Отсчёт до следующей доливки воды или электролита начнётся заново. Прежний остаток вернуть нельзя.','Отметить',false))return action('/maint?done=1','Обслуживание отмечено');return false}
async function resetCycles(){if(await ask('Сбросить циклы реле?','Счётчики клапана, пьезо, вентилятора и MOSFET начнутся с нуля. Делай это после замены реле.','Сбросить'))action('/resetcycles','Циклы реле сброшены')}
async function resetRuntime(){if(await ask('Сбросить наработку?','Счётчик часов работы электролизёра начнётся с нуля. От него считается и напоминание об обслуживании.','Сбросить'))action('/resetruntime','Наработка сброшена')}
const EVT={1:['Загрузка',''],2:['Авария: давление','bad'],3:['Авария: наклон','bad'],4:['Перегрев','bad'],5:['Бак не набирает давление','warn'],6:['Датчик давления неисправен','bad'],7:['Датчик давления в норме',''],8:['Нет данных температуры','warn'],9:['Подозрение на утечку','warn'],10:['Утечка — система выключена','bad'],11:['Таймер выключил систему',''],12:['Пора обслуживать','warn'],13:['Обслуживание отмечено',''],14:['Часы синхронизированы',''],15:['Утечка: частая подкачка при удержании','warn'],16:['Датчик наклона не отвечает','bad'],17:['Продувка не завершена','bad'],18:['Калибровка датчика давления сброшена — откалибруй заново','bad']};
const RST=['?','питание','внешний сброс','программный','сбой','WDT прерываний','сторожевой таймер','WDT','сон','просадка питания','SDIO'];
function evtText(r){const e=EVT[r.c]||['Событие '+r.c,''];let m=e[0];if(r.c===1)m+=' · '+(RST[r.v]||r.v);else if(r.c===2||r.c===5)m+=' · '+fmt(r.v/100,2)+' бар';else if(r.c===3)m+=' · '+r.v+'°';else if(r.c===4)m+=' · '+r.v+' °C';else if(r.c===6||r.c===7)m+=' · '+fmt(r.v/1000,2)+' В';else if(r.c===9||r.c===10)m+=' · −'+fmt(r.v/100,2)+' бар за 30 с';else if(r.c===15)m+=' · MOSFET '+r.v+'% времени';else if(r.c===11)m+=' · '+r.v+' мин';else if(r.c===12)m+=' · '+r.v+' ч';else if(r.c===17)m+=' · осталось '+fmt(r.v/100,2)+' бар';return [m,e[1]]}
function evtTime(r,j){if(r.e){const d=new Date(r.e*1000);return d.toLocaleDateString('ru-RU',{day:'2-digit',month:'2-digit'})+' '+d.toLocaleTimeString('ru-RU',{hour:'2-digit',minute:'2-digit'})}return (r.b===j.boot?'':'№'+r.b+' ')+'+'+fmtTime(r.u)}
async function loadLog(){const holder=byId('logList');try{const j=JSON.parse(await brain('/log'));text('logCount',j.list.length+' / 50');holder.textContent='';if(!j.list.length){const row=document.createElement('div');row.className='diag-row';row.textContent='Событий нет';holder.appendChild(row);return}j.list.forEach(r=>{const t=evtText(r);const row=document.createElement('div');row.className='diag-row';const time=document.createElement('span');time.className='log-t';time.textContent=evtTime(r,j);const msg=document.createElement('span');msg.className=t[1];msg.textContent=t[0];row.append(time,msg);holder.appendChild(row)})}catch(e){if(online)toast('Журнал не загрузился. Нажми «Обновить»',true)}}
async function clearLog(){if(await ask('Очистить журнал событий?','Все записи аварий и событий в мозге будут удалены.','Очистить')&&await action('/log/clear','Журнал очищен'))loadLog()}
function renderChecks(d){const h=[];if(!d.oledok&&!d.mpuok)h.push('Не отвечают дисплей и MPU на общей шине I²C: проверь SDA→21, SCL→22, общую землю и уровни напряжения.');else{if(!d.oledok)h.push('Дисплей не найден: LCD2004 — адреса 0x27/0x3F, OLED — 0x3C. Проверь SDA→21, SCL→22, питание и общую землю.');if(!d.mpuok)h.push('MPU-6050 не найден: та же шина I²C (21/22), AD0→GND (адрес 0x68), VCC→3,3 В.')}if(d.mpuf)h.push('MPU-6050 перестал отвечать в работе: проверь провода I²C и питание модуля. Электролизёр заблокирован до восстановления.');if(!d.nrfok)h.push('nRF24 не найден: SCK→18, MISO→19, MOSI→23, CE→4, CSN→5; VCC→3,3 В (не 5 В!), конденсатор 470 мкФ у модуля.');if(d.psf)h.push('Датчик давления вне диапазона 0,15–3,2 В: обрыв, замыкание или нет питания датчика. Проверь делитель и провод к GPIO36.');if(!d.tok)h.push('Нет данных датчика температуры: DS18B20 DATA→15 и резистор 4,7 кОм к 3,3 В.');if(!d.fsok)h.push('Память мелодий (LittleFS) недоступна: свои мелодии не загрузить, встроенные работают. Прошивай со схемой разделов «Default 4MB with spiffs».');const list=byId('checkList'),key=h.join('|');if(list.dataset.key!==key){list.dataset.key=key;list.textContent='';h.forEach(s=>{const li=document.createElement('li');li.textContent=s;list.appendChild(li)})}byId('checkCard').hidden=!h.length;const ms=byId('modulesState');text('modulesState',h.length?'Нужна проверка: '+h.length:'Все в норме');ms.className=h.length?'warn-text':''}
function renderService(d){const n=v=>v===undefined?'—':Number(v).toLocaleString('ru-RU');[['rcValve','rcv'],['rcPiezo','rcp'],['rcFan','rcf2'],['rcElectro','rce']].forEach(x=>{text(x[0],n(d[x[1]]));byId(x[0]).classList.toggle('warn-text',x[1]!=='rce'&&Number(d[x[1]])>100000)});text('rtDiag',fmtTime(d.rt));text('maintDiag',d.mntd?'пора':'осталось '+d.mnt+' ч');byId('maintDiag').classList.toggle('warn-text',!!d.mntd);text('relayState',d.relw?'Ресурс на исходе':'Реле ~100 000 циклов');byId('relayState').classList.toggle('warn-text',!!d.relw);calSync(d)}
// ── v1.3.0: калибровка датчика давления (поля — из pco/pcs, пока не правим) ──
function calValues(){return {off:Number(byId('calOffset').value),scl:Number(byId('calScale').value)}}
function calEdit(){calDirty=true;calPaint();paintDirty()}
function calPaint(){const d=state||{},c=calValues(),pv=Number(d.pv),live=online&&Number.isFinite(pv);text('calLive',live?fmt(pv,3)+' В · '+fmt(Math.max(0,Number(d.p)||0),2)+' бар':'— В · — бар');let s='Сохранено в мозге: '+(d.pco!==undefined?fmt(d.pco,3)+' В · '+fmt(d.pcs,3)+' бар/В':'—');if(calDirty&&live)s+=c.off>=0&&c.off<=2&&c.scl>=0.1&&c.scl<=10?' · с новыми значениями '+fmt(Math.max(0,(pv-c.off)*c.scl),2)+' бар':' · смещение 0–2 В, масштаб 0,1–10';text('calPreview',s)}
function calSync(d){if(!calDirty&&d.pco!==undefined&&document.activeElement!==byId('calOffset')&&document.activeElement!==byId('calScale')){byId('calOffset').value=Number(d.pco).toFixed(3);byId('calScale').value=Number(d.pcs).toFixed(3)}calPaint()}
function calZero(){const d=state||{};if(!online||!Number.isFinite(Number(d.pv))){toast('Нет показаний датчика давления. Дождись связи с мозгом',true);return}if(d.psf){toast('Датчик давления неисправен — калибровка невозможна',true);return}byId('calOffset').value=Number(d.pv).toFixed(3);calEdit();toast('Смещение = текущее напряжение. Бак должен быть стравлен до 0 бар')}
async function saveCal(){const d=state||{},c=calValues();if(!(c.off>=0&&c.off<=2)||!(c.scl>=0.1&&c.scl<=10)){toast('Смещение 0–2 В, масштаб 0,1–10 бар/В',true);return}if(d.pwr||d.prg){toast('Сначала выключи систему и дождись конца продувки',true);return}const after=Number.isFinite(Number(d.pv))?' Давление станет '+fmt(Math.max(0,(d.pv-c.off)*c.scl),2)+' бар (сейчас '+fmt(Number(d.p)||0,2)+').':'';if(!await ask('Сохранить калибровку?','Смещение '+fmt(c.off,3)+' В, масштаб '+fmt(c.scl,3)+' бар/В.'+after+' Защита по давлению работает по этим значениям: неверная калибровка опасна.','Сохранить'))return;
// v1.3.0: пока окно было открыто, систему могли включить с пульта — мозг /calibrate не проверяет, проверяем по свежему статусу
const seen=lastUpdate;await refreshNow();if(lastUpdate===seen){toast(NO_ANSWER,true);return}const n=state||{};if(n.pwr||n.prg){toast('Сначала выключи систему и дождись конца продувки',true);return}if(await action('/calibrate?offset='+c.off.toFixed(3)+'&scale='+c.scl.toFixed(3),'Калибровка сохранена')){calDirty=false;paintDirty()}}
// ── v1.3.0: OTA-обновление прошивки мозга (multipart POST /update, поле "firmware") ──
let otaFile=null,otaBusy=false,otaPickSeq=0;
// v1.3.0: метку образа ESP32 (0xE9) проверяем сразу при выборе — чужой .bin не доходит до «Готово к загрузке» и окна подтверждения
function otaPick(inp){const f=inp.files[0];inp.value='';if(!f)return;otaFile=null;const seq=++otaPickSeq;if(!/\.bin$/i.test(f.name))text('otaName','Нужен файл прошивки .bin');else if(f.size<100000||f.size>1310720)text('otaName',f.name+' · '+fmtBytes(f.size)+' — не похоже на прошивку мозга (нужно 100 КБ – 1,25 МБ)');else{text('otaName',f.name+' · проверка…');melReadFile(f.slice(0,1),false).then(b=>{if(seq!==otaPickSeq)return;if(new Uint8Array(b)[0]!==0xE9){text('otaName',f.name+' — это не образ прошивки ESP32 (нет метки 0xE9)');return}otaFile=f;text('otaName',f.name+' · '+fmtBytes(f.size));otaPaint()},e=>{if(seq===otaPickSeq)text('otaName',e.message)})}otaPaint()}
function otaPaint(){const d=state||{},running=!!(d.pwr||d.prg||d.psh);setOff(byId('otaButton'),!otaFile||otaBusy||!online||running);setOff(byId('otaPickButton'),otaBusy);if(!otaBusy)text('otaHint',!online?'Нет связи с мозгом':running?'Выключи систему и дождись конца продувки':otaFile?'Готово к загрузке':'')}
async function otaStart(){const d=state||{};if(!otaFile||otaBusy||!online||d.pwr||d.prg||d.psh){otaPaint();return}if(melSaving){toast('Дождись сохранения мелодии',true);return}if(!await ask('Обновить прошивку мозга?','Файл '+otaFile.name+'. Во время записи выходы выключены, после неё мозг перезагрузится. Не уходи из Wi-Fi H2_PRO.','Загрузить',false))return;otaBusy=true;otaPaint();const bar=byId('otaBar');byId('otaProg').hidden=false;bar.style.width='0%';text('otaHint','Подготовка файла…');try{const buf=new Uint8Array(await melReadFile(otaFile,false));if(buf[0]!==0xE9)throw new Error('Это не образ прошивки ESP32 (нет метки 0xE9)');const B='h2ota'+Date.now().toString(36);await brain('/update',{method:'POST',type:'multipart/form-data; boundary='+B,body:bufToB64(multipart(B,'firmware','firmware.bin',buf)),timeout:240000,progress:(s,t)=>{const pc=Math.round(s/t*100);bar.style.width=pc+'%';text('otaHint','Отправлено '+pc+' % — не закрывай приложение')}});bar.style.width='100%';otaFile=null;text('otaName','Прошивка записана, мозг перезагружается');toast('Прошивка загружена — мозг перезагружается')}catch(e){toast(e.code===403?'Сначала выключи систему и дождись конца продувки':e.code===500?'Мозг отклонил прошивку: файл не подошёл или загрузка прервалась':e.message,true)}finally{otaBusy=false;byId('otaProg').hidden=true;otaPaint();if(parkedFocus===byId('otaButton')&&byId('otaButton').disabled)parkedFocus=byId('otaPickButton');restoreFocus()}}
function multipart(boundary,field,filename,bytes){const enc=new TextEncoder(),head=enc.encode('--'+boundary+'\r\nContent-Disposition: form-data; name="'+field+'"; filename="'+filename+'"\r\nContent-Type: application/octet-stream\r\n\r\n'),tail=enc.encode('\r\n--'+boundary+'--\r\n'),out=new Uint8Array(head.length+bytes.length+tail.length);out.set(head,0);out.set(bytes,head.length);out.set(tail,head.length+bytes.length);return out}
function bufToB64(u){let s='';for(let i=0;i<u.length;i+=32768)s+=String.fromCharCode.apply(null,u.subarray(i,i+32768));return btoa(s)}
function b64ToBuf(b){const s=atob(b),u=new Uint8Array(s.length);for(let i=0;i<s.length;i++)u[i]=s.charCodeAt(i);return u.buffer}
// ── v1.3.0: локальная история {ts,p,t} — как в веб-интерфейсе (раз в 30 с, до 20000 точек) ──
const HIST_KEY='h2pro_history',MAX_POINTS=20000;
let hist=[],histBytes=0,lastHistPush=0,histMax={p:0,t:0},chartRange='1h',lastChartDraw=0;
function loadHistory(){try{const s=localStorage.getItem(HIST_KEY)||'[]';hist=JSON.parse(s);histBytes=s.length}catch(e){hist=[]}if(!Array.isArray(hist))hist=[];histStats()}
function saveHistory(){let ok=true;try{const s=JSON.stringify(hist);histBytes=s.length;localStorage.setItem(HIST_KEY,s)}catch(e){ok=false;toast('История не сохранилась: память заполнена. Экспортируй и очисти базу на вкладке «История»',true)}dbPaint();return ok}
function histStats(){let p=0,t=0;hist.forEach(r=>{if(r.p>p)p=r.p;if(r.t>t)t=r.t});histMax={p,t};dbPaint()}
function dbPaint(){text('dbInfo',hist.length+' · '+fmt(histBytes/1024,1)+' КБ');text('chartInfo','Точек: '+hist.length)}
function pushHistory(p,t){const now=Date.now();if(now-lastHistPush<30000)return;lastHistPush=now;const r={ts:now,p:+p.toFixed(2),t:+t.toFixed(1)};hist.push(r);if(hist.length>MAX_POINTS){hist=hist.slice(-MAX_POINTS);histStats()}else{histMax.p=Math.max(histMax.p,r.p);histMax.t=Math.max(histMax.t,r.t)}saveHistory()}
function renderHistory(d){const p=Math.max(0,Number(d.p)),t=d.tok?Number(d.t):0;pushHistory(p,t);text('stPnow',fmt(p,2)+' бар');text('stPmax',fmt(Math.max(histMax.p,p),2)+' бар');text('stTnow',d.tok?fmt(t)+' °C':'—');const tm=Math.max(histMax.t,t);text('stTmax',tm>0?fmt(tm)+' °C':'—');text('stRt',fmtTime(d.rt));text('stUp',fmtTime(d.up));if(currentTab==='history'&&Date.now()-lastChartDraw>10000)drawCharts()}
function rangeMs(){return chartRange==='1h'?3600e3:chartRange==='1d'?86400e3:604800e3}
function setRange(r){chartRange=r;document.querySelectorAll('[data-range]').forEach(b=>{const s=b.dataset.range===r;b.classList.toggle('selected',s);b.setAttribute('aria-pressed',String(s))});drawCharts()}
function chartTime(ts){const d=new Date(ts);return chartRange==='1w'?d.toLocaleDateString('ru-RU',{day:'2-digit',month:'2-digit'}):d.toLocaleTimeString('ru-RU',{hour:'2-digit',minute:'2-digit'})}
function drawCharts(){lastChartDraw=Date.now();const end=Date.now(),start=end-rangeMs();let data=hist.filter(r=>r.ts>=start);const step=Math.ceil(data.length/1500);if(step>1)data=data.filter((r,i)=>i%step===0||i===data.length-1);const css=getComputedStyle(document.documentElement);const maxT=data.reduce((m,r)=>Math.max(m,Number(r.t)||0),0);const maxTY=Math.max(40,Math.ceil(maxT/10)*10);drawLine(byId('chartP'),data,'p',css.getPropertyValue('--accent').trim(),6.5,2,css,start,end);drawLine(byId('chartT'),data,'t',css.getPropertyValue('--green').trim(),maxTY,maxTY>60?20:10,css,start,end)}
function drawLine(cv,data,key,color,maxY,tick,css,start,end){const dpr=window.devicePixelRatio||1,w=cv.clientWidth,h=cv.clientHeight;if(!w||!h)return;cv.width=Math.round(w*dpr);cv.height=Math.round(h*dpr);const ctx=cv.getContext('2d');ctx.setTransform(dpr,0,0,dpr,0,0);ctx.clearRect(0,0,w,h);const L=30,T=8,B=20,R=4,ph=h-T-B,pw=w-L-R,muted=css.getPropertyValue('--muted').trim();ctx.font='11px -apple-system,Roboto,Arial,sans-serif';ctx.lineWidth=1;for(let v=0;v<=maxY+1e-6;v+=tick){const y=Math.round(T+(1-v/maxY)*ph)+.5;ctx.strokeStyle=css.getPropertyValue('--line').trim();ctx.beginPath();ctx.moveTo(L,y);ctx.lineTo(w-R,y);ctx.stroke();ctx.fillStyle=muted;ctx.textAlign='right';ctx.textBaseline='middle';ctx.fillText(fmt(v,0),L-5,y)}ctx.textBaseline='alphabetic';ctx.textAlign='left';ctx.fillText(chartTime(start),L,h-4);ctx.textAlign='right';ctx.fillText(chartTime(end),w-R,h-4);if(data.length<2){ctx.textAlign='center';ctx.textBaseline='middle';ctx.fillText('Накопление данных…',L+pw/2,T+ph/2);return}const gap=Math.max(180000,rangeMs()/150);ctx.strokeStyle=color;ctx.lineWidth=2;ctx.lineJoin='round';ctx.lineCap='round';ctx.beginPath();let prev=-Infinity;data.forEach(r=>{const x=L+(r.ts-start)/(end-start)*pw,y=T+(1-Math.min(Math.max(Number(r[key])||0,0)/maxY,1))*ph;if(r.ts-prev>gap)ctx.moveTo(x,y);else ctx.lineTo(x,y);prev=r.ts});ctx.stroke()}
function saveText(name,mime,content){if(window.Brain&&Brain.saveFile)return bridge(0,id=>Brain.saveFile(id,name,mime,content)).then(()=>toast('Файл сохранён'),e=>{if(e.code!==499)toast(e.message,true)});try{const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([content],{type:mime}));a.download=name;document.body.appendChild(a);a.click();a.remove()}catch(e){toast('Сохранение файлов недоступно',true)}return Promise.resolve()}
function exportCSV(){if(!hist.length){toast('База пуста',true);return}let csv='timestamp,iso,pressure_bar,temp_c\n';hist.forEach(r=>{csv+=r.ts+','+new Date(r.ts).toISOString()+','+r.p+','+r.t+'\n'});saveText('h2pro_'+Date.now()+'.csv','text/csv',csv)}
function exportJSON(){if(!hist.length){toast('База пуста',true);return}saveText('h2pro_'+Date.now()+'.json','application/json',JSON.stringify(hist))}
// v1.3.0: файл без точек — ошибка (раньше «Импортировано»), сбой разбора JSON — по-русски, а не текстом движка
function histPoint(x){return x&&Number.isFinite(+x.ts)&&+x.ts>0&&Number.isFinite(+x.p)}
function importData(inp){const f=inp.files[0];inp.value='';if(!f)return;const r=new FileReader();r.onload=()=>{try{const src=String(r.result);let data;if(/\.json$/i.test(f.name)||/^\s*\[/.test(src)){try{data=JSON.parse(src)}catch(e){throw new Error('файл повреждён или это не JSON')}}else{data=[];src.split(/\r?\n/).slice(1).forEach(line=>{const c=line.split(',');if(c.length>=4)data.push({ts:+c[0],p:+c[2],t:+c[3]})})}if(!Array.isArray(data))throw new Error('нужен список точек');const valid=data.filter(histPoint).length;if(!valid)throw new Error('в файле нет точек истории. Нужен CSV или JSON из экспорта H2 PRO');const map={};hist.concat(data).forEach(x=>{if(histPoint(x))map[+x.ts]={ts:+x.ts,p:+x.p,t:Number.isFinite(+x.t)?+x.t:0}});hist=Object.keys(map).map(k=>map[k]).sort((a,b)=>a.ts-b.ts);if(hist.length>MAX_POINTS)hist=hist.slice(-MAX_POINTS);const saved=saveHistory();histStats();drawCharts();if(saved)toast('Импортировано '+valid+' '+plural(valid,'точка','точки','точек')+'. Всего в базе: '+hist.length)}catch(e){toast('Ошибка импорта: '+e.message,true)}};r.onerror=()=>toast('Не удалось прочитать файл',true);r.readAsText(f)}
async function clearHistory(){if(!await ask('Очистить базу истории?','Все точки давления и температуры на этом телефоне будут удалены. Сначала можно сделать экспорт.','Очистить'))return;hist=[];saveHistory();histStats();drawCharts();toast('База истории очищена')}
function setMonitor(on){try{if(window.Brain&&Brain.monitor){Brain.monitor(on);if(!on){byId('monitor').checked=false;toast('Уведомления выключены');setTimeout(monitorPaint,300)}return}}catch(e){}byId('monitor').checked=false;toast('Уведомления недоступны',true)}
// v1.3.0: результат запроса разрешения — переключатель показывает реальное состояние
window.h2MonitorResult=function(ok){byId('monitor').checked=ok;toast(ok?'Уведомления включены':'Разреши уведомления для H2 PRO в настройках телефона',!ok);monitorPaint()};
function monitorPaint(){let blocked=false;try{blocked=!!(window.Brain&&Brain.monitorBlocked&&Brain.monitorBlocked())}catch(e){}byId('monitorWarn').hidden=!blocked}
function openNotifySettings(){try{Brain.openNotificationSettings()}catch(e){toast('Открой настройки уведомлений телефона',true)}}
function openBattery(){try{Brain.openBatterySettings()}catch(e){toast('Открой настройки батареи телефона',true)}}
function selectFan(mode,manual=true){fanMode=mode;if(manual){fanDirty=true;paintDirty()}document.querySelectorAll('[data-mode]').forEach(x=>{const selected=Number(x.dataset.mode)===mode;x.classList.toggle('selected',selected);x.setAttribute('aria-pressed',String(selected))})}
async function setFan(){const on=Number(byId('fanOn').value),off=Number(byId('fanOff').value);if(!Number.isInteger(on)||!Number.isInteger(off)||on<20||on>90||off<15||off>=on){toast('Включение — 20–90 °C, выключение — от 15 °C и ниже порога включения',true);return}if(await action('/setfans?i=2&mode='+fanMode+'&ton='+on+'&toff='+off,'Настройки вентилятора сохранены')){fanDirty=false;paintDirty()}}
// ── Звук ──
// ⚡ v6.5: «Где звучит» применяется сразу (/snd/set?out= — только вывод): весь звук,
// включая уже звучащую мелодию, переходит на выбранное устройство без «Сохранить». Пока запрос
// в пути (soundOutPending), статус не перекрашивает выбор; ошибка — откат к подтверждённому.
let soundOutPending=0,soundOutSeq=0,soundOutOk=2;
async function setSoundOut(out){if(out===soundOut&&!soundOutPending)return;if(!online){toast('Нет связи с мозгом. Подключись к Wi-Fi H2_PRO',true);return}const my=++soundOutSeq;paintSoundOut(out);soundOutPending++;haptic();try{await brain('/snd/set?out='+out);soundOutOk=out;if(soundCfg)soundCfg.out=out;if(my===soundOutSeq)toast(out===3?'Звук выключен':'Звук: '+['пульт','мозг','оба'][out]);setTimeout(poll,250)}catch(e){if(my===soundOutSeq){paintSoundOut(soundOutOk);toast(e.message||'Не удалось сменить, где звучит',true)}}finally{setTimeout(()=>soundOutPending--,800)}}
function selectSound(out,manual=true){if(manual){setSoundOut(out);return}if(!soundOutPending){soundOutOk=out;paintSoundOut(out)}}
function paintSoundOut(out){soundOut=out;document.querySelectorAll('[data-out]').forEach(x=>{const selected=Number(x.dataset.out)===out;x.classList.toggle('selected',selected);x.setAttribute('aria-pressed',String(selected))})}
// v1.3.0: полоса «Играет · Остановить» — только пока звучит (кнопка остановки рядом с индикатором)
function showPlaying(on){byId('soundBanner').hidden=!on}
function userMelodies(){return ((soundCfg&&soundCfg.list)||[]).slice().sort((a,b)=>a.id-b.id)}
// Мелодия события: Нет / встроенные / свои; во время правки выбор не сбрасываем
// v1.4.0: мелодии событий — [ключ /snd/cfg, id списка, встроенная, можно «Нет», предел своей мелодии, мс]
const SOUND_EVENTS=[['boot','soundBoot',-1,1,0],['on','soundOn',-2,1,0],['off','soundOff',-4,1,0],['pur','soundPur',-6,1,0],['alr','soundAlr',-3,0,10000],['wrn','soundWrn',-5,0,10000]];
function fillEventSelect(ev,val){const[,id,def,none,max]=ev,s=byId(id),want=soundDirty&&s.value!==''?s.value:String(val===undefined?def:val);s.textContent='';const add=(v,t)=>{const o=document.createElement('option');o.value=String(v);o.textContent=t;s.appendChild(o)};if(none)add(-9,'Нет');const nm=v=>(BUILTIN.find(b=>b.id===v)||{name:'Стандартная'}).name+' (встроенная)';add(def,nm(def));[-1,-2].filter(v=>v!==def).forEach(v=>add(v,nm(v)));userMelodies().filter(m=>!max||m.ms<=max).forEach(m=>add(m.id,m.name));s.value=want;if(s.value!==want)s.value=String(def)}
async function loadSound(){try{const cfg=JSON.parse(await brain('/snd/cfg'));soundCfg=cfg;selectSound(Number(cfg.out),false);if(!soundDirty){byId('volumeSlider').value=cfg.vol;rangeOutput(byId('volumeSlider'),'volumeValue',0,'/ 10');byId('soundAlarms').checked=!!cfg.alm;byId('soundClicks').checked=!!cfg.clk}SOUND_EVENTS.forEach(e=>fillEventSelect(e,cfg[e[0]]));renderLibrary();showPlaying(!!cfg.play)}catch(e){if(online)toast('Настройки звука не загрузились. Открой вкладку «Звук» ещё раз',true)}}
async function setSound(){let q='/snd/set?out='+soundOut+'&vol='+Number(byId('volumeSlider').value)+'&alm='+(byId('soundAlarms').checked?1:0)+'&clk='+(byId('soundClicks').checked?1:0);if(soundCfg&&SOUND_EVENTS.every(e=>byId(e[1]).value!==''))q+=SOUND_EVENTS.map(e=>'&'+e[0]+'='+byId(e[1]).value).join('');if(await action(q,'Звук сохранён')){soundDirty=false;paintDirty();loadSound()}}
// Играть там, что выбрано на экране; «Выкл» — подсказка вместо ложного «запущена»
function playSound(id){if(soundOut===3){toast('Звук выключен — выбери, где играть',true);return Promise.resolve(false)}return action('/snd/play?id='+id,'Мелодия запущена')}
function stopSound(){melStopPreview();return action('/snd/stop','Звук остановлен')}
const BUILTIN=[{id:-1,name:'Загрузка H2 PRO',note:'Запуск устройства'},{id:-2,name:'Включение',note:'Система готова к работе'},{id:-3,name:'Тревога',note:'Проверка сигнала'},{id:-4,name:'Выключение',note:'Система выключена'},{id:-5,name:'Предупреждение',note:'Внимание'},{id:-6,name:'Продувка завершена',note:'Давление сброшено'}];
function iconButton(icon,label,handler,cls){const b=document.createElement('button');b.className='play-button'+(cls?' '+cls:'');b.setAttribute('aria-label',label);b.innerHTML='<svg><use href="#'+icon+'"/></svg>';b.onclick=handler;return b}
function melodyRow(m,index,note,custom){const row=document.createElement('div');row.className='melody';const i=document.createElement('span');i.className='track-index';i.textContent=String(index).padStart(2,'0');const title=document.createElement('span');title.className='track-title';title.textContent=m.name;const hint=document.createElement('small');hint.textContent=note;title.appendChild(hint);const play=iconButton('i-play','Играть на устройстве: '+m.name,()=>playSound(m.id));play.dataset.control='';const ear=iconButton('i-ear','Прослушать на телефоне: '+m.name,()=>melPreview(m.id),'icon-only');ear.dataset.pv=String(m.id);ear.dataset.name=m.name;const acts=document.createElement('span');acts.className='row-actions';acts.append(ear,play);row.append(i,title,acts);if(custom){const up=iconButton('i-upload','Отправить в пульт: '+m.name,()=>rmSendBrain(m.id),'icon-only');up.dataset.control='';const del=iconButton('i-x','Удалить: '+m.name,()=>deleteMelody(m.id),'icon-only danger-text');del.dataset.control='';acts.append(up,del)}else{const ph=document.createElement('span');ph.className='play-ph';acts.appendChild(ph)}return row}
// v1.3.0: список строится заново — фокус остаётся на той же кнопке, а после удаления — на соседней строке
function renderLibrary(){const holder=byId('melodyList'),user=userMelodies(),a=document.activeElement,fa=holder.contains(a)&&a!==holder?a:parkedFocus&&holder.contains(parkedFocus)?parkedFocus:null,fl=fa&&fa.getAttribute('aria-label'),fi=fa?[...holder.children].indexOf(fa.closest('.melody')):-1;holder.textContent='';if(fa&&parkedFocus===fa)parkedFocus=null;BUILTIN.forEach((m,i)=>holder.appendChild(melodyRow(m,i+1,m.note,false)));user.forEach((m,i)=>holder.appendChild(melodyRow(m,i+1+BUILTIN.length,m.n+' '+plural(m.n,'нота','ноты','нот')+' · '+fmt(m.ms/1000,1)+' с',true)));text('melodyCount',soundCfg?user.length+' из 16 слотов':'Встроенные');const info=!soundCfg?'':!soundCfg.fsok?'Память мелодий (LittleFS) недоступна — работают только встроенные.':'Свободно '+fmtBytes(soundCfg.free)+' из '+fmtBytes(soundCfg.total)+(user.length?'':' · своих мелодий пока нет');text('melodyInfo',info);byId('melodyInfo').hidden=!info;melPlUi();updateControls();if(fa){const rows=holder.children,same=[...holder.querySelectorAll('button')].find(b=>b.getAttribute('aria-label')===fl),row=rows[Math.min(Math.max(fi,0),rows.length-1)],t=same||(row&&row.querySelector('button'));if(t)try{t.focus({preventScroll:true})}catch(e){}}}
async function deleteMelody(id){const m=userMelodies().find(x=>x.id===id);if(!await ask('Удалить мелодию?','«'+(m?m.name:id)+'» будет удалена из мозга. Если она назначена событию, вернётся встроенная.','Удалить'))return;if(melPl&&melPl.owner==='lib'+id)melStopPreview();if(await action('/snd/del?id='+id,'Мелодия удалена'))loadSound()}
function melPreview(id){if(melPl&&melPl.owner==='lib'+id){melStopPreview();return}try{melCtx()}catch(e){toast(e.message,true);return}brain('/snd/get?id='+id).then(b64=>melPlay(melParseH2M(b64ToBuf(b64)).notes,'lib'+id,false)).catch(e=>toast(e.message||'Мелодия недоступна',true))}

/* ══════════ v1.3.0: из веб-интерфейса мозга — прослушивание (WebAudio), формат H2M1, разбор
   MIDI / RTTTL / аудио и подготовка под зуммер. Логика без изменений, вывод — в элементы приложения. ══════════ */
let melAc=null,melPl=null;
function melCtx(){
  if(!melAc){ const AC=window.AudioContext||window.webkitAudioContext;
    if(!AC) throw new Error('Телефон не поддерживает WebAudio'); melAc=new AC(); }
  if(melAc.state==='suspended') melAc.resume();
  return melAc;
}
// v1.4.0: прослушивание «как на зуммере» — H2Mel.play (h2_mel_edit.js): скважность громкости
// из «Звука», резонанс зуммера; music — огибающая кнопки МЕЛОДИЯ пульта
function melPlay(notes,owner,music){
  melStopPreview();
  const h=H2Mel.play(melCtx(),notes,{vol:Number(byId('volumeSlider').value)||10,music,onEnd:()=>{ if(melPl&&melPl.h===h){ melPl=null; melPlUi(); } }});
  melPl={h,owner};
  melPlUi();
}
function melStopPreview(){
  if(!melPl) return;
  const p=melPl; melPl=null; p.h.stop();
  melPlUi();
}
function melPlUi(){
  const own=melPl?melPl.owner:'';
  text('melListenBtn',own==='res'?'Стоп':'Прослушать');
  document.querySelectorAll('[data-pv]').forEach(b=>{ const pv=b.dataset.pv, on=own===(pv.charAt(0)==='p'?pv:'lib'+pv);
    if(b.classList.contains('on')!==on){ b.classList.toggle('on',on); b.innerHTML='<svg><use href="#'+(on?'i-stop':'i-ear')+'"/></svg>';
      b.setAttribute('aria-label',(on?'Остановить прослушивание: ':'Прослушать на телефоне: ')+(b.dataset.name||'')); } });
}
// 'H2M1' | nameLen u8 (≤48) | name UTF-8 | noteCount u16 | {freq u16, dur u16}×N, little-endian
function melNameBytes(s){
  const enc=new TextEncoder(), parts=[]; let n=0;
  for(const ch of s.replace(/[\x00-\x1f\x7f]/g,'')){ const b=enc.encode(ch); if(n+b.length>48) break; parts.push(b); n+=b.length; }
  const out=new Uint8Array(n); let p=0; parts.forEach(b=>{ out.set(b,p); p+=b.length; });
  return out;
}
function melBuildH2M(name,notes){
  const nb=melNameBytes(name), u=new Uint8Array(4+1+nb.length+2+notes.length*4), dv=new DataView(u.buffer);
  u.set([72,50,77,49],0); u[4]=nb.length; u.set(nb,5);
  let p=5+nb.length; dv.setUint16(p,notes.length,true); p+=2;
  for(const n of notes){ dv.setUint16(p,n.f,true); dv.setUint16(p+2,n.d,true); p+=4; }
  return u;
}
function melParseH2M(ab){
  const u=new Uint8Array(ab), dv=new DataView(ab);
  if(u.length<7||u[0]!==72||u[1]!==50||u[2]!==77||u[3]!==49) throw new Error('неверный формат мелодии');
  let p=5+u[4]; if(u.length<p+2) throw new Error('файл мелодии повреждён');
  const name=new TextDecoder().decode(u.subarray(5,p)), cnt=dv.getUint16(p,true); p+=2;
  if(u.length<p+cnt*4) throw new Error('файл мелодии повреждён');
  const notes=[]; for(let i=0;i<cnt;i++,p+=4) notes.push({f:dv.getUint16(p,true),d:dv.getUint16(p+2,true)});
  return {name,notes};
}
// Разборщики дают «сырую» монофоническую мелодию [{m,d}]: m — нота MIDI (A4=69), m<0 — пауза; d — мс
function melReadFile(f,asText){
  return new Promise((res,rej)=>{ const r=new FileReader();
    r.onload=()=>res(r.result); r.onerror=()=>rej(new Error('Не удалось прочитать файл'));
    if(asText) r.readAsText(f); else r.readAsArrayBuffer(f); });
}
function melText(u){
  let s=''; try{ s=new TextDecoder('utf-8',{fatal:true}).decode(u); }
  catch(e){ try{ s=new TextDecoder('windows-1251').decode(u); }catch(e2){ s=''; } }
  return s.replace(/[\x00-\x1f]/g,'').trim();
}
// RTTTL: «имя:d=4,o=5,b=63:8c,8e.,p,c6» — ноты [длит][нота][#][.][октава][.]
function melParseRtttl(src){
  const lines=String(src).split(/\r?\n/).map(s=>s.trim()).filter(Boolean);
  const i=lines.findIndex(l=>(l.match(/:/g)||[]).length>=2);
  if(i<0) throw new Error('Не похоже на RTTTL: нужен формат «имя:d=4,o=5,b=120:ноты»');
  let s=lines[i]; for(let j=i+1;j<lines.length&&lines[j].indexOf(':')<0;j++) s+=lines[j];
  const c1=s.indexOf(':'), c2=s.indexOf(':',c1+1), def={d:4,o:6,b:63};
  s.slice(c1+1,c2).split(',').forEach(kv=>{ const m=kv.trim().toLowerCase().match(/^([dob])\s*=\s*(\d+)$/); if(m) def[m[1]]=+m[2]; });
  if(!(def.b>0)||!(def.d>0)) throw new Error('RTTTL: неверные параметры d= или b=');
  const q=60000/def.b, SEMI={c:0,d:2,e:4,f:5,g:7,a:9,b:11,h:11}, raw=[];
  s.slice(c2+1).split(',').forEach(tk=>{ tk=tk.trim().toLowerCase(); if(!tk) return;
    const m=tk.match(/^(\d{1,2})?([a-hp])(#)?(\.)?(\d)?(\.)?$/);
    if(!m) throw new Error('RTTTL: не удалось разобрать «'+tk+'»');
    const len=m[1]?+m[1]:def.d; if(!(len>0)) throw new Error('RTTTL: нулевая длительность в «'+tk+'»');
    let d=4*q/len; if(m[4]||m[6]) d*=1.5;
    if(m[2]==='p') raw.push({m:-1,d});
    else raw.push({m:12*((m[5]!==undefined?+m[5]:def.o)+1)+SEMI[m[2]]+(m[3]?1:0),d});
  });
  if(!raw.some(x=>x.m>=0)) throw new Error('RTTTL: нет нот');
  return {name:s.slice(0,c1).trim(),raw};
}
// Карта темпа MIDI: тик → мс (по всем meta 0x51 всех дорожек)
function melTempoMap(tempos,div){
  tempos.sort((a,b)=>a.tick-b.tick);
  const seg=[{tick:0,ms:0,us:500000}];
  for(const tp of tempos){ const L=seg[seg.length-1];
    if(tp.tick===L.tick) L.us=tp.us;
    else seg.push({tick:tp.tick,ms:L.ms+(tp.tick-L.tick)*L.us/div/1000,us:tp.us}); }
  return tick=>{ let lo=0,hi=seg.length-1;
    while(lo<hi){ const mid=(lo+hi+1)>>1; if(seg[mid].tick<=tick) lo=mid; else hi=mid-1; }
    const g=seg[lo]; return g.ms+(tick-g.tick)*g.us/div/1000; };
}
// Standard MIDI File 0/1: дорожки с нотами (канал 10 — ударные — исключён)
function melParseMidi(ab){
  const b=new Uint8Array(ab);
  let base=-1;   // MThd может быть внутри RIFF/RMID — ищем в начале файла
  for(let i=0;i+4<=Math.min(b.length,64);i++) if(b[i]===77&&b[i+1]===84&&b[i+2]===104&&b[i+3]===100){ base=i; break; }
  if(base<0||b.length<base+14) throw new Error('Это не MIDI-файл (нет заголовка MThd)');
  const u16=p=>(b[p]<<8)|b[p+1], u32=p=>((b[p]<<24)>>>0)+(b[p+1]<<16)+(b[p+2]<<8)+b[p+3];
  const fmtM=u16(base+8), ntr=u16(base+10), div=u16(base+12);
  if(div&0x8000) throw new Error('MIDI с SMPTE-временем не поддерживается');
  if(fmtM>1) throw new Error('MIDI формата 2 не поддерживается (нужен формат 0 или 1)');
  if(!div) throw new Error('Повреждённый MIDI-файл');
  const tracks=[], tempos=[];
  let p=base+8+u32(base+4), tn=0;
  while(p+8<=b.length&&tn<ntr){
    const id=String.fromCharCode(b[p],b[p+1],b[p+2],b[p+3]), end=Math.min(p+8+u32(p+4),b.length);
    p+=8;
    if(id!=='MTrk'){ p=end; continue; }
    tn++;
    const tr={num:tn,name:'',notes:[]}, open=new Map();
    let tick=0, rs=0;
    const vlq=()=>{ let v=0,c; do{ if(p>=end) return v; c=b[p++]; v=v*128+(c&127); }while(c&128); return v; };
    while(p<end){
      tick+=vlq(); if(p>=end) break;
      let s=b[p];
      if(s&128) p++; else if(rs) s=rs; else break;   // running status
      if(s===0xFF){ const ty=b[p++], l=vlq(), q=p; p+=l;
        if(ty===0x51&&l>=3) tempos.push({tick,us:(b[q]<<16)|(b[q+1]<<8)|b[q+2]});
        else if(ty===0x03&&!tr.name) tr.name=melText(b.subarray(q,Math.min(q+l,end)));
        else if(ty===0x2F) break; }
      else if(s===0xF0||s===0xF7){ p+=vlq(); }
      else if(s>0xF0){ p+=(s===0xF2?2:s===0xF3?1:0); }
      else{ rs=s;
        const hi=s&0xF0, ch=s&15, one=(hi===0xC0||hi===0xD0), d1=b[p], d2=one?0:b[p+1];
        p+=one?1:2;
        if(ch===9) continue;   // ударные
        const key=ch*128+d1;
        if(hi===0x90&&d2>0){ if(!open.has(key)) open.set(key,[]); open.get(key).push(tick); }
        else if(hi===0x80||hi===0x90){ const qq=open.get(key);
          if(qq&&qq.length){ const s0=qq.shift(); if(tick>s0) tr.notes.push({s:s0,e:tick,n:d1}); } } }
    }
    open.forEach((qq,key)=>qq.forEach(s0=>{ if(tick>s0) tr.notes.push({s:s0,e:tick,n:key&127}); }));   // незакрытые
    p=end;
    if(tr.notes.length) tracks.push(tr);
  }
  if(!tracks.length) throw new Error('В MIDI-файле нет нот (ударные не считаются)');
  let auto=0, bn=-1, ba=-1;   // «Авто»: больше всего нот, при равенстве — выше средняя высота
  tracks.forEach((t,i)=>{ const avg=t.notes.reduce((a,x)=>a+x.n,0)/t.notes.length;
    if(t.notes.length>bn||(t.notes.length===bn&&avg>ba)){ bn=t.notes.length; ba=avg; auto=i; } });
  return {tracks,auto,toMs:melTempoMap(tempos,div)};
}
// Полифония → монофония «по верхнему голосу»: в каждый момент начала нот — самая высокая
function melMidiRaw(mid,sel){
  const src=sel==='all'?[].concat(...mid.tracks.map(t=>t.notes)):mid.tracks[sel==='auto'?mid.auto:+sel].notes;
  const a=src.slice().sort((x,y)=>x.s-y.s||y.n-x.n), top=[];
  for(const x of a){ const L=top[top.length-1]; if(!L||L.s!==x.s) top.push(x); }
  const raw=[]; let t=0;
  for(let i=0;i<top.length;i++){ const x=top[i], s=mid.toMs(x.s);
    let e=mid.toMs(x.e); if(i+1<top.length) e=Math.min(e,mid.toMs(top[i+1].s));
    if(s>t) raw.push({m:-1,d:s-t});
    if(e>s){ raw.push({m:x.n,d:e-s}); t=e; } else t=Math.max(t,s); }
  return raw;
}
function melHz(m){ return 440*Math.pow(2,(m-69)/12); }
function melPrepare(raw,o){
  if(!raw.some(x=>x.m>=0&&x.d>0)) throw new Error('В источнике не найдено ни одной ноты');
  let k=0;
  if(o.oct==='auto'){ let best=-1;   // сдвиг, при котором больше всего звучания в 700–3500 Гц
    for(const kk of [0,1,-1,2,-2,3,-3,4,-4]){ let s=0;
      for(const x of raw) if(x.m>=0){ const f=melHz(x.m+12*kk); if(f>=700&&f<=3500) s+=x.d; }
      if(s>best){ best=s; k=kk; } } }
  else k=o.oct;
  const sc=100/o.tempo;
  // 1) сдвиг октав, сворачивание в 200–5000 Гц, темп; соседние паузы — вместе
  const a=[];
  for(const x of raw){ if(!(x.d>0)) continue;
    let m=-1; if(x.m>=0){ m=x.m+12*k; while(melHz(m)<200) m+=12; while(melHz(m)>5000) m-=12; }
    const Lp=a[a.length-1]; if(m<0&&Lp&&Lp.m<0) Lp.d+=x.d*sc; else a.push({m,d:x.d*sc}); }
  // 2) ноты короче 25 мс и паузы короче 10 мс — в предыдущий элемент
  const b=[];
  for(const x of a){ const Lp=b[b.length-1];
    if(Lp&&((x.m>=0&&x.d<25)||(x.m<0&&(x.d<10||Lp.m<0)))) Lp.d+=x.d; else b.push(x); }
  // 3) артикуляция: между соседними нотами без паузы — короткая пауза
  const c=[];
  for(let i=0;i<b.length;i++){ const x=b[i], nx=b[i+1];
    if(o.gap>0&&x.m>=0&&nx&&nx.m>=0){ const g=Math.min(o.gap,x.d*0.25); c.push({m:x.m,d:x.d-g},{m:-1,d:g}); }
    else c.push(x); }
  // 4) паузы < 10 мс убрать (время — предыдущей ноте), соседние паузы объединить
  const e=[];
  for(const x of c){ const Lp=e[e.length-1];
    if(x.m<0&&Lp&&Lp.m<0){ Lp.d+=x.d; continue; }
    if(x.m<0&&x.d<10){ if(Lp) Lp.d+=x.d; continue; }
    e.push(x); }
  // 5) паузы по краям — прочь; 6) звуки длиннее 10 с — на части
  while(e.length&&e[0].m<0) e.shift();
  while(e.length&&e[e.length-1].m<0) e.pop();
  const f=[];
  for(const x of e){ const parts=Math.ceil(x.d/10000); for(let i=0;i<parts;i++) f.push({m:x.m,d:x.d/parts}); }
  // 7) ограничения: ≤ 1200 нот и ≤ макс. длительности
  const g=[]; let tot=0, cutT=false, cutN=false;
  for(const x of f){
    if(g.length>=1200){ cutN=true; break; }
    if(tot+x.d>o.maxMs){ const rest=o.maxMs-tot; if(x.m>=0&&rest>=25) g.push({m:x.m,d:rest}); cutT=true; break; }
    g.push(x); tot+=x.d; }
  while(g.length&&g[g.length-1].m<0) g.pop();
  // 8) целые мс (накопленное округление — без дрейфа темпа), частоты — равномерная темперация
  const notes=[]; let acc=0, prev=0, fmin=1e9, fmax=0;
  for(const x of g){ acc+=x.d; let d=Math.round(acc)-prev; d=Math.max(1,Math.min(10000,d)); prev+=d;
    const fr=x.m<0?0:Math.round(melHz(x.m));
    if(fr){ fmin=Math.min(fmin,fr); fmax=Math.max(fmax,fr); }
    notes.push({f:fr,d}); }
  if(!fmax) throw new Error('После подготовки не осталось нот');
  return {notes,k,auto:o.oct==='auto',ms:prev,fmin,fmax,cutT,cutN,maxMs:o.maxMs};
}
// Импорт: файл / RTTTL-текст → (отрезки) → «Подготовить» → «Прослушать» → «В мозг» / «В пульт»; смена параметров пересчитывает сразу.
// v1.4.0: шкала трека с отрезками (H2Mel.SegEditor): склеить в одну мелодию с паузами или каждый отдельно;
// аудио распознаётся только на выбранных отрезках.
const MEL_FILE_HINT='MIDI, RTTTL или аудио (mp3, wav, ogg, m4a)';
let melSrc=null,melMidi=null,melAudio=null,melRaw=null,melLastPieces=null,melRes=null,melBusy=false,melNameAuto=true,melSaving=false,melSeg=null,melTextT=0;
function melSegInit(){if(!melSeg)melSeg=new H2Mel.SegEditor(byId('melSeg'),{getCtx:()=>{try{return melCtx()}catch(e){return null}},onChange:melOnSeg})}
function melMsgSet(id,t,c){const e=byId(id);e.textContent=t;e.className='fine msg'+(c?' '+c:'')}
function melProg(fr,label){const p=byId('melProg');if(fr<0){p.hidden=true;return}p.hidden=false;byId('melProgBar').style.width=Math.round(fr*100)+'%';melMsgSet('melMsg',label+(fr>0?' '+Math.round(fr*100)+' %':''),'')}
function melSetName(n){if(melNameAuto)byId('melName').value=n||'Мелодия'}
function melOpts(){const o=byId('melOct').value;return {oct:o==='auto'?'auto':+o,tempo:+byId('melTempo').value,gap:+byId('melGap').value,maxMs:+byId('melMax').value*1000}}
function melOwnRes(){return melPl&&(melPl.owner==='res'||melPl.owner.charAt(0)==='p')}
function melClearRes(){if(melOwnRes())melStopPreview();melRes=null;byId('melRes').hidden=true;melMsgSet('melSaveMsg','','')}
function melShowSeg(src){melSegInit();melSeg.setSource(src);byId('melSegLbl').hidden=!src}
function melNewSource(){melClearRes();melLastPieces=null;melMidi=null;melAudio=null;melRaw=null;melNameAuto=true;byId('melTrkFld').hidden=true;melMsgSet('melMsg','','');melShowSeg(null)}
async function melOnFile(inp){const f=inp.files[0];inp.value='';if(!f)return;melNewSource();melSrc=null;byId('melText').value='';const ext=(f.name.split('.').pop()||'').toLowerCase();const kind=['mid','midi','kar','rmi'].includes(ext)?'midi':['rtttl','rtx','txt'].includes(ext)?'rtttl':(['mp3','wav','ogg','oga','opus','m4a','aac','flac','webm'].includes(ext)||/^audio\//.test(f.type))?'audio':null;text('melFileName',f.name);if(!kind){text('melFileName',MEL_FILE_HINT);melMsgSet('melMsg','Формат не поддерживается: нужен .mid, .rtttl/.txt или аудиофайл','bad');return}const src={kind,file:f};melSrc=src;melSetName(f.name.replace(/\.[^.]*$/,''));
  try{if(kind==='midi'){melMidi=melParseMidi(await melReadFile(f,false));const s=byId('melTrk');s.textContent='';const add=(v,t)=>{const o=document.createElement('option');o.value=String(v);o.textContent=t;s.appendChild(o)};const lbl=t=>t.num+': '+(t.name||'Дорожка')+' ('+t.notes.length+' '+plural(t.notes.length,'нота','ноты','нот')+')';add('auto','Авто — '+lbl(melMidi.tracks[melMidi.auto]));add('all','Все дорожки (верхний голос)');melMidi.tracks.forEach((t,i)=>add(i,lbl(t)));byId('melTrkFld').hidden=false;melRaw=melMidiRaw(melMidi,'auto');melShowSeg({kind:'notes',raw:melRaw})}
    else if(kind==='rtttl'){const r=melParseRtttl(await melReadFile(f,true));if(r.name)melSetName(r.name);melRaw=r.raw;melShowSeg({kind:'notes',raw:melRaw})}
    else{melMsgSet('melMsg','Декодирование аудио…','');const ab=await melReadFile(f,false),buf=await H2Mel.decode(melCtx(),ab);if(src!==melSrc)return;melAudio={file:f,buffer:buf,cache:new Map()};melShowSeg({kind:'audio',buffer:buf});melMsgSet('melMsg','Длительность '+fmt(buf.duration,1)+' с. Выдели отрезки на шкале или нажми «Подготовить» — возьмётся начало трека.','')}}
  catch(e){if(src!==melSrc)return;melSrc=null;melShowSeg(null);melMsgSet('melMsg',(e&&e.message)||String(e),'bad')}}
function melOnText(){const has=byId('melText').value.trim()!=='';if(!melSrc||melSrc.kind!=='text'){melNewSource();text('melFileName',MEL_FILE_HINT)}else melClearRes();melSrc=has?{kind:'text'}:null;clearTimeout(melTextT);melTextT=setTimeout(()=>{if(!melSrc||melSrc.kind!=='text')return;try{const r=melParseRtttl(byId('melText').value);melSetName(r.name);melRaw=r.raw;if(melSeg&&melSeg.src&&melSeg.src.kind==='notes')melSeg.updateNotes(melRaw);else melShowSeg({kind:'notes',raw:melRaw})}catch(e){melRaw=null;melShowSeg(null)}},400)}
function melSegs(){return melSeg&&melSeg.src?melSeg.segs.map(s=>({a:Math.round(s.a),b:Math.round(s.b),gap:s.gap|0})):[]}
async function melGetPieces(){if(!melSrc)throw new Error('Выбери файл или вставь RTTTL-текст');const segs=melSegs(),join=!melSeg||melSeg.join;
  if(melSrc.kind!=='audio'){if(melSrc.kind==='text'){const r=melParseRtttl(byId('melText').value);melSetName(r.name);melRaw=r.raw}if(!melRaw)throw new Error('В источнике не найдено нот');return H2Mel.pieces(segs,join,s=>H2Mel.slice(melRaw,s.a,s.b),()=>melRaw)}
  const A=melAudio;if(!A)throw new Error('Аудио ещё не декодировано');const parts=segs.length?segs:[{a:0,b:Math.min(A.buffer.duration*1000,+byId('melSecs').value*1000),gap:0}],tot=parts.reduce((s,x)=>s+x.b-x.a,0);
  if(tot>180000)throw new Error('Отрезки длиннее 3 минут в сумме — распознавание будет слишком долгим, сократи их');
  let done=0;const raws=[];for(const p of parts){const key=p.a+'-'+p.b;if(!A.cache.has(key))A.cache.set(key,await H2Mel.pitchTrack(A.buffer,p.a,p.b,fr=>melProg((done+fr*(p.b-p.a))/tot,'Анализ аудио…')));done+=p.b-p.a;raws.push(A.cache.get(key))}
  if(!raws.some(r=>r.some(x=>x.m>=0)))throw new Error('В аудио не найдено устойчивых нот — лучше всего подходят простые одноголосые рингтоны');let i=0;return H2Mel.pieces(segs,join,()=>raws[i++],()=>raws[0])}
function melPieceName(i){const b=byId('melName').value.trim()||'Мелодия';return melRes&&melRes.list.length>1?b+' '+(i+1):b}
function melRunPrep(pieces){if(melOwnRes())melStopPreview();const o=melOpts(),list=[],skipped=[];pieces.forEach((p,i)=>{try{list.push(melPrepare(p,o))}catch(e){skipped.push(i+1)}});if(!list.length)throw new Error('После подготовки не осталось нот');melRes={list,skipped};melLastPieces=pieces;
  const r=list[0],ms=list.reduce((s,x)=>s+x.ms,0),n=list.reduce((s,x)=>s+x.notes.length,0),fmin=Math.min(...list.map(x=>x.fmin)),fmax=Math.max(...list.map(x=>x.fmax)),ex=['сдвиг октав '+(r.k>0?'+':'')+String(r.k).replace('-','−')+(r.auto?' (авто)':'')];
  if(list.some(x=>x.cutT))ex.push('обрезано до '+(r.maxMs/1000)+' с');if(list.some(x=>x.cutN))ex.push('обрезано до 1200 нот');if(skipped.length)ex.push('без нот, пропущены отрезки: '+skipped.join(', '));
  text('melStat',(list.length>1?list.length+' '+plural(list.length,'мелодия','мелодии','мелодий')+' · ':'')+n+' '+plural(n,'нота','ноты','нот')+' · '+fmt(ms/1000,1)+' с · '+fmin+'–'+fmax+' Гц');text('melStat2',ex.join(' · '));
  const box=byId('melPieces');box.textContent='';if(list.length>1)list.forEach((x,i)=>{const row=document.createElement('div');row.className='melody';const idx=document.createElement('span');idx.className='track-index';idx.textContent=String(i+1).padStart(2,'0');const t=document.createElement('span');t.className='track-title';t.textContent=melPieceName(i);const h=document.createElement('small');h.textContent=x.notes.length+' '+plural(x.notes.length,'нота','ноты','нот')+' · '+fmt(x.ms/1000,1)+' с';t.appendChild(h);const ear=iconButton('i-ear','Прослушать на телефоне: '+melPieceName(i),()=>melListenPiece(i),'icon-only');ear.dataset.pv='p'+i;ear.dataset.name=melPieceName(i);row.append(idx,t,ear);box.appendChild(row)});
  byId('melRes').hidden=false;melMsgSet('melSaveMsg','','');melPlUi()}
async function melPrepareClick(){if(melBusy)return;const btn=byId('melPrepBtn'),src=melSrc;melBusy=true;setOff(btn,true);melMsgSet('melMsg','','');try{melCtx();const pieces=await melGetPieces();if(src!==melSrc){melMsgSet('melMsg','Источник изменён — нажми «Подготовить».','');return}melRunPrep(pieces);melMsgSet('melMsg','Готово — прослушай и сохрани.','ok')}catch(e){if(src!==melSrc)return;melClearRes();melMsgSet('melMsg',(e&&e.message)||String(e),'bad')}finally{melBusy=false;setOff(btn,false);restoreFocus();melProg(-1)}}
function melReprep(){if(!melLastPieces)return;try{melRunPrep(melLastPieces)}catch(e){melClearRes();melMsgSet('melMsg',e.message,'bad')}}
function melOnSeg(){if(!melRes||melBusy)return;if(melSrc&&melSrc.kind==='audio'){melClearRes();melLastPieces=null;melMsgSet('melMsg','Отрезки изменены — нажми «Подготовить».','');return}melGetPieces().then(melRunPrep).catch(e=>{melClearRes();melMsgSet('melMsg',e.message,'bad')})}
function melParamLabels(){const t=byId('melTempo'),g=byId('melGap'),m=byId('melMax'),s=byId('melSecs');[t,g,m,s].forEach(rangePaint);text('melTempoV',t.value+' %');text('melGapV',g.value+' мс');text('melMaxV',m.value+' с');text('melSecsV',s.value+' с')}
function melOnParam(){melParamLabels();if(melRes&&!melBusy)melReprep()}
function melOnSecs(){melParamLabels();if(melSrc&&melSrc.kind==='audio'&&!melSegs().length&&melRes&&!melBusy){melClearRes();melLastPieces=null;melMsgSet('melMsg','Длительность анализа изменена — нажми «Подготовить».','')}}
function melOnTrack(){if(!melMidi)return;melRaw=melMidiRaw(melMidi,byId('melTrk').value);melSeg.updateNotes(melRaw)}
function melPvMusic(){return byId('melPvAs').value==='remote'}
function melOnPvAs(){if(melOwnRes())melListen()}
function melListen(){if(melPl&&melPl.owner==='res'){melStopPreview();return}if(!melRes)return;try{const all=[];melRes.list.forEach((x,i)=>{if(i)all.push({f:0,d:600});all.push(...x.notes)});melPlay(all,'res',melPvMusic())}catch(e){melMsgSet('melSaveMsg',e.message,'bad')}}
function melListenPiece(i){if(melPl&&melPl.owner==='p'+i){melStopPreview();return}try{melPlay(melRes.list[i].notes,'p'+i,melPvMusic())}catch(e){melMsgSet('melSaveMsg',e.message,'bad')}}
function melFiles(){return melRes.list.map((x,i)=>({name:melPieceName(i),bytes:melBuildH2M(melPieceName(i),x.notes)}))}
function melUploadBody(bytes){const B='h2mel'+Date.now().toString(36);return {method:'POST',type:'multipart/form-data; boundary='+B,body:bufToB64(multipart(B,'f','melody.h2m',bytes)),timeout:20000}}
function melSaveReady(){if(!melRes||melSaving)return false;if(!online){melMsgSet('melSaveMsg','Нет связи с мозгом','bad');return false}if(otaBusy){melMsgSet('melSaveMsg','Дождись окончания обновления прошивки','bad');return false}return true}
// загрузка в мозг — multipart POST /snd/up (поле "f", melody.h2m) через мост, по одной мелодии
async function melSave(){if(!melSaveReady())return;const files=melFiles();if(soundCfg&&!soundCfg.fsok){melMsgSet('melSaveMsg','Память мелодий (LittleFS) недоступна','bad');return}if(soundCfg&&(soundCfg.list||[]).length+files.length>16){melMsgSet('melSaveMsg','Не хватит слотов мозга (16) — удали ненужные мелодии','bad');return}const btn=byId('melSaveBtn');melSaving=true;setOff(btn,true);let ok=0;
  try{for(const f of files){melMsgSet('melSaveMsg','Загрузка «'+f.name+'»…','');const t=await brain('/snd/up',melUploadBody(f.bytes));let j=null;try{j=JSON.parse(t)}catch(e){}if(!(j&&j.ok))throw new Error((j&&j.err)||t||'Ошибка загрузки');ok++}melMsgSet('melSaveMsg','Сохранено в мозг: '+ok+'. Мелодию можно выбрать для события выше.','ok');toast(ok>1?'Мелодии сохранены':'Мелодия сохранена')}
  catch(e){melMsgSet('melSaveMsg',(ok?'Сохранено '+ok+', затем ошибка: ':'')+e.message,'bad')}finally{melSaving=false;setOff(btn,false);restoreFocus();loadSound()}}
// v1.4.0: загрузка в пульт — POST /rmel/up, мозг передаёт файл по радио; ждём итог по /rmel/cfg
async function melSaveRemote(){if(!melSaveReady())return;const btn=byId('melRmBtn');melSaving=true;setOff(btn,true);
  try{const n=await H2Mel.uploadRemote(melFiles(),b=>brain('/rmel/up',melUploadBody(b)).then(t=>{try{return JSON.parse(t)}catch(e){return {ok:0,err:t}}}),rmCfgJson,t=>melMsgSet('melSaveMsg',t,''));melMsgSet('melSaveMsg','В пульт загружено: '+n+'. Выбери её для кнопки МЕЛОДИЯ в блоке «Пульт».','ok');toast('Мелодия в пульте')}
  catch(e){melMsgSet('melSaveMsg',e.message,'bad')}finally{melSaving=false;setOff(btn,false);restoreFocus();loadRemote(true)}}

// ── v1.4.0: Мелодии пульта (/rmel/*) — опись пульт присылает мозгу по радио ──
let rmCfg=null,rmDirty=false,rmTimer=0,rmBusy=false;
function rmCfgJson(){return brain('/rmel/cfg').then(t=>{const c=JSON.parse(t);rmCfg=c;renderRemote();return c})}
async function loadRemote(force){clearTimeout(rmTimer);try{const c=await rmCfgJson(),st=c.job&&c.job.st,active=st==='send'||st==='check';if(currentTab==='sound'&&(active||(c.online&&!c.complete)||force===true&&!c.known))rmTimer=setTimeout(loadRemote,active?500:900)}catch(e){text('rmState','нет ответа')}}
function rmFillSel(id,val,first){const s=byId(id),want=rmDirty&&s.value!==''?s.value:String(val);s.textContent='';const add=(v,t)=>{const o=document.createElement('option');o.value=String(v);o.textContent=t;s.appendChild(o)};add(first[0],first[1]);((rmCfg&&rmCfg.list)||[]).forEach(m=>add(m.id,m.name));s.value=want;if(s.value!==want)s.value=String(first[0])}
function renderRemote(){const c=rmCfg;if(!c)return;text('rmState',!c.online?'Нет связи':c.known?c.list.length+' из '+c.slots:'На связи');const j=c.job||{},act=j.st==='send'||j.st==='check';byId('rmProg').hidden=!act;if(act)byId('rmProgBar').style.width=(j.pct|0)+'%';
  const box=byId('rmList');box.textContent='';const note=t=>{const p=document.createElement('p');p.className='fine';p.textContent=t;box.appendChild(p)};
  if(!c.known)note(c.online?'Запрашиваю список у пульта…':'Пульт не на связи — включи его.');else if(!c.fsok)note('Память пульта (LittleFS) недоступна.');else if(!c.list.length)note('В пульте только «Бумер» — добавь свою мелодию ниже (кнопка «В пульт»).');
  else c.list.forEach((m,i)=>{const row=document.createElement('div');row.className='melody';const idx=document.createElement('span');idx.className='track-index';idx.textContent=String(i+1).padStart(2,'0');const t=document.createElement('span');t.className='track-title';t.textContent=m.name;const h=document.createElement('small');h.textContent=(m.ms?m.n+' '+plural(m.n,'нота','ноты','нот')+' · '+fmt(m.ms/1000,1)+' с':'…')+(m.id===c.act?' · играет по кнопке':'')+(m.id===c.boot?' · при включении':'');t.appendChild(h);const del=iconButton('i-x','Удалить из пульта: '+m.name,()=>rmDel(m.id),'icon-only danger-text');del.dataset.control='';row.append(idx,t,del);box.appendChild(row)});
  if(c.known){rmFillSel('rmAct',c.act,[-1,'«Бумер» (встроенный)']);rmFillSel('rmBoot',c.boot,[-9,'Нет'])}updateControls()}
async function rmApply(){if(!rmCfg||!rmCfg.known){toast('Список пульта ещё не получен',true);loadRemote(true);return}const r=await action('/rmel/set?act='+byId('rmAct').value+'&boot='+byId('rmBoot').value,null);if(r===false)return;let j=null;try{j=JSON.parse(r)}catch(e){}if(!j||!j.ok){toast((j&&j.err)||'Пульт не принял настройку',true);return}rmDirty=false;toast('Отправлено на пульт');setTimeout(()=>loadRemote(true),600)}
async function rmDel(id){const m=((rmCfg&&rmCfg.list)||[]).find(x=>x.id===id);if(!await ask('Удалить из пульта?','«'+(m?m.name:id)+'» будет удалена из памяти пульта. Если она играла по кнопке, вернётся «Бумер».','Удалить'))return;const r=await action('/rmel/del?id='+id,null);if(r===false)return;let j=null;try{j=JSON.parse(r)}catch(e){}if(!j||!j.ok){toast((j&&j.err)||'Пульт не ответил',true);return}toast('Удалено из пульта');setTimeout(()=>loadRemote(true),600)}
// мелодия из библиотеки мозга → пульт (мозг читает свой файл и передаёт по радио)
async function rmSendBrain(id){if(rmBusy)return;if(!online){toast('Нет связи с мозгом',true);return}rmBusy=true;const m=userMelodies().find(x=>x.id===id);try{await H2Mel.uploadRemote([{name:m?m.name:'мелодия',bytes:null}],()=>brain('/rmel/send?id='+id).then(t=>{try{return JSON.parse(t)}catch(e){return {ok:0,err:t}}}),rmCfgJson,t=>text('rmState',t));toast('Мелодия в пульте — выбери её в блоке «Пульт»')}catch(e){toast(e.message,true)}finally{rmBusy=false;loadRemote(true)}}
// ── v1.5.0: пульт — телеметрия и настройки (/rset), режим отладки (/dbg) ──
const CMD_NAME={1:'Вкл/выкл системы',2:'Блокировка / сброс защёлок',3:'Репульсор: выстрел газ + поджиг',4:'Поджиг 500 мс',5:'Мини-продувка 200 мс',7:'Ручная подача газа вкл/выкл',8:'Цель +0,5 бар',9:'Цель −0,5 бар',12:'Заглушить тревогу',99:'Продувка до нуля'};
const DBG_SRC=['Мозг','Пульт','Веб','Приложение','Радио'];
const DBG_ACT={1:'нажатие',2:'двойное',3:'удержание',4:'в меню',5:'в игре'};
// [название, за что отвечает] — кнопки мозга и пульта (коды из прошивки)
const BTN_BRAIN=[['POWER','1× вкл/выкл системы, 2× продувка до нуля'],['LOCK','Блокировка, сброс защёлок'],['ПРОДУВКА','Мини-продувка 200 мс']];
const BTN_REMOTE=[['POWER','1× вкл/выкл, 2× или удержание 1 с — продувка до нуля'],['LOCK','Коротко — блокировка и сброс защёлок, удержание 1 с — заглушить тревогу'],['РЕПУЛЬСОР','Выстрел: клапан 800 мс, поджиг с 700-й мс'],['РУЧНАЯ ПОДАЧА','Газ вкл/выкл'],['ПОДЖИГ','Поджиг 500 мс'],['ЦЕЛЬ +','Цель +0,5 бар; в меню — вверх'],['ЦЕЛЬ −','Цель −0,5 бар; в меню — вниз; будит пульт'],['МЕЛОДИЯ','1× музыка вкл/выкл, 2× игра, удержание 1 с — меню; будит пульт']];
// Что сделало локальное действие пульта (команда 0)
function remoteLocal(b,a){if(b===7)return a===2?'Игра «динозаврик»: старт/выход':a===3?'Меню пульта: открыть/закрыть':a===5?'Игра: прыжок/рестарт':a===4?'Меню: выбрать/изменить':'Музыка вкл/выкл';if(a===4)return 'Меню: '+(b===5?'вверх':'вниз');return 'Только на пульте'}
function dbgMeaning(e){if(e.c)return CMD_NAME[e.c]||('Команда '+e.c);if(e.src===1)return remoteLocal(e.b,e.a);return '—'}
function dbgButton(e){if(e.src===0)return (BTN_BRAIN[e.b]||['?'])[0];if(e.src===1)return (BTN_REMOTE[e.b]||['?'])[0];return e.src===4?'команда принята':'команда'}
let dbgSeq=0,dbgLog=[],dbgTimer=0,dbgBusy=false;
function debugOn(){return !!preferences.debug}
function debugPaint(){byId('debugCard').hidden=!debugOn();byId('debugMode').checked=debugOn();clearTimeout(dbgTimer);if(debugOn()&&currentTab==='diag'){renderDebugLegend();dbgPoll()}}
async function dbgPoll(){clearTimeout(dbgTimer);if(!debugOn()||currentTab!=='diag')return;if(online&&!dbgBusy&&!document.hidden){dbgBusy=true;try{const j=JSON.parse(await brain('/dbg?since='+dbgSeq)),now=Date.now();if(j.seq<dbgSeq)dbgLog=[];j.ev.forEach(e=>{e.at=now-e.t;dbgLog.unshift(e)});dbgLog=dbgLog.slice(0,60);dbgSeq=j.seq;renderDebugLog()}catch(e){}finally{dbgBusy=false}}dbgTimer=setTimeout(dbgPoll,700)}
function renderDebugLog(){const box=byId('debugLog');box.textContent='';if(!dbgLog.length){const p=document.createElement('p');p.className='fine';p.textContent='Нажми любую кнопку на мозге или пульте — запись появится здесь.';box.appendChild(p);return}dbgLog.forEach(e=>{const row=document.createElement('div');row.className='dbg-row';const t=document.createElement('span');t.className='dbg-time';t.textContent=new Date(e.at).toLocaleTimeString('ru-RU');const src=document.createElement('span');src.className='dbg-src s'+e.src;src.textContent=DBG_SRC[e.src]||'?';const what=document.createElement('span');what.className='dbg-what';const b=document.createElement('strong');b.textContent=dbgButton(e)+(e.a&&DBG_ACT[e.a]?' · '+DBG_ACT[e.a]:'');const m=document.createElement('small');m.textContent=dbgMeaning(e)+(e.c?' (cmd '+e.c+')':'');what.append(b,m);row.append(t,src,what);box.appendChild(row)})}
function renderDebugLegend(){const box=byId('debugLegend');if(box.childElementCount)return;[['Мозг',BTN_BRAIN],['Пульт',BTN_REMOTE]].forEach(([dev,list])=>{const h=document.createElement('strong');h.className='dbg-dev';h.textContent=dev;box.appendChild(h);list.forEach(([n,d])=>{const r=document.createElement('div');r.className='dbg-kv';const k=document.createElement('span');k.textContent=n;const v=document.createElement('span');v.textContent=d;r.append(k,v);box.appendChild(r)})})}
const onOff=v=>v?'вкл':'выкл';
function renderDebugState(d){if(!debugOn())return;const rows=[['Система',onOff(d.pwr)],['Блокировка',d.lck?'да':'нет'],['Электролизёр (MOSFET)',onOff(d.ele)],['Клапан',d.val?'открыт':'закрыт'],['Поджиг',onOff(d.pzo)],['Продувка до нуля',d.prg?'идёт':'нет'],['Мини-продувка',d.psh?'идёт':'нет'],['Репульсор',d.rep?'выстрел':'нет'],['Ручной газ',onOff(d.mgs)],['Давление / цель',fmt(d.p,2)+' / '+fmt(d.tgt,1)+' бар'],['Датчик давления',d.psf?'НЕИСПРАВЕН ('+fmt(d.pv,2)+' В)':fmt(d.pv,2)+' В'],['Защёлка давления',d.ovl?'АВАРИЯ':d.ovw?'предупреждение':'нет'],['Наклон',d.tlk?'АВАРИЯ '+d.ta+'°':d.mpuf?'датчик не отвечает':d.ta+'°'],['Температура',d.tok?fmt(d.t,1)+' °C'+(d.oh?' ПЕРЕГРЕВ':''):'нет данных'],['Утечка',d.lkl?'АВАРИЯ':d.lkw?'подозрение':'нет'],['Бак не набирает',d.wd?'да':'нет'],['Выводной вентилятор',onOff(d.f2)],['Звук',['пульт','мозг','оба','выкл'][d.sout]+', громк. '+d.svol+(d.sp?', играет':'')],['Пульт: связь',d.rf?'есть':'нет'],['Пульт: принято пакетов',d.rinf>=0&&d.rinf<15?d.rrx+' %':'—'],['Пульт: батарея',d.rinf>=0&&d.rinf<15?(d.rbat>=0?d.rbat+' % ('+(d.rbmv/1000).toFixed(2)+' В)':d.rbaten?'нет аккумулятора':'не отслеживается'):'—'],['Пульт: прошивка',d.rfw?'v'+(d.rfw/10).toFixed(1):'—'],['Радиоканал',d.rfc+' · перескоков '+d.rfh],['Аптайм мозга',Math.floor(d.up/3600)+' ч '+Math.floor(d.up%3600/60)+' мин']];const box=byId('debugState');if(box.childElementCount!==rows.length){box.textContent='';rows.forEach(()=>{const r=document.createElement('div');r.className='dbg-kv';r.append(document.createElement('span'),document.createElement('span'));box.appendChild(r)})}rows.forEach((x,i)=>{const r=box.children[i];if(r.children[0].textContent!==x[0])r.children[0].textContent=x[0];if(r.children[1].textContent!==x[1])r.children[1].textContent=x[1];r.classList.toggle('alarm',/АВАРИЯ|НЕИСПРАВЕН|ПЕРЕГРЕВ/.test(x[1]))})}
// Пульт в настройках: заряд, связь, настройки (/rset — мозг передаёт пульту по радио)
let remoteSetBusy=false;
function renderRemoteInfo(d){const fresh=d.rinf>=0&&d.rinf<15;text('remoteInfo',!fresh?'Пульт не на связи или прошивка до v6.8':(d.rbat>=0?'Заряд '+d.rbat+' % · '+(d.rbmv/1000).toFixed(2)+' В':d.rbaten?'Аккумулятор не обнаружен (питание по USB?)':'Батарея не отслеживается')+' · пакетов '+d.rrx+' % · v'+(d.rfw/10).toFixed(1));if(remoteSetBusy||!fresh)return;byId('remoteBat').checked=!!d.rbaten;byId('remoteSleep').value=String(d.rslp);const c=byId('remoteCon');if(document.activeElement!==c){c.value=d.rcon;rangeOutput(c,'remoteConV',0,'/ 10')}}
async function setRemote(q){if(!online){toast('Нет связи с мозгом',true);return}remoteSetBusy=true;try{const j=JSON.parse(await brain('/rset?'+q));if(!j.ok)throw new Error(j.err||'Пульт не принял настройку');toast('Отправлено на пульт')}catch(e){toast(e.message,true)}finally{setTimeout(()=>{remoteSetBusy=false},6000)}}
function applyPreferences(){const dark=preferences.theme==='dark'||preferences.theme==='auto'&&systemDark.matches;document.documentElement.dataset.theme=dark?'dark':'light';document.querySelector('meta[name="theme-color"]').content=dark?'#171a18':'#f1f0ec';document.querySelectorAll('[data-theme-choice]').forEach(el=>{const selected=preferences.theme===el.dataset.themeChoice;el.classList.toggle('selected',selected);el.classList.toggle('current',preferences.theme==='auto'&&el.dataset.themeChoice===(dark?'dark':'light'));el.setAttribute('aria-pressed',String(selected))});byId('systemTheme').checked=preferences.theme==='auto';byId('keepAwake').checked=preferences.awake;byId('haptics').checked=preferences.haptics;document.querySelectorAll('[data-refresh]').forEach(el=>{const selected=Number(el.dataset.refresh)===preferences.refresh;el.classList.toggle('selected',selected);el.setAttribute('aria-pressed',String(selected))});document.querySelectorAll('[data-digits]').forEach(el=>{const selected=Number(el.dataset.digits)===preferences.digits;el.classList.toggle('selected',selected);el.setAttribute('aria-pressed',String(selected))});if(state&&online){text('pressure',fmt(state.p,preferences.digits));fitPressure()}try{if(window.Brain&&Brain.appearance)Brain.appearance(dark,preferences.awake)}catch(e){}try{if(window.Brain&&Brain.monitorEnabled)byId('monitor').checked=Brain.monitorEnabled()}catch(e){}monitorPaint();debugPaint();if(currentTab==='history')drawCharts()}
function restartPolling(){clearInterval(pollTimer);pollTimer=setInterval(poll,preferences.refresh)}
function setPreference(key,value){preferences[key]=value;try{localStorage.setItem('h2.preferences',JSON.stringify(preferences))}catch(e){toast('Не удалось сохранить настройки',true)}applyPreferences();if(key==='refresh')restartPolling();haptic()}
function openWifi(){try{if(window.Brain&&Brain.openWifiSettings){Brain.openWifiSettings();return}}catch(e){}toast('Открой Wi-Fi телефона и выбери H2_PRO')}
if(systemDark.addEventListener)systemDark.addEventListener('change',()=>{if(preferences.theme==='auto')applyPreferences()});
document.addEventListener('visibilitychange',()=>{if(document.hidden)clearInterval(pollTimer);else{applyPreferences();poll();restartPolling()}});
window.addEventListener('resize',()=>{fitPressure();if(currentTab==='history')drawCharts()});
loadHistory();document.querySelectorAll('input[type=range]').forEach(rangePaint);targetPaint();melParamLabels();renderLibrary();applyPreferences();setOnline(false);poll();restartPolling();
