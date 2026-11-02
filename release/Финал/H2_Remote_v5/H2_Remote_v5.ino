/*
 * =====================================================================================
 *  H2 CONTROL SYSTEM — ПУЛЬТ v5.0  (ESP32 DevKit v1)
 * =====================================================================================
 *
 *  Адаптирован под мозг v5.0.
 *
 *  ⚡ НОВОЕ В v5.0:
 *    • Пакет StatusPacket расширен до 15 байт: добавлены температура (temp)
 *      и обороты вентилятора (fanDuty). Пульт показывает их на OLED.
 *    • Резервный канал NRF24 (channel hopping): пульт синхронно с мозгом
 *      перебирает список каналов RF_CHANNELS при пропадании связи, пока не
 *      найдёт активный. Защита от помех на одной частоте.
 *    • ГЛУБОКИЙ СОН (deep sleep) при простое: если ни одна кнопка не нажата
 *      в течение IDLE_SLEEP_MS (по умолч. 2 минуты), пульт уходит в сон для
 *      экономии батареи (ток падает с ~80 мА до ~10 мкА). Пробуждение —
 *      ТОЛЬКО кнопкой TARGET− (GPIO 33): это RTC-пин, используется ext0.
 *      После пробуждения пульт сразу восстанавливает связь. На внешнем
 *      питании (USB) сон можно отключить через DEEP_SLEEP_ENABLE = false.
 *    • Бит6/бит7 в flags2 теперь watchdog/overheat — пульт показывает
 *      "БАК НЕ НАБИРАЕТ" и "ПЕРЕГРЕВ" на OLED.
 *
 *  ⚡ ИЗ v4.4:
 *    • Кнопка РЕПУЛЬСОР (cmd=3) — одноразовый выстрел газ+поджиг.
 *    • OLED дублирует экран мозга, показывает драйвер MOS/REL и гистерезис.
 *
 *  ОБЩИЕ ВОЗМОЖНОСТИ:
 *    • NRF24 на 2 Мбит/с, двусторонняя связь
 *    • Приём статуса каждые 100 мс (10 Гц)
 *    • Структура StatusPacket (15 байт): pressure, target, flags, flags2,
 *      tiltAngle, hystX10, temp, fanDuty
 *    • Индикация TILT-сработки и текущего угла наклона на OLED
 *    • Кнопка LOCK сбрасывает все защёлки (LOCK, TILT, overpressure)
 *    • Регулировка целевого давления двумя кнопками ±0.5 бар
 *    • POWER: одиночный = вкл/выкл, двойной (≤400 мс) = продувка до нуля
 *
 *  ПРОБУЖДЕНИЕ ИЗ СНА:
 *    Будит ТОЛЬКО кнопка TGT− (GPIO 33) — ext0 на одном RTC-пине (ext1 с
 *    ALL_LOW потребовал бы зажать обе кнопки 32+33 одновременно).
 *    Будить по POWER (GPIO 16) нельзя — он не RTC-пин.
 *    Нажатие для пробуждения не отправляет команду, только включает пульт.
 *
 *  ПРИМЕЧАНИЕ ПРО ПЕРЕКЛЮЧАТЕЛЬ ДРАЙВЕРА (MOSFET ↔ РЕЛЕ):
 *  На пульте отдельной кнопки для смены драйвера НЕТ — это редкая операция,
 *  выполняется один раз при пуско-наладке через веб-интерфейс мозга
 *  (http://192.168.4.1, блок "Драйвер электролизера + гистерезис").
 *  Если очень нужно — отправить cmd=10 через любую свободную кнопку.
 *
 *  СХЕМА ПОДКЛЮЧЕНИЯ
 *  -----------------
 *    NRF24L01:
 *        CE=GPIO4   CSN=GPIO5   SCK=GPIO18   MISO=GPIO19   MOSI=GPIO23
 *        VCC=3.3V + конденсатор 470 мкФ × 10 В между VCC и GND модуля!
 *
 *    OLED SSD1306 (I2C, 128×64):
 *        SDA=GPIO21   SCL=GPIO22   VCC=3.3V   адрес 0x3C
 *
 *    КНОПКИ (INPUT_PULLUP, замыкаются на GND):
 *        GPIO16 → POWER             1× = cmd 1 (вкл/выкл)
 *                                   2× = cmd 99 (продувка до нуля)
 *        GPIO17 → LOCK              cmd 2 (блокировка + сброс TILT/overpressure)
 *        GPIO25 → РЕПУЛЬСОР         cmd 3 (выстрел 800мс газ + 100мс поджиг)
 *        GPIO26 → РУЧНАЯ ПОДАЧА     cmd 7
 *        GPIO27 → ПОДЖИГ            cmd 4 (500 мс)
 *        GPIO32 → TARGET +0.5 бар   cmd 8
 *        GPIO33 → TARGET -0.5 бар   cmd 9
 *
 *    СВЕТОДИОДЫ (резистор 220 Ом → GND):
 *        GPIO14 → LED зелёный  (мигает при отправке)
 *        GPIO13 → LED синий    (горит — пульт включён)
 *
 *  ПИТАНИЕ: PowerBank → micro-USB / 3×AA батарейки
 *
 *  ВАЖНО: скорость NRF24 на пульте и мозге ДОЛЖНА совпадать (2 Мбит/с)!
 *         Структуры пакетов байт-в-байт идентичны с мозгом.
 * =====================================================================================
 */

