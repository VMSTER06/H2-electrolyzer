/*
 * H2_Brain_v3_TILT.ino
 * Система управления электролизером H2 PRO
 * v3.0 — добавлен датчик наклона MPU6050 + физическая кнопка LOCK на корпусе
 *
 * Плата: ESP32 DevKit v1 (WROOM-32D)
 * Среда: Arduino IDE с пакетом esp32 by Espressif
 *
 * Библиотеки (установить через Library Manager):
 *   RF24              — TMRh20
 *   ESPAsyncWebServer — me-no-dev
 *   AsyncTCP          — me-no-dev
 *   WebSockets        — Markus Sattler
 *   Adafruit SSD1306  — Adafruit
 *   Adafruit GFX      — Adafruit
 *   ArduinoJson       — Benoit Blanchon
 */

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <WebSocketsServer.h>
#include <SPI.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include <math.h>

// ================================================================
//  КОНФИГУРАЦИЯ — МЕНЯЙ ТОЛЬКО ЭТИ СТРОКИ
// ================================================================
const char* WIFI_SSID = "H2_PRO_STATION";
const char* WIFI_PASS = "hydro_pass";

// ── Реле (активный уровень HIGH) ────────────────────────────────
#define RELAY_ELECTRO  25
#define RELAY_VALVE    26
#define RELAY_PIEZO    27

// ── NRF24L01 ────────────────────────────────────────────────────
#define NRF_CE         4
#define NRF_CSN        5

// ── Датчик давления ─────────────────────────────────────────────
#define PIN_PRESSURE   36   // 12-bit ADC, вход только для чтения

// ── Светодиоды ─────────────────────────────────────────────────
#define LED_POWER      17
#define LED_LOCK       12   // ! strapping-pin, если мозг не стартует — отключи
#define LED_MODE       2    // ! strapping-pin

// ── Кнопки ─────────────────────────────────────────────────────
#define BTN_POWER      13   // одиночный = вкл/выкл, двойной = продувка
#define BTN_LOCK       14   // НОВОЕ: lock/unlock + разблокировка TILT

// ── I2C (OLED 0x3C + MPU6050 0x68 на одной шине) ──────────────
#define I2C_SDA        21
#define I2C_SCL        22

// ── MPU6050 ────────────────────────────────────────────────────
#define MPU_ADDR          0x68
#define TILT_THRESHOLD    30.0f   // градусы, порог тревоги
#define TILT_HOLDOFF_MS   500UL   // мс удержания перед срабатыванием
#define CALIB_SAMPLES     60      // сэмплов для калибровки вертикали

// ── Безопасность давления ──────────────────────────────────────
#define MAX_SAFE_PRESSURE 4.8f    // бар, аварийное отключение

// ── Продувка ───────────────────────────────────────────────────
#define PURGE_DURATION_MS 5000UL

// ── Пьезо ──────────────────────────────────────────────────────
#define PIEZO_DURATION_MS 500UL

// ── Двойной клик (мс между нажатиями) ─────────────────────────
#define DBL_CLICK_MS      400UL


// ================================================================
//  СОСТОЯНИЯ И ДАННЫЕ
// ================================================================
enum SystemState { S_OFF, S_READY, S_PURGE, S_TILT_LOCK };

SystemState  state   = S_OFF;
bool         locked  = false;
bool         constGas = false;
bool         manualGas = false;
bool         piezoOn = false;
int          mode    = 1;      // 1 или 4 бар
float        pressure = 0.0f;

// TILT
float        gRefX, gRefY, gRefZ;   // опорный вектор гравитации
bool         gCalibDone   = false;
unsigned long tiltSince   = 0;
bool         tiltPending  = false;

// Таймеры
unsigned long purgeStart  = 0;
unsigned long piezoStart  = 0;

// Кнопка POWER — двойной клик
unsigned long pwrLastMs   = 0;
int           pwrClicks   = 0;

