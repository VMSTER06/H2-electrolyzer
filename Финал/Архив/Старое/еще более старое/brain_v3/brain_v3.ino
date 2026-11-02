/*
 * =====================================================================
 *  H2 CONTROL SYSTEM — МОЗГ v3.0  (ESP32 DevKit v1)
 * =====================================================================
 *  Библиотеки (Менеджер библиотек Arduino IDE):
 *    - RF24                  (by TMRh20)
 *    - ESPAsyncWebServer     (by me-no-dev)
 *    - AsyncTCP              (by me-no-dev)   ← ESP32, НЕ ESPAsyncTCP!
 *    - Adafruit SSD1306      (by Adafruit)
 *    - Adafruit GFX Library  (by Adafruit)
 *
 *  СХЕМА ПОДКЛЮЧЕНИЯ:
 *    NRF24L01:  CE=GPIO4   CSN=GPIO5
 *               SCK=GPIO18 MISO=GPIO19 MOSI=GPIO23
 *               VCC=3.3V  + конденсатор 470мкФ между VCC и GND!
 *
 *    OLED SSD1306 (I2C):
 *               SDA=GPIO21  SCL=GPIO22  VCC=3.3V
 *
 *    Реле электролизера : GPIO25
 *    Реле клапана        : GPIO26
 *    Реле пьезо/поджига  : GPIO27
 *
 *    Кнопка POWER (на GND): GPIO16 — встроенный INPUT_PULLUP
 *    Датчик давления      : GPIO34 — через делитель 10кОм/20кОм
 *
 *  Wi-Fi точка доступа: SSID="H2_Control"  PWD="hydrogen1"
 *  IP: 192.168.4.1
 * =====================================================================
 */

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ======================== Wi-Fi ========================
const char* AP_SSID     = "H2_Control";
const char* AP_PASSWORD = "hydrogen1";

// ======================== ПИНЫ ========================
#define PIN_RELAY_ELECTRO  25
#define PIN_RELAY_VALVE    26
#define PIN_RELAY_PIEZO    27
#define PIN_BUTTON         16   // INPUT_PULLUP, кнопка замкнута на GND
#define PIN_PRESSURE       34   // ADC1 (только вход, нет pullup)
#define PIN_NRF_CE          4
#define PIN_NRF_CSN         5
// SPI по умолчанию ESP32 VSPI: SCK=18, MISO=19, MOSI=23

// OLED I2C — по умолчанию ESP32: SDA=21, SCL=22

// ======================== РЕЛЕ ========================
// true  = LOW-active  (большинство китайских модулей: LOW = включено)
// false = HIGH-active
#define RELAY_ACTIVE_LOW  true
#define RELAY_ON          (RELAY_ACTIVE_LOW ? LOW  : HIGH)
#define RELAY_OFF         (RELAY_ACTIVE_LOW ? HIGH : LOW)

// ======================== ТАЙМИНГИ (мс) ========================
const unsigned long T_PIEZO_WORK    = 500;
const unsigned long T_PURGE         = 5000;
const unsigned long T_DOUBLE_CLICK  = 400;
const unsigned long T_DEBOUNCE      = 50;
const unsigned long T_STATUS_TX     = 500;   // Период отправки статуса на пульт
const unsigned long T_DISPLAY_UPD   = 250;   // Период обновления дисплея
const unsigned long T_PRESSURE_SAMP = 100;   // Период выборки давления
const unsigned long T_RF_TIMEOUT    = 3000;  // Таймаут связи с пультом

// ======================== NRF24L01 ========================
RF24 radio(PIN_NRF_CE, PIN_NRF_CSN);
const byte PIPE_R2B[6] = "CMD01";  // Remote → Brain
const byte PIPE_B2R[6] = "STS01";  // Brain  → Remote

// Пакет команды (Remote → Brain)
struct __attribute__((packed)) CmdPacket {
  uint8_t type;   // 0x55
  uint8_t cmd;
};

// Пакет статуса (Brain → Remote)
struct __attribute__((packed)) StatusPacket {
  uint8_t type;       // 0xAA
  float   pressure;   // текущее давление, бар
  uint8_t flags;      // битовые флаги (см. ниже)
  uint8_t targetBar;  // целевое давление: 1 или 4
};
// flags: bit0=systemOn  bit1=isLocked  bit2=isPurging
//        bit3=constGas  bit4=manualGas
//        bit5=electroOn bit6=valveOn   bit7=piezoOn