#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_sleep.h>     // ⚡ v5.0: глубокий сон
#include "h2_protocol.h"   // ⚡ общий протокол: CmdPacket/StatusPacket, трубы, каналы,
                           //    MIN/MAX_TARGET_PRESSURE (единый для мозга и пульта)


// ======================== ПИНЫ NRF24 ========================
#define PIN_NRF_CE   4
#define PIN_NRF_CSN  5


// ======================== КНОПКИ ========================
// POWER обрабатывается отдельно (двойной клик), остальные — простой клик.
const uint8_t NUM_SIMPLE_BTNS = 6;
const int     SIMPLE_BTN_PINS[NUM_SIMPLE_BTNS]   = { 17,  25,  26,  27,  32,  33 };
const uint8_t SIMPLE_BTN_CMDS[NUM_SIMPLE_BTNS]   = {  2,   3,   7,   4,   8,   9 };
const char*   SIMPLE_BTN_LABELS[NUM_SIMPLE_BTNS] = {
  "LOCK/RESET", "РЕПУЛЬСОР", "MANUAL", "PIEZO", "TGT +0.5", "TGT -0.5"
};

#define PIN_BTN_POWER   16


// ======================== СВЕТОДИОДЫ ========================
#define PIN_LED_TX    14
#define PIN_LED_POWER 13


// ======================== OLED ========================
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
bool oledOk = false;


// ======================== NRF24 ========================
RF24 radio(PIN_NRF_CE, PIN_NRF_CSN);
// Трубы PIPE_R2B/PIPE_B2R и структуры CmdPacket/StatusPacket вынесены в общий
// h2_protocol.h (включён выше) — протокол байт-в-байт идентичен с прошивкой мозга.


// ======================== ТАЙМИНГИ ========================
const unsigned long T_DEBOUNCE     = 50;
const unsigned long T_DOUBLE_CLICK = 400;
const unsigned long T_RF_TIMEOUT   = 3000;
const unsigned long T_DISPLAY_UPD  = 200;
const unsigned long T_LED_BLINK    = 60;
const unsigned long T_SHOW_TARGET  = 2500;   // Спецэкран установки давления, как на мозге

// Пороги MIN/MAX_TARGET_PRESSURE и каналы RF_CHANNELS/RF_CHANNEL_CNT — в общем
// h2_protocol.h (единый источник для мозга и пульта).