// Кнопки — антидребезг
bool lastPwrRaw  = HIGH, lastPwrState  = HIGH;
bool lastLckRaw  = HIGH, lastLckState  = HIGH;
unsigned long pwrDebounce = 0, lckDebounce = 0;


// ================================================================
//  ОБЪЕКТЫ
// ================================================================
RF24             radio(NRF_CE, NRF_CSN);
AsyncWebServer   httpServer(80);
WebSocketsServer ws(81);
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
const byte       nrfAddr[6] = "HYDRO";


// ================================================================
//  HTML — веб-интерфейс
// ================================================================
const char HTML[] PROGMEM = R"rawhtml(
<!DOCTYPE html><html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>H2 PRO</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#111;color:#eee;font-family:Arial,sans-serif;padding:12px;max-width:480px;margin:auto}
h1{text-align:center;font-size:1.3em;margin-bottom:10px;color:#4af}
#tiltBanner{display:none;background:#c00;color:#fff;text-align:center;
  font-weight:bold;padding:10px;border-radius:8px;margin-bottom:10px;
  animation:blink 0.8s step-start infinite}
@keyframes blink{50%{opacity:0.3}}
.pressure{text-align:center;font-size:2.4em;font-weight:bold;
  color:#4af;margin:10px 0}
.pressure span{font-size:0.5em;color:#888}
.badges{display:flex;flex-wrap:wrap;gap:6px;justify-content:center;margin-bottom:14px}
.badge{padding:4px 10px;border-radius:12px;font-size:0.78em;
  background:#333;color:#666;border:1px solid #444}
.badge.on{color:#fff;border-color:transparent}
.bg-green{background:#1a7a1a}.bg-red{background:#7a1a1a}
.bg-blue{background:#1a3a7a}.bg-orange{background:#7a4a00}
.bg-cyan{background:#006060}.bg-purple{background:#4a007a}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:10px}
.btn{padding:14px 8px;border:none;border-radius:10px;font-size:0.9em;
  font-weight:bold;cursor:pointer;background:#1e1e1e;color:#aaa;
  border:1px solid #333;transition:0.15s}
.btn:active{transform:scale(0.95)}
.btn.on{background:#1a4a7a;color:#fff;border-color:#4af}
.btn.danger{background:#3a0000;color:#f66;border-color:#700}
.btn.danger.on{background:#7a0000;color:#fff}
#wsSt{text-align:center;font-size:0.7em;color:#555;margin-top:8px}
</style>
</head>
<body>
<h1>⚡ H2 PRO</h1>
<div id="tiltBanner">⚠ TILT — АВАРИЙНАЯ ОСТАНОВКА<br>
  <small>Верните устройство вертикально и нажмите LOCK</small></div>

<div class="pressure" id="prsVal">—<span> бар</span></div>

<div class="badges">
  <span id="ind-pwr"  class="badge">PWR</span>
  <span id="ind-lck"  class="badge">LOCK</span>
  <span id="ind-mode" class="badge">4 BAR</span>
  <span id="ind-purge"class="badge">PURGE</span>
  <span id="ind-cg"   class="badge">CONST GAS</span>
  <span id="ind-mg"   class="badge">MANUAL</span>
  <span id="ind-pz"   class="badge">PIEZO</span>
  <span id="ind-tlt"  class="badge">TILT</span>
</div>

<div class="grid">
  <button class="btn" id="btn-pwr"  onclick="cmd(1)">⏻ POWER</button>
  <button class="btn" id="btn-lck"  onclick="cmd(2)">🔒 LOCK</button>
  <button class="btn" id="btn-cg"   onclick="cmd(3)">💨 CONST GAS</button>
  <button class="btn danger" id="btn-pz" onclick="cmd(4)">⚡ PIEZO</button>
  <button class="btn" id="btn-mg"   onclick="cmd(7)">🔧 MANUAL GAS</button>
  <button class="btn" id="btn-mode" onclick="cmd(6)">🎚 MODE</button>
</div>
<button class="btn" style="width:100%;background:#0a2a0a;color:#4f4"
  onclick="dblPwr()">⏏ SAFE OFF (продувка)</button>

<div id="wsSt">Подключение…</div>

<script>
var ws=new WebSocket('ws://'+location.hostname+':81/');
ws.onopen=function(){document.getElementById('wsSt').textContent='🟢 Онлайн';}
ws.onclose=function(){document.getElementById('wsSt').textContent='🔴 Нет связи';}
ws.onmessage=function(e){
  var d=JSON.parse(e.data);
  document.getElementById('prsVal').innerHTML=d.prs.toFixed(2)+'<span> бар</span>';

  function upd(id,on,cls){
    var el=document.getElementById(id);
    el.className=on?'badge on '+cls:'badge';
  }
  upd('ind-pwr',  d.pwr, 'bg-green');
  upd('ind-lck',  d.lck, 'bg-red');
  upd('ind-mode', d.mod===4,'bg-purple');
  upd('ind-purge',d.pg,  'bg-cyan');
  upd('ind-cg',   d.cg,  'bg-blue');
  upd('ind-mg',   d.mg,  'bg-blue');
  upd('ind-pz',   d.pz,  'bg-orange');
  upd('ind-tlt',  d.tlt, 'bg-red');

  document.getElementById('tiltBanner').style.display=d.tlt?'block':'none';

  function btnOn(id,on){
    document.getElementById(id).className='btn'+(on?' on':'');
  }
  btnOn('btn-pwr', d.pwr);
  btnOn('btn-lck', d.lck);
  btnOn('btn-cg',  d.cg);
  btnOn('btn-mg',  d.mg);
  document.getElementById('btn-mode').textContent='🎚 '+d.mod+' BAR';
};
function cmd(c){ws.send(JSON.stringify({cmd:c}));}
function dblPwr(){ws.send(JSON.stringify({cmd:99}));}
</script>
</body></html>
)rawhtml";


// ================================================================
//  MPU6050 — ПРЯМАЯ РАБОТА ЧЕРЕЗ Wire (без сторонней библиотеки)
// ================================================================

void mpuInit() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);   // PWR_MGMT_1
  Wire.write(0x00);   // Сбросить sleep-режим
  Wire.endTransmission();
  delay(150);

  // Диапазон акселерометра ±2g (0x00)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x1C);   // ACCEL_CONFIG
  Wire.write(0x00);
  Wire.endTransmission();
}

void mpuReadAccel(float &ax, float &ay, float &az) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);   // ACCEL_XOUT_H
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)6, (uint8_t)true);

  int16_t rx = (Wire.read() << 8) | Wire.read();
  int16_t ry = (Wire.read() << 8) | Wire.read();
  int16_t rz = (Wire.read() << 8) | Wire.read();

  // LSB/g для ±2g = 16384
  ax = rx / 16384.0f;
  ay = ry / 16384.0f;
  az = rz / 16384.0f;
}

void calibrateMPU() {
  // Показываем прогресс на OLED
  oled.clearDisplay();
  oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0,0); oled.println("Калибровка TILT...");
  oled.println("Не двигайте корпус"); oled.display();

  double sx=0, sy=0, sz=0;
  for (int i=0; i<CALIB_SAMPLES; i++) {
    float ax, ay, az;
    mpuReadAccel(ax, ay, az);
    sx += ax; sy += ay; sz += az;
    delay(20);
  }
  gRefX = sx / CALIB_SAMPLES;
  gRefY = sy / CALIB_SAMPLES;
  gRefZ = sz / CALIB_SAMPLES;
  gCalibDone = true;
}

float getTiltAngle() {
  if (!gCalibDone) return 0.0f;
  float ax, ay, az;
  mpuReadAccel(ax, ay, az);

  float dot     = ax*gRefX + ay*gRefY + az*gRefZ;
  float magRef  = sqrtf(gRefX*gRefX + gRefY*gRefY + gRefZ*gRefZ);
  float magCur  = sqrtf(ax*ax + ay*ay + az*az);

  if (magRef < 0.001f || magCur < 0.001f) return 0.0f;

  float cosA = dot / (magRef * magCur);
  cosA = constrain(cosA, -1.0f, 1.0f);
  return acosf(cosA) * 180.0f / (float)M_PI;
}


// ================================================================
//  TILT — ЛОГИКА
// ================================================================

void triggerTiltLock() {
  // Аварийно выключаем всё
  digitalWrite(RELAY_PIEZO, LOW);
  digitalWrite(RELAY_ELECTRO, LOW);
  digitalWrite(RELAY_VALVE, LOW);
  piezoOn   = false;
  constGas  = false;
  manualGas = false;
  state     = S_TILT_LOCK;
  tiltPending = false;
  Serial.println("[TILT] LOCK! Все реле выключены.");
}

void checkTilt() {
  // Мониторинг только когда система работает
  if (state == S_OFF || state == S_TILT_LOCK) return;

  float angle = getTiltAngle();

  if (angle > TILT_THRESHOLD) {
    if (!tiltPending) {
      tiltPending = true;
      tiltSince   = millis();
    } else if ((millis() - tiltSince) >= TILT_HOLDOFF_MS) {
      triggerTiltLock();
    }
  } else {
    tiltPending = false;
  }
}


// ================================================================
//  WebSocket — РАССЫЛКА СТАТУСА
// ================================================================

void broadcastStatus() {
  StaticJsonDocument<160> doc;
  doc["pwr"]  = (state == S_READY || state == S_PURGE) ? 1 : 0;
  doc["lck"]  = locked ? 1 : 0;
  doc["mod"]  = mode;
  doc["cg"]   = constGas ? 1 : 0;
  doc["mg"]   = manualGas ? 1 : 0;
  doc["pz"]   = piezoOn ? 1 : 0;
  doc["pg"]   = (state == S_PURGE) ? 1 : 0;
  doc["prs"]  = pressure;
  doc["tlt"]  = (state == S_TILT_LOCK) ? 1 : 0;
  char buf[160];
  serializeJson(doc, buf);
  ws.broadcastTXT(buf);
}

void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
  if (type != WStype_TEXT) return;

  StaticJsonDocument<64> doc;
  if (deserializeJson(doc, payload, len) != DeserializationError::Ok) return;

  int c = doc["cmd"] | 0;
  if (c == 0) return;

  // В TILT-блоке все команды игнорируются (кроме безопасного выхода через кнопку)
  if (state == S_TILT_LOCK) {
    broadcastStatus(); // напоминаем клиенту о блокировке
    return;
  }

  switch(c) {
    case 1:  // Power
      if (state == S_OFF) state = S_READY;
      else if (state == S_READY) state = S_OFF;
      break;
    case 2:  // Lock toggle
      locked = !locked;
      break;
    case 3:  // Const gas
      if (!locked) constGas = !constGas;
      break;
    case 4:  // Piezo
      if (!locked && state == S_READY) {
        piezoOn = true;
        piezoStart = millis();
        digitalWrite(RELAY_PIEZO, HIGH);
      }
      break;
    case 6:  // Mode
      mode = (mode == 1) ? 4 : 1;
      break;
    case 7:  // Manual gas
      if (!locked) manualGas = !manualGas;
      break;
    case 99: // Продувка
      if (state == S_READY) {
        digitalWrite(RELAY_PIEZO, LOW);  piezoOn = false;
        digitalWrite(RELAY_ELECTRO, LOW);
        digitalWrite(RELAY_VALVE, HIGH);
        purgeStart = millis();
        state = S_PURGE;
      }
      break;
  }
  broadcastStatus();
}


// ================================================================
//  БЕЗОПАСНОСТЬ ДАВЛЕНИЯ
// ================================================================

void safetyCheck() {
  if (pressure > MAX_SAFE_PRESSURE && state == S_READY) {
    digitalWrite(RELAY_ELECTRO, LOW);
    constGas = false;
    Serial.printf("[SAFETY] Давление %.2f > %.2f, электролизер ВЫКЛ\n",
                  pressure, MAX_SAFE_PRESSURE);
  }
}


// ================================================================
//  КНОПКИ
// ================================================================

// Кнопка POWER — одиночный клик вкл/выкл, двойной — продувка
void handlePowerBtn() {
  bool raw = digitalRead(BTN_POWER);
  if (raw != lastPwrRaw) { pwrDebounce = millis(); lastPwrRaw = raw; }
  if ((millis() - pwrDebounce) < 50) return;
  if (raw == lastPwrState) return;
  lastPwrState = raw;

  if (raw == LOW) {  // нажата
    unsigned long now = millis();
    if ((now - pwrLastMs) < DBL_CLICK_MS) {
      pwrClicks++;
    } else {
      pwrClicks = 1;
    }
    pwrLastMs = now;

    if (pwrClicks >= 2) {
      // Двойной клик → продувка (безопасное выключение)
      if (state == S_READY) {
        digitalWrite(RELAY_PIEZO, LOW);  piezoOn = false;
        digitalWrite(RELAY_ELECTRO, LOW);
        digitalWrite(RELAY_VALVE, HIGH);
        purgeStart = millis();
        state = S_PURGE;
        broadcastStatus();
      }
      pwrClicks = 0;
    }
  } else {  // отпущена
    // Одиночный клик — обработка по истечении окна двойного клика
    // (будет обработан в loop через таймер)
  }
}

// Кнопка LOCK — обычная блокировка ИЛИ разблокировка TILT
void handleLockBtn() {
  bool raw = digitalRead(BTN_LOCK);
  if (raw != lastLckRaw) { lckDebounce = millis(); lastLckRaw = raw; }
  if ((millis() - lckDebounce) < 50) return;
  if (raw == lastLckState) return;
  lastLckState = raw;

  if (raw == LOW) {  // нажата
    if (state == S_TILT_LOCK) {
      // Попытка выйти из TILT-блокировки
      float angle = getTiltAngle();
      if (angle < TILT_THRESHOLD) {
        // Угол в норме — разблокировка
        state = S_OFF;
        Serial.println("[TILT] Разблокировка по кнопке LOCK.");
      } else {
        // Ещё наклонён — мигнуть LED_LOCK как сигнал
        for (int i=0; i<3; i++) {
          digitalWrite(LED_LOCK, HIGH); delay(80);
          digitalWrite(LED_LOCK, LOW);  delay(80);
        }
        Serial.printf("[TILT] Кнопка нажата, угол %.1f° > %.1f° — ещё наклонён\n",
                      angle, TILT_THRESHOLD);
      }
    } else {
      // Обычная блокировка
      locked = !locked;
    }
    broadcastStatus();
  }
}


// ================================================================
//  РЕЛЕ — ПРИМЕНИТЬ СОСТОЯНИЕ
// ================================================================

void applyRelayState() {
  switch(state) {
    case S_READY:
      if (!locked) {
        bool electroNeed = (pressure < (float)mode) || manualGas;
        digitalWrite(RELAY_ELECTRO, electroNeed ? HIGH : LOW);
        digitalWrite(RELAY_VALVE,   (constGas || manualGas) ? HIGH : LOW);
      } else {
        digitalWrite(RELAY_ELECTRO, LOW);
        digitalWrite(RELAY_VALVE,   LOW);
      }
      // Таймер пьезо
      if (piezoOn && (millis() - piezoStart >= PIEZO_DURATION_MS)) {
        piezoOn = false;
        digitalWrite(RELAY_PIEZO, LOW);
      }
      break;

    case S_PURGE:
      // Клапан открыт, остальное выключено
      digitalWrite(RELAY_ELECTRO, LOW);
      if ((millis() - purgeStart) >= PURGE_DURATION_MS) {
        digitalWrite(RELAY_VALVE, LOW);
        state = S_OFF;
        broadcastStatus();
      }
      break;

    case S_OFF:
    case S_TILT_LOCK:
      digitalWrite(RELAY_ELECTRO, LOW);
      digitalWrite(RELAY_VALVE,   LOW);
      digitalWrite(RELAY_PIEZO,   LOW);
      break;
  }
}


// ================================================================
//  OLED — ОБНОВЛЕНИЕ ЭКРАНА
// ================================================================

void updateOLED() {
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  if (state == S_TILT_LOCK) {
    // Полноэкранная TILT-тревога
    oled.setTextSize(2);
    oled.setCursor(10, 0);  oled.println("! TILT !");
    oled.setTextSize(1);
    oled.setCursor(0, 20);  oled.println("Аварийная остановка");
    oled.setCursor(0, 32);  oled.println("Верните вертикально");
    oled.setCursor(0, 44);  oled.println("Нажмите кнопку LOCK");
    float a = getTiltAngle();
    oled.setCursor(0, 56);
    oled.printf("Угол: %.1f (порог %d)", a, (int)TILT_THRESHOLD);
  } else {
    // Нормальный экран
    oled.setTextSize(1);
    oled.setCursor(0, 0);
    oled.printf("H2 PRO  [%s]",
      state==S_OFF   ? "ВЫКЛ" :
      state==S_READY ? "РАБОТА" : "ПРОДУВКА");

    oled.setCursor(0, 12);
    oled.printf("Давление: %.2f бар", pressure);

    oled.setCursor(0, 24);
    oled.printf("Режим: %d бар  %s",
      mode, locked ? "[БЛОК]" : "");

    oled.setCursor(0, 36);
    oled.printf("Газ:%s  Ручной:%s",
      constGas  ? "ВКЛ" : "выкл",
      manualGas ? "ВКЛ" : "выкл");

    oled.setCursor(0, 48);
    if (gCalibDone) {
      float a = getTiltAngle();
      oled.printf("Наклон: %.1f%c", a, 0xF8); // ° символ
    } else {
      oled.print("TILT: калибровка...");
    }
  }

  oled.display();
}


// ================================================================
//  СВЕТОДИОДЫ
// ================================================================

void updateLEDs() {
  digitalWrite(LED_POWER, (state == S_READY || state == S_PURGE) ? HIGH : LOW);
  digitalWrite(LED_MODE,  (mode == 4) ? HIGH : LOW);

  // LED_LOCK: горит при lock, быстро мигает при TILT
  if (state == S_TILT_LOCK) {
    digitalWrite(LED_LOCK, (millis() / 200) % 2 ? HIGH : LOW);
  } else {
    digitalWrite(LED_LOCK, locked ? HIGH : LOW);
  }
}


// ================================================================
//  SETUP
// ================================================================

void setup() {
  Serial.begin(115200);
  Serial.println("\n[H2 PRO v3.0 TILT] Старт...");

  // Реле — всё выключено при старте
  pinMode(RELAY_ELECTRO, OUTPUT); digitalWrite(RELAY_ELECTRO, LOW);
  pinMode(RELAY_VALVE,   OUTPUT); digitalWrite(RELAY_VALVE,   LOW);
  pinMode(RELAY_PIEZO,   OUTPUT); digitalWrite(RELAY_PIEZO,   LOW);

  // LED
  pinMode(LED_POWER, OUTPUT); digitalWrite(LED_POWER, LOW);
  pinMode(LED_LOCK,  OUTPUT); digitalWrite(LED_LOCK,  LOW);
  pinMode(LED_MODE,  OUTPUT); digitalWrite(LED_MODE,  LOW);

  // Кнопки
  pinMode(BTN_POWER, INPUT_PULLUP);
  pinMode(BTN_LOCK,  INPUT_PULLUP);

  // I2C
  Wire.begin(I2C_SDA, I2C_SCL);

  // OLED
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("[OLED] Не найден — проверь подключение!");
  }
  oled.clearDisplay();
  oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0,0); oled.println("H2 PRO v3.0");
  oled.println("Инициализация..."); oled.display();

  // MPU6050 — инит + калибровка вертикали
  mpuInit();
  calibrateMPU();
  Serial.printf("[MPU] Опора: X=%.3f Y=%.3f Z=%.3f\n", gRefX, gRefY, gRefZ);

  // Wi-Fi точка доступа
  WiFi.softAP(WIFI_SSID, WIFI_PASS);
  Serial.printf("[WiFi] AP: %s  IP: %s\n", WIFI_SSID,
                WiFi.softAPIP().toString().c_str());

  // NRF24
  if (!radio.begin()) {
    Serial.println("[NRF24] Не найден!");
  } else {
    radio.openReadingPipe(1, nrfAddr);
    radio.setPALevel(RF24_PA_MAX);
    radio.startListening();
    Serial.println("[NRF24] OK");
  }

  // HTTP-сервер
  httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", HTML);
  });
  httpServer.begin();

  // WebSocket
  ws.begin();
  ws.onEvent(onWebSocketEvent);

  Serial.println("[H2 PRO] Готов.");
}


