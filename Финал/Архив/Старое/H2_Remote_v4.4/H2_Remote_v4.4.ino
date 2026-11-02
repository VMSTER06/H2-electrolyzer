/*
 * =====================================================================================
 *  H2 CONTROL SYSTEM — ПУЛЬТ v4.4  (ESP32 DevKit v1)
 * =====================================================================================
 *
 *  Адаптирован под мозг v4.3 (РЕПУЛЬСОР + регулируемый гистерезис).
 *
 *  ⚡ ОТЛИЧИЕ v4.4 ОТ v4.3:
 *    • Кнопка ГОРЕЛКА переименована в РЕПУЛЬСОР, отправляет тот же cmd=3,
 *      но мозг теперь делает одноразовый выстрел: клапан 800 мс, пьезо
 *      со 700-й мс на 100 мс. Повторное нажатие во время цикла игнорируется.
 *    • Структура StatusPacket расширена с 13 до 15 байт — добавлен hystX10
 *      (гистерезис × 10) и бит5 во flags2 = driver (0=MOSFET, 1=РЕЛЕ).
 *    • Нижняя строка OLED в авто-режиме теперь показывает драйвер и
 *      гистерезис, например:  MOS h=0.20 T=2.5  или  REL h=0.40 T=2.5.
 *      Это синхронизирует пульт с мозгом без расхождений.
 *    • Бит3 в flags переименован burner → repulsor (значение бита то же,
 *      но семантика изменилась: 1=идёт цикл репульсора). При активном
 *      репульсоре нижняя строка показывает >>РЕПУЛЬСОР ОГОНЬ<<.
 *
 *  ⚡ ОТЛИЧИЕ v4.3 ОТ v4.2:
 *    Экран пульта полностью дублирует экран мозга — те же тексты,
 *    тот же спецэкран установки давления на 2.5 сек при изменении target,
 *    тот же формат полей E:ON/OFF, V:ON/OFF, P:ON/OF.
 *
 *  ОБЩИЕ ВОЗМОЖНОСТИ:
 *    • NRF24 на 2 Мбит/с, двусторонняя связь
 *    • Приём статуса каждые 100 мс (10 Гц)
 *    • Структура StatusPacket (15 байт): float pressure, float target,
 *      flags, flags2, tiltAngle, hystX10
 *    • Индикация TILT-сработки и текущего угла наклона на OLED
 *    • Кнопка LOCK сбрасывает все защёлки (LOCK, TILT, overpressure)
 *    • Регулировка целевого давления двумя кнопками ±0.5 бар
 *    • POWER: одиночный = вкл/выкл, двойной (≤400 мс) = продувка до нуля
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
const byte PIPE_R2B[6] = "CMD01";
const byte PIPE_B2R[6] = "STS01";

struct __attribute__((packed)) CmdPacket {
  uint8_t type;   // 0x55
  uint8_t cmd;
};

// Должна совпадать с мозгом v4.3 (15 байт)
struct __attribute__((packed)) StatusPacket {
  uint8_t type;       // 0xAA
  float   pressure;
  float   target;
  uint8_t flags;      // bit0=systemOn   bit1=isLocked  bit2=isPurging
                      // bit3=repulsor   bit4=manual    bit5=electro
                      // bit6=valve      bit7=piezo
  uint8_t flags2;     // bit0=ovWarn  bit1=ovLock  bit2=purgeShort
                      // bit3=tiltLock bit4=tiltCal bit5=driver(0=MOS,1=REL)
  uint8_t tiltAngle;  // 0..180 градусов
  uint8_t hystX10;    // гистерезис × 10 (0..10 → 0.0..1.0 бар)
};


// ======================== ТАЙМИНГИ ========================
const unsigned long T_DEBOUNCE     = 50;
const unsigned long T_DOUBLE_CLICK = 400;
const unsigned long T_RF_TIMEOUT   = 3000;
const unsigned long T_DISPLAY_UPD  = 200;
const unsigned long T_LED_BLINK    = 60;
const unsigned long T_SHOW_TARGET  = 2500;   // Спецэкран установки давления, как на мозге

// Пороги (для индикации, должны совпадать с мозгом)
const float MIN_TARGET_PRESSURE = 0.5f;
const float MAX_TARGET_PRESSURE = 6.0f;


// ======================== СОСТОЯНИЕ (от мозга) ========================
float   remPressure  = 0.0f;
float   remTarget    = 1.0f;
float   prevRemTarget = -1.0f;       // Для детектирования изменения target
uint8_t remFlags     = 0;
uint8_t remFlags2    = 0;
uint8_t remTiltAngle = 0;
uint8_t remHystX10   = 0;            // ⚡ v4.3: гистерезис × 10
bool    brainOnline  = false;
unsigned long lastRxTime = 0;
unsigned long lastTargetChange = 0;  // Когда мозг последний раз изменил target

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
unsigned long lastDispUpd      = 0;
unsigned long ledTxOff         = 0;


// =====================================================================================
//  ОТПРАВКА КОМАНДЫ
// =====================================================================================
void sendCmd(uint8_t cmd, const char* label) {
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
    oled.setCursor(0, 18); oled.print("МОЗГ НЕ ОТВЕЧАЕТ");
    oled.setCursor(0, 30); oled.print("Проверь питание");
    oled.setCursor(0, 40); oled.print("и связь NRF24");
    oled.setCursor(0, 52); oled.print("2 Mbps, канал 76");
    oled.display();
    return;
  }

  // ── 2. Мозг сейчас калибрует MPU6050 ───────────────────────────────────
  if (r_tiltCal()) {
    oled.setTextSize(1);
    oled.setCursor(0, 0);  oled.print("H2 REMOTE");
    oled.setCursor(80, 0); oled.print("RF:OK");
    oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);
    oled.setCursor(0, 18); oled.print("МОЗГ: КАЛИБРОВКА");
    oled.setCursor(0, 30); oled.print("MPU6050...");
    oled.setCursor(0, 44); oled.print("Не двигайте систему!");
    oled.display();
    return;
  }

  // ── 3. Экран установки целевого давления (2.5 сек после изменения) ─────
  //      Полностью идентичен спецэкрану мозга
  if (now - lastTargetChange < T_SHOW_TARGET) {
    oled.setTextSize(1);
    oled.setCursor(8, 0); oled.print("ЦЕЛЕВОЕ ДАВЛЕНИЕ");
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
    oled.printf("%.1f         %.1f бар", MIN_TARGET_PRESSURE, MAX_TARGET_PRESSURE);
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
  else if (r_ovLock())     oled.print("!АВАР");
  else if (r_purging())    oled.print("ПРОД");
  else if (r_purgeShort()) oled.print("ПРОД");
  else                     oled.print(r_sysOn() ? (r_locked() ? "ЛОК" : "ВКЛ") : "ВЫКЛ");
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
  const float SCALE = 6.0f;
  int bw = constrain((int)(remPressure / SCALE * 127.0f), 0, 127);
  oled.drawRect(0, 30, 128, 5, SSD1306_WHITE);
  if (bw > 0) oled.fillRect(1, 31, bw, 3, SSD1306_WHITE);
  int tx = constrain((int)(remTarget / SCALE * 127.0f), 0, 127);
  oled.drawLine(tx, 28, tx, 36, SSD1306_WHITE);
  oled.drawLine(tx-1, 28, tx+1, 28, SSD1306_WHITE);
  oled.drawLine(0, 38, 127, 38, SSD1306_WHITE);

  // E/V/P + цель — теперь в полном формате как на мозге (ON/OFF)
  oled.setCursor(0, 41);
  oled.print("E:"); oled.print(r_electro() ? "ON " : "OFF");
  oled.print(" V:"); oled.print(r_valve()   ? "ON " : "OFF");
  oled.print(" P:"); oled.print(r_piezo()   ? "ON"  : "OF");
  char tgtBuf[6]; dtostrf(remTarget, 3, 1, tgtBuf);
  oled.setCursor(70, 41); oled.print("T="); oled.print(tgtBuf);

  // Статус / режим — те же тексты что в мозге updateDisplay()
  oled.setCursor(0, 52);
  if (r_tiltLock())            { oled.print("TILT! "); oled.print(remTiltAngle); oled.print("° >ЛОК< "); }
  else if (r_ovLock())         oled.print(">СБРОСЬТЕ ДАВЛЕНИЕ<");
  else if (r_purging())        { oled.print("ПРОДУВКА "); oled.print(pBuf); oled.print("b"); }
  else if (r_purgeShort())     oled.print("МИН.ПРОДУВКА 200мс");
  else if (r_repulsor())       oled.print(">>РЕПУЛЬСОР ОГОНЬ<<");
  else if (!r_sysOn())         { oled.print("ВЫКЛ  T="); oled.print(tgtBuf); }
  else if (r_locked())         oled.print("ЗАБЛОКИРОВАНО");
  else if (r_manual())         oled.print("РУЧНАЯ ПОДАЧА");
  else {
    // АВТО — показываем драйвер и гистерезис (как на мозге)
    char hBuf[8]; dtostrf(r_hyst(), 3, 2, hBuf);
    oled.print(r_drvRelay() ? "REL h=" : "MOS h=");
    oled.print(hBuf);
    oled.print(" T="); oled.print(tgtBuf);
  }

  oled.display();
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
  Serial.println("\n=== H2 PRO Remote v4.4 ===");

  for (uint8_t i = 0; i < NUM_SIMPLE_BTNS; i++) {
    pinMode(SIMPLE_BTN_PINS[i], INPUT_PULLUP);
    simpleLastRaw[i] = HIGH;
    simpleStable[i]  = HIGH;
    simpleDebTime[i] = 0;
  }
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);

  pinMode(PIN_LED_TX,    OUTPUT); digitalWrite(PIN_LED_TX,    LOW);
  pinMode(PIN_LED_POWER, OUTPUT); digitalWrite(PIN_LED_POWER, HIGH);

  Wire.begin(21, 22);
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledOk) Serial.println("OLED: нет");
  else {
    oled.clearDisplay(); oled.setTextSize(1); oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(12, 20); oled.print("H2 REMOTE v4.4");
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
    radio.setChannel(76);
    radio.startListening();
    Serial.println("NRF24: OK (2 Мбит/с)");
    for (int i = 0; i < 2; i++) {
      digitalWrite(PIN_LED_TX, HIGH); delay(80);
      digitalWrite(PIN_LED_TX, LOW);  delay(80);
    }
  }

  Serial.println("Пульт готов:");
  Serial.println("  GPIO16 → POWER (1× вкл/выкл, 2× продувка)");
  for (uint8_t i = 0; i < NUM_SIMPLE_BTNS; i++) {
    Serial.printf("  GPIO%d → %s (cmd=%d)\n",
      SIMPLE_BTN_PINS[i], SIMPLE_BTN_LABELS[i], SIMPLE_BTN_CMDS[i]);
  }
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
      lastRxTime   = now;
    }
  }
  brainOnline = (now - lastRxTime < T_RF_TIMEOUT) && (lastRxTime > 0);

  handlePowerBtn(now);
  handleSimpleBtns(now);

  if (ledTxOff > 0 && now >= ledTxOff) {
    digitalWrite(PIN_LED_TX, LOW);
    ledTxOff = 0;
  }

  if (oledOk && (now - lastDispUpd >= T_DISPLAY_UPD)) {
    updateDisplay();
    lastDispUpd = now;
  }
}