// ⚡ Период перебора НАМЕРЕННО не равен мозговому (4000 мс): если обе стороны
// ищут друг друга с ОДИНАКОВЫМ периодом, они могут прыгать «в противофазе»
// (постоянное смещение на 1 канал) и не встретиться никогда. Разные периоды
// сдвигают фазу на каждом цикле и гарантируют пересечение на общем канале.
const unsigned long RF_HOP_SILENCE = 5000;   // мс тишины до прыжка на след. канал
const unsigned long RF_SCAN_FAST   = 1200;   // быстрый скан после старта/пробуждения

// ⚡ v5.0: Глубокий сон при простое
const bool          DEEP_SLEEP_ENABLE = true;     // false если пульт на USB-питании
const unsigned long IDLE_SLEEP_MS     = 120000;   // 2 мин без нажатий → сон
// ⚡ v5.0: пробуждение из сна — ext0 по кнопке TGT− (GPIO 33), активный уровень LOW.
// (ext1 ALL_LOW потребовал бы зажать ОБЕ кнопки 32+33 — поэтому используем ext0
//  на одном RTC-пине, это будит по нажатию одной кнопки.)


// ======================== СОСТОЯНИЕ (от мозга) ========================
float   remPressure  = 0.0f;
float   remTarget    = 1.0f;
float   prevRemTarget = -1.0f;       // Для детектирования изменения target
uint8_t remFlags     = 0;
uint8_t remFlags2    = 0;
uint8_t remTiltAngle = 0;
uint8_t remHystX10   = 0;            // ⚡ v4.3: гистерезис × 10
int8_t  remTemp      = 25;           // ⚡ v5.0: температура
uint8_t remFanDuty   = 0;            // ⚡ v5.0: обороты вентилятора
bool    brainOnline  = false;
unsigned long lastRxTime = 0;
unsigned long lastTargetChange = 0;  // Когда мозг последний раз изменил target

// ⚡ v5.0: RF channel hopping
uint8_t       rfChannelIdx   = 0;
unsigned long rfLastActivity = 0;

// ⚡ v5.0: deep sleep
unsigned long lastActivityMs = 0;    // последнее нажатие кнопки

bool r_sysOn()    { return remFlags  & (1 << 0); }
bool r_locked()   { return remFlags  & (1 << 1); }
bool r_purging()  { return remFlags  & (1 << 2); }
bool r_repulsor() { return remFlags  & (1 << 3); }  // ⚡ v4.3 (бывший r_burner)
bool r_manual()   { return remFlags  & (1 << 4); }
bool r_electro()  { return remFlags  & (1 << 5); }
bool r_valve()    { return remFlags  & (1 << 6); }
bool r_piezo()    { return remFlags  & (1 << 7); }
bool r_ovWarn()   { return remFlags2 & (1 << 0); }
bool r_ovLock()   { return remFlags2 & (1 << 1); }
bool r_purgeShort(){return remFlags2 & (1 << 2); }
bool r_tiltLock() { return remFlags2 & (1 << 3); }
bool r_tiltCal()  { return remFlags2 & (1 << 4); }
bool r_drvRelay() { return remFlags2 & (1 << 5); }  // ⚡ v4.3: true=РЕЛЕ false=MOSFET
bool r_watchdog() { return remFlags2 & (1 << 6); }  // ⚡ v5.0: бак не набирает
bool r_overheat() { return remFlags2 & (1 << 7); }  // ⚡ v5.0: перегрев
float r_hyst()    { return remHystX10 * 0.1f; }     // 0..10 → 0.0..1.0 бар


// ======================== АНТИДРЕБЕЗГ ========================
bool          simpleLastRaw[NUM_SIMPLE_BTNS];
bool          simpleStable[NUM_SIMPLE_BTNS];
unsigned long simpleDebTime[NUM_SIMPLE_BTNS];

bool          pwrLastRaw   = HIGH;
bool          pwrStable    = HIGH;
unsigned long pwrDebTime   = 0;
unsigned long pwrLastClick = 0;


