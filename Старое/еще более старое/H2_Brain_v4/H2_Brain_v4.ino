/*
 * =====================================================================================
 *  H2 CONTROL SYSTEM — МОЗГ v4.0  (ESP32 DevKit v1, WROOM-32D)
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
 *
 *  ОТЛИЧИЯ ОТ v3.0
 *  ---------------
 *    • NRF24L01 теперь работает на 2 Мбит/с (было 250 кбит/с) — выше пропускная,
 *      ниже задержка, но меньше дальность. Использовать при отсутствии помех.
 *    • Статус на пульт отправляется каждые 100 мс (10 Гц) — пульт отображает
 *      давление практически в реальном времени.
 *    • Произвольное целевое давление 0.5–6.0 бар с шагом 0.5 бар (через пульт)
 *      или 0.1 бар (через веб). Никаких "режимов 1/4 бар" больше нет.
 *    • Аварийное отключение по давлению при превышении 6.5 бар (предупреждение
 *      с 5.5 бар).
 *    • Режим "ПОСТОЯННЫЙ ГАЗ" переименован в "ГОРЕЛКА" с сохранением логики:
 *      автоматический одноразовый поджиг пьезо при первом достижении цели.
 *    • Кнопка POWER: одиночный клик срабатывает мгновенно (power toggle),
 *      двойной клик в пределах 400 мс запускает продувку.
 *    • Распиновка из TILT-версии: две физические кнопки, три LED-индикатора.
 *    • OLED — графический режим с полоской заполнения 0–6 бар и маркером цели.
 *    • Веб-интерфейс улучшен: live-график давления, индикация приближения к аварии,
 *      кнопки ±0.5 бар у слайдера, время работы (uptime), визуализация активного режима.
 *
 *  СХЕМА ПОДКЛЮЧЕНИЯ
 *  -----------------
 *    NRF24L01+:
 *        CE      → GPIO4
 *        CSN     → GPIO5
 *        SCK     → GPIO18  (VSPI SCK)
 *        MISO    → GPIO19  (VSPI MISO)
 *        MOSI    → GPIO23  (VSPI MOSI)
 *        VCC     → 3.3 V   ОБЯЗАТЕЛЬНО электролитический конденсатор
 *                          470 мкФ × 10 В между VCC и GND модуля!
 *                          Без него NRF24 будет давать сбои.
 *        IRQ     → не подключается
 *
 *    OLED SSD1306 (I2C, 128×64):
 *        SDA     → GPIO21
 *        SCL     → GPIO22
 *        VCC     → 3.3 V
 *        Адрес   → 0x3C (по умолчанию). Если не работает — попробуйте 0x3D.
 *
 *    Реле (модули SRD-05VDC, HIGH-active, см. RELAY_ACTIVE_LOW ниже):
 *        Электролизер → GPIO25
 *        Клапан       → GPIO26
 *        Пьезоподжиг  → GPIO27
 *
 *        ВНИМАНИЕ: реле должны питаться от ОТДЕЛЬНОГО источника 5 В.
 *        ESP32 не вытянет ток нескольких модулей одновременно.
 *        Между GND ESP32 и GND реле должна быть общая земля.
 *
 *    Физические кнопки (INPUT_PULLUP, замыкаются на GND):
 *        POWER → GPIO13   одиночный клик = вкл/выкл системы
 *                         двойной клик   = безопасное выключение (продувка)
 *        LOCK  → GPIO14   блокировка/разблокировка
 *
 *    Светодиоды (через резистор 220 Ом → GND):
 *        LED_POWER → GPIO17   горит когда система работает
 *        LED_LOCK  → GPIO12   горит при блокировке (strapping-pin — см. примечание)
 *        LED_MODE  → GPIO2   мигает при включенной ГОРЕЛКЕ (встроенный LED ESP32)
 *
 *        Примечание о strapping-pins (GPIO2, GPIO12):
 *        Эти пины влияют на режим загрузки ESP32. Если плата не стартует
 *        с подключенными светодиодами — попробуйте перенести на GPIO16
 *        и GPIO33 соответственно.
 *
 *    Датчик давления (например, DXP-2-K-0-10-bar или аналог 0.5–4.5 В):
 *        Сигнал → GPIO36 (SENSOR_VP, только вход, 12-битный ADC)
 *
 *        ВАЖНО про делитель напряжения:
 *        ESP32 ADC принимает 0–3.3 В. Если ваш датчик выдает 0.5–4.5 В,
 *        нужен делитель напряжения 1:1.5 (например, 10 кОм + 20 кОм).
 *        Формула в коде ниже сейчас НЕ учитывает делитель (как в TILT-версии).
 *        Если у вас есть делитель — раскомментируйте строку с VOLTAGE_DIVIDER
 *        в функции readPressure().
 *
 *  ИСПОЛЬЗУЕМЫЕ БИБЛИОТЕКИ (установить через Library Manager Arduino IDE)
 *  ---------------------------------------------------------------------
 *    RF24                 — by TMRh20
 *    ESPAsyncWebServer    — by me-no-dev / lacamera (fork)
 *    AsyncTCP             — by me-no-dev / esphome (NE ESPAsyncTCP — это для ESP8266!)
 *    Adafruit SSD1306     — by Adafruit
 *    Adafruit GFX Library — by Adafruit (подтянется автоматически)
 *
 *  WI-FI ТОЧКА ДОСТУПА
 *  -------------------
 *    SSID: H2_PRO
 *    PASS: hydrogen_2024
 *    IP:   192.168.4.1
 *    Откройте http://192.168.4.1 в браузере после подключения.
 *
 *  СВЯЗЬ С ПУЛЬТОМ (NRF24)
 *  -----------------------
 *    Скорость:         2 Мбит/с (RF24_2MBPS)
 *    Мощность:         MAX (PA_MAX, ~0 dBm)
 *    CRC:              16 бит
 *    Период отправки:  100 мс (10 Гц)
 *    Канал:            76 (стандартный, можно изменить radio.setChannel())
 *    Адреса труб:
 *        "CMD01" — пульт → мозг (команды)
 *        "STS01" — мозг → пульт (статус)
 *
 *  ПОВЕДЕНИЕ ПО УМОЛЧАНИЮ
 *  ----------------------
 *    При старте:
 *      • Система ВЫКЛЮЧЕНА   (systemOn = false)
 *      • Блокировка ВКЛЮЧЕНА (isLocked = true)  — безопасное состояние
 *      • Целевое давление    = 1.0 бар
 *      • Все реле сразу обесточиваются перед инициализацией
 *
 *    При превышении 6.5 бар:
 *      • Электролизер выключается принудительно
 *      • Активный режим ГОРЕЛКА сбрасывается
 *      • Поднимается флаг overpressure в статусе
 *
 *  АВТОР: Claude + пользователь, 2026
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


// =====================================================================================
//  WI-FI
// =====================================================================================
const char* AP_SSID     = "H2_PRO";
const char* AP_PASSWORD = "hydrogen_2024";


// =====================================================================================
//  ПИНЫ (распиновка TILT)
// =====================================================================================
#define PIN_RELAY_ELECTRO   25
#define PIN_RELAY_VALVE     26
#define PIN_RELAY_PIEZO     27

#define PIN_BTN_POWER       13   // Одиночный=ON/OFF, двойной=продувка
#define PIN_BTN_LOCK        14   // Блокировка/разблокировка

#define PIN_LED_POWER       17   // Горит когда система работает
#define PIN_LED_LOCK        12   // ! strapping-pin: если не стартует — перенести
#define PIN_LED_MODE         2   // ! strapping-pin: встроенный синий LED

#define PIN_PRESSURE        36   // ADC1_CH0, только вход

#define PIN_NRF_CE           4
#define PIN_NRF_CSN          5
// VSPI: SCK=18, MISO=19, MOSI=23 (по умолчанию)

#define I2C_SDA             21
#define I2C_SCL             22


// =====================================================================================
//  РЕЛЕ — УРОВЕНЬ АКТИВНОСТИ
//  TILT-style: HIGH = реле включено (большинство 5В модулей с оптопарой)
//  Если у вас "китайские" LOW-active модули — поменяйте на true
// =====================================================================================
#define RELAY_ACTIVE_LOW    false
#define RELAY_ON            (RELAY_ACTIVE_LOW ? LOW  : HIGH)
#define RELAY_OFF           (RELAY_ACTIVE_LOW ? HIGH : LOW)


// =====================================================================================
//  ДАВЛЕНИЕ — ПОРОГИ И ОГРАНИЧЕНИЯ
// =====================================================================================
const float MAX_SAFE_PRESSURE   = 6.5f;   // Аварийное отключение электролизера
const float WARN_PRESSURE       = 5.5f;   // Предупреждение (флаг overpressureWarn)
const float MIN_TARGET_PRESSURE = 0.5f;   // Минимальная цель
const float MAX_TARGET_PRESSURE = 6.0f;   // Максимальная цель регулировки
const float TARGET_STEP         = 0.5f;   // Шаг кнопок +/- на пульте


// =====================================================================================
//  ТАЙМИНГИ (мс)
// =====================================================================================
const unsigned long T_PIEZO_WORK     = 500;    // Длительность поджига
const unsigned long T_PURGE          = 5000;   // Длительность продувки
const unsigned long T_DOUBLE_CLICK   = 400;    // Окно двойного клика POWER
const unsigned long T_DEBOUNCE       = 50;     // Антидребезг кнопок
const unsigned long T_STATUS_TX      = 100;    // Период отправки статуса (RF + WS)
const unsigned long T_DISPLAY_UPD    = 200;    // Период обновления OLED
const unsigned long T_PRESSURE_SAMP  = 100;    // Период чтения давления (TILT-style)
const unsigned long T_RF_TIMEOUT     = 3000;   // Тайм-аут связи с пультом


// =====================================================================================
//  NRF24L01 — ОБЪЕКТ И ПРОТОКОЛ
// =====================================================================================
RF24 radio(PIN_NRF_CE, PIN_NRF_CSN);

const byte PIPE_R2B[6] = "CMD01";  // Remote → Brain
const byte PIPE_B2R[6] = "STS01";  // Brain  → Remote

// Команда пульт → мозг (2 байта)
struct __attribute__((packed)) CmdPacket {
  uint8_t type;   // 0x55 — маркер валидного пакета
  uint8_t cmd;    // см. таблицу команд
};

// Статус мозг → пульт (11 байт)
struct __attribute__((packed)) StatusPacket {
  uint8_t type;       // 0xAA
  float   pressure;   // Текущее давление, бар
  float   target;     // Целевое давление, бар
  uint8_t flags;      // см. ниже
  uint8_t flags2;     // расширенные флаги
};
// flags:  bit0=systemOn  bit1=isLocked  bit2=isPurging
//         bit3=burnerOn  bit4=manualOn
//         bit5=electroOn bit6=valveOn   bit7=piezoOn
// flags2: bit0=overpressureWarn  bit1=overpressureLock  bit2..7 = reserved


// =====================================================================================
//  OLED
// =====================================================================================
#define OLED_W 128
#define OLED_H  64
Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);
bool oledOk = false;


// =====================================================================================
//  ВЕБ-СЕРВЕР
// =====================================================================================
AsyncWebServer server(80);


// =====================================================================================
//  СОСТОЯНИЕ СИСТЕМЫ
// =====================================================================================
bool  systemOn        = false;
bool  isLocked        = true;     // Стартовое состояние — заблокировано (brain_v3-style)
bool  isPurging       = false;
bool  burnerActive    = false;    // Бывший constGasActive ("ГОРЕЛКА")
bool  manualGasActive = false;
bool  piezoActive     = false;
bool  cgPiezoFired    = false;    // Был ли уже автоподжиг в текущем цикле ГОРЕЛКА
bool  electroOn       = false;
bool  valveOn         = false;
bool  overpressureLock = false;   // Защёлка аварийного давления
float targetPressure  = 1.0f;
float currentPressure = 0.0f;

// RF
bool          remoteOnline = false;
unsigned long lastRfRx     = 0;

// Антидребезг физических кнопок
bool          pwrLastRaw   = HIGH, pwrStable = HIGH;
bool          lckLastRaw   = HIGH, lckStable = HIGH;
unsigned long pwrDebTime   = 0,    lckDebTime = 0;
unsigned long pwrLastClick = 0;    // Для определения двойного клика

// Таймеры
unsigned long purgeStart    = 0;
unsigned long piezoStart    = 0;
unsigned long lastStatusTx  = 0;
unsigned long lastDispUpd   = 0;
unsigned long lastPressureT = 0;
unsigned long bootMs        = 0;   // Для uptime


// =====================================================================================
//  HTML-ИНТЕРФЕЙС — основа brain_v3, дополнен графиком, кнопками ±, uptime,
//  индикацией предупреждения/аварии по давлению
// =====================================================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>H2 PRO Control</title>
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
    max-width:460px;margin:0 auto;
    display:flex;flex-direction:column;gap:10px;
  }

  /* предупреждение по давлению */
  .alert-bar{
    background:#1a1500;border:1px solid var(--warn);color:var(--warn);
    border-radius:10px;padding:8px 12px;text-align:center;
    font-size:.75em;letter-spacing:2px;text-transform:uppercase;
    font-family:'Share Tech Mono',monospace;display:none;
  }
  .alert-bar.show{display:block;animation:warnpulse 1.5s infinite}
  .alert-bar.danger{background:#1a0008;border-color:var(--danger);color:var(--danger)}
  @keyframes warnpulse{0%,100%{opacity:.7}50%{opacity:1}}

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
  .pressure.warn::before{background:linear-gradient(90deg,transparent,var(--warn),transparent)}
  .pressure.danger::before{background:linear-gradient(90deg,transparent,var(--danger),transparent)}
  .p-lbl{font-size:.65em;letter-spacing:4px;color:var(--dim);text-transform:uppercase;margin-bottom:6px}
  .p-num{
    font-family:'Share Tech Mono',monospace;
    font-size:3.4em;color:var(--accent);line-height:1;
    text-shadow:0 0 30px rgba(0,212,255,.6);
    transition:color .3s;
  }
  .p-num.warn{color:var(--warn);text-shadow:0 0 30px rgba(255,204,0,.6)}
  .p-num.danger{color:var(--danger);text-shadow:0 0 30px rgba(255,51,85,.6)}
  .p-unit{font-size:.4em;color:#5577aa;margin-left:6px}
  .p-bar-wrap{position:relative;margin-top:10px}
  .p-bar{height:6px;background:#111128;border-radius:3px;overflow:hidden}
  .p-fill{height:100%;border-radius:3px;background:linear-gradient(90deg,var(--ok),var(--accent));transition:width .8s ease,background .3s}
  .p-fill.warn{background:linear-gradient(90deg,var(--accent),var(--warn))}
  .p-fill.danger{background:linear-gradient(90deg,var(--warn),var(--danger))}
  .p-tgt{position:absolute;top:-3px;width:2px;height:13px;
    background:var(--warn);border-radius:1px;transition:left .5s ease;pointer-events:none;}
  .p-scale{display:flex;justify-content:space-between;font-size:.55em;color:var(--dim);
    font-family:'Share Tech Mono',monospace;margin-top:4px;letter-spacing:1px}

  /* график давления */
  .chart-box{
    background:var(--card);border:1px solid var(--border);
    border-radius:14px;padding:10px 12px;
  }
  .chart-lbl{font-size:.6em;letter-spacing:3px;color:var(--dim);
    text-transform:uppercase;margin-bottom:4px;display:flex;justify-content:space-between;}
  .chart-lbl span:last-child{color:var(--accent2);font-family:'Share Tech Mono',monospace}
  #chart{width:100%;height:50px;display:block}

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

  /* RF + uptime */
  .info-row{display:flex;justify-content:space-between;align-items:center;
    padding:0 4px;font-size:.62em;letter-spacing:2px;color:var(--dim);
    font-family:'Share Tech Mono',monospace;}
  .rf-indicator{display:flex;align-items:center;gap:6px;}
  .rf-dot{width:7px;height:7px;border-radius:50%;background:var(--dim);transition:all .3s}
  .rf-dot.ok{background:var(--ok);box-shadow:0 0 8px var(--ok)}
  .rf-dot.fail{background:var(--danger);animation:pulse .9s infinite}
  .uptime{color:var(--accent2)}

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

  /* целевое давление */
  .tgt-block{background:var(--card);border:1px solid var(--border);border-radius:12px;
    padding:14px;position:relative;overflow:hidden;}
  .tgt-block::before{content:'';position:absolute;top:0;left:0;right:0;height:2px;
    background:linear-gradient(90deg,transparent,var(--warn),transparent);}
  .t-lbl{font-size:.62em;letter-spacing:4px;color:var(--dim);text-transform:uppercase;
    text-align:center;margin-bottom:10px;}
  .t-row{display:flex;align-items:center;gap:8px;margin-bottom:10px;}
  .t-val{font-family:'Share Tech Mono',monospace;font-size:1.9em;color:var(--warn);
    min-width:88px;text-align:center;}
  .t-unit{font-size:.42em;color:#887700;}
  .t-pm{width:38px;height:38px;border-radius:9px;border:1px solid var(--warn);
    background:#1a1400;color:var(--warn);font-size:1.3em;font-weight:800;
    cursor:pointer;display:flex;align-items:center;justify-content:center;
    transition:all .2s;font-family:'Exo 2',sans-serif;}
  .t-pm:hover{background:#2a2000;}
  .t-pm:active{transform:scale(.92);}
  .t-pm:disabled{opacity:.3;cursor:not-allowed;}
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
</style>
</head>
<body>
<h1>⚡ H2 PRO</h1>
<div class="panel">

  <!-- Предупреждение / авария по давлению -->
  <div class="alert-bar" id="alertBar"></div>

  <!-- ДАВЛЕНИЕ -->
  <div class="pressure" id="pBox">
    <div class="p-lbl">Давление в баке</div>
    <div class="p-num" id="pNum"><span id="pv">0.00</span><span class="p-unit">бар</span></div>
    <div class="p-bar-wrap">
      <div class="p-bar">
        <div class="p-fill" id="pf" style="width:0%"></div>
      </div>
      <div class="p-tgt" id="ptgt" style="left:16.6%"></div>
      <div class="p-scale"><span>0</span><span>2</span><span>4</span><span>6 бар</span></div>
    </div>
  </div>

  <!-- ГРАФИК ИСТОРИИ ДАВЛЕНИЯ -->
  <div class="chart-box">
    <div class="chart-lbl"><span>История · 60 сек</span><span id="chartMax">max 0.00</span></div>
    <canvas id="chart" width="440" height="50"></canvas>
  </div>

  <!-- БЕЙДЖИ СОСТОЯНИЯ -->
  <div class="badges">
    <span class="badge" id="bPwr">POWER</span>
    <span class="badge" id="bLck">LOCK</span>
    <span class="badge" id="bPrg">PURGE</span>
    <span class="badge" id="bBrn">ГОРЕЛКА</span>
    <span class="badge" id="bMgs">MANUAL</span>
    <span class="badge" id="bPzo">PIEZO</span>
    <span class="badge" id="bEle">ELECTRO</span>
    <span class="badge" id="bVal">VALVE</span>
  </div>

  <!-- RF + UPTIME -->
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
      <div class="dot"></div>БЛОКИРОВКА
    </button>
    <button class="ctrl" id="cBrn" onclick="cmd(3)" disabled>
      <div class="dot"></div>ГОРЕЛКА
    </button>
    <button class="ctrl" id="cMgs" onclick="cmd(7)" disabled>
      <div class="dot"></div>РУЧНАЯ ПОДАЧА
    </button>
    <button class="ctrl active-warn wide" id="cPzo" onclick="cmd(4)" disabled>
      <div class="dot"></div>ПОДЖИГ
    </button>
  </div>

  <!-- ВВОД ЦЕЛЕВОГО ДАВЛЕНИЯ (0.5..6.0 бар) -->
  <div class="tgt-block">
    <div class="t-lbl">Целевое давление (max 6.0 бар)</div>
    <div class="t-row">
      <button class="t-pm" id="tMinus" onclick="stepTgt(-0.5)">−</button>
      <span class="t-val"><span id="tval">1.0</span><span class="t-unit"> бар</span></span>
      <button class="t-pm" id="tPlus" onclick="stepTgt(+0.5)">+</button>
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
    <button class="apply-btn" id="applyBtn" onclick="applyTgt()" disabled>УСТАНОВИТЬ ДАВЛЕНИЕ</button>
    <div class="t-ok" id="tok"></div>
  </div>

  <div class="footer" id="foot"></div>
</div>

<script>
  let ct=null,purging=false,pendingTgt=1.0;
  // История давления для графика
  const HIST_LEN=60;
  const hist=new Array(HIST_LEN).fill(0);
  let histMax=0;

  function onSlide(v){
    v=parseFloat(v);
    pendingTgt=v;
    document.getElementById('tval').innerText=v.toFixed(1);
    const pct=((v-0.5)/5.5*100).toFixed(1);
    document.getElementById('tslider').style.setProperty('--tp',pct+'%');
    updateQbtns(v);
  }

  function setQ(v){
    pendingTgt=v;
    document.getElementById('tslider').value=v;
    document.getElementById('tval').innerText=v.toFixed(1);
    const pct=((v-0.5)/5.5*100).toFixed(1);
    document.getElementById('tslider').style.setProperty('--tp',pct+'%');
    updateQbtns(v);
  }

  function stepTgt(d){
    let v=pendingTgt+d;
    v=Math.max(0.5,Math.min(6.0,Math.round(v*2)/2));
    setQ(v);
  }

  function updateQbtns(v){
    const p=[1,2,3,4,5,6];
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

  function cmd(id){fetch('/cmd?id='+id).catch(()=>{});}
  function cls(id,val,c){document.getElementById(id).className='badge'+(val?' '+c:'');}

  // Отрисовка графика
  function drawChart(){
    const c=document.getElementById('chart');
    const ctx=c.getContext('2d');
    const w=c.width,h=c.height;
    ctx.clearRect(0,0,w,h);
    // сетка
    ctx.strokeStyle='#1e1e3a';
    ctx.lineWidth=1;
    for(let y=0;y<=4;y++){
      const yy=y*(h-2)/4+1;
      ctx.beginPath();ctx.moveTo(0,yy);ctx.lineTo(w,yy);ctx.stroke();
    }
    // данные (0..6 бар)
    const maxScale=6.5;
    const step=w/(HIST_LEN-1);
    ctx.strokeStyle='#00d4ff';
    ctx.lineWidth=1.5;
    ctx.shadowColor='rgba(0,212,255,.6)';
    ctx.shadowBlur=6;
    ctx.beginPath();
    for(let i=0;i<HIST_LEN;i++){
      const x=i*step;
      const y=h-(hist[i]/maxScale)*(h-2)-1;
      if(i===0)ctx.moveTo(x,y);else ctx.lineTo(x,y);
    }
    ctx.stroke();
    ctx.shadowBlur=0;
    // линия цели
    if(pendingTgt>0){
      const yt=h-(pendingTgt/maxScale)*(h-2)-1;
      ctx.strokeStyle='rgba(255,204,0,.5)';
      ctx.setLineDash([3,3]);
      ctx.beginPath();ctx.moveTo(0,yt);ctx.lineTo(w,yt);ctx.stroke();
      ctx.setLineDash([]);
    }
  }

  function fmtUptime(s){
    const h=Math.floor(s/3600).toString().padStart(2,'0');
    const m=Math.floor((s%3600)/60).toString().padStart(2,'0');
    const ss=Math.floor(s%60).toString().padStart(2,'0');
    return h+':'+m+':'+ss;
  }

  function tick(){
    fetch('/status').then(r=>r.json()).then(d=>{
      // Давление
      const p=d.p;
      document.getElementById('pv').innerText=p.toFixed(2);
      document.getElementById('pf').style.width=Math.min(p/6.5*100,100)+'%';
      document.getElementById('ptgt').style.left='calc('+Math.min(d.tgt/6.5*100,100)+'% - 1px)';

      // Подсветка уровня давления
      const pBox=document.getElementById('pBox');
      const pNum=document.getElementById('pNum');
      const pf=document.getElementById('pf');
      pBox.className='pressure';
      pNum.className='p-num';
      pf.className='p-fill';
      if(d.ovl){pBox.classList.add('danger');pNum.classList.add('danger');pf.classList.add('danger');}
      else if(d.ovw){pBox.classList.add('warn');pNum.classList.add('warn');pf.classList.add('warn');}

      // Алерт-бар
      const ab=document.getElementById('alertBar');
      if(d.ovl){
        ab.className='alert-bar show danger';
        ab.innerText='⚠ ПРЕВЫШЕНИЕ '+d.p.toFixed(2)+' БАР — ЭЛЕКТРОЛИЗЕР ОТКЛЮЧЁН';
      } else if(d.ovw){
        ab.className='alert-bar show';
        ab.innerText='⚠ Приближение к аварии: '+d.p.toFixed(2)+' / 6.5 бар';
      } else {
        ab.className='alert-bar';
      }

      // График — добавляем точку каждый tick
      hist.shift();hist.push(p);
      histMax=Math.max(...hist);
      document.getElementById('chartMax').innerText='max '+histMax.toFixed(2);
      drawChart();

      // Бейджи
      cls('bPwr',d.pwr,'ok');
      cls('bLck',d.lck,'warn');
      cls('bPrg',d.prg,'danger');
      cls('bBrn',d.brn,'ok');
      cls('bMgs',d.mgs,'ok');
      cls('bPzo',d.pzo,'warn');
      cls('bEle',d.ele,'ok');
      cls('bVal',d.val,'ok');

      // Кнопка питания
      const bp=document.getElementById('btnPwr');
      bp.className='btn-power'+(d.pwr?' on':'');
      bp.innerText=d.pwr?'ВЫКЛЮЧИТЬ  (2× = ПРОДУВКА)':'ВКЛЮЧИТЬ СИСТЕМУ';

      // Блокировка кнопок
      const dis=!d.pwr||d.prg||d.lck;
      ['cBrn','cMgs','cPzo'].forEach(i=>document.getElementById(i).disabled=dis);
      document.getElementById('applyBtn').disabled=false;  // цель можно менять всегда
      // LOCK можно нажимать всегда — это разблокировка

      // Стили кнопок управления
      document.getElementById('cLck').className='ctrl wide'+(d.lck?' active-warn':'');
      document.getElementById('cBrn').className='ctrl'+(d.brn?' active':'');
      document.getElementById('cMgs').className='ctrl'+(d.mgs?' active':'');
      document.getElementById('cPzo').className='ctrl active-warn wide'+(d.pzo?' active-red':'');

      // Слайдер: синхронизируем если пользователь его не трогает
      const sl=document.getElementById('tslider');
      if(document.activeElement!==sl){
        const v=parseFloat(d.tgt);
        sl.value=v;
        document.getElementById('tval').innerText=v.toFixed(1);
        const pct=((v-0.5)/5.5*100).toFixed(1);
        sl.style.setProperty('--tp',pct+'%');
        pendingTgt=v;
        updateQbtns(v);
      }

      // RF пульт
      const rf=document.getElementById('rfDot'),rt=document.getElementById('rfTxt');
      rf.className='rf-dot'+(d.rf?' ok':' fail');
      rt.innerText='ПУЛЬТ: '+(d.rf?'ОНЛАЙН':'ОФФЛАЙН');

      // Uptime
      document.getElementById('uptime').innerText='UP '+fmtUptime(d.up);

      // Футер
      purging=d.prg;
      if(d.prg)document.getElementById('foot').innerText='⏳ ПРОДУВКА — ОЖИДАЙТЕ';
      else if(d.ovl)document.getElementById('foot').innerText='⚠ АВАРИЯ — СБРОСЬТЕ ДАВЛЕНИЕ';
      else if(!d.pwr)document.getElementById('foot').innerText='';
    }).catch(()=>{
      document.getElementById('foot').innerText='Нет связи с мозгом...';
    });
  }

  // Инициализация
  document.getElementById('tslider').style.setProperty('--tp','9.1%');
  setInterval(tick,250);
  tick();
</script>
</body>
</html>
)rawliteral";


// =====================================================================================
//  УТИЛИТЫ
// =====================================================================================

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
  burnerActive    = false;
  manualGasActive = false;
  piezoActive     = false;
  cgPiezoFired    = false;
}

// Мгновенное чтение давления (TILT-style, без делителя в формуле)
float readPressure() {
  float v = analogRead(PIN_PRESSURE) / 4095.0f * 3.3f;
  // Если у вас аппаратный делитель 10к/20к (1:1.5), раскомментируйте:
  // const float VOLTAGE_DIVIDER = 1.5f;
  // v *= VOLTAGE_DIVIDER;
  float bar = (v - 0.5f) / 4.0f * 10.0f;
  return max(0.0f, bar);
}

void startPiezo() {
  if (!piezoActive && systemOn && !isLocked && !isPurging) {
    piezoActive = true;
    piezoStart  = millis();
    setRelay(PIN_RELAY_PIEZO, true);
  }
}


// =====================================================================================
//  ИЗМЕНЕНИЕ ЦЕЛЕВОГО ДАВЛЕНИЯ (с ограничениями)
// =====================================================================================
void setTarget(float v) {
  v = constrain(v, MIN_TARGET_PRESSURE, MAX_TARGET_PRESSURE);
  // Округление до 0.1 для аккуратного отображения
  v = roundf(v * 10.0f) / 10.0f;
  targetPressure = v;
}

void bumpTarget(float delta) {
  // Шаг 0.5 — выравниваем по кратности
  float v = targetPressure + delta;
  v = roundf(v * 2.0f) / 2.0f;  // округление до 0.5
  setTarget(v);
}


// =====================================================================================
//  ВЫПОЛНЕНИЕ КОМАНДЫ
// =====================================================================================
void executeCommand(uint8_t cmd) {
  // Во время продувки разрешена только разблокировка (LOCK toggle)
  if (isPurging && cmd != 2) return;

  switch (cmd) {
    case 1:  // POWER toggle
      if (!systemOn) {
        // При включении сбрасываем аварийную защёлку
        if (overpressureLock && currentPressure < WARN_PRESSURE) {
          overpressureLock = false;
        }
        systemOn = true;
      } else {
        systemOn = false;
        resetProcesses();
        allRelaysOff();
      }
      break;

    case 2:  // LOCK toggle
      isLocked = !isLocked;
      if (isLocked) { resetProcesses(); allRelaysOff(); }
      break;

    case 3:  // ГОРЕЛКА toggle (бывший CONST GAS, логика brain_v3)
      if (systemOn && !isLocked) {
        burnerActive = !burnerActive;
        if (burnerActive) {
          manualGasActive = false;
          cgPiezoFired    = false;   // Новый цикл — поджиг разрешён
        }
      }
      break;

    case 4:  // PIEZO одноразовый
      startPiezo();
      break;

    case 7:  // MANUAL GAS toggle
      if (systemOn && !isLocked) {
        manualGasActive = !manualGasActive;
        if (manualGasActive) burnerActive = false;
      }
      break;

    case 8:  // TARGET +0.5 бар
      bumpTarget(+TARGET_STEP);
      break;

    case 9:  // TARGET -0.5 бар
      bumpTarget(-TARGET_STEP);
      break;

    case 99: // SAFE OFF — продувка 5 секунд
      if (systemOn) {
        resetProcesses();
        setRelay(PIN_RELAY_ELECTRO, false);
        setRelay(PIN_RELAY_PIEZO,   false);
        setRelay(PIN_RELAY_VALVE,   true);
        electroOn   = false;
        valveOn     = true;
        isPurging   = true;
        purgeStart  = millis();
      }
      break;
  }
}


// =====================================================================================
//  БЕЗОПАСНОСТЬ ПО ДАВЛЕНИЮ
// =====================================================================================
void safetyCheck() {
  if (currentPressure > MAX_SAFE_PRESSURE && systemOn) {
    // АВАРИЯ — отключаем электролизер, сбрасываем ГОРЕЛКУ
    setRelay(PIN_RELAY_ELECTRO, false);
    electroOn        = false;
    burnerActive     = false;
    overpressureLock = true;
    Serial.printf("[SAFETY] Аварийное давление %.2f > %.2f бар!\n",
                  currentPressure, MAX_SAFE_PRESSURE);
  }
  // Снятие защёлки происходит только при нажатии POWER (см. executeCommand)
}


// =====================================================================================
//  OLED — ГРАФИЧЕСКОЕ ОТОБРАЖЕНИЕ (полоска 0–6 бар, маркер цели, RF, E/V/P)
// =====================================================================================
void updateDisplay() {
  if (!oledOk) return;

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // === Заголовок ===
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print("H2 PRO");
  oled.setCursor(48, 0);
  oled.print("RF:");
  oled.print(remoteOnline ? "OK" : "--");
  oled.setCursor(86, 0);
  if (overpressureLock) {
    oled.print("!АВАРИЯ");
  } else {
    oled.print(systemOn ? (isLocked ? "ЛОК" : "ВКЛ") : "ВЫКЛ");
  }

  oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  // === Давление крупным шрифтом ===
  oled.setTextSize(2);
  oled.setCursor(0, 12);
  char pBuf[8];
  dtostrf(currentPressure, 4, 2, pBuf);
  oled.print(pBuf);
  oled.setTextSize(1);
  oled.setCursor(86, 18);
  oled.print("bar");

  // === Полоска давления 0..6 бар + маркер цели ===
  // Шкала: 0..6 бар = вся ширина (128 px). 6.5 = выход за границу = аварийный
  const float SCALE_MAX = 6.0f;
  int barW = (int)((currentPressure / SCALE_MAX) * 127.0f);
  barW = constrain(barW, 0, 127);

  oled.drawRect(0, 30, 128, 5, SSD1306_WHITE);
  if (barW > 0) oled.fillRect(1, 31, barW, 3, SSD1306_WHITE);

  // Маркер целевого давления — тонкий вертикальный штрих над полоской
  int tgtX = (int)((targetPressure / SCALE_MAX) * 127.0f);
  tgtX = constrain(tgtX, 0, 127);
  oled.drawLine(tgtX, 28, tgtX, 36, SSD1306_WHITE);
  oled.drawLine(tgtX-1, 28, tgtX+1, 28, SSD1306_WHITE);  // "крышечка"

  oled.drawLine(0, 38, 127, 38, SSD1306_WHITE);

  // === Статус реле E V P ===
  oled.setCursor(0, 41);
  oled.print("E:");
  oled.print(electroOn   ? "ON " : "OFF");
  oled.print(" V:");
  oled.print(valveOn     ? "ON " : "OFF");
  oled.print(" P:");
  oled.print(piezoActive ? "ON" : "OF");

  // === Режим работы / целевое давление ===
  oled.setCursor(0, 52);
  char tgtBuf[6];
  dtostrf(targetPressure, 3, 1, tgtBuf);

  if (overpressureLock) {
    oled.print(">СБРОСЬТЕ ДАВЛЕНИЕ<");
  } else if (isPurging) {
    oled.print(">>> ПРОДУВКА <<<");
  } else if (!systemOn) {
    oled.print("СИСТЕМА ВЫКЛ  T=");
    oled.print(tgtBuf);
  } else if (isLocked) {
    oled.print("ЗАБЛОКИРОВАНО");
  } else if (burnerActive) {
    oled.print("ГОРЕЛКА  T=");
    oled.print(tgtBuf);
  } else if (manualGasActive) {
    oled.print("РУЧНАЯ ПОДАЧА");
  } else {
    oled.print("АВТО     T=");
    oled.print(tgtBuf);
  }

  oled.display();
}


// =====================================================================================
//  СВЕТОДИОДЫ
// =====================================================================================
void updateLEDs() {
  digitalWrite(PIN_LED_POWER, (systemOn && !isPurging) ? HIGH : LOW);
  digitalWrite(PIN_LED_LOCK,  isLocked ? HIGH : LOW);

  // LED_MODE мигает при активной ГОРЕЛКЕ, горит при MANUAL, иначе выкл
  if (burnerActive) {
    digitalWrite(PIN_LED_MODE, (millis() / 400) % 2 ? HIGH : LOW);
  } else if (manualGasActive) {
    digitalWrite(PIN_LED_MODE, HIGH);
  } else {
    digitalWrite(PIN_LED_MODE, LOW);
  }
}


// =====================================================================================
//  NRF24 — ОТПРАВКА СТАТУСА ПУЛЬТУ
// =====================================================================================
void sendStatusToRemote() {
  StatusPacket pkt;
  pkt.type     = 0xAA;
  pkt.pressure = currentPressure;
  pkt.target   = targetPressure;

  uint8_t f = 0;
  if (systemOn)        f |= (1 << 0);
  if (isLocked)        f |= (1 << 1);
  if (isPurging)       f |= (1 << 2);
  if (burnerActive)    f |= (1 << 3);
  if (manualGasActive) f |= (1 << 4);
  if (electroOn)       f |= (1 << 5);
  if (valveOn)         f |= (1 << 6);
  if (piezoActive)     f |= (1 << 7);
  pkt.flags = f;

  uint8_t f2 = 0;
  if (currentPressure > WARN_PRESSURE) f2 |= (1 << 0);   // overpressureWarn
  if (overpressureLock)                f2 |= (1 << 1);   // overpressureLock
  pkt.flags2 = f2;

  radio.stopListening();
  radio.write(&pkt, sizeof(pkt));
  radio.startListening();
}


// =====================================================================================
//  ОБРАБОТКА КНОПОК (с антидребезгом)
// =====================================================================================

// POWER: одиночный = power toggle (мгновенно), двойной в окне 400 мс = продувка
void handlePowerBtn(unsigned long now) {
  bool raw = digitalRead(PIN_BTN_POWER);
  if (raw != pwrLastRaw) pwrDebTime = now;
  if ((now - pwrDebTime) > T_DEBOUNCE && raw != pwrStable) {
    pwrStable = raw;
    if (pwrStable == LOW) {  // нажатие
      if ((now - pwrLastClick) < T_DOUBLE_CLICK) {
        executeCommand(99);  // двойной → продувка
      } else {
        executeCommand(1);   // одиночный → power
      }
      pwrLastClick = now;
    }
  }
  pwrLastRaw = raw;
}

// LOCK: обычное переключение блокировки
void handleLockBtn(unsigned long now) {
  bool raw = digitalRead(PIN_BTN_LOCK);
  if (raw != lckLastRaw) lckDebTime = now;
  if ((now - lckDebTime) > T_DEBOUNCE && raw != lckStable) {
    lckStable = raw;
    if (lckStable == LOW) {
      executeCommand(2);
    }
  }
  lckLastRaw = raw;
}


// =====================================================================================
//  SETUP
// =====================================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== H2 PRO Brain v4.0 ===");

  bootMs = millis();

  // Реле — сразу выключить (ДО любых других инициализаций)
  pinMode(PIN_RELAY_ELECTRO, OUTPUT);
  pinMode(PIN_RELAY_VALVE,   OUTPUT);
  pinMode(PIN_RELAY_PIEZO,   OUTPUT);
  allRelaysOff();

  // LED
  pinMode(PIN_LED_POWER, OUTPUT); digitalWrite(PIN_LED_POWER, LOW);
  pinMode(PIN_LED_LOCK,  OUTPUT); digitalWrite(PIN_LED_LOCK,  HIGH);  // start locked
  pinMode(PIN_LED_MODE,  OUTPUT); digitalWrite(PIN_LED_MODE,  LOW);

  // Кнопки — внутренний pull-up, нажатие = LOW
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);
  pinMode(PIN_BTN_LOCK,  INPUT_PULLUP);

  // OLED
  Wire.begin(I2C_SDA, I2C_SCL);
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledOk) {
    Serial.println("OLED: не найден (проверьте I2C на 0x3C/0x3D)");
  } else {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(8, 24);
    oled.print("H2 PRO  Brain v4.0");
    oled.setCursor(20, 40);
    oled.print("Инициализация...");
    oled.display();
    Serial.println("OLED: OK");
  }

  // NRF24L01 — 2 Мбит/с, MAX мощность, CRC16
  SPI.begin(18, 19, 23, PIN_NRF_CSN);
  if (radio.begin()) {
    radio.openWritingPipe(PIPE_B2R);
    radio.openReadingPipe(1, PIPE_R2B);
    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_2MBPS);     // ← 2 Мбит/с
    radio.setCRCLength(RF24_CRC_16);
    radio.setChannel(76);
    radio.startListening();
    Serial.println("NRF24: OK  (2 Мбит/с)");
  } else {
    Serial.println("NRF24: FAIL — проверьте подключение и питание 3.3 V!");
  }

  // Wi-Fi точка доступа
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.print("WiFi AP: "); Serial.print(AP_SSID);
  Serial.print("  IP: ");    Serial.println(WiFi.softAPIP());

  // HTTP маршруты
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send_P(200, "text/html", INDEX_HTML);
  });

  server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (req->hasParam("id"))
      executeCommand((uint8_t)req->getParam("id")->value().toInt());
    req->send(200, "text/plain", "OK");
  });

  server.on("/status", HTTP_GET, [](AsyncWebServerRequest* req) {
    unsigned long up = (millis() - bootMs) / 1000UL;
    bool ovw = currentPressure > WARN_PRESSURE;
    String j = "{";
    j += "\"p\":"   + String(currentPressure, 2) + ",";
    j += "\"tgt\":" + String(targetPressure,  1) + ",";
    j += "\"pwr\":" + String(systemOn        ? 1 : 0) + ",";
    j += "\"lck\":" + String(isLocked        ? 1 : 0) + ",";
    j += "\"prg\":" + String(isPurging       ? 1 : 0) + ",";
    j += "\"brn\":" + String(burnerActive    ? 1 : 0) + ",";
    j += "\"mgs\":" + String(manualGasActive ? 1 : 0) + ",";
    j += "\"pzo\":" + String(piezoActive     ? 1 : 0) + ",";
    j += "\"ele\":" + String(electroOn       ? 1 : 0) + ",";
    j += "\"val\":" + String(valveOn         ? 1 : 0) + ",";
    j += "\"rf\":"  + String(remoteOnline    ? 1 : 0) + ",";
    j += "\"ovw\":" + String(ovw             ? 1 : 0) + ",";
    j += "\"ovl\":" + String(overpressureLock? 1 : 0) + ",";
    j += "\"up\":"  + String(up);
    j += "}";
    req->send(200, "application/json", j);
  });

  server.on("/setpressure", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (req->hasParam("val")) {
      float v = req->getParam("val")->value().toFloat();
      setTarget(v);
      Serial.printf("[WEB] targetPressure = %.1f\n", targetPressure);
    }
    req->send(200, "text/plain", "OK");
  });

  server.begin();
  Serial.println("WebServer: OK  →  http://192.168.4.1");
  Serial.println("Старт: systemOn=false, isLocked=true (безопасное состояние)");
}


