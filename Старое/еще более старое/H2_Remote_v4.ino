/*
 * =====================================================================================
 *  H2 CONTROL SYSTEM — ПУЛЬТ v4.0  (ESP32 DevKit v1)
 * =====================================================================================
 *
 *  Адаптирован под мозг v4.0:
 *    • NRF24 на 2 Мбит/с
 *    • Двусторонняя связь, приём статуса 10 Гц (каждые 100 мс)
 *    • Новая структура статус-пакета (float pressure + float target + flags + flags2)
 *    • Регулировка давления двумя кнопками ±0.5 бар (новые команды 8/9)
 *    • Кнопка POWER: одиночный = вкл/выкл, двойной = продувка (как в brain_v3)
 *    • Полоска давления 0–6 бар, маркер цели
 *    • Индикация предупреждения и аварии по давлению (флаги flags2)
 *
 *  СХЕМА ПОДКЛЮЧЕНИЯ
 *  -----------------
 *    NRF24L01:  CE=GPIO4   CSN=GPIO5
 *               SCK=GPIO18 MISO=GPIO19 MOSI=GPIO23
 *               VCC=3.3V  + конденсатор 470мкФ × 10В между VCC и GND!
 *
 *    OLED SSD1306 (I2C):
 *               SDA=GPIO21  SCL=GPIO22  VCC=3.3V
 *
 *    КНОПКИ (INPUT_PULLUP, замыкаются на GND):
 *               GPIO16 → POWER             (одиночный = cmd 1, двойной = cmd 99)
 *               GPIO17 → LOCK              (cmd = 2)
 *               GPIO25 → ГОРЕЛКА           (cmd = 3)
 *               GPIO26 → РУЧНАЯ ПОДАЧА     (cmd = 7)
 *               GPIO27 → ПОДЖИГ            (cmd = 4)
 *               GPIO32 → TARGET +0.5 бар   (cmd = 8)
 *               GPIO33 → TARGET -0.5 бар   (cmd = 9)
 *
 *    СВЕТОДИОДЫ (резистор 220 Ом → GND):
 *               GPIO14 → LED зелёный  (мигает при отправке)
 *               GPIO13 → LED синий    (горит — пульт включён)
 *
 *  ВАЖНО: скорость NRF24 на пульте и мозге ДОЛЖНА совпадать (2 Мбит/с)!
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
// Все кнопки кроме POWER — простой "однократный клик → команда".
// POWER обрабатывается отдельно: одиночный=cmd 1, двойной в окне 400 мс=cmd 99.
const uint8_t NUM_SIMPLE_BTNS = 6;
const int     SIMPLE_BTN_PINS[NUM_SIMPLE_BTNS] = { 17,  25,  26,  27,  32,  33 };
const uint8_t SIMPLE_BTN_CMDS[NUM_SIMPLE_BTNS] = {  2,   3,   7,   4,   8,   9 };
const char*   SIMPLE_BTN_LABELS[NUM_SIMPLE_BTNS] = {
  "LOCK",
  "ГОРЕЛКА",
  "MANUAL",
  "PIEZO",
  "TGT +0.5",
  "TGT -0.5"
};

#define PIN_BTN_POWER   16


// ======================== СВЕТОДИОДЫ ========================
#define PIN_LED_TX    14
#define PIN_LED_POWER 13


// ======================== OLED ========================
#define OLED_W 128
#define OLED_H  64
Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);
bool oledOk = false;


// ======================== NRF24 ========================
RF24 radio(PIN_NRF_CE, PIN_NRF_CSN);
const byte PIPE_R2B[6] = "CMD01";
const byte PIPE_B2R[6] = "STS01";

// Команда пульт → мозг (СИНХРОНИЗИРОВАНО С МОЗГОМ)
struct __attribute__((packed)) CmdPacket {
  uint8_t type;   // 0x55
  uint8_t cmd;
};

// Статус мозг → пульт (СИНХРОНИЗИРОВАНО С МОЗГОМ — 11 байт)
struct __attribute__((packed)) StatusPacket {
  uint8_t type;       // 0xAA
  float   pressure;
  float   target;
  uint8_t flags;
  uint8_t flags2;
};
// flags:  bit0=systemOn  bit1=isLocked  bit2=isPurging
//         bit3=burnerOn  bit4=manualOn
//         bit5=electroOn bit6=valveOn   bit7=piezoOn
// flags2: bit0=overpressureWarn  bit1=overpressureLock


// ======================== ТАЙМИНГИ ========================
const unsigned long T_DEBOUNCE     = 50;
const unsigned long T_DOUBLE_CLICK = 400;     // Окно двойного клика POWER
const unsigned long T_RF_TIMEOUT   = 3000;
const unsigned long T_DISPLAY_UPD  = 200;
const unsigned long T_LED_BLINK    = 60;


// ======================== СОСТОЯНИЕ (от мозга) ========================
float   remPressure = 0.0f;
float   remTarget   = 1.0f;
uint8_t remFlags    = 0;
uint8_t remFlags2   = 0;
bool    brainOnline = false;
unsigned long lastRxTime = 0;

bool r_sysOn()    { return remFlags & (1 << 0); }
bool r_locked()   { return remFlags & (1 << 1); }
bool r_purging()  { return remFlags & (1 << 2); }
bool r_burner()   { return remFlags & (1 << 3); }
bool r_manual()   { return remFlags & (1 << 4); }
bool r_electro()  { return remFlags & (1 << 5); }
bool r_valve()    { return remFlags & (1 << 6); }
bool r_piezo()    { return remFlags & (1 << 7); }
bool r_ovWarn()   { return remFlags2 & (1 << 0); }
bool r_ovLock()   { return remFlags2 & (1 << 1); }


// ======================== АНТИДРЕБЕЗГ ========================
bool          simpleLastRaw[NUM_SIMPLE_BTNS];
bool          simpleStable[NUM_SIMPLE_BTNS];
unsigned long simpleDebTime[NUM_SIMPLE_BTNS];

// POWER — отдельная логика двойного клика
bool          pwrLastRaw   = HIGH;
bool          pwrStable    = HIGH;
unsigned long pwrDebTime   = 0;
unsigned long pwrLastClick = 0;


// ======================== ПРОЧЕЕ ========================
char          lastCmdLabel[16] = "---";
unsigned long lastDispUpd      = 0;
unsigned long ledTxOff         = 0;


// =====================================================================================
//  ОТПРАВКА КОМАНДЫ ПО NRF24
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

  Serial.print("[TX] ");
  Serial.print(label);
  Serial.print(" (");
  Serial.print(cmd);
  Serial.print(") -> ");
  Serial.println(ok ? "OK" : "FAIL");
}


// =====================================================================================
//  OLED — ПОЛОСКА 0–6 БАР + МАРКЕР ЦЕЛИ + ИНДИКАЦИЯ АВАРИИ
// =====================================================================================
void updateDisplay() {
  if (!oledOk) return;

  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);

  // === Заголовок ===
  oled.setTextSize(1);
  oled.setCursor(0, 0);
  oled.print("H2 REMOTE");
  oled.setCursor(80, 0);
  oled.print("RF:");
  oled.print(brainOnline ? "OK" : "--");

  oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  if (!brainOnline) {
    oled.setTextSize(1);
    oled.setCursor(0, 16);
    oled.print("МОЗГ НЕ ОТВЕЧАЕТ");
    oled.setCursor(0, 28);
    oled.print("Проверьте питание");
    oled.setCursor(0, 38);
    oled.print("и связь NRF24L01");
    oled.setCursor(0, 52);
    oled.print("Скорость: 2 Mbps");
    oled.display();
    return;
  }

  // === Давление крупным шрифтом ===
  oled.setTextSize(2);
  oled.setCursor(0, 12);
  char pBuf[8];
  dtostrf(remPressure, 4, 2, pBuf);
  oled.print(pBuf);
  oled.setTextSize(1);
  oled.setCursor(86, 18);
  oled.print("bar");

  // Иконка состояния системы справа
  oled.setCursor(108, 12);
  if (r_ovLock())       oled.print("!!");
  else if (r_purging()) oled.print(">>");
  else if (r_locked())  oled.print("L ");
  else if (r_sysOn())   oled.print("ON");
  else                  oled.print("--");

  // === Полоска давления 0..6 бар + маркер цели ===
  const float SCALE_MAX = 6.0f;
  int bw = (int)((remPressure / SCALE_MAX) * 127.0f);
  bw = constrain(bw, 0, 127);

  oled.drawRect(0, 30, 128, 5, SSD1306_WHITE);
  if (bw > 0) oled.fillRect(1, 31, bw, 3, SSD1306_WHITE);

  // Маркер целевого давления
  int tgtX = (int)((remTarget / SCALE_MAX) * 127.0f);
  tgtX = constrain(tgtX, 0, 127);
  oled.drawLine(tgtX, 28, tgtX, 36, SSD1306_WHITE);
  oled.drawLine(tgtX-1, 28, tgtX+1, 28, SSD1306_WHITE);

  oled.drawLine(0, 38, 127, 38, SSD1306_WHITE);

  // === Статус реле E V P ===
  oled.setCursor(0, 41);
  oled.print("E:");
  oled.print(r_electro() ? "1" : "0");
  oled.print(" V:");
  oled.print(r_valve()   ? "1" : "0");
  oled.print(" P:");
  oled.print(r_piezo()   ? "1" : "0");

  // Цель справа от E/V/P
  char tgtBuf[6];
  dtostrf(remTarget, 3, 1, tgtBuf);
  oled.setCursor(70, 41);
  oled.print("T=");
  oled.print(tgtBuf);
  oled.print("b");

  // === Режим работы / последняя команда ===
  oled.setCursor(0, 52);
  if (r_ovLock()) {
    oled.print(">АВАРИЯ ПО ДАВЛЕНИЮ<");
  } else if (r_purging()) {
    oled.print(">>> ПРОДУВКА <<<");
  } else if (!r_sysOn()) {
    oled.print("ВЫКЛ  >");
    oled.print(lastCmdLabel);
  } else if (r_locked()) {
    oled.print("ЗАБЛОКИРОВАНО");
  } else if (r_burner()) {
    oled.print("ГОРЕЛКА  >");
    oled.print(lastCmdLabel);
  } else if (r_manual()) {
    oled.print("РУЧН.ПОДАЧА");
  } else if (r_ovWarn()) {
    oled.print("!ПРЕДУПР  АВТО");
  } else {
    oled.print("АВТО  >");
    oled.print(lastCmdLabel);
  }

  oled.display();
}


// =====================================================================================
//  ОБРАБОТКА КНОПКИ POWER (одиночный мгновенно, двойной = продувка)
// =====================================================================================
void handlePowerBtn(unsigned long now) {
  bool raw = digitalRead(PIN_BTN_POWER);
  if (raw != pwrLastRaw) pwrDebTime = now;
  if ((now - pwrDebTime) > T_DEBOUNCE && raw != pwrStable) {
    pwrStable = raw;
    if (pwrStable == LOW) {
      if ((now - pwrLastClick) < T_DOUBLE_CLICK) {
        sendCmd(99, "SAFE OFF");
      } else {
        sendCmd(1,  "POWER");
      }
      pwrLastClick = now;
    }
  }
  pwrLastRaw = raw;
}


// =====================================================================================
//  ОБРАБОТКА ПРОСТЫХ КНОПОК (LOCK, ГОРЕЛКА, MANUAL, PIEZO, ±)
// =====================================================================================
void handleSimpleBtns(unsigned long now) {
  for (uint8_t i = 0; i < NUM_SIMPLE_BTNS; i++) {
    bool raw = digitalRead(SIMPLE_BTN_PINS[i]);
    if (raw != simpleLastRaw[i]) simpleDebTime[i] = now;
    if ((now - simpleDebTime[i]) > T_DEBOUNCE && raw != simpleStable[i]) {
      simpleStable[i] = raw;
      if (simpleStable[i] == LOW) {
        sendCmd(SIMPLE_BTN_CMDS[i], SIMPLE_BTN_LABELS[i]);
      }
    }
    simpleLastRaw[i] = raw;
  }
}


// =====================================================================================
//  SETUP
// =====================================================================================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== H2 PRO Remote v4.0 ===");

  // Простые кнопки
  for (uint8_t i = 0; i < NUM_SIMPLE_BTNS; i++) {
    pinMode(SIMPLE_BTN_PINS[i], INPUT_PULLUP);
    simpleLastRaw[i] = HIGH;
    simpleStable[i]  = HIGH;
    simpleDebTime[i] = 0;
  }
  // POWER
  pinMode(PIN_BTN_POWER, INPUT_PULLUP);

  // Светодиоды
  pinMode(PIN_LED_TX,    OUTPUT);
  pinMode(PIN_LED_POWER, OUTPUT);
  digitalWrite(PIN_LED_TX,    LOW);
  digitalWrite(PIN_LED_POWER, HIGH);

  // OLED
  Wire.begin(21, 22);
  oledOk = oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (!oledOk) {
    Serial.println("OLED: не найден!");
  } else {
    oled.clearDisplay();
    oled.setTextSize(1);
    oled.setTextColor(SSD1306_WHITE);
    oled.setCursor(12, 20);
    oled.print("H2 PRO REMOTE v4.0");
    oled.setCursor(20, 36);
    oled.print("NRF24 @ 2 Mbps");
    oled.display();
    Serial.println("OLED: OK");
  }

  // NRF24L01 — 2 Мбит/с, MAX мощность, CRC16
  SPI.begin(18, 19, 23, PIN_NRF_CSN);
  if (!radio.begin()) {
    Serial.println("NRF24: FAIL! Проверьте подключение и питание 3.3 V!");
    for (int i = 0; i < 3; i++) {
      digitalWrite(PIN_LED_TX, HIGH); delay(200);
      digitalWrite(PIN_LED_TX, LOW);  delay(200);
    }
  } else {
    radio.openWritingPipe(PIPE_R2B);
    radio.openReadingPipe(1, PIPE_B2R);
    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_2MBPS);       // ← 2 Мбит/с — должно совпадать с мозгом!
    radio.setCRCLength(RF24_CRC_16);
    radio.setChannel(76);
    radio.startListening();
    Serial.println("NRF24: OK  (2 Мбит/с)");
    for (int i = 0; i < 2; i++) {
      digitalWrite(PIN_LED_TX, HIGH); delay(80);
      digitalWrite(PIN_LED_TX, LOW);  delay(80);
    }
  }

  Serial.println("Пульт готов. Раскладка кнопок:");
  Serial.println("  GPIO16 → POWER (1× вкл/выкл, 2× продувка)");
  for (uint8_t i = 0; i < NUM_SIMPLE_BTNS; i++) {
    Serial.print("  GPIO"); Serial.print(SIMPLE_BTN_PINS[i]);
    Serial.print(" → ");
    Serial.print(SIMPLE_BTN_LABELS[i]);
    Serial.print("  (cmd=");
    Serial.print(SIMPLE_BTN_CMDS[i]);
    Serial.println(")");
  }
}


// =====================================================================================
//  LOOP
// =====================================================================================
void loop() {
  unsigned long now = millis();

  // ---- Приём статуса от мозга ----
  if (radio.available()) {
    StatusPacket pkt;
    radio.read(&pkt, sizeof(pkt));
    if (pkt.type == 0xAA) {
      remPressure = pkt.pressure;
      remTarget   = pkt.target;
      remFlags    = pkt.flags;
      remFlags2   = pkt.flags2;
      lastRxTime  = now;
    }
  }
  brainOnline = (now - lastRxTime < T_RF_TIMEOUT) && (lastRxTime > 0);

  // ---- Кнопки ----
  handlePowerBtn(now);
  handleSimpleBtns(now);

  // ---- LED TX выключение ----
  if (ledTxOff > 0 && now >= ledTxOff) {
    digitalWrite(PIN_LED_TX, LOW);
    ledTxOff = 0;
  }

  // ---- OLED ----
  if (oledOk && (now - lastDispUpd >= T_DISPLAY_UPD)) {
    updateDisplay();
    lastDispUpd = now;
  }
}