// ======================== ПРОЧЕЕ ========================
char          lastCmdLabel[16] = "---";
unsigned long ledTxOff         = 0;
// ⚡ ТЗ 7.3: перерисовка OLED только при изменении отображаемых данных
unsigned long lastDispCheck    = 0;   // последняя проверка (троттлинг T_DISPLAY_UPD)
unsigned long lastDispDraw     = 0;   // время последней реальной перерисовки
char          lastDispSig[110] = "";  // сигнатура отображаемых полей


// =====================================================================================
//  ОТПРАВКА КОМАНДЫ
// =====================================================================================
void sendCmd(uint8_t cmd, const char* label) {
  lastActivityMs = millis();   // ⚡ v5.0: любое нажатие сбрасывает таймер сна
  CmdPacket pkt = { 0x55, cmd };
  radio.stopListening();
  bool ok = radio.write(&pkt, sizeof(pkt));
  radio.startListening();

  strncpy(lastCmdLabel, label, sizeof(lastCmdLabel) - 1);
  lastCmdLabel[sizeof(lastCmdLabel) - 1] = '\0';

  if (ok) {
    digitalWrite(PIN_LED_TX, HIGH);
    ledTxOff = millis() + T_LED_BLINK;
  } else {
    for (int i = 0; i < 2; i++) {
      digitalWrite(PIN_LED_TX, HIGH); delay(80);
      digitalWrite(PIN_LED_TX, LOW);  delay(60);
    }
  }
  Serial.printf("[TX] %s (%d) -> %s\n", label, cmd, ok ? "OK" : "FAIL");
}


