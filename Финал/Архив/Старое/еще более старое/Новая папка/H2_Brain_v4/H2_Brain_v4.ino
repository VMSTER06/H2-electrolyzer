/*
 * =====================================================================================
 *  H2 CONTROL SYSTEM — МОЗГ v4.2  (ESP32 DevKit v1, WROOM-32D)
 * =====================================================================================
 *
 *  НАЗНАЧЕНИЕ
 *  ----------
 *  Прошивка центрального контроллера водородной установки H2 PRO. Объединяет:
 *    • Веб-интерфейс (HTTP polling) для управления со смартфона/планшета
 *    • Двустороннюю радиосвязь с физическим пультом по NRF24L01+ (2 Мбит/с)
 *    • Графический OLED-индикатор для локального наблюдения
 *    • Управление электролизером, продувочным клапаном и пьезоподжигом
 *    • Двухуровневую защиту по давлению (предупреждение + аварийное отключение)
 *    • Защиту по наклону через датчик MPU6050 (TILT-protection) с автокалибровкой
 *
 *  ОТЛИЧИЯ ОТ v4.1
 *  ---------------
 *    • Добавлен датчик наклона MPU6050 (I2C-шина общая с OLED).
 *      Калибровка при включении мозга: 50 измерений за ~0.5 сек, усреднение
 *      даёт эталонный вектор гравитации. Дальнейшие отклонения сравниваются
 *      с этим эталоном.
 *    • Порог TILT-сработки: 30 градусов (можно изменить TILT_ANGLE_DEG).
 *    • При TILT-сработке принудительно отключается ПИТАНИЕ ЭЛЕКТРОЛИЗЕРА И КЛАПАНА.
 *      Пьезоподжиг при этом завершает свой цикл (если был запущен) сам.
 *    • Разблокировка после TILT: одной кнопкой LOCK (на пульте, мозге или в вебе).
 *      Если устройство всё ещё наклонено — на следующем тике защёлка взведётся
 *      снова. Это означает, что для разблокировки сначала надо физически
 *      вернуть устройство в исходное положение.
 *    • Восстановлен подробный хедер с инструкциями.
 *
 *  ПРИНЦИП РАБОТЫ TILT-ЗАЩИТЫ
 *  --------------------------
 *  При старте мозга MPU6050 пробуждается и снимает 50 измерений вектора
 *  ускорения свободного падения. Усреднённый вектор сохраняется как baseline
 *  (опорное положение). Во время работы каждые 100 мс берётся новый вектор,
 *  и вычисляется угол между ним и baseline по формуле скалярного произведения:
 *
 *      cos(θ) = (a·b) / (|a|·|b|)
 *      θ      = acos(cos(θ)) · 180 / π
 *
 *  Это даёт угол отклонения от стартового положения, независимо от того,
 *  как именно устройство было ориентировано при старте — горизонтально,
 *  вертикально, или под углом. Если угол > 30° дольше одного цикла, поднимается
 *  защёлка tiltLock, и логика управления полностью обесточивает электролизер
 *  и клапан. Защёлка сбрасывается командой LOCK (cmd=2).
 *
 *  Для устойчивости к вибрации используется скользящее среднее по 5 измерениям.
 *
 *  СХЕМА ПОДКЛЮЧЕНИЯ
 *  -----------------
 *    NRF24L01+:
 *        CE      → GPIO4
 *        CSN     → GPIO5
 *        SCK     → GPIO18  (VSPI SCK)
 *        MISO    → GPIO19  (VSPI MISO)
 *        MOSI    → GPIO23  (VSPI MOSI)
 *        VCC     → 3.3 V
 *        IRQ     → не подключается
 *        ⚠ ОБЯЗАТЕЛЬНО электролитический конденсатор 470 мкФ × 10 В между
 *          VCC и GND модуля! Без него на MAX-мощности модуль сбоит.
 *
 *    OLED SSD1306 (I2C, 128×64):
 *        SDA     → GPIO21
 *        SCL     → GPIO22
 *        VCC     → 3.3 V
 *        Адрес   → 0x3C (если не работает — попробуйте 0x3D)
 *
 *    MPU6050 (I2C, 6-осевой акселерометр+гироскоп):
 *        SDA     → GPIO21  (на ТУ ЖЕ шину, что OLED)
 *        SCL     → GPIO22  (на ТУ ЖЕ шину, что OLED)
 *        VCC     → 3.3 V   (на модулях GY-521 встроенный регулятор LDO,
 *                          можно питать и от 5 V — смотрите по плате)
 *        GND     → GND
 *        AD0     → GND     → адрес 0x68 (по умолчанию)
 *                  VCC     → адрес 0x69 (если хотите изменить)
 *        INT     → не подключается
 *        Подтяжки SDA/SCL к 3.3 V — уже стоят на модуле GY-521, отдельные
 *        резисторы не нужны.
 *
 *    Реле (модули SRD-05VDC-SL-C или аналогичные, HIGH-active по умолчанию):
 *        Электролизер → GPIO25
 *        Клапан       → GPIO26
 *        Пьезоподжиг  → GPIO27
 *        ⚠ Реле должны питаться от ОТДЕЛЬНОГО источника 5 В. ESP32 не вытянет
 *          ток нескольких модулей одновременно. Общая земля с ESP32 ОБЯЗАТЕЛЬНА.
 *        Если у вас LOW-active модули — установите RELAY_ACTIVE_LOW = true.
 *
 *    Физические кнопки на корпусе мозга (INPUT_PULLUP, замыкаются на GND):
 *        POWER → GPIO13  — одиночный клик: вкл/выкл системы
 *                         — двойной клик в окне 400 мс: продувка до нуля
 *                           (открывает клапан, ждёт пока давление упадёт
 *                           ниже 0.05 бар или до тайм-аута 90 сек,
 *                           затем выключает систему)
 *        LOCK  → GPIO14  — переключает блокировку
 *                         — также используется для сброса защёлки TILT
 *                           и защёлки overpressure
 *        PURGE → GPIO33  — короткая ручная продувка: открывает клапан на 200 мс
 *                          для разового стравливания небольшого давления
 *
 *    Светодиоды (через резистор 220 Ом → GND):
 *        LED_POWER → GPIO17  — горит когда система работает (не в продувке)
 *        LED_LOCK  → GPIO12  — горит при блокировке  ⚠ strapping-pin!
 *        LED_MODE  → GPIO2   — мигает при ГОРЕЛКЕ, горит при MANUAL GAS
 *                              ⚠ strapping-pin! Это встроенный синий LED ESP32
 *
 *        О strapping-pins (GPIO0, GPIO2, GPIO12, GPIO15):
 *        Эти пины влияют на режим загрузки ESP32. Если плата не стартует
 *        с подключёнными светодиодами — отсоедините их временно, проверьте
 *        загрузку, и при необходимости перенесите LED на другие пины
 *        (например, GPIO16, GPIO19, GPIO22 — НО учтите, GPIO22 уже занят I2C).
 *
 *    Датчик давления (например, DXP-2-K-0-10-bar или аналог 0.5–4.5 В):
 *        Сигнал → GPIO36 (SENSOR_VP, только вход, 12-битный ADC)
 *
 *        ⚠ ВАЖНО про делитель напряжения:
 *        ESP32 ADC принимает 0–3.3 В. Если ваш датчик выдаёт 0.5–4.5 В,
 *        нужен делитель 1:1.5 (например, 10 кОм + 20 кОм или 10к + 15к).
 *        Формула в readPressure() сейчас НЕ учитывает делитель.
 *        Если у вас есть делитель — раскомментируйте две строки про
 *        VOLTAGE_DIVIDER в функции readPressure().
 *
 *  ИСПОЛЬЗУЕМЫЕ БИБЛИОТЕКИ (установить через Library Manager Arduino IDE)
 *  ---------------------------------------------------------------------
 *    RF24                  — by TMRh20
 *    ESPAsyncWebServer     — by me-no-dev / lacamera (fork)
 *    AsyncTCP              — by me-no-dev / esphome (ESP32-версия, НЕ ESPAsyncTCP!)
 *    Adafruit SSD1306      — by Adafruit
 *    Adafruit GFX Library  — by Adafruit (подтянется автоматически)
 *
 *    MPU6050 НЕ требует отдельной библиотеки — используется прямой I2C-доступ
 *    через стандартную Wire.h. Это упрощает зависимости и снимает конфликты
 *    с другими MPU-библиотеками.
 *
 *  WI-FI ТОЧКА ДОСТУПА
 *  -------------------
 *    SSID:    H2_PRO
 *    Пароль:  hydrogen_2024
 *    IP:      192.168.4.1
 *    После подключения к Wi-Fi откройте в браузере: http://192.168.4.1
 *
 *  СВЯЗЬ С ПУЛЬТОМ (NRF24L01+)
 *  ---------------------------
 *    Скорость:         2 Мбит/с (RF24_2MBPS)
 *    Мощность:         MAX (RF24_PA_MAX, ~0 dBm)
 *    CRC:              16 бит
 *    Канал:            76
 *    Адреса труб:
 *        "CMD01"  — пульт → мозг (команды)
 *        "STS01"  — мозг → пульт (статус)
 *    Статус отправляется каждые 100 мс (10 Гц).
 *    Команды от пульта принимаются по мере прихода (асинхронно).
 *
 *  ТАБЛИЦА КОМАНД (передаются от пульта или из веб-интерфейса)
 *  -----------------------------------------------------------
 *    cmd= 1  → POWER toggle (вкл/выкл системы)
 *    cmd= 2  → LOCK toggle (блокировка/разблокировка + сброс защёлок
 *                            TILT и overpressure)
 *    cmd= 3  → ГОРЕЛКА toggle (бывший CONST GAS; качаем до targetPressure
 *                              с открытым клапаном, при первом достижении
 *                              цели автоматически срабатывает пьезоподжиг)
 *    cmd= 4  → PIEZO ручной поджиг (импульс 500 мс)
 *    cmd= 5  → PURGE_SHORT (открыть клапан на 200 мс для стравливания)
 *    cmd= 7  → MANUAL GAS toggle (электролизер+клапан безусловно)
 *    cmd= 8  → TARGET +0.5 бар
 *    cmd= 9  → TARGET -0.5 бар
 *    cmd=99  → SAFE OFF (продувка до давления ≤ 0.05 бар, потом выключение
 *                        системы; макс. время 90 секунд, потом принудительно)
 *
 *  ПОРОГИ ДАВЛЕНИЯ И НАКЛОНА
 *  --------------------------
 *    MIN_TARGET_PRESSURE  = 0.5 бар  (минимум регулировки)
 *    MAX_TARGET_PRESSURE  = 6.0 бар  (максимум регулировки)
 *    WARN_PRESSURE        = 5.5 бар  (предупреждение, флаг ovWarn)
 *    MAX_SAFE_PRESSURE    = 6.5 бар  (АВАРИЯ — защёлка ovLock, электролизер OFF)
 *    PURGE_ZERO_BAR       = 0.05 бар (порог окончания продувки до нуля)
 *    TILT_ANGLE_DEG       = 30°      (порог TILT-сработки)
 *
 *  ПОВЕДЕНИЕ ПО УМОЛЧАНИЮ (стартовое состояние)
 *  --------------------------------------------
 *    • Система ВЫКЛЮЧЕНА   (systemOn = false)
 *    • Блокировка ВКЛЮЧЕНА (isLocked = true)
 *    • Целевое давление    = 1.0 бар
 *    • Все реле обесточиваются сразу в setup() до любой другой инициализации
 *    • MPU6050 калибруется один раз при старте мозга (~0.5 сек)
 *
 *  ПОВЕДЕНИЕ ПРИ АВАРИЯХ
 *  ---------------------
 *    Превышение 6.5 бар:
 *        → электролизер выключается принудительно
 *        → ГОРЕЛКА сбрасывается
 *        → поднимается ovLock (защёлка)
 *        → снимается только нажатием LOCK при давлении < 5.5 бар
 *
 *    Наклон > 30°:
 *        → электролизер и клапан обесточиваются
 *        → поднимается tiltLock (защёлка) + isLocked = true
 *        → снимается нажатием LOCK; если угол всё ещё > 30°,
 *          защёлка снова взводится автоматически
 *
 *    Потеря связи с пультом (3 секунды без приёма):
 *        → флаг remoteOnline = false (отображается на OLED и в вебе),
 *          но управление продолжается (веб-интерфейс остаётся рабочим)
 *
 *  АВТОР: ChatGPT/Claude + пользователь, 2026
 * =====================================================================================
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
#include <math.h>


// =====================================================================================
//  WI-FI
// =====================================================================================
const char* AP_SSID     = "H2_PRO";
const char* AP_PASSWORD = "hydrogen_2024";


// =====================================================================================
//  ПИНЫ
// =====================================================================================
#define PIN_RELAY_ELECTRO   25
#define PIN_RELAY_VALVE     26
#define PIN_RELAY_PIEZO     27

#define PIN_BTN_POWER       13   // 1×=ON/OFF  2×=продувка до нуля
#define PIN_BTN_LOCK        14   // блокировка + сброс защёлок
#define PIN_BTN_PURGE       33   // короткая продувка 200 мс

#define PIN_LED_POWER       17
#define PIN_LED_LOCK        12   // ! strapping-pin
#define PIN_LED_MODE         2   // ! strapping-pin (встроенный LED)

#define PIN_PRESSURE        36   // ADC1_CH0, только вход

#define PIN_NRF_CE           4
#define PIN_NRF_CSN          5
// VSPI: SCK=18, MISO=19, MOSI=23

#define I2C_SDA             21
#define I2C_SCL             22

#define MPU6050_ADDR        0x68


// =====================================================================================
//  РЕЛЕ — УРОВЕНЬ АКТИВНОСТИ
// =====================================================================================
#define RELAY_ACTIVE_LOW    false
#define RELAY_ON            (RELAY_ACTIVE_LOW ? LOW  : HIGH)
#define RELAY_OFF           (RELAY_ACTIVE_LOW ? HIGH : LOW)


// =====================================================================================
//  ПОРОГИ
// =====================================================================================
const float MAX_SAFE_PRESSURE   = 6.5f;
const float WARN_PRESSURE       = 5.5f;
const float MIN_TARGET_PRESSURE = 0.5f;
const float MAX_TARGET_PRESSURE = 6.0f;
const float TARGET_STEP         = 0.5f;
const float PURGE_ZERO_BAR      = 0.05f;
const float TILT_ANGLE_DEG      = 30.0f;
const int   TILT_AVG_SAMPLES    = 5;     // Скользящее среднее
const int   TILT_CAL_SAMPLES    = 50;    // Калибровка


// =====================================================================================
//  ТАЙМИНГИ (мс)
// =====================================================================================
const unsigned long T_PIEZO_WORK    = 500;
const unsigned long T_PURGE_MAX     = 90000;  // макс. продувка
const unsigned long T_PURGE_SHORT   = 200;    // импульс клапана
const unsigned long T_DOUBLE_CLICK  = 400;
const unsigned long T_DEBOUNCE      = 50;
const unsigned long T_STATUS_TX     = 100;
const unsigned long T_DISPLAY_UPD   = 200;
const unsigned long T_PRESSURE_SAMP = 100;
const unsigned long T_TILT_SAMP     = 100;
const unsigned long T_RF_TIMEOUT    = 3000;
const unsigned long T_SHOW_TARGET   = 2500;   // OLED-экран установки давления


// =====================================================================================
//  NRF24
// =====================================================================================
RF24 radio(PIN_NRF_CE, PIN_NRF_CSN);
const byte PIPE_R2B[6] = "CMD01";
const byte PIPE_B2R[6] = "STS01";

struct __attribute__((packed)) CmdPacket {
  uint8_t type;   // 0x55
  uint8_t cmd;
};

// Статус мозг → пульт (13 байт)
struct __attribute__((packed)) StatusPacket {
  uint8_t type;       // 0xAA
  float   pressure;
  float   target;
  uint8_t flags;      // bit0=sysOn  bit1=locked  bit2=purging(full)
                      // bit3=burner bit4=manual  bit5=electro
                      // bit6=valve  bit7=piezo
  uint8_t flags2;     // bit0=ovWarn  bit1=ovLock  bit2=purgeShort
                      // bit3=tiltLock  bit4=tiltCalibrating
  uint8_t tiltAngle;  // 0..180 градусов (округлённое)
};


// =====================================================================================
//  OLED
// =====================================================================================
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
bool oledOk = false;


// =====================================================================================
//  ВЕБ
// =====================================================================================
AsyncWebServer server(80);


// =====================================================================================
//  СОСТОЯНИЕ
// =====================================================================================
bool  systemOn         = false;
bool  isLocked         = true;
bool  isPurging        = false;
bool  isPurgeShort     = false;
bool  burnerActive     = false;
bool  manualGasActive  = false;
bool  piezoActive      = false;
bool  cgPiezoFired     = false;
bool  electroOn        = false;
bool  valveOn          = false;
bool  overpressureLock = false;
bool  tiltLock         = false;
bool  tiltCalibrating  = true;        // true пока идёт калибровка в setup
bool  mpuOk            = false;
float targetPressure   = 1.0f;
float currentPressure  = 0.0f;
float currentTiltDeg   = 0.0f;        // Текущий угол наклона относительно baseline

// MPU6050 — baseline (эталонный вектор гравитации после калибровки)
float baseAx = 0, baseAy = 0, baseAz = 0;
// Скользящее среднее угла
float angleBuf[TILT_AVG_SAMPLES] = {0};
int   angleIdx = 0;
bool  angleBufFilled = false;

// RF
bool          remoteOnline = false;
unsigned long lastRfRx     = 0;

// Кнопки
bool          pwrLastRaw   = HIGH, pwrStable   = HIGH;
bool          lckLastRaw   = HIGH, lckStable   = HIGH;
bool          prgLastRaw   = HIGH, prgStable   = HIGH;
unsigned long pwrDebTime   = 0,    lckDebTime  = 0,   prgDebTime = 0;
unsigned long pwrLastClick = 0;

// Таймеры
unsigned long purgeStart       = 0;
unsigned long purgeShortStart  = 0;
unsigned long piezoStart       = 0;
unsigned long lastStatusTx     = 0;
unsigned long lastDispUpd      = 0;
unsigned long lastPressureT    = 0;
unsigned long lastTiltT        = 0;
unsigned long bootMs           = 0;
unsigned long lastTargetChange = 0;


// =====================================================================================
//  HTML
// =====================================================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>H2 PRO Control</title>
<style>
  @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&family=Exo+2:wght@300;600;800&display=swap');
  :root{
    --bg:#080810;--card:#131326;--border:#1e1e3a;
    --accent:#00d4ff;--ok:#00ff88;--warn:#ffcc00;--danger:#ff3355;--dim:#3a3a5a;--text:#c8d0e0;
  }
  *{box-sizing:border-box;margin:0;padding:0}
  body{background:var(--bg);color:var(--text);font-family:'Exo 2',sans-serif;
    min-height:100vh;padding:14px 10px;
    background-image:radial-gradient(ellipse at 20% 0%,rgba(0,212,255,.07) 0%,transparent 60%),
      radial-gradient(ellipse at 80% 100%,rgba(77,138,255,.07) 0%,transparent 60%);}
  h1{text-align:center;font-size:1.1em;font-weight:800;letter-spacing:6px;color:var(--accent);
    text-transform:uppercase;margin-bottom:14px;text-shadow:0 0 20px rgba(0,212,255,.5);}
  .panel{max-width:460px;margin:0 auto;display:flex;flex-direction:column;gap:10px;}

  .alert-bar{background:#1a1500;border:1px solid var(--warn);color:var(--warn);
    border-radius:10px;padding:8px 12px;text-align:center;font-size:.75em;
    letter-spacing:2px;text-transform:uppercase;font-family:'Share Tech Mono',monospace;display:none;}
  .alert-bar.show{display:block;animation:ap 1.5s infinite}
  .alert-bar.danger{background:#1a0008;border-color:var(--danger);color:var(--danger)}
  .alert-bar.tilt{background:#1a0a1a;border-color:#ff6bff;color:#ff6bff}
  @keyframes ap{0%,100%{opacity:.7}50%{opacity:1}}

  .pressure{background:var(--card);border:1px solid var(--border);border-radius:14px;
    padding:16px;text-align:center;position:relative;overflow:hidden;}
  .pressure::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;
    background:linear-gradient(90deg,transparent,var(--accent),transparent);}
  .pressure.warn::before{background:linear-gradient(90deg,transparent,var(--warn),transparent)}
  .pressure.danger::before{background:linear-gradient(90deg,transparent,var(--danger),transparent)}
  .p-lbl{font-size:.65em;letter-spacing:4px;color:var(--dim);text-transform:uppercase;margin-bottom:6px}
  .p-num{font-family:'Share Tech Mono',monospace;font-size:3.4em;color:var(--accent);
    line-height:1;text-shadow:0 0 30px rgba(0,212,255,.6);transition:color .3s;}
  .p-num.warn{color:var(--warn);text-shadow:0 0 30px rgba(255,204,0,.6)}
  .p-num.danger{color:var(--danger);text-shadow:0 0 30px rgba(255,51,85,.6)}
  .p-unit{font-size:.4em;color:#5577aa;margin-left:6px}
  .p-bar-wrap{position:relative;margin-top:10px}
  .p-bar{height:6px;background:#111128;border-radius:3px;overflow:hidden}
  .p-fill{height:100%;border-radius:3px;background:linear-gradient(90deg,var(--ok),var(--accent));transition:width .8s ease,background .3s}
  .p-fill.warn{background:linear-gradient(90deg,var(--accent),var(--warn))}
  .p-fill.danger{background:linear-gradient(90deg,var(--warn),var(--danger))}
  .p-tgt{position:absolute;top:-3px;width:2px;height:13px;background:var(--warn);
    border-radius:1px;transition:left .5s ease;pointer-events:none;}
  .p-scale{display:flex;justify-content:space-between;font-size:.55em;color:var(--dim);
    font-family:'Share Tech Mono',monospace;margin-top:4px;letter-spacing:1px}

  .chart-box{background:var(--card);border:1px solid var(--border);border-radius:14px;padding:10px 12px;}
  .chart-lbl{font-size:.6em;letter-spacing:3px;color:var(--dim);text-transform:uppercase;
    margin-bottom:4px;display:flex;justify-content:space-between;}
  .chart-lbl span:last-child{color:#4d8aff;font-family:'Share Tech Mono',monospace}
  #chart{width:100%;height:50px;display:block}

  .tilt-row{display:flex;justify-content:space-between;align-items:center;
    background:var(--card);border:1px solid var(--border);border-radius:10px;
    padding:8px 12px;font-size:.65em;letter-spacing:2px;
    font-family:'Share Tech Mono',monospace;color:var(--dim);}
  .tilt-row.warn{border-color:var(--warn);color:var(--warn);}
  .tilt-row.danger{border-color:#ff6bff;color:#ff6bff;animation:pulse .9s infinite;}
  .tilt-val{color:var(--ok);font-size:1.2em;}
  .tilt-val.warn{color:var(--warn);}
  .tilt-val.danger{color:#ff6bff;}

  .badges{display:flex;flex-wrap:wrap;gap:6px}
  .badge{padding:4px 10px;border-radius:20px;font-size:.62em;font-weight:600;
    letter-spacing:1.5px;text-transform:uppercase;background:var(--card);color:var(--dim);
    border:1px solid var(--border);transition:all .25s;}
  .badge.ok{background:#001a0e;color:var(--ok);border-color:var(--ok);box-shadow:0 0 8px rgba(0,255,136,.2)}
  .badge.warn{background:#1a1500;color:var(--warn);border-color:var(--warn);box-shadow:0 0 8px rgba(255,204,0,.2)}
  .badge.danger{background:#1a0008;color:var(--danger);border-color:var(--danger);animation:pulse .9s infinite}
  .badge.tilt{background:#1a0a1a;color:#ff6bff;border-color:#ff6bff;animation:pulse .9s infinite}
  @keyframes pulse{0%,100%{box-shadow:0 0 6px rgba(255,51,85,.3)}50%{box-shadow:0 0 14px rgba(255,51,85,.7)}}

  .info-row{display:flex;justify-content:space-between;align-items:center;padding:0 4px;
    font-size:.62em;letter-spacing:2px;color:var(--dim);font-family:'Share Tech Mono',monospace;}
  .rf-indicator{display:flex;align-items:center;gap:6px;}
  .rf-dot{width:7px;height:7px;border-radius:50%;background:var(--dim);transition:all .3s}
  .rf-dot.ok{background:var(--ok);box-shadow:0 0 8px var(--ok)}
  .rf-dot.fail{background:var(--danger);animation:pulse .9s infinite}
  .uptime{color:#4d8aff}

  .btn-power{width:100%;padding:16px;border:none;border-radius:12px;cursor:pointer;
    font-family:'Exo 2',sans-serif;font-size:.9em;font-weight:800;letter-spacing:3px;
    text-transform:uppercase;transition:all .2s;
    background:var(--card);color:var(--text);border:1px solid var(--border);}
  .btn-power.on{background:linear-gradient(135deg,#1a0010,#330015);color:var(--danger);
    border-color:var(--danger);box-shadow:0 0 20px rgba(255,51,85,.2);}
  .btn-power:active{transform:scale(.97)}

  .grid{display:grid;grid-template-columns:1fr 1fr;gap:8px}
  .ctrl{padding:14px 8px;border:1px solid var(--border);border-radius:11px;
    background:var(--card);cursor:pointer;font-family:'Exo 2',sans-serif;
    font-size:.7em;font-weight:600;letter-spacing:1px;text-transform:uppercase;
    color:var(--text);transition:all .2s;display:flex;flex-direction:column;
    align-items:center;gap:8px;}
  .ctrl .dot{width:8px;height:8px;border-radius:50%;background:var(--dim);transition:all .25s}
  .ctrl.wide{grid-column:1/-1}
  .ctrl:hover:not(:disabled){filter:brightness(1.2)}
  .ctrl:active:not(:disabled){transform:scale(.95)}
  .ctrl:disabled{opacity:.25;cursor:not-allowed}
  .ctrl.active{background:#001a0e;border-color:var(--ok)}
  .ctrl.active .dot{background:var(--ok);box-shadow:0 0 8px var(--ok)}
  .ctrl.active-warn{background:#1a1500;border-color:var(--warn)}
  .ctrl.active-warn .dot{background:var(--warn);box-shadow:0 0 8px var(--warn)}
  .ctrl.active-red{background:#1a0008;border-color:var(--danger)}
  .ctrl.active-red .dot{background:var(--danger);box-shadow:0 0 8px var(--danger)}
  .ctrl.purge-btn{background:#0a1a0a;border-color:#2a4a2a;color:#8f8;}
  .ctrl.purge-btn:hover{background:#152515;border-color:#4a7a4a;}

  .footer{text-align:center;font-size:.7em;letter-spacing:2px;color:var(--warn);
    min-height:16px;font-family:'Share Tech Mono',monospace;}

  .tgt-block{background:var(--card);border:1px solid var(--border);border-radius:12px;
    padding:14px;position:relative;overflow:hidden;}
  .tgt-block::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;
    background:linear-gradient(90deg,transparent,var(--warn),transparent);}
  .t-lbl{font-size:.62em;letter-spacing:4px;color:var(--dim);text-transform:uppercase;
    text-align:center;margin-bottom:10px;}
  .t-row{display:flex;align-items:center;gap:8px;margin-bottom:10px;}
  .t-val{font-family:'Share Tech Mono',monospace;font-size:1.9em;color:var(--warn);min-width:88px;text-align:center;}
  .t-unit{font-size:.42em;color:#887700;}
  .t-pm{width:38px;height:38px;border-radius:9px;border:1px solid var(--warn);
    background:#1a1400;color:var(--warn);font-size:1.3em;font-weight:800;
    cursor:pointer;display:flex;align-items:center;justify-content:center;transition:all .2s;}
  .t-pm:hover{background:#2a2000;}.t-pm:active{transform:scale(.92);}
  .t-pm:disabled{opacity:.3;cursor:not-allowed;}
  input[type=range]{flex:1;-webkit-appearance:none;height:4px;border-radius:2px;outline:none;cursor:pointer;
    background:linear-gradient(90deg,var(--warn) var(--tp,10%),#1e1e3a var(--tp,10%));}
  input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:18px;height:18px;
    border-radius:50%;background:var(--warn);cursor:pointer;}
  .qrow{display:flex;gap:5px;margin-bottom:10px;}
  .qb{flex:1;padding:6px 2px;border:1px solid var(--border);border-radius:7px;
    background:var(--card);cursor:pointer;font-family:'Share Tech Mono',monospace;
    font-size:.7em;color:var(--dim);transition:all .2s;text-align:center;}
  .qb.active,.qb:hover{border-color:var(--warn);color:var(--warn);background:#1a1400;}
  .apply-btn{width:100%;padding:10px;border:1px solid var(--warn);border-radius:9px;
    background:#1a1400;color:var(--warn);cursor:pointer;font-family:'Exo 2',sans-serif;
    font-size:.72em;font-weight:800;letter-spacing:3px;text-transform:uppercase;transition:all .2s;}
  .apply-btn:hover{background:#2a2000;}.apply-btn:active{transform:scale(.97);}
  .apply-btn:disabled{opacity:.3;cursor:not-allowed;}
  .t-ok{text-align:center;font-size:.6em;font-family:'Share Tech Mono',monospace;
    color:var(--ok);min-height:14px;margin-top:5px;transition:opacity .4s;opacity:0;}
</style>
</head>
<body>
<h1>⚡ H2 PRO</h1>
<div class="panel">

  <div class="alert-bar" id="alertBar"></div>

  <div class="pressure" id="pBox">
    <div class="p-lbl">Давление в баке</div>
    <div class="p-num" id="pNum"><span id="pv">0.00</span><span class="p-unit">бар</span></div>
    <div class="p-bar-wrap">
      <div class="p-bar"><div class="p-fill" id="pf" style="width:0%"></div></div>
      <div class="p-tgt" id="ptgt" style="left:16.6%"></div>
      <div class="p-scale"><span>0</span><span>2</span><span>4</span><span>6 бар</span></div>
    </div>
  </div>

  <div class="chart-box">
    <div class="chart-lbl"><span>История · 60 сек</span><span id="chartMax">max 0.00</span></div>
    <canvas id="chart" width="440" height="50"></canvas>
  </div>

  <div class="tilt-row" id="tiltRow">
    <span id="tiltLbl">НАКЛОН (порог 30°)</span>
    <span class="tilt-val" id="tiltVal">--°</span>
  </div>

  <div class="badges">
    <span class="badge" id="bPwr">POWER</span>
    <span class="badge" id="bLck">LOCK</span>
    <span class="badge" id="bTlt">TILT</span>
    <span class="badge" id="bPrg">PURGE</span>
    <span class="badge" id="bPsh">PURGE SHORT</span>
    <span class="badge" id="bBrn">ГОРЕЛКА</span>
    <span class="badge" id="bMgs">MANUAL</span>
    <span class="badge" id="bPzo">PIEZO</span>
    <span class="badge" id="bEle">ELECTRO</span>
    <span class="badge" id="bVal">VALVE</span>
  </div>

  <div class="info-row">
    <div class="rf-indicator">
      <div class="rf-dot" id="rfDot"></div>
      <span id="rfTxt">ПУЛЬТ: —</span>
    </div>
    <div class="uptime" id="uptime">UP 00:00:00</div>
  </div>

  <button class="btn-power" id="btnPwr" onclick="handlePower()">ВКЛЮЧИТЬ СИСТЕМУ</button>

  <div class="grid">
    <button class="ctrl wide" id="cLck" onclick="cmd(2)">
      <div class="dot"></div>БЛОКИРОВКА / СБРОС АВАРИЙ
    </button>
    <button class="ctrl" id="cBrn" onclick="cmd(3)" disabled>
      <div class="dot"></div>ГОРЕЛКА
    </button>
    <button class="ctrl" id="cMgs" onclick="cmd(7)" disabled>
      <div class="dot"></div>РУЧНАЯ ПОДАЧА
    </button>
    <button class="ctrl active-warn wide" id="cPzo" onclick="cmd(4)" disabled>
      <div class="dot"></div>ПОДЖИГ (500 мс)
    </button>
    <button class="ctrl purge-btn wide" id="cPsh" onclick="cmd(5)" disabled>
      <div class="dot"></div>ПРОДУТЬ (200 мс клапан)
    </button>
  </div>

  <div class="tgt-block">
    <div class="t-lbl">Целевое давление (max 6.0 бар)</div>
    <div class="t-row">
      <button class="t-pm" id="tMinus" onclick="stepTgt(-0.5)">−</button>
      <span class="t-val"><span id="tval">1.0</span><span class="t-unit"> бар</span></span>
      <button class="t-pm" id="tPlus"  onclick="stepTgt(+0.5)">+</button>
    </div>
    <div class="t-row">
      <input type="range" id="tslider" min="0.5" max="6.0" step="0.1" value="1.0" oninput="onSlide(this.value)">
    </div>
    <div class="qrow">
      <button class="qb active" onclick="setQ(1)">1</button>
      <button class="qb" onclick="setQ(2)">2</button>
      <button class="qb" onclick="setQ(3)">3</button>
      <button class="qb" onclick="setQ(4)">4</button>
      <button class="qb" onclick="setQ(5)">5</button>
      <button class="qb" onclick="setQ(6)">6</button>
    </div>
    <button class="apply-btn" id="applyBtn" onclick="applyTgt()">УСТАНОВИТЬ ДАВЛЕНИЕ</button>
    <div class="t-ok" id="tok"></div>
  </div>

  <div class="footer" id="foot"></div>
</div>

<script>
  let ct=null,purging=false,pendingTgt=1.0;
  const HIST_LEN=60;
  const hist=new Array(HIST_LEN).fill(0);

  function onSlide(v){
    v=parseFloat(v);pendingTgt=v;
    document.getElementById('tval').innerText=v.toFixed(1);
    document.getElementById('tslider').style.setProperty('--tp',((v-0.5)/5.5*100).toFixed(1)+'%');
    updateQbtns(v);
  }
  function setQ(v){
    pendingTgt=v;
    document.getElementById('tslider').value=v;
    document.getElementById('tval').innerText=v.toFixed(1);
    document.getElementById('tslider').style.setProperty('--tp',((v-0.5)/5.5*100).toFixed(1)+'%');
    updateQbtns(v);
  }
  function stepTgt(d){
    let v=Math.max(0.5,Math.min(6.0,Math.round((pendingTgt+d)*2)/2));
    setQ(v);
  }
  function updateQbtns(v){
    [1,2,3,4,5,6].forEach((p,i)=>{
      document.querySelectorAll('.qb')[i].className='qb'+(p===v?' active':'');
    });
  }
  function applyTgt(){
    fetch('/setpressure?val='+pendingTgt.toFixed(1)).catch(()=>{});
    const tok=document.getElementById('tok');
    tok.innerText='✓ установлено: '+pendingTgt.toFixed(1)+' бар';
    tok.style.opacity='1';setTimeout(()=>tok.style.opacity='0',2500);
  }
  function handlePower(){
    if(purging)return;
    if(!ct){ct=setTimeout(()=>{ct=null;cmd(1);},380);}
    else{clearTimeout(ct);ct=null;document.getElementById('foot').innerText='ПРОДУВКА ДО НУЛЯ...';cmd(99);}
  }
  function cmd(id){fetch('/cmd?id='+id).catch(()=>{});}
  function cls(id,val,c){document.getElementById(id).className='badge'+(val?' '+c:'');}

  function drawChart(tgt){
    const c=document.getElementById('chart');
    const ctx=c.getContext('2d');
    const w=c.width,h=c.height;
    ctx.clearRect(0,0,w,h);
    ctx.strokeStyle='#1e1e3a';ctx.lineWidth=1;
    for(let y=0;y<=4;y++){const yy=y*(h-2)/4+1;ctx.beginPath();ctx.moveTo(0,yy);ctx.lineTo(w,yy);ctx.stroke();}
    const maxS=6.5,step=w/(HIST_LEN-1);
    ctx.strokeStyle='#00d4ff';ctx.lineWidth=1.5;
    ctx.shadowColor='rgba(0,212,255,.6)';ctx.shadowBlur=6;
    ctx.beginPath();
    for(let i=0;i<HIST_LEN;i++){const x=i*step,y=h-(hist[i]/maxS)*(h-2)-1;i===0?ctx.moveTo(x,y):ctx.lineTo(x,y);}
    ctx.stroke();ctx.shadowBlur=0;
    if(tgt>0){
      const yt=h-(tgt/maxS)*(h-2)-1;
      ctx.strokeStyle='rgba(255,204,0,.5)';ctx.setLineDash([3,3]);
      ctx.beginPath();ctx.moveTo(0,yt);ctx.lineTo(w,yt);ctx.stroke();ctx.setLineDash([]);
    }
  }
  function fmtUp(s){
    return [Math.floor(s/3600),Math.floor(s%3600/60),s%60].map(n=>String(n).padStart(2,'0')).join(':');
  }

  function tick(){
    fetch('/status').then(r=>r.json()).then(d=>{
      const p=d.p;
      document.getElementById('pv').innerText=p.toFixed(2);
      document.getElementById('pf').style.width=Math.min(p/6.5*100,100)+'%';
      document.getElementById('ptgt').style.left='calc('+Math.min(d.tgt/6.5*100,100)+'% - 1px)';
      const pBox=document.getElementById('pBox');
      const pNum=document.getElementById('pNum');
      const pf=document.getElementById('pf');
      pBox.className='pressure';pNum.className='p-num';pf.className='p-fill';
      if(d.ovl){pBox.classList.add('danger');pNum.classList.add('danger');pf.classList.add('danger');}
      else if(d.ovw){pBox.classList.add('warn');pNum.classList.add('warn');pf.classList.add('warn');}

      // Tilt-индикатор
      const tRow=document.getElementById('tiltRow');
      const tVal=document.getElementById('tiltVal');
      const tLbl=document.getElementById('tiltLbl');
      tRow.className='tilt-row';tVal.className='tilt-val';
      if(d.tcal){
        tLbl.innerText='КАЛИБРОВКА MPU6050...';
        tVal.innerText='wait';
      } else if(d.tlk){
        tRow.classList.add('danger');tVal.classList.add('danger');
        tLbl.innerText='НАКЛОН — АВАРИЯ (LOCK для сброса)';
        tVal.innerText=d.ta+'°';
      } else if(d.ta>=20){
        tRow.classList.add('warn');tVal.classList.add('warn');
        tLbl.innerText='НАКЛОН (порог 30°)';
        tVal.innerText=d.ta+'°';
      } else {
        tLbl.innerText='НАКЛОН (порог 30°)';
        tVal.innerText=d.ta+'°';
      }

      const ab=document.getElementById('alertBar');
      if(d.tlk){ab.className='alert-bar show tilt';ab.innerText='⚠ НАКЛОН '+d.ta+'° — ЭЛЕКТРОЛИЗЕР И КЛАПАН ОБЕСТОЧЕНЫ';}
      else if(d.ovl){ab.className='alert-bar show danger';ab.innerText='⚠ АВАРИЯ '+p.toFixed(2)+' БАР — ЭЛЕКТРОЛИЗЕР ОТКЛЮЧЁН';}
      else if(d.ovw){ab.className='alert-bar show';ab.innerText='⚠ Приближение к аварии: '+p.toFixed(2)+' / 6.5 бар';}
      else{ab.className='alert-bar';}

      hist.shift();hist.push(p);
      document.getElementById('chartMax').innerText='max '+Math.max(...hist).toFixed(2);
      drawChart(d.tgt);

      cls('bPwr',d.pwr,'ok');cls('bLck',d.lck,'warn');
      cls('bTlt',d.tlk,'tilt');
      cls('bPrg',d.prg,'danger');cls('bPsh',d.psh,'warn');
      cls('bBrn',d.brn,'ok');cls('bMgs',d.mgs,'ok');
      cls('bPzo',d.pzo,'warn');cls('bEle',d.ele,'ok');cls('bVal',d.val,'ok');

      const bp=document.getElementById('btnPwr');
      bp.className='btn-power'+(d.pwr?' on':'');
      bp.innerText=d.pwr?'ВЫКЛЮЧИТЬ  (2× = ПРОДУВКА ДО НУЛЯ)':'ВКЛЮЧИТЬ СИСТЕМУ';

      const dis=!d.pwr||d.prg||d.lck||d.tlk;
      ['cBrn','cMgs','cPzo','cPsh'].forEach(i=>document.getElementById(i).disabled=dis);
      document.getElementById('cLck').className='ctrl wide'+(d.lck?' active-warn':'');
      document.getElementById('cBrn').className='ctrl'+(d.brn?' active':'');
      document.getElementById('cMgs').className='ctrl'+(d.mgs?' active':'');
      document.getElementById('cPzo').className='ctrl active-warn wide'+(d.pzo?' active-red':'');
      document.getElementById('cPsh').className='ctrl purge-btn wide'+(d.psh?' active-red':'');

      const sl=document.getElementById('tslider');
      if(document.activeElement!==sl){
        const v=parseFloat(d.tgt);
        sl.value=v;document.getElementById('tval').innerText=v.toFixed(1);
        sl.style.setProperty('--tp',((v-0.5)/5.5*100).toFixed(1)+'%');
        pendingTgt=v;updateQbtns(v);
      }
      document.getElementById('rfDot').className='rf-dot'+(d.rf?' ok':' fail');
      document.getElementById('rfTxt').innerText='ПУЛЬТ: '+(d.rf?'ОНЛАЙН':'ОФФЛАЙН');
      document.getElementById('uptime').innerText='UP '+fmtUp(d.up);

      purging=d.prg;
      if(d.prg)document.getElementById('foot').innerText='⏳ ПРОДУВКА ДО НУЛЯ — '+p.toFixed(2)+' бар';
      else if(d.psh)document.getElementById('foot').innerText='⚡ МИНИ-ПРОДУВКА 200 мс';
      else if(d.tlk)document.getElementById('foot').innerText='⚠ TILT — выровняйте устройство и нажмите LOCK';
      else if(d.ovl)document.getElementById('foot').innerText='⚠ АВАРИЯ — СБРОСЬТЕ ДАВЛЕНИЕ';
      else if(!d.pwr)document.getElementById('foot').innerText='';
      else document.getElementById('foot').innerText='';
    }).catch(()=>{document.getElementById('foot').innerText='Нет связи с мозгом...';});
  }
  document.getElementById('tslider').style.setProperty('--tp','9.1%');
  setInterval(tick,250);tick();
</script>
</body></html>
)rawliteral";


// =====================================================================================
//  MPU6050 — ПРЯМОЙ I2C
// =====================================================================================
bool mpuWriteReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return (Wire.endTransmission() == 0);
}

bool mpuReadAccel(float &ax, float &ay, float &az) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B);  // ACCEL_XOUT_H
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)MPU6050_ADDR, (uint8_t)6) != 6) return false;
  int16_t rx = (Wire.read() << 8) | Wire.read();
  int16_t ry = (Wire.read() << 8) | Wire.read();
  int16_t rz = (Wire.read() << 8) | Wire.read();
  // Делитель для ±2g (LSB sensitivity = 16384) — для угла масштаб не важен,
  // считаем в "сырых" единицах для скорости
  ax = (float)rx;
  ay = (float)ry;
  az = (float)rz;
  return true;
}

bool mpuInit() {
  // Пробуждение (PWR_MGMT_1 = 0)
  if (!mpuWriteReg(0x6B, 0x00)) return false;
  delay(50);
  // Диапазон акселерометра ±2g (ACCEL_CONFIG = 0)
  if (!mpuWriteReg(0x1C, 0x00)) return false;
  // Лёгкий фильтр (CONFIG = 3, ~44 Hz)
  if (!mpuWriteReg(0x1A, 0x03)) return false;
  delay(50);
  // Проверка чтения
  float ax, ay, az;
  if (!mpuReadAccel(ax, ay, az)) return false;
  return true;
}

void mpuCalibrate() {
  // Снимаем TILT_CAL_SAMPLES измерений, усредняем
  Serial.print("[MPU] Калибровка ");
  double sx = 0, sy = 0, sz = 0;
  int got = 0;
  for (int i = 0; i < TILT_CAL_SAMPLES; i++) {
    float ax, ay, az;
    if (mpuReadAccel(ax, ay, az)) {
      sx += ax; sy += ay; sz += az;
      got++;
    }
    if (i % 10 == 0) Serial.print('.');
    delay(10);
  }
  if (got > 0) {
    baseAx = sx / got;
    baseAy = sy / got;
    baseAz = sz / got;
    Serial.printf(" OK (samples=%d, baseline=[%.0f,%.0f,%.0f])\n", got, baseAx, baseAy, baseAz);
  } else {
    Serial.println(" FAIL");
  }
}

// Угол между текущим вектором гравитации и baseline, в градусах
float mpuAngleVsBaseline() {
  float ax, ay, az;
  if (!mpuReadAccel(ax, ay, az)) return -1.0f;
  float dot = ax*baseAx + ay*baseAy + az*baseAz;
  float magA = sqrtf(ax*ax + ay*ay + az*az);
  float magB = sqrtf(baseAx*baseAx + baseAy*baseAy + baseAz*baseAz);
  if (magA < 1.0f || magB < 1.0f) return 0;
  float c = dot / (magA * magB);
  if (c > 1.0f)  c = 1.0f;
  if (c < -1.0f) c = -1.0f;
  return acosf(c) * 180.0f / (float)M_PI;
}


// =====================================================================================
//  УТИЛИТЫ
// =====================================================================================
void setRelay(int pin, bool on) { digitalWrite(pin, on ? RELAY_ON : RELAY_OFF); }

void allRelaysOff() {
  setRelay(PIN_RELAY_ELECTRO, false);
  setRelay(PIN_RELAY_VALVE,   false);
  setRelay(PIN_RELAY_PIEZO,   false);
  electroOn = false; valveOn = false;
}

// Только электролизер и клапан (для TILT-сработки во время продувки)
void cutElectroAndValve() {
  setRelay(PIN_RELAY_ELECTRO, false);
  setRelay(PIN_RELAY_VALVE,   false);
  electroOn = false; valveOn = false;
}

void resetProcesses() {
  burnerActive = false; manualGasActive = false;
  piezoActive  = false; cgPiezoFired    = false;
}

float readPressure() {
  float v = analogRead(PIN_PRESSURE) / 4095.0f * 3.3f;
  // const float VOLTAGE_DIVIDER = 1.5f; v *= VOLTAGE_DIVIDER;  // если есть делитель 10к/20к
  return max(0.0f, (v - 0.5f) / 4.0f * 10.0f);
}

void startPiezo() {
  if (!piezoActive && systemOn && !isLocked && !isPurging && !tiltLock) {
    piezoActive = true; piezoStart = millis();
    setRelay(PIN_RELAY_PIEZO, true);
  }
}

void setTarget(float v) {
  v = constrain(roundf(v * 10.0f) / 10.0f, MIN_TARGET_PRESSURE, MAX_TARGET_PRESSURE);
  targetPressure   = v;
  lastTargetChange = millis();
}

void bumpTarget(float delta) {
  setTarget(roundf((targetPressure + delta) * 2.0f) / 2.0f);
}


// =====================================================================================
//  КОМАНДЫ
// =====================================================================================
void executeCommand(uint8_t cmd) {
  if (isPurging && cmd != 2) return;

  switch (cmd) {
    case 1:  // POWER
      if (!systemOn) {
        if (overpressureLock && currentPressure < WARN_PRESSURE) overpressureLock = false;
        systemOn = true;
      } else {
        systemOn = false; resetProcesses(); allRelaysOff();
      }
      break;

    case 2:  // LOCK toggle + сброс защёлок TILT/overpressure
      if (isLocked || tiltLock || overpressureLock) {
        // Попытка разблокировки: сбрасываем всё, что можем
        tiltLock = false;
        if (currentPressure < WARN_PRESSURE) overpressureLock = false;
        isLocked = false;
        // Если устройство всё ещё наклонено, на следующем тике tiltLock
        // снова взведётся — и isLocked тоже
      } else {
        isLocked = true;
        resetProcesses();
        allRelaysOff();
      }
      break;

    case 3:  // ГОРЕЛКА
      if (systemOn && !isLocked && !tiltLock) {
        burnerActive = !burnerActive;
        if (burnerActive) { manualGasActive = false; cgPiezoFired = false; }
      }
      break;

    case 4: startPiezo(); break;

    case 5:  // PURGE SHORT
      if (systemOn && !isLocked && !tiltLock && !isPurging && !isPurgeShort) {
        isPurgeShort    = true;
        purgeShortStart = millis();
        setRelay(PIN_RELAY_VALVE, true);
        valveOn = true;
      }
      break;

    case 7:  // MANUAL GAS
      if (systemOn && !isLocked && !tiltLock) {
        manualGasActive = !manualGasActive;
        if (manualGasActive) burnerActive = false;
      }
      break;

    case 8: bumpTarget(+TARGET_STEP); break;
    case 9: bumpTarget(-TARGET_STEP); break;

    case 99: // SAFE OFF — продувка до нуля
      if (systemOn) {
        resetProcesses();
        setRelay(PIN_RELAY_ELECTRO, false);
        setRelay(PIN_RELAY_PIEZO,   false);
        setRelay(PIN_RELAY_VALVE,   true);
        electroOn = false; valveOn = true;
        isPurging = true; purgeStart = millis();
      }
      break;
  }
}


// =====================================================================================
//  БЕЗОПАСНОСТЬ — ДАВЛЕНИЕ
// =====================================================================================
void safetyCheck() {
  if (currentPressure > MAX_SAFE_PRESSURE && systemOn) {
    setRelay(PIN_RELAY_ELECTRO, false);
    electroOn = false; burnerActive = false; overpressureLock = true;
  }
}


// =====================================================================================
//  БЕЗОПАСНОСТЬ — НАКЛОН
// =====================================================================================
void tiltCheck() {
  if (!mpuOk || tiltCalibrating) return;

  float ang = mpuAngleVsBaseline();
  if (ang < 0) return;  // ошибка чтения — пропускаем тик

  // Скользящее среднее
  angleBuf[angleIdx] = ang;
  angleIdx = (angleIdx + 1) % TILT_AVG_SAMPLES;
  if (angleIdx == 0) angleBufFilled = true;
  int n = angleBufFilled ? TILT_AVG_SAMPLES : angleIdx;
  if (n == 0) return;
  float sum = 0;
  for (int i = 0; i < n; i++) sum += angleBuf[i];
  currentTiltDeg = sum / n;

  if (currentTiltDeg > TILT_ANGLE_DEG) {
    if (!tiltLock) Serial.printf("[TILT] Сработка: %.1f° > %.1f°\n", currentTiltDeg, TILT_ANGLE_DEG);
    tiltLock = true;
    cutElectroAndValve();
    burnerActive    = false;
    manualGasActive = false;
  }
}


// =====================================================================================
//  OLED
// =====================================================================================
void updateDisplay() {
  if (!oledOk) return;
  unsigned long now = millis();
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // ── Экран установки целевого давления ──────────────────────────────────
  if (now - lastTargetChange < T_SHOW_TARGET) {
    oled.setTextSize(1);
    oled.setCursor(8, 0); oled.print("ЦЕЛЕВОЕ ДАВЛЕНИЕ");
    oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);
    oled.setTextSize(3);
    char tBuf[8]; dtostrf(targetPressure, 4, 1, tBuf);
    oled.setCursor(8, 14); oled.print(tBuf);
    oled.setTextSize(1);
    oled.setCursor(100, 22); oled.print("bar");
    int tw = constrain((int)(targetPressure / MAX_TARGET_PRESSURE * 127.0f), 0, 127);
    oled.drawRect(0, 42, 128, 5, SSD1306_WHITE);
    if (tw > 0) oled.fillRect(1, 43, tw, 3, SSD1306_WHITE);
    oled.setCursor(0, 50);
    oled.printf("%.1f         %.1f бар", MIN_TARGET_PRESSURE, MAX_TARGET_PRESSURE);
    oled.display();
    return;
  }

  // ── Заголовок ───────────────────────────────────────────────────────────
  oled.setTextSize(1);
  oled.setCursor(0, 0); oled.print("H2 PRO");
  oled.setCursor(48, 0); oled.print("RF:"); oled.print(remoteOnline ? "OK" : "--");
  oled.setCursor(86, 0);
  if (tiltCalibrating)    oled.print("CAL..");
  else if (tiltLock)      oled.print("TILT!");
  else if (overpressureLock) oled.print("!АВАР");
  else if (isPurging)     oled.print("ПРОД");
  else if (isPurgeShort)  oled.print("ПРОД");
  else                    oled.print(systemOn ? (isLocked ? "ЛОК" : "ВКЛ") : "ВЫКЛ");
  oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  // ── Давление крупно ─────────────────────────────────────────────────────
  oled.setTextSize(2);
  oled.setCursor(0, 12);
  char pBuf[8]; dtostrf(currentPressure, 4, 2, pBuf);
  oled.print(pBuf);
  oled.setTextSize(1);
  oled.setCursor(86, 18); oled.print("bar");

  // ── Угол наклона справа от давления ──────────────────────────────────────
  oled.setCursor(108, 12);
  if (tiltCalibrating) { oled.print("CAL"); }
  else if (mpuOk)      { oled.printf("%3d", (int)currentTiltDeg); oled.setCursor(108, 22); oled.print("deg"); }
  else                 { oled.print("---"); }

  // ── Полоска давления 0..6 бар + маркер цели ─────────────────────────────
  const float SCALE = 6.0f;
  int bw = constrain((int)(currentPressure / SCALE * 127.0f), 0, 127);
  oled.drawRect(0, 30, 128, 5, SSD1306_WHITE);
  if (bw > 0) oled.fillRect(1, 31, bw, 3, SSD1306_WHITE);
  int tx = constrain((int)(targetPressure / SCALE * 127.0f), 0, 127);
  oled.drawLine(tx, 28, tx, 36, SSD1306_WHITE);
  oled.drawLine(tx-1, 28, tx+1, 28, SSD1306_WHITE);
  oled.drawLine(0, 38, 127, 38, SSD1306_WHITE);

  // ── E/V/P + цель ─────────────────────────────────────────────────────────
  oled.setCursor(0, 41);
  oled.print("E:"); oled.print(electroOn   ? "ON " : "OFF");
  oled.print(" V:"); oled.print(valveOn     ? "ON " : "OFF");
  oled.print(" P:"); oled.print(piezoActive ? "ON"  : "OF");
  char tgtBuf[6]; dtostrf(targetPressure, 3, 1, tgtBuf);
  oled.setCursor(70, 41); oled.print("T="); oled.print(tgtBuf);

  // ── Статус / режим ──────────────────────────────────────────────────────
  oled.setCursor(0, 52);
  if (tiltCalibrating)     oled.print("КАЛИБРОВКА MPU...");
  else if (tiltLock)       { oled.print("TILT! "); oled.print((int)currentTiltDeg); oled.print("° >ЛОК< "); }
  else if (overpressureLock) oled.print(">СБРОСЬТЕ ДАВЛЕНИЕ<");
  else if (isPurging)      { oled.print("ПРОДУВКА "); oled.print(pBuf); oled.print("b"); }
  else if (isPurgeShort)   oled.print("МИН.ПРОДУВКА 200мс");
  else if (!systemOn)      { oled.print("ВЫКЛ  T="); oled.print(tgtBuf); }
  else if (isLocked)       oled.print("ЗАБЛОКИРОВАНО");
  else if (burnerActive)   { oled.print("ГОРЕЛКА  T="); oled.print(tgtBuf); }
  else if (manualGasActive) oled.print("РУЧНАЯ ПОДАЧА");
  else                     { oled.print("АВТО    T="); oled.print(tgtBuf); }

  oled.display();
}


// =====================================================================================
//  LED
// =====================================================================================
void updateLEDs() {
  digitalWrite(PIN_LED_POWER, (systemOn && !isPurging) ? HIGH : LOW);
  digitalWrite(PIN_LED_LOCK,  (isLocked || tiltLock || overpressureLock) ? HIGH : LOW);
  if (tiltLock)
    digitalWrite(PIN_LED_MODE, (millis() / 150) % 2 ? HIGH : LOW);   // быстрое мигание при TILT
  else if (burnerActive)
    digitalWrite(PIN_LED_MODE, (millis() / 400) % 2 ? HIGH : LOW);
  else
    digitalWrite(PIN_LED_MODE, manualGasActive ? HIGH : LOW);
}


// =====================================================================================
//  NRF24 — отправка статуса
// =====================================================================================
void sendStatusToRemote() {
  StatusPacket pkt;
  pkt.type     = 0xAA;
  pkt.pressure = currentPressure;
  pkt.target   = targetPressure;

  uint8_t f = 0;
  if (systemOn)        f |= (1<<0);
  if (isLocked)        f |= (1<<1);
  if (isPurging)       f |= (1<<2);
  if (burnerActive)    f |= (1<<3);
  if (manualGasActive) f |= (1<<4);
  if (electroOn)       f |= (1<<5);
  if (valveOn)         f |= (1<<6);
  if (piezoActive)     f |= (1<<7);
  pkt.flags = f;

  uint8_t f2 = 0;
  if (currentPressure > WARN_PRESSURE) f2 |= (1<<0);
  if (overpressureLock)                f2 |= (1<<1);
  if (isPurgeShort)                    f2 |= (1<<2);
  if (tiltLock)                        f2 |= (1<<3);
  if (tiltCalibrating)                 f2 |= (1<<4);
  pkt.flags2 = f2;

  int a = (int)roundf(currentTiltDeg);
  if (a < 0) a = 0; if (a > 180) a = 180;
  pkt.tiltAngle = (uint8_t)a;

  radio.stopListening();
  radio.write(&pkt, sizeof(pkt));
  radio.startListening();
}


// =====================================================================================
//  КНОПКИ
// =====================================================================================
void handlePowerBtn(unsigned long now) {
  bool raw = digitalRead(PIN_BTN_POWER);
  if (raw != pwrLastRaw) pwrDebTime = now;
  if ((now - pwrDebTime) > T_DEBOUNCE && raw != pwrStable) {
    pwrStable = raw;
    if (pwrStable == LOW) {
      if ((now - pwrLastClick) < T_DOUBLE_CLICK) executeCommand(99);
      else                                        executeCommand(1);
      pwrLastClick = now;
    }
  }
  pwrLastRaw = raw;
}

void handleLockBtn(unsigned long now) {
  bool raw = digitalRead(PIN_BTN_LOCK);
  if (raw != lckLastRaw) lckDebTime = now;
  if ((now - lckDebTime) > T_DEBOUNCE && raw != lckStable) {
    lckStable = raw;
    if (lckStable == LOW) executeCommand(2);
  }
  lckLastRaw = raw;
}

void handlePurgeBtn(unsigned long now) {
  bool raw = digitalRead(PIN_BTN_PURGE);
  if (raw != prgLastRaw) prgDebTime = now;
  if ((now - prgDebTime) > T_DEBOUNCE && raw != prgStable) {
    prgStable = raw;
    if (prgStable == LOW) executeCommand(5);
  }
  prgLastRaw = raw;
}


// =====================================================================================
//  SETUP
// =====================================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== H2 PRO Brain v4.2 ===");
  bootMs = millis();

  // Реле — сразу выключить
  pinMode(PIN_RELAY_ELECTRO, OUTPUT);
  pinMode(PIN_RELAY_VALVE,   OUTPUT);
  pinMode(PIN_RELAY_PIEZO,   OUTPUT);
  allRelaysOff();

  // LED
  pinMode(PIN_LED_POWER, OUTPUT); digitalWrite(PIN_LED_POWER, LOW);
  pinMode(PIN_LED_LOCK,  OUTPUT); digitalWrite(PIN_LED_LOCK,  HIGH);
  pinMode(PIN_LED_MODE,  OUTPUT); digitalWrite(PIN_LED_MODE,  LOW);

  // Кнопки
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);
  pinMode(PIN_BTN_LOCK,  INPUT_PULLUP);
  pinMode(PIN_BTN_PURGE, INPUT_PULLUP);

  // I2C (общая шина: OLED + MPU6050)
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);

  // OLED
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledOk) {
    Serial.println("OLED: нет (проверьте I2C, адрес 0x3C/0x3D)");
  } else {
    oled.clearDisplay(); oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(8, 16); oled.print("H2 PRO  Brain v4.2");
    oled.setCursor(8, 32); oled.print("MPU6050 калибровка");
    oled.setCursor(8, 44); oled.print("Не двигайте устройство!");
    oled.display();
    Serial.println("OLED: OK");
  }

  // MPU6050 — инициализация и калибровка
  tiltCalibrating = true;
  mpuOk = mpuInit();
  if (mpuOk) {
    Serial.println("MPU6050: OK");
    mpuCalibrate();
  } else {
    Serial.println("MPU6050: НЕТ — TILT-защита отключена!");
    if (oledOk) {
      oled.clearDisplay(); oled.setCursor(8, 24);
      oled.print("MPU6050: НЕ НАЙДЕН");
      oled.setCursor(8, 40);
      oled.print("TILT-защита OFF");
      oled.display();
      delay(1500);
    }
  }
  tiltCalibrating = false;

  // NRF24
  SPI.begin(18, 19, 23, PIN_NRF_CSN);
  if (radio.begin()) {
    radio.openWritingPipe(PIPE_B2R);
    radio.openReadingPipe(1, PIPE_R2B);
    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_16);
    radio.setChannel(76);
    radio.startListening();
    Serial.println("NRF24: OK (2 Мбит/с)");
  } else Serial.println("NRF24: FAIL");

  // Wi-Fi AP
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("WiFi AP: "); Serial.print(AP_SSID);
  Serial.print("  IP: "); Serial.println(WiFi.softAPIP());

  // HTTP
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", INDEX_HTML);
  });
  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (req->hasParam("id")) executeCommand((uint8_t)req->getParam("id")->value().toInt());
    req->send(200, "text/plain", "OK");
  });
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    unsigned long up = (millis() - bootMs) / 1000UL;
    bool ovw = currentPressure > WARN_PRESSURE;
    String j = "{";
    j += "\"p\":"   + String(currentPressure, 2) + ",";
    j += "\"tgt\":" + String(targetPressure,  1) + ",";
    j += "\"pwr\":" + String(systemOn         ?1:0) + ",";
    j += "\"lck\":" + String(isLocked         ?1:0) + ",";
    j += "\"prg\":" + String(isPurging        ?1:0) + ",";
    j += "\"psh\":" + String(isPurgeShort     ?1:0) + ",";
    j += "\"brn\":" + String(burnerActive     ?1:0) + ",";
    j += "\"mgs\":" + String(manualGasActive  ?1:0) + ",";
    j += "\"pzo\":" + String(piezoActive      ?1:0) + ",";
    j += "\"ele\":" + String(electroOn        ?1:0) + ",";
    j += "\"val\":" + String(valveOn          ?1:0) + ",";
    j += "\"rf\":"  + String(remoteOnline     ?1:0) + ",";
    j += "\"ovw\":" + String(ovw              ?1:0) + ",";
    j += "\"ovl\":" + String(overpressureLock ?1:0) + ",";
    j += "\"tlk\":" + String(tiltLock         ?1:0) + ",";
    j += "\"tcal\":" + String(tiltCalibrating ?1:0) + ",";
    j += "\"ta\":"  + String((int)roundf(currentTiltDeg)) + ",";
    j += "\"up\":"  + String(up);
    j += "}";
    req->send(200, "application/json", j);
  });
  server.on("/setpressure", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (req->hasParam("val")) setTarget(req->getParam("val")->value().toFloat());
    req->send(200, "text/plain", "OK");
  });
  server.begin();
  Serial.println("WebServer: OK  →  http://192.168.4.1");
  Serial.println("Старт: systemOn=false, isLocked=true");
}


// =====================================================================================
//  LOOP
// =====================================================================================
void loop() {
  unsigned long now = millis();

  // Давление каждые 100 мс
  if (now - lastPressureT >= T_PRESSURE_SAMP) {
    lastPressureT   = now;
    currentPressure = readPressure();
    safetyCheck();
  }

  // Угол наклона каждые 100 мс
  if (now - lastTiltT >= T_TILT_SAMP) {
    lastTiltT = now;
    tiltCheck();
  }

  // Кнопки
  handlePowerBtn(now);
  handleLockBtn(now);
  handlePurgeBtn(now);

  // NRF24 — приём
  if (radio.available()) {
    CmdPacket pkt; radio.read(&pkt, sizeof(pkt));
    if (pkt.type == 0x55) {
      executeCommand(pkt.cmd);
      lastRfRx = now;
      Serial.print("[RF] cmd="); Serial.println(pkt.cmd);
    }
  }
  remoteOnline = (now - lastRfRx < T_RF_TIMEOUT) && (lastRfRx > 0);

  // NRF24 — отправка статуса
  if (now - lastStatusTx >= T_STATUS_TX) {
    sendStatusToRemote(); lastStatusTx = now;
  }

  // ── ПРОДУВКА ДО НУЛЯ ───────────────────────────────────────────────────
  if (isPurging) {
    // TILT во время продувки — продувка продолжается, но электролизер
    // и так выключен. Просто принудительно убедимся.
    if (tiltLock) {
      // Клапан оставляем открытым (это сброс давления), но electro=OFF.
      setRelay(PIN_RELAY_ELECTRO, false);
      electroOn = false;
    }
    bool zeroed  = currentPressure <= PURGE_ZERO_BAR;
    bool timeout = (now - purgeStart) >= T_PURGE_MAX;
    if (zeroed || timeout) {
      setRelay(PIN_RELAY_VALVE, false);
      valveOn = false; isPurging = false; systemOn = false;
      Serial.println(timeout ? "[PURGE] Таймаут — выкл." : "[PURGE] ≤0.05 бар — выкл.");
    }
    updateLEDs();
    if (oledOk && (now - lastDispUpd >= T_DISPLAY_UPD)) { updateDisplay(); lastDispUpd = now; }
    return;
  }

  // ── МИНИ-ПРОДУВКА 200 мс ────────────────────────────────────────────────
  if (isPurgeShort) {
    if (tiltLock || (now - purgeShortStart) >= T_PURGE_SHORT) {
      setRelay(PIN_RELAY_VALVE, false);
      valveOn = false; isPurgeShort = false;
    }
    updateLEDs();
    if (oledOk && (now - lastDispUpd >= T_DISPLAY_UPD)) { updateDisplay(); lastDispUpd = now; }
    return;
  }

  // ── ВЫКЛ / БЛОКИРОВКА / АВАРИЯ ДАВЛЕНИЯ / TILT ────────────────────────────
  if (!systemOn || isLocked || overpressureLock || tiltLock) {
    if (tiltLock && !isLocked) {
      // TILT принудительно блокирует, чтобы пользователь увидел причину
      isLocked = true;
    }
    allRelaysOff();
    if (!systemOn || isLocked) resetProcesses();
    updateLEDs();
    if (oledOk && (now - lastDispUpd >= T_DISPLAY_UPD)) { updateDisplay(); lastDispUpd = now; }
    return;
  }

  // ── ОСНОВНАЯ ЛОГИКА УПРАВЛЕНИЯ ──────────────────────────────────────────
  bool needElectro = false, needValve = false;
  if (burnerActive) {
    if (currentPressure < targetPressure) {
      needElectro = needValve = true;
      if (!cgPiezoFired) { cgPiezoFired = true; startPiezo(); }
    }
  } else if (manualGasActive) {
    needElectro = needValve = true;
  } else {
    if (currentPressure < targetPressure) needElectro = true;
  }

  setRelay(PIN_RELAY_ELECTRO, needElectro);
  setRelay(PIN_RELAY_VALVE,   needValve);
  electroOn = needElectro; valveOn = needValve;

  if (piezoActive && (now - piezoStart >= T_PIEZO_WORK)) {
    piezoActive = false; setRelay(PIN_RELAY_PIEZO, false);
  }

  updateLEDs();
  if (oledOk && (now - lastDispUpd >= T_DISPLAY_UPD)) { updateDisplay(); lastDispUpd = now; }
}