// =====================================================================================
//  LOOP
// =====================================================================================
void loop() {
  unsigned long now = millis();

  // ---- Чтение давления (мгновенное, каждые 100 мс, TILT-style) ----
  if (now - lastPressureT >= T_PRESSURE_SAMP) {
    lastPressureT  = now;
    currentPressure = readPressure();
    safetyCheck();
  }

  // ---- Физические кнопки ----
  handlePowerBtn(now);
  handleLockBtn(now);

  // ---- Приём команд от пульта по NRF24 ----
  if (radio.available()) {
    CmdPacket pkt;
    radio.read(&pkt, sizeof(pkt));
    if (pkt.type == 0x55) {
      executeCommand(pkt.cmd);
      lastRfRx = now;
      Serial.print("[RF RX] cmd="); Serial.println(pkt.cmd);
    }
  }
  remoteOnline = (now - lastRfRx < T_RF_TIMEOUT) && (lastRfRx > 0);

  // ---- Отправка статуса пульту каждые 100 мс ----
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
      Serial.println("[SYS] Продувка завершена.");
    }
    updateLEDs();
    if (oledOk && (now - lastDispUpd >= T_DISPLAY_UPD)) { updateDisplay(); lastDispUpd = now; }
    return;
  }

  // ВЫКЛЮЧЕНА / ЗАБЛОКИРОВАНА / АВАРИЯ
  if (!systemOn || isLocked || overpressureLock) {
    allRelaysOff();
    if (!systemOn || isLocked) resetProcesses();
    // При аварийной защёлке — не сбрасываем burnerActive/manualGasActive,
    // чтобы пользователь видел, в каком режиме была попытка работы
    updateLEDs();
    if (oledOk && (now - lastDispUpd >= T_DISPLAY_UPD)) { updateDisplay(); lastDispUpd = now; }
    return;
  }

  // УПРАВЛЕНИЕ ЭЛЕКТРОЛИЗЕРОМ И КЛАПАНОМ (логика brain_v3)
  bool needElectro = false;
  bool needValve   = false;

  if (burnerActive) {
    // ГОРЕЛКА: качаем до целевого давления + открытый клапан + автоподжиг
    if (currentPressure < targetPressure) {
      needElectro = true;
      needValve   = true;
      if (!cgPiezoFired) { cgPiezoFired = true; startPiezo(); }
    }
  } else if (manualGasActive) {
    // РУЧНАЯ ПОДАЧА: электролизер + открытый клапан без условий
    needElectro = true;
    needValve   = true;
  } else {
    // АВТО: только электролизер до цели, клапан закрыт
    if (currentPressure < targetPressure) needElectro = true;
  }

  setRelay(PIN_RELAY_ELECTRO, needElectro);
  setRelay(PIN_RELAY_VALVE,   needValve);
  electroOn = needElectro;
  valveOn   = needValve;

  // Таймер пьезоподжига
  if (piezoActive && (now - piezoStart >= T_PIEZO_WORK)) {
    piezoActive = false;
    setRelay(PIN_RELAY_PIEZO, false);
  }

  // LED
  updateLEDs();

  // OLED
  if (oledOk && (now - lastDispUpd >= T_DISPLAY_UPD)) {
    updateDisplay();
    lastDispUpd = now;
  }
}