// =====================================================================================
//  OLED — ПОЛНОЕ ДУБЛИРОВАНИЕ ЭКРАНА МОЗГА
//  Все экраны и тексты идентичны мозговой версии updateDisplay() для
//  максимальной предсказуемости: что видит человек у мозга — то же видит
//  и у пульта, без расхождений в формулировках.
//
//  Список экранов (в порядке приоритета):
//    1. ❌ Связь потеряна — спец-экран "МОЗГ НЕ ОТВЕЧАЕТ"
//    2. ⚙ TILT-калибровка идёт на мозге — "КАЛИБРОВКА MPU..."
//    3. 🎯 Только что изменилось целевое давление — крупный экран на 2.5 сек
//    4. 📊 Основной экран — заголовок, давление, угол, полоска, E/V/P, статус
// =====================================================================================
void updateDisplay() {
  if (!oledOk) return;
  unsigned long now = millis();

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // ── 1. Нет связи с мозгом ───────────────────────────────────────────────
  if (!brainOnline) {
    oled.setTextSize(1);
    oled.setCursor(0, 0);  oled.print("H2 REMOTE");
    oled.setCursor(80, 0); oled.print("RF:--");
    oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);
    oled.setCursor(0, 18); oled.print("BRAIN OFFLINE");
    oled.setCursor(0, 30); oled.print("Check power");
    oled.setCursor(0, 40); oled.print("and NRF24 link");
    oled.setCursor(0, 52); oled.printf("2 Mbps, ch %d", RF_CHANNELS[rfChannelIdx]);
    oled.display();
    return;
  }

  // ── 2. Мозг сейчас калибрует MPU6050 ───────────────────────────────────
  if (r_tiltCal()) {
    oled.setTextSize(1);
    oled.setCursor(0, 0);  oled.print("H2 REMOTE");
    oled.setCursor(80, 0); oled.print("RF:OK");
    oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);
    oled.setCursor(0, 18); oled.print("BRAIN: CALIBRATING");
    oled.setCursor(0, 30); oled.print("MPU6050...");
    oled.setCursor(0, 44); oled.print("Do not move it!");
    oled.display();
    return;
  }

  // ── 3. Экран установки целевого давления (2.5 сек после изменения) ─────
  //      Полностью идентичен спецэкрану мозга
  if (now - lastTargetChange < T_SHOW_TARGET) {
    oled.setTextSize(1);
    oled.setCursor(8, 0); oled.print("TARGET PRESSURE");
    oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);
    oled.setTextSize(3);
    char tBuf[8]; dtostrf(remTarget, 4, 1, tBuf);
    oled.setCursor(8, 14); oled.print(tBuf);
    oled.setTextSize(1);
    oled.setCursor(100, 22); oled.print("bar");
    int tw = constrain((int)(remTarget / MAX_TARGET_PRESSURE * 127.0f), 0, 127);
    oled.drawRect(0, 42, 128, 5, SSD1306_WHITE);
    if (tw > 0) oled.fillRect(1, 43, tw, 3, SSD1306_WHITE);
    oled.setCursor(0, 50);
    oled.printf("%.1f         %.1f bar", MIN_TARGET_PRESSURE, MAX_TARGET_PRESSURE);
    oled.display();
    return;
  }

  // ── 4. ОСНОВНОЙ ЭКРАН — копия мозгового ────────────────────────────────

  // Заголовок (как на мозге, но "H2 REMOTE" вместо "H2 PRO" чтобы человек
  // мог отличить, на какое устройство он смотрит)
  oled.setTextSize(1);
  oled.setCursor(0, 0); oled.print("H2 REMOTE");
  oled.setCursor(48, 0); oled.print("RF:OK");
  oled.setCursor(86, 0);
  if (r_tiltLock())        oled.print("TILT!");
  else if (r_ovLock())     oled.print("!OVP");
  else if (r_purging())    oled.print("PURG");
  else if (r_purgeShort()) oled.print("PURG");
  else                     oled.print(r_sysOn() ? (r_locked() ? "LOCK" : "ON") : "OFF");
  oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  // Давление крупно — точь-в-точь как на мозге
  oled.setTextSize(2);
  oled.setCursor(0, 12);
  char pBuf[8]; dtostrf(remPressure, 4, 2, pBuf);
  oled.print(pBuf);
  oled.setTextSize(1);
  oled.setCursor(86, 18); oled.print("bar");

  // Угол наклона справа от давления
  oled.setCursor(108, 12); oled.printf("%3d", remTiltAngle);
  oled.setCursor(108, 22); oled.print("deg");

  // Полоска давления 0..6 бар + маркер цели
  const float SCALE = 7.0f;   // шкала давления 0..7 бар (запас над аварийным 6.8)
  int bw = constrain((int)(remPressure / SCALE * 127.0f), 0, 127);
  oled.drawRect(0, 30, 128, 5, SSD1306_WHITE);
  if (bw > 0) oled.fillRect(1, 31, bw, 3, SSD1306_WHITE);
  int tx = constrain((int)(remTarget / SCALE * 127.0f), 0, 127);
  oled.drawLine(tx, 28, tx, 36, SSD1306_WHITE);
  oled.drawLine(tx-1, 28, tx+1, 28, SSD1306_WHITE);
  oled.drawLine(0, 38, 127, 38, SSD1306_WHITE);

  // E/V/P + температура/вентилятор (как на мозге v5.0)
  oled.setCursor(0, 41);
  oled.print("E:"); oled.print(r_electro() ? "ON " : "OFF");
  oled.print(" V:"); oled.print(r_valve()   ? "ON " : "OFF");
  oled.print(" P:"); oled.print(r_piezo()   ? "ON"  : "OF");
  char tgtBuf[6]; dtostrf(remTarget, 3, 1, tgtBuf);
  // Температура и обороты вентилятора справа
  oled.setCursor(86, 41);
  oled.printf("%2d", (int)remTemp); oled.print("\xF8"); oled.printf("%d", remFanDuty);   // \xF8 = ° (CP437)

  // Статус / режим — те же тексты что в мозге updateDisplay()
  oled.setCursor(0, 52);
  if (r_tiltLock())            { oled.print("TILT! "); oled.print(remTiltAngle); oled.print(" deg LOCK"); }
  else if (r_ovLock())         oled.print(">RELEASE PRESSURE!<");
  else if (r_overheat())       { oled.print("!OVERHEAT "); oled.print((int)remTemp); oled.print("C!"); }
  else if (r_purging())        { oled.print("PURGE "); oled.print(pBuf); oled.print(" bar"); }
  else if (r_purgeShort())     oled.print("MINI-PURGE 200ms");
  else if (r_repulsor())       oled.print(">>REPULSOR FIRE!<<");
  else if (!r_sysOn())         { oled.print("OFF  T="); oled.print(tgtBuf); }
  else if (r_locked())         oled.print("LOCKED");
  else if (r_watchdog())       oled.print("!NO PRESSURE RISE!");
  else if (r_manual())         oled.print("MANUAL GAS");
  else {
    // АВТО — показываем драйвер и гистерезис (как на мозге)
    char hBuf[8]; dtostrf(r_hyst(), 3, 2, hBuf);
    oled.print(r_drvRelay() ? "REL h=" : "MOS h=");
    oled.print(hBuf);
    oled.print(" T="); oled.print(tgtBuf);
  }

  oled.display();
}