// ================================================================
//  LOOP
// ================================================================

void loop() {
  ws.loop();

  // Давление (100 мс усреднение)
  static unsigned long lastPrsT = 0;
  if (millis() - lastPrsT >= 100) {
    lastPrsT = millis();
    // Датчик 0-10 бар, выход 0.5-4.5 В → делитель → 0-3.3 В на ADC
    // Формула: давление = (ADC/4095*3.3 - 0.5) / 4.0 * 10.0
    float v = analogRead(PIN_PRESSURE) / 4095.0f * 3.3f;
    pressure = max(0.0f, (v - 0.5f) / 4.0f * 10.0f);
  }

  // Проверка давления
  safetyCheck();

  // Датчик наклона (200 мс)
  static unsigned long lastTiltT = 0;
  if (millis() - lastTiltT >= 200) {
    lastTiltT = millis();
    checkTilt();
  }

  // NRF24
  if (radio.available()) {
    byte cmd;
    radio.read(&cmd, 1);
    // Обрабатываем как будто пришло с веб — переиспользуем ту же логику
    uint8_t payload[16];
    StaticJsonDocument<32> doc;
    doc["cmd"] = (int)cmd;
    char buf[32]; serializeJson(doc, buf);
    onWebSocketEvent(0, WStype_TEXT, (uint8_t*)buf, strlen(buf));
  }

  // Управление реле
  applyRelayState();

  // Кнопки
  handlePowerBtn();
  handleLockBtn();

  // Одиночный клик POWER — обработка по истечении DBL_CLICK_MS
  static unsigned long lastPwrCheck = 0;
  if (millis() - lastPwrCheck >= DBL_CLICK_MS + 50) {
    lastPwrCheck = millis();
    if (pwrClicks == 1) {
      // Одиночный клик — вкл/выкл
      if (state == S_TILT_LOCK) {
        // Power в TILT — игнор
      } else if (state == S_OFF) {
        state = S_READY;
      } else if (state == S_READY) {
        state = S_OFF;
      }
      broadcastStatus();
      pwrClicks = 0;
    }
  }

  // Светодиоды
  updateLEDs();

  // OLED — раз в 300 мс
  static unsigned long lastOledT = 0;
  if (millis() - lastOledT >= 300) {
    lastOledT = millis();
    updateOLED();
    broadcastStatus();
  }
}