// ======================== OLED ========================
#define OLED_W 128
#define OLED_H  64
Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);
bool oledOk = false;

// ======================== ВЕБ-СЕРВЕР ========================
AsyncWebServer server(80);

// ======================== СОСТОЯНИЕ СИСТЕМЫ ========================
bool  systemOn        = false;
bool  isLocked        = true;
bool  isPurging       = false;
bool  constGasActive  = false;
bool  manualGasActive = false;
bool  piezoActive     = false;
bool  cgPiezoFired    = false;
bool  electroOn       = false;
bool  valveOn         = false;
float targetPressure  = 1.0f;
float currentPressure = 0.0f;

// RF связь
bool          rfOk        = false;
unsigned long lastRfRx    = 0;

// Антидребезг кнопки
bool          btnLastRaw   = HIGH;
bool          btnStable    = HIGH;
unsigned long btnDebTime   = 0;
unsigned long lastClickMs  = 0;

// Таймеры
unsigned long purgeStart    = 0;
unsigned long piezoStart    = 0;
unsigned long lastStatusTx  = 0;
unsigned long lastDispUpd   = 0;

// Усреднение давления
float         pSum   = 0.0f;
int           pCnt   = 0;
unsigned long pLast  = 0;

// ======================== HTML ИНТЕРФЕЙС ========================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>H2 Control</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;600;800&display=swap');
  :root{
    --bg:#080810;--surface:#0e0e1c;--card:#131326;--border:#1e1e3a;
    --accent:#00d4ff;--accent2:#4d8aff;--ok:#00ff88;--warn:#ffcc00;
    --danger:#ff3355;--dim:#3a3a5a;--text:#c8d0e0;
  }
  *{box-sizing:border-box;margin:0;padding:0}
  body{
    background:var(--bg);color:var(--text);
    font-family:'Exo 2',sans-serif;
    min-height:100vh;padding:14px 10px;
    background-image:
      radial-gradient(ellipse at 20% 0%,rgba(0,212,255,.07) 0%,transparent 60%),
      radial-gradient(ellipse at 80% 100%,rgba(77,138,255,.07) 0%,transparent 60%);
  }
  h1{
    text-align:center;font-size:1.1em;font-weight:800;
    letter-spacing:6px;color:var(--accent);text-transform:uppercase;
    margin-bottom:14px;text-shadow:0 0 20px rgba(0,212,255,.5);
  }
  .panel{
    max-width:440px;margin:0 auto;
    display:flex;flex-direction:column;gap:10px;
  }

  /* давление */
  .pressure{
    background:var(--card);border:1px solid var(--border);
    border-radius:14px;padding:16px;text-align:center;
    position:relative;overflow:hidden;
  }
  .pressure::before{
    content:'';position:absolute;top:0;left:0;right:0;height:2px;
    background:linear-gradient(90deg,transparent,var(--accent),transparent);
  }
  .p-lbl{font-size:.65em;letter-spacing:4px;color:var(--dim);text-transform:uppercase;margin-bottom:6px}
  .p-num{
    font-family:'Share Tech Mono',monospace;
    font-size:3.4em;color:var(--accent);line-height:1;
    text-shadow:0 0 30px rgba(0,212,255,.6);
  }
  .p-unit{font-size:.4em;color:#5577aa;margin-left:6px}
  .p-bar{height:4px;background:#111128;border-radius:2px;margin-top:10px;overflow:hidden}
  .p-fill{height:100%;border-radius:2px;background:linear-gradient(90deg,var(--ok),var(--accent));transition:width .8s ease}

  /* бейджи */
  .badges{display:flex;flex-wrap:wrap;gap:6px}
  .badge{
    padding:4px 10px;border-radius:20px;font-size:.62em;
    font-weight:600;letter-spacing:1.5px;text-transform:uppercase;
    background:var(--card);color:var(--dim);border:1px solid var(--border);
    transition:all .25s;
  }
  .badge.ok   {background:#001a0e;color:var(--ok);  border-color:var(--ok);  box-shadow:0 0 8px rgba(0,255,136,.2)}
  .badge.warn {background:#1a1500;color:var(--warn); border-color:var(--warn);box-shadow:0 0 8px rgba(255,204,0,.2)}
  .badge.danger{
    background:#1a0008;color:var(--danger);border-color:var(--danger);
    animation:pulse .9s infinite;
  }
  @keyframes pulse{0%,100%{box-shadow:0 0 6px rgba(255,51,85,.3)}50%{box-shadow:0 0 14px rgba(255,51,85,.7)}}

  /* кнопка питания */
  .btn-power{
    width:100%;padding:16px;border:none;border-radius:12px;cursor:pointer;
    font-family:'Exo 2',sans-serif;font-size:.9em;font-weight:800;
    letter-spacing:3px;text-transform:uppercase;transition:all .2s;
    background:var(--card);color:var(--text);border:1px solid var(--border);
  }
  .btn-power.on{
    background:linear-gradient(135deg,#1a0010,#330015);
    color:var(--danger);border-color:var(--danger);
    box-shadow:0 0 20px rgba(255,51,85,.2);
  }
  .btn-power:active{transform:scale(.97)}

  /* управление */
  .grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
  .ctrl{
    padding:14px 8px;border:1px solid var(--border);border-radius:11px;
    background:var(--card);cursor:pointer;
    font-family:'Exo 2',sans-serif;font-size:.7em;font-weight:600;
    letter-spacing:1px;text-transform:uppercase;
    color:var(--text);transition:all .2s;
    display:flex;flex-direction:column;align-items:center;gap:8px;
  }
  .ctrl .dot{width:8px;height:8px;border-radius:50%;background:var(--dim);transition:all .25s}
  .ctrl.wide{grid-column:1/-1}
  .ctrl:hover:not(:disabled){filter:brightness(1.2)}
  .ctrl:active:not(:disabled){transform:scale(.95)}
  .ctrl:disabled{opacity:.25;cursor:not-allowed}
  .ctrl.active            {background:#001a0e;border-color:var(--ok)}
  .ctrl.active .dot       {background:var(--ok);box-shadow:0 0 8px var(--ok)}
  .ctrl.active-warn       {background:#1a1500;border-color:var(--warn)}
  .ctrl.active-warn .dot  {background:var(--warn);box-shadow:0 0 8px var(--warn)}
  .ctrl.active-red        {background:#1a0008;border-color:var(--danger)}
  .ctrl.active-red .dot   {background:var(--danger);box-shadow:0 0 8px var(--danger)}

  .footer{
    text-align:center;font-size:.7em;letter-spacing:2px;
    color:var(--warn);min-height:16px;font-family:'Share Tech Mono',monospace;
  }
  .rf-indicator{
    display:flex;align-items:center;justify-content:center;gap:6px;
    font-size:.65em;letter-spacing:2px;color:var(--dim);
  }
  .rf-dot{width:7px;height:7px;border-radius:50%;background:var(--dim);transition:all .3s}
  .rf-dot.ok{background:var(--ok);box-shadow:0 0 8px var(--ok)}
  .rf-dot.fail{background:var(--danger);animation:pulse .9s infinite}
  /* целевое давление */
  .tgt-block{background:var(--card);border:1px solid var(--border);border-radius:12px;
    padding:14px;position:relative;overflow:hidden;}
  .tgt-block::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;
    background:linear-gradient(90deg,transparent,var(--warn),transparent);}
  .t-lbl{font-size:.62em;letter-spacing:4px;color:var(--dim);text-transform:uppercase;
    text-align:center;margin-bottom:10px;}
  .t-row{display:flex;align-items:center;gap:10px;margin-bottom:10px;}
  .t-val{font-family:'Share Tech Mono',monospace;font-size:1.9em;color:var(--warn);min-width:76px;text-align:center;}
  .t-unit{font-size:.42em;color:#887700;}
  input[type=range]{flex:1;-webkit-appearance:none;height:4px;border-radius:2px;outline:none;cursor:pointer;
    background:linear-gradient(90deg,var(--warn) var(--tp,10%),#1e1e3a var(--tp,10%));}
  input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;
    border-radius:50%;background:var(--warn);cursor:pointer;transition:box-shadow .2s;}
  input[type=range]::-webkit-slider-thumb:hover{box-shadow:0 0 12px rgba(255,204,0,.6);}
  .qrow{display:flex;gap:5px;margin-bottom:10px;}
  .qb{flex:1;padding:6px 2px;border:1px solid var(--border);border-radius:7px;
    background:var(--card);cursor:pointer;font-family:'Share Tech Mono',monospace;
    font-size:.7em;color:var(--dim);transition:all .2s;text-align:center;}
  .qb.active,.qb:hover{border-color:var(--warn);color:var(--warn);background:#1a1400;}
  .apply-btn{width:100%;padding:10px;border:1px solid var(--warn);border-radius:9px;
    background:#1a1400;color:var(--warn);cursor:pointer;
    font-family:'Exo 2',sans-serif;font-size:.72em;font-weight:800;letter-spacing:3px;
    text-transform:uppercase;transition:all .2s;}
  .apply-btn:hover{background:#2a2000;}
  .apply-btn:active{transform:scale(.97);}
  .apply-btn:disabled{opacity:.3;cursor:not-allowed;}
  .t-ok{text-align:center;font-size:.6em;font-family:'Share Tech Mono',monospace;
    color:var(--ok);min-height:14px;margin-top:5px;transition:opacity .4s;opacity:0;}
  /* маркер цели на полоске давления */
  .p-bar-wrap{position:relative;}
  .p-tgt{position:absolute;top:-4px;width:2px;height:13px;
    background:var(--warn);border-radius:1px;transition:left .5s ease;pointer-events:none;}
</style>
</head>
<body>
<h1>⚡ H2 Control</h1>
<div class="panel">

  <div class="pressure">
    <div class="p-lbl">Давление в баке</div>
    <div class="p-num"><span id="pv">0.00</span><span class="p-unit">бар</span></div>
    <div class="p-bar-wrap">
      <div class="p-bar">
        <div class="p-fill" id="pf" style="width:0%"></div>
        <div class="p-tgt" id="ptgt" style="left:10%"></div>
      </div>
    </div>
  </div>

  <div class="badges">
    <span class="badge" id="bPwr">POWER</span>
    <span class="badge" id="bLck">LOCK</span>
    <span class="badge" id="bPrg">PURGE</span>
    <span class="badge" id="bCgs">CONST GAS</span>
    <span class="badge" id="bMgs">MANUAL</span>
    <span class="badge" id="bPzo">PIEZO</span>
    <span class="badge" id="bEle">ELECTRO</span>
    <span class="badge" id="bVal">VALVE</span>
  </div>

  <div class="rf-indicator">
    <div class="rf-dot" id="rfDot"></div>
    <span id="rfTxt">RF ПУЛЬТ: —</span>
  </div>

  <button class="btn-power" id="btnPwr" onclick="handlePower()">ВКЛЮЧИТЬ СИСТЕМУ</button>

  <div class="grid">
    <button class="ctrl wide" id="cLck" onclick="cmd(2)" disabled>
      <div class="dot"></div>БЛОКИРОВКА
    </button>
    <button class="ctrl" id="cCgs" onclick="cmd(3)" disabled>
      <div class="dot"></div>ПОСТ. ГАЗ
    </button>
    <button class="ctrl" id="cMgs" onclick="cmd(7)" disabled>
      <div class="dot"></div>РУЧНАЯ ПОДАЧА
    </button>
    <button class="ctrl active-warn" id="cPzo" onclick="cmd(4)" disabled>
      <div class="dot"></div>ПОДЖИГ
    </button>
    <button class="ctrl" id="cMod" onclick="cmd(6)" disabled>
      <div class="dot"></div>
      <span id="modLbl">АВТО  1.0 БАР</span>
    </button>
  </div>

  <!-- ВВОД ЦЕЛЕВОГО ДАВЛЕНИЯ -->
  <div class="tgt-block">
    <div class="t-lbl">Целевое давление</div>
    <div class="t-row">
      <span class="t-val"><span id="tval">1.0</span><span class="t-unit"> бар</span></span>
      <input type="range" id="tslider" min="0.5" max="10" step="0.1" value="1.0" oninput="onSlide(this.value)">
    </div>
    <div class="qrow">
      <button class="qb active" onclick="setQ(1)">1</button>
      <button class="qb" onclick="setQ(2)">2</button>
      <button class="qb" onclick="setQ(4)">4</button>
      <button class="qb" onclick="setQ(6)">6</button>
      <button class="qb" onclick="setQ(8)">8</button>
      <button class="qb" onclick="setQ(10)">10</button>
    </div>
    <button class="apply-btn" id="applyBtn" onclick="applyTgt()" disabled>УСТАНОВИТЬ ДАВЛЕНИЕ</button>
    <div class="t-ok" id="tok"></div>
  </div>

  <div class="footer" id="foot"></div>
</div>

<script>
  let ct=null,purging=false,pendingTgt=1.0;

  function onSlide(v){
    v=parseFloat(v);
    pendingTgt=v;
    document.getElementById('tval').innerText=v.toFixed(1);
    const pct=((v-0.5)/9.5*100).toFixed(1);
    document.getElementById('tslider').style.setProperty('--tp',pct+'%');
    updateQbtns(v);
  }

  function setQ(v){
    pendingTgt=v;
    document.getElementById('tslider').value=v;
    document.getElementById('tval').innerText=v.toFixed(1);
    const pct=((v-0.5)/9.5*100).toFixed(1);
    document.getElementById('tslider').style.setProperty('--tp',pct+'%');
    updateQbtns(v);
  }

  function updateQbtns(v){
    const p=[1,2,4,6,8,10];
    document.querySelectorAll('.qb').forEach((b,i)=>{
      b.className='qb'+(p[i]===v?' active':'');
    });
  }

  function applyTgt(){
    fetch('/setpressure?val='+pendingTgt.toFixed(1)).catch(()=>{});
    const tok=document.getElementById('tok');
    tok.innerText='✓ установлено: '+pendingTgt.toFixed(1)+' бар';
    tok.style.opacity='1';
    setTimeout(()=>tok.style.opacity='0',2500);
  }

  function handlePower(){
    if(purging)return;
    if(!ct){ct=setTimeout(()=>{ct=null;cmd(1);},380);}
    else{clearTimeout(ct);ct=null;document.getElementById('foot').innerText='БЕЗОПАСНОЕ ВЫКЛЮЧЕНИЕ...';cmd(99);}
  }

  function cmd(id){fetch('/cmd?id='+id);}

  function cls(id,val,c){document.getElementById(id).className='badge'+(val?' '+c:'');}

  function tick(){
    fetch('/status').then(r=>r.json()).then(d=>{
      // Давление + маркер цели
      document.getElementById('pv').innerText=d.p.toFixed(2);
      document.getElementById('pf').style.width=Math.min(d.p/10*100,100)+'%';
      document.getElementById('ptgt').style.left='calc('+Math.min(d.tgt/10*100,100)+'% - 1px)';

      // Бейджи
      cls('bPwr',d.pwr,'ok'); cls('bLck',d.lck,'warn');
      cls('bPrg',d.prg,'danger'); cls('bCgs',d.cgs,'ok');
      cls('bMgs',d.mgs,'ok'); cls('bPzo',d.pzo,'warn');
      cls('bEle',d.ele,'ok'); cls('bVal',d.val,'ok');

      // Кнопка питания
      const bp=document.getElementById('btnPwr');
      bp.className='btn-power'+(d.pwr?' on':'');
      bp.innerText=d.pwr?'ВЫКЛЮЧИТЬ  (2× = сброс)':'ВКЛЮЧИТЬ СИСТЕМУ';

      // Блокировка кнопок
      const dis=!d.pwr||d.prg;
      ['cLck','cCgs','cMgs','cPzo','cMod'].forEach(i=>document.getElementById(i).disabled=dis);
      document.getElementById('applyBtn').disabled=dis;

      // Стили кнопок управления
      document.getElementById('cLck').className='ctrl wide'+(d.lck?' active-warn':'');
      document.getElementById('cCgs').className='ctrl'+(d.cgs?' active':'');
      document.getElementById('cMgs').className='ctrl'+(d.mgs?' active':'');
      document.getElementById('cPzo').className='ctrl active-warn'+(d.pzo?' active-red':'');
      document.getElementById('cMod').className='ctrl'+(d.tgt==4?' active':'');

      // Метка режима: показываем актуальное целевое давление с мозга
      document.getElementById('modLbl').innerText='АВТО  '+parseFloat(d.tgt).toFixed(1)+' БАР';

      // Синхронизируем слайдер с мозгом если пользователь его не трогал
      const sl=document.getElementById('tslider');
      if(document.activeElement!==sl){
        const v=parseFloat(d.tgt);
        sl.value=v;
        document.getElementById('tval').innerText=v.toFixed(1);
        const pct=((v-0.5)/9.5*100).toFixed(1);
        sl.style.setProperty('--tp',pct+'%');
        pendingTgt=v;
        updateQbtns(v);
      }

      // RF
      const rf=document.getElementById('rfDot'),rt=document.getElementById('rfTxt');
      rf.className='rf-dot'+(d.rf?' ok':' fail');
      rt.innerText='RF ПУЛЬТ: '+(d.rf?'ОНЛАЙН':'ОФЛАЙН');

      purging=d.prg;
      if(d.prg)document.getElementById('foot').innerText='⏳ ПРОДУВКА — ОЖИДАЙТЕ';
      else if(!d.pwr)document.getElementById('foot').innerText='';
    }).catch(()=>{});
  }

  // Инициализация слайдера
  document.getElementById('tslider').style.setProperty('--tp','10%');
  setInterval(tick,600);
  tick();
</script>
</body>
</html>
)rawliteral";

// ======================== ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ========================

void setRelay(int pin, bool on) {
  digitalWrite(pin, on ? RELAY_ON : RELAY_OFF);
}

void allRelaysOff() {
  setRelay(PIN_RELAY_ELECTRO, false);
  setRelay(PIN_RELAY_VALVE,   false);
  setRelay(PIN_RELAY_PIEZO,   false);
  electroOn = false;
  valveOn   = false;
}

void resetProcesses() {
  constGasActive  = false;
  manualGasActive = false;
  piezoActive     = false;
  cgPiezoFired    = false;
}

float readPressure() {
  // GPIO34 → делитель 10кОм/20кОм → ESP32 ADC 12-бит (0-4095, 0-3.3V)
  // Vout = Vsensor × 20/(10+20) → Vsensor = Vout × 1.5
  // Датчик 0-10 бар: 0.5-4.5V → P = (V - 0.5) / 4.0 × 10
  int   raw    = analogRead(PIN_PRESSURE);
  float vOut   = (raw / 4095.0f) * 3.3f;
  float vSens  = vOut * 1.5f;
  float bar    = ((vSens - 0.5f) / 4.0f) * 10.0f;
  return max(0.0f, bar);
}

void startPiezo() {
  if (!piezoActive && systemOn && !isLocked && !isPurging) {
    piezoActive  = true;
    piezoStart   = millis();
    setRelay(PIN_RELAY_PIEZO, true);
  }
}

// ======================== ВЫПОЛНЕНИЕ КОМАНДЫ ========================
void executeCommand(uint8_t cmd) {
  if (isPurging && cmd != 2) return;  // Во время продувки только разблокировка

  switch (cmd) {
    case 1:  // Power toggle
      if (!systemOn) {
        systemOn = true;
      } else {
        systemOn = false;
        resetProcesses();
        allRelaysOff();
      }
      break;

    case 2:  // Lock / Unlock
      isLocked = !isLocked;
      if (isLocked) { resetProcesses(); allRelaysOff(); }
      break;

    case 3:  // Постоянный газ
      if (systemOn && !isLocked) {
        constGasActive = !constGasActive;
        if (constGasActive) { manualGasActive = false; cgPiezoFired = false; }
      }
      break;

    case 4:  // Разовый поджиг
      startPiezo();
      break;

    case 6:  // Быстрое переключение 1 ↔ 4 бар (для пульта)
      targetPressure = (targetPressure < 2.0f) ? 4.0f : 1.0f;
      break;

    case 7:  // Ручная подача
      if (systemOn && !isLocked) {
        manualGasActive = !manualGasActive;
        if (manualGasActive) constGasActive = false;
      }
      break;

    case 99: // Безопасное выключение — продувка 5 секунд
      if (systemOn) {
        resetProcesses();
        setRelay(PIN_RELAY_ELECTRO, false);
        setRelay(PIN_RELAY_PIEZO,   false);
        setRelay(PIN_RELAY_VALVE,   true);  // Открываем клапан на продувку
        electroOn   = false;
        valveOn     = true;
        isPurging   = true;
        purgeStart  = millis();
      }
      break;
  }
}

// ======================== OLED ДИСПЛЕЙ ========================
void updateDisplay() {
  if (!oledOk) return;

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // === Заголовок ===
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print("H2 CONTROL");
  // RF статус справа
  oled.setCursor(86, 0);
  oled.print("RF:");
  oled.setCursor(110, 0);
  if (rfOk) {
    oled.print("OK");
  } else {
    oled.print("--");
  }

  // Горизонтальная линия
  oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  // === Давление ===
  oled.setTextSize(2);
  oled.setCursor(0, 12);
  char pBuf[8];
  dtostrf(currentPressure, 4, 2, pBuf);
  oled.print(pBuf);
  oled.setTextSize(1);
  oled.setCursor(90, 18);
  oled.print("bar");

  // Полоска давления (0-10 бар)
  int barW = (int)((currentPressure / 10.0f) * 127.0f);
  barW = constrain(barW, 0, 127);
  oled.drawRect(0, 30, 128, 5, SSD1306_WHITE);
  if (barW > 0) oled.fillRect(1, 31, barW, 3, SSD1306_WHITE);

  oled.drawLine(0, 36, 127, 36, SSD1306_WHITE);

  // === Статус реле ===
  oled.setTextSize(1);
  oled.setCursor(0, 39);
  oled.print("E:");
  oled.print(electroOn ? "ON " : "OFF");
  oled.print(" V:");
  oled.print(valveOn ? "ON " : "OFF");
  oled.print(" P:");
  oled.print(piezoActive ? "ON" : "OF");

  // === Режим работы ===
  oled.setCursor(0, 49);
  if (isPurging) {
    oled.print(">>> ПРОДУВКА <<<");
  } else if (!systemOn) {
    oled.print("СИСТЕМА ВЫКЛ");
  } else if (isLocked) {
    oled.print("ЗАБЛОКИРОВАНО");
  } else if (constGasActive) {
    oled.print("ПОСТ.ГАЗ  ");
    oled.print((int)targetPressure);
    oled.print("bar");
  } else if (manualGasActive) {
    oled.print("РУЧНАЯ ПОДАЧА");
  } else {
    oled.print("АВТО  ");
    oled.print((int)targetPressure);
    oled.print("bar");
  }

  oled.display();
}

// ======================== ОТПРАВКА СТАТУСА НА ПУЛЬТ ========================
void sendStatusToRemote() {
  StatusPacket pkt;
  pkt.type      = 0xAA;
  pkt.pressure  = currentPressure;
  pkt.targetBar = (uint8_t)roundf(targetPressure);

  uint8_t f = 0;
  if (systemOn)        f |= (1 << 0);
  if (isLocked)        f |= (1 << 1);
  if (isPurging)       f |= (1 << 2);
  if (constGasActive)  f |= (1 << 3);
  if (manualGasActive) f |= (1 << 4);
  if (electroOn)       f |= (1 << 5);
  if (valveOn)         f |= (1 << 6);
  if (piezoActive)     f |= (1 << 7);
  pkt.flags = f;

  radio.stopListening();
  radio.write(&pkt, sizeof(pkt));
  radio.startListening();
}

// ======================== SETUP ========================
void setup() {
  Serial.begin(115200);
  Serial.println("\n--- H2 Control Brain v3.0 ---");

  // Реле — сразу выключить
  pinMode(PIN_RELAY_ELECTRO, OUTPUT);
  pinMode(PIN_RELAY_VALVE,   OUTPUT);
  pinMode(PIN_RELAY_PIEZO,   OUTPUT);
  allRelaysOff();

  // Кнопка — внутренний PULLUP, нажатие = LOW
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  // OLED
  Wire.begin(21, 22);
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledOk) {
    Serial.println("OLED: не найден (проверь адрес 0x3C/0x3D и подключение)");
  } else {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(20, 24);
    oled.print("H2 CONTROL v3.0");
    oled.display();
    Serial.println("OLED: OK");
  }

  // NRF24L01
  SPI.begin(18, 19, 23, PIN_NRF_CSN);
  if (radio.begin()) {
    radio.openWritingPipe(PIPE_B2R);        // Отправляем статус пульту
    radio.openReadingPipe(1, PIPE_R2B);     // Принимаем команды от пульта
    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_250KBPS);
    radio.setCRCLength(RF24_CRC_16);
    radio.startListening();
    Serial.println("NRF24: OK");
  } else {
    Serial.println("NRF24: FAIL — проверь подключение и 3.3V!");
  }

  // Wi-Fi точка доступа
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("WiFi AP: "); Serial.print(AP_SSID);
  Serial.print("  IP: "); Serial.println(WiFi.softAPIP());

  // Маршруты веб-сервера
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (req->hasParam("id"))
      executeCommand((uint8_t)req->getParam("id")->value().toInt());
    req->send(200, "text/plain", "OK");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    String j = "{";
    j += "\"p\":"   + String(currentPressure, 2)    + ",";
    j += "\"pwr\":" + String(systemOn       ? 1:0)  + ",";
    j += "\"lck\":" + String(isLocked        ? 1:0)  + ",";
    j += "\"prg\":" + String(isPurging       ? 1:0)  + ",";
    j += "\"cgs\":" + String(constGasActive  ? 1:0)  + ",";
    j += "\"mgs\":" + String(manualGasActive ? 1:0)  + ",";
    j += "\"pzo\":" + String(piezoActive     ? 1:0)  + ",";
    j += "\"ele\":" + String(electroOn       ? 1:0)  + ",";
    j += "\"val\":" + String(valveOn         ? 1:0)  + ",";
    j += "\"rf\":"  + String(rfOk            ? 1:0)  + ",";
    j += "\"tgt\":"   + String(targetPressure, 1);
    j += "}";
    req->send(200, "application/json", j);
  });

  // Установка произвольного давления: /setpressure?val=3.5
  server.on("/setpressure", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (req->hasParam("val")) {
      float v = req->getParam("val")->value().toFloat();
      v = constrain(v, 0.1f, 10.0f);
      targetPressure = v;
      Serial.print("[WEB] targetPressure = "); Serial.println(v, 1);
    }
    req->send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println("WebServer: OK  →  http://192.168.4.1");
}

// ======================== LOOP ========================
void loop() {
  unsigned long now = millis();

  // ---- Усреднение давления (каждые 100 мс, 5 выборок) ----
  if (now - pLast >= T_PRESSURE_SAMP) {
    pSum += readPressure();
    pCnt++;
    pLast = now;
    if (pCnt >= 5) {
      currentPressure = pSum / pCnt;
      pSum = 0.0f; pCnt = 0;
    }
  }

  // ---- Физическая кнопка (INPUT_PULLUP → нажатие = LOW) ----
  bool raw = digitalRead(PIN_BUTTON);
  if (raw != btnLastRaw) btnDebTime = now;
  if ((now - btnDebTime) > T_DEBOUNCE && raw != btnStable) {
    btnStable = raw;
    if (btnStable == LOW) {
      if ((now - lastClickMs) < T_DOUBLE_CLICK)
        executeCommand(99);
      else
        executeCommand(1);
      lastClickMs = now;
    }
  }
  btnLastRaw = raw;

  // ---- Приём команд от пульта (NRF24) ----
  if (radio.available()) {
    CmdPacket pkt;
    radio.read(&pkt, sizeof(pkt));
    if (pkt.type == 0x55) {
      executeCommand(pkt.cmd);
      lastRfRx = now;
      Serial.print("[RF RX] cmd="); Serial.println(pkt.cmd);
    }
  }
  rfOk = (now - lastRfRx < T_RF_TIMEOUT) && (lastRfRx > 0);

  // ---- Отправка статуса пульту (каждые 500 мс) ----
  if (now - lastStatusTx >= T_STATUS_TX) {
    sendStatusToRemote();
    lastStatusTx = now;
  }

  // ======================== ЛОГИКА УПРАВЛЕНИЯ ========================

  // ПРОДУВКА
  if (isPurging) {
    if (now - purgeStart >= T_PURGE) {
      setRelay(PIN_RELAY_VALVE, false);
      valveOn   = false;
      isPurging = false;
      systemOn  = false;
      Serial.println("[SYS] Продувка завершена. Система выключена.");
    }
    if (oledOk && (now - lastDispUpd >= T_DISPLAY_UPD)) { updateDisplay(); lastDispUpd = now; }
    return;
  }

  // ВЫКЛЮЧЕНА или ЗАБЛОКИРОВАНА
  if (!systemOn || isLocked) {
    allRelaysOff();
    resetProcesses();
    if (oledOk && (now - lastDispUpd >= T_DISPLAY_UPD)) { updateDisplay(); lastDispUpd = now; }
    return;
  }

  // УПРАВЛЕНИЕ ЭЛЕКТРОЛИЗЕРОМ И КЛАПАНОМ
  bool needElectro = false;
  bool needValve   = false;

  if (constGasActive) {
    if (currentPressure < targetPressure) {
      needElectro = true;
      needValve   = true;
      if (!cgPiezoFired) { cgPiezoFired = true; startPiezo(); }
    }
  } else if (manualGasActive) {
    needElectro = true;
    needValve   = true;
  } else {
    if (currentPressure < targetPressure) needElectro = true;
  }

  setRelay(PIN_RELAY_ELECTRO, needElectro);
  setRelay(PIN_RELAY_VALVE,   needValve);
  electroOn = needElectro;
  valveOn   = needValve;

  // ТАЙМЕР ПЬЕЗО
  if (piezoActive && (now - piezoStart >= T_PIEZO_WORK)) {
    piezoActive = false;
    setRelay(PIN_RELAY_PIEZO, false);
  }

  // ОБНОВЛЕНИЕ ДИСПЛЕЯ
  if (oledOk && (now - lastDispUpd >= T_DISPLAY_UPD)) {
    updateDisplay();
    lastDispUpd = now;
  }
}