// ⚡ ТЗ 7.3: перерисовывать OLED не чаще раза в T_DISPLAY_UPD и ТОЛЬКО при изменении
// отображаемых данных (либо принудительно раз в 1 с, либо на «живом» экране показа
// установки цели). Убирает мерцание и снижает нагрузку на шину I2C.
void maybeUpdateDisplay(unsigned long now) {
  if (!oledOk) return;
  if (now - lastDispCheck < T_DISPLAY_UPD) return;
  lastDispCheck = now;
  char sig[110];
  snprintf(sig, sizeof(sig), "%d|%u|%u|%.2f|%.1f|%u|%d|%u|%u",
    brainOnline ? 1 : 0, (unsigned)remFlags, (unsigned)remFlags2,
    remPressure, remTarget, (unsigned)remTiltAngle, (int)remTemp,
    (unsigned)remFanDuty, (unsigned)remHystX10);
  bool animated = (now - lastTargetChange < T_SHOW_TARGET);   // «живой» экран цели
  bool forced   = (now - lastDispDraw >= 1000);
  if (animated || forced || strcmp(sig, lastDispSig) != 0) {
    updateDisplay();
    strncpy(lastDispSig, sig, sizeof(lastDispSig) - 1);
    lastDispSig[sizeof(lastDispSig) - 1] = '\0';
    lastDispDraw = now;
  }
}


// =====================================================================================
//  POWER (двойной клик)
// =====================================================================================
void handlePowerBtn(unsigned long now) {
  bool raw = digitalRead(PIN_BTN_POWER);
  if (raw != pwrLastRaw) pwrDebTime = now;
  if ((now - pwrDebTime) > T_DEBOUNCE && raw != pwrStable) {
    pwrStable = raw;
    if (pwrStable == LOW) {
      if ((now - pwrLastClick) < T_DOUBLE_CLICK) sendCmd(99, "SAFE OFF");
      else                                        sendCmd(1,  "POWER");
      pwrLastClick = now;
    }
  }
  pwrLastRaw = raw;
}


// =====================================================================================
//  Простые кнопки
// =====================================================================================
void handleSimpleBtns(unsigned long now) {
  for (uint8_t i = 0; i < NUM_SIMPLE_BTNS; i++) {
    bool raw = digitalRead(SIMPLE_BTN_PINS[i]);
    if (raw != simpleLastRaw[i]) simpleDebTime[i] = now;
    if ((now - simpleDebTime[i]) > T_DEBOUNCE && raw != simpleStable[i]) {
      simpleStable[i] = raw;
      if (simpleStable[i] == LOW) sendCmd(SIMPLE_BTN_CMDS[i], SIMPLE_BTN_LABELS[i]);
    }
    simpleLastRaw[i] = raw;
  }
}


// =====================================================================================
//  SETUP
// =====================================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== H2 PRO Remote v5.0 ===");

  for (uint8_t i = 0; i < NUM_SIMPLE_BTNS; i++) {
    pinMode(SIMPLE_BTN_PINS[i], INPUT_PULLUP);
    // Стартуем с ФАКТИЧЕСКОГО уровня пина, а не HIGH: если проснулись от
    // удержанной кнопки TGT− (GPIO 33), фронта HIGH→LOW не возникает и
    // команда 9 (TGT −0.5) НЕ уходит — пробуждение лишь включает пульт,
    // как и заявлено. Команда уйдёт только при следующем полном нажатии.
    bool lvl = digitalRead(SIMPLE_BTN_PINS[i]);
    simpleLastRaw[i] = lvl;
    simpleStable[i]  = lvl;
    simpleDebTime[i] = 0;
  }
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);

  pinMode(PIN_LED_TX,    OUTPUT); digitalWrite(PIN_LED_TX,    LOW);
  pinMode(PIN_LED_POWER, OUTPUT); digitalWrite(PIN_LED_POWER, HIGH);

  Wire.begin(21, 22);
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledOk) Serial.println("OLED: нет");
  else {
    oled.cp437(true);   // корректная таблица CP437: \xF8 = знак градуса
    oled.clearDisplay(); oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(12, 20); oled.print("H2 REMOTE v5.0");
    oled.setCursor(20, 36); oled.print("NRF24 @ 2 Mbps");
    oled.display();
    Serial.println("OLED: OK");
  }

  SPI.begin(18, 19, 23, PIN_NRF_CSN);
  if (!radio.begin()) {
    Serial.println("NRF24: FAIL");
    for (int i = 0; i < 3; i++) {
      digitalWrite(PIN_LED_TX, HIGH); delay(200);
      digitalWrite(PIN_LED_TX, LOW);  delay(200);
    }
  } else {
    radio.openWritingPipe(PIPE_R2B);
    radio.openReadingPipe(1, PIPE_B2R);
    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_2MBPS);
    radio.setCRCLength(RF24_CRC_16);
    // ⚡ Ограничиваем автоповторы: при отсутствующем мозге radio.write() команды
    // не должен блокировать пульт надолго. 3 попытки × 500 мкс ≈ 1.5 мс макс
    // (вместо 15 попыток дефолтом). Команды редкие, надёжность сохраняется.
    radio.setRetries(2, 3);
    rfChannelIdx = 0;
    radio.setChannel(RF_CHANNELS[rfChannelIdx]);   // ⚡ v5.0: канал из массива
    radio.startListening();
    Serial.printf("NRF24: OK (2 Мбит/с, канал %d, +резерв)\n", RF_CHANNELS[rfChannelIdx]);
    for (int i = 0; i < 2; i++) {
      digitalWrite(PIN_LED_TX, HIGH); delay(80);
      digitalWrite(PIN_LED_TX, LOW);  delay(80);
    }
  }

  // ⚡ v5.0: инициализация таймеров RF и сна
  rfLastActivity = millis();
  lastActivityMs = millis();

  // Если проснулись из сна — сообщим в Serial
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("[WAKE] Пробуждение из сна кнопкой TGT-.");
  }

  Serial.println("Пульт готов:");
  Serial.println("  GPIO16 → POWER (1× вкл/выкл, 2× продувка)");
  for (uint8_t i = 0; i < NUM_SIMPLE_BTNS; i++) {
    Serial.printf("  GPIO%d → %s (cmd=%d)\n",
      SIMPLE_BTN_PINS[i], SIMPLE_BTN_LABELS[i], SIMPLE_BTN_CMDS[i]);
  }
}


// =====================================================================================
//  ⚡ v5.0: ГЛУБОКИЙ СОН
// =====================================================================================
void goToSleep() {
  // Показываем уведомление на OLED перед сном
  if (oledOk) {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setCursor(10, 20); oled.print("SLEEP MODE");
    oled.setCursor(2, 34);  oled.print("TGT- wakes up");
    oled.display();
    delay(1500);
    // Гасим панель напрямую командой (без clearDisplay+display перед DISPLAYOFF —
    // лишняя перерисовка не нужна, важно именно выключить контроллер дисплея)
    oled.ssd1306_command(SSD1306_DISPLAYOFF);
  }
  digitalWrite(PIN_LED_POWER, LOW);
  digitalWrite(PIN_LED_TX, LOW);
  // ⚡ ИСПРАВЛЕНО: ext1 с ALL_LOW будит только когда ВСЕ пины маски в LOW
  // одновременно (т.е. надо зажать обе кнопки). Это неверно для пробуждения
  // «по любой кнопке». На классическом ESP32 для пробуждения по нажатию ОДНОЙ
  // кнопки (активный уровень LOW) используем ext0 на одном RTC-пине.
  // Будим по TGT− (GPIO 33). Кнопка на GND, внутренняя подтяжка вверх, нажатие = LOW = 0.
  esp_sleep_enable_ext0_wakeup((gpio_num_t)33, 0);
  Serial.println("[SLEEP] Уход в глубокий сон. Пробуждение кнопкой TGT- (GPIO 33).");
  Serial.flush();
  esp_deep_sleep_start();
  // после пробуждения выполнится setup() заново (это not a return point)
}


// =====================================================================================
//  LOOP
// =====================================================================================
void loop() {
  unsigned long now = millis();

  if (radio.available()) {
    StatusPacket pkt;
    radio.read(&pkt, sizeof(pkt));
    if (pkt.type == 0xAA) {
      remPressure  = pkt.pressure;
      // Если target изменился — запоминаем время для показа спецэкрана
      if (prevRemTarget < 0) {
        prevRemTarget = pkt.target;            // первый приём — без спецэкрана
      } else if (fabsf(pkt.target - prevRemTarget) > 0.01f) {
        lastTargetChange = now;                // мозг изменил target
        prevRemTarget    = pkt.target;
      }
      remTarget    = pkt.target;
      remFlags     = pkt.flags;
      remFlags2    = pkt.flags2;
      remTiltAngle = pkt.tiltAngle;
      remHystX10   = pkt.hystX10;     // ⚡ v4.3
      remTemp      = pkt.temp;        // ⚡ v5.0
      remFanDuty   = pkt.fanDuty;     // ⚡ v5.0
      lastRxTime   = now;
      rfLastActivity = now;           // ⚡ v5.0: связь жива на этом канале
    }
  }
  brainOnline = (now - lastRxTime < T_RF_TIMEOUT) && (lastRxTime > 0);

  // ⚡ v5.0: резервный канал — если тишина, прыгаем на следующий.
  // Пока мозг ни разу не услышан (старт или пробуждение из deep sleep) —
  // сканируем быстро (1.2 с/канал): пока пульт спал, мозг мог прыгать по
  // каналам. Быстрый скан 3 каналов находит его за ≤4 с, и экран сразу
  // показывает актуальное состояние.
  unsigned long hopSilence = (lastRxTime == 0) ? RF_SCAN_FAST : RF_HOP_SILENCE;
  if (now - rfLastActivity >= hopSilence) {
    rfChannelIdx = (rfChannelIdx + 1) % RF_CHANNEL_CNT;
    radio.setChannel(RF_CHANNELS[rfChannelIdx]);
    rfLastActivity = now;
  }

  handlePowerBtn(now);
  handleSimpleBtns(now);

  if (ledTxOff > 0 && now >= ledTxOff) {
    digitalWrite(PIN_LED_TX, LOW);
    ledTxOff = 0;
  }

  maybeUpdateDisplay(now);

  // ⚡ v5.0: глубокий сон при длительном простое (экономия батареи)
  if (DEEP_SLEEP_ENABLE && (now - lastActivityMs >= IDLE_SLEEP_MS)) {
    goToSleep();
  }
}
