/*
 * =====================================================================
 *  H2 CONTROL SYSTEM — ПУЛЬТ v3.0  (ESP32 DevKit v1)
 * =====================================================================
 *  Библиотеки (Менеджер библиотек Arduino IDE):
 *    - RF24                  (by TMRh20)
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
 *    КНОПКИ (INPUT_PULLUP, кнопка замкнута на GND):
 *               GPIO16 → POWER ON/OFF          (cmd=1)
 *               GPIO17 → БЕЗОПАСН. ВЫКЛЮЧЕНИЕ  (cmd=99)
 *               GPIO25 → БЛОКИРОВКА            (cmd=2)
 *               GPIO26 → ПОСТОЯННЫЙ ГАЗ        (cmd=3)
 *               GPIO27 → РУЧНАЯ ПОДАЧА         (cmd=7)
 *               GPIO32 → ПОДЖИГ (500 мс)       (cmd=4)
 *               GPIO33 → РЕЖИМ 1/4 БАР         (cmd=6)
 *
 *    СВЕТОДИОДЫ (резистор 220 Ом → GND):
 *               GPIO14 → LED зелёный  (мигает при отправке)
 *               GPIO13 → LED синий    (горит — пульт включён)
 *
 *  ПИТАНИЕ: PowerBank → micro-USB / 3×AA батарейки
 * =====================================================================
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
// VSPI: SCK=18, MISO=19, MOSI=23

// ======================== КНОПКИ ========================
const uint8_t NUM_BTNS                   = 7;
const int     BTN_PINS[NUM_BTNS]         = { 16,  17,  25,  26,  27,  32,  33 };
const uint8_t BTN_CMDS[NUM_BTNS]         = {  1,  99,   2,   3,   7,   4,   6 };
const char*   BTN_LABELS[NUM_BTNS]       = {
  "POWER",
  "SAFE OFF",
  "LOCK",
  "CONST GAS",
  "MANUAL",
  "PIEZO",
  "MODE"
};

// ======================== СВЕТОДИОДЫ ========================
#define PIN_LED_TX    14   // Зелёный: подтверждение отправки
#define PIN_LED_POWER 13   // Синий:   питание пульта

// ======================== OLED ========================
#define OLED_W 128
#define OLED_H  64
Adafruit_SSD1306 oled(OLED_W, OLED_H, &Wire, -1);
bool oledOk = false;

// ======================== NRF24 ========================
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
  uint8_t type;
  float   pressure;
  uint8_t flags;
  uint8_t targetBar;
};
// flags: bit0=sysOn bit1=locked bit2=purging bit3=constGas bit4=manualGas
//        bit5=electro bit6=valve bit7=piezo

// ======================== ТАЙМИНГИ ========================
const unsigned long T_DEBOUNCE      = 50;
const unsigned long T_RF_TIMEOUT    = 3000;   // Связь потеряна если нет ответа 3 сек
const unsigned long T_DISPLAY_UPD   = 200;
const unsigned long T_LED_BLINK     = 60;

// ======================== СОСТОЯНИЕ (от мозга) ========================
float   remPressure   = 0.0f;
uint8_t remFlags      = 0;
uint8_t remTargetBar  = 1;
bool    brainOnline   = false;
unsigned long lastRxTime = 0;

// Флаги из remFlags
bool r_sysOn()    { return remFlags & (1 << 0); }
bool r_locked()   { return remFlags & (1 << 1); }
bool r_purging()  { return remFlags & (1 << 2); }
bool r_constGas() { return remFlags & (1 << 3); }
bool r_manual()   { return remFlags & (1 << 4); }
bool r_electro()  { return remFlags & (1 << 5); }
bool r_valve()    { return remFlags & (1 << 6); }
bool r_piezo()    { return remFlags & (1 << 7); }

// ======================== АНТИДРЕБЕЗГ ========================
bool          btnLastRaw[NUM_BTNS];
bool          btnStable[NUM_BTNS];
unsigned long btnDebTime[NUM_BTNS];

// ======================== ПРОЧЕЕ ========================
char          lastCmdLabel[16] = "---";
unsigned long lastDispUpd      = 0;
unsigned long ledTxOff         = 0;   // Время выключения LED TX

// ======================== ФУНКЦИЯ ОТПРАВКИ КОМАНДЫ ========================
void sendCmd(uint8_t cmd, const char* label) {
  CmdPacket pkt = { 0x55, cmd };

  radio.stopListening();
  bool ok = radio.write(&pkt, sizeof(pkt));
  radio.startListening();

  // Обновляем метку последней команды
  strncpy(lastCmdLabel, label, sizeof(lastCmdLabel) - 1);
  lastCmdLabel[sizeof(lastCmdLabel) - 1] = '\0';

  // LED TX: быстрое мигание = OK, два мигания = FAIL
  if (ok) {
    digitalWrite(PIN_LED_TX, HIGH);
    ledTxOff = millis() + T_LED_BLINK;
  } else {
    // Два коротких — отказ
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

// ======================== OLED ДИСПЛЕЙ ========================
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
  oled.setCursor(104, 0);
  oled.print(brainOnline ? "OK " : "-- ");

  oled.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  // === Давление + онлайн-статус ===
  oled.setTextSize(2);
  oled.setCursor(0, 12);
  if (brainOnline) {
    char pBuf[8];
    dtostrf(remPressure, 4, 2, pBuf);
    oled.print(pBuf);
    oled.setTextSize(1);
    oled.setCursor(90, 18);
    oled.print("bar");
  } else {
    oled.setTextSize(1);
    oled.setCursor(0, 12);
    oled.print("МОЗГ НЕ");
    oled.setCursor(0, 22);
    oled.print("ОТВЕЧАЕТ");
  }

  // Полоска давления
  if (brainOnline) {
    int bw = (int)((remPressure / 10.0f) * 127.0f);
    bw = constrain(bw, 0, 127);
    oled.drawRect(0, 30, 128, 5, SSD1306_WHITE);
    if (bw > 0) oled.fillRect(1, 31, bw, 3, SSD1306_WHITE);
  }

  oled.drawLine(0, 36, 127, 36, SSD1306_WHITE);

  // === Статус системы ===
  oled.setTextSize(1);
  oled.setCursor(0, 39);
  if (brainOnline) {
    oled.print(r_sysOn() ? "ON  " : "OFF ");
    oled.print(r_locked() ? "LOCK " : "UNLK ");
    oled.print("E:");
    oled.print(r_electro() ? "1" : "0");
    oled.print(" V:");
    oled.print(r_valve() ? "1" : "0");
  } else {
    oled.print("НАЖМИ КНОПКУ...");
  }

  // === Режим / последняя команда ===
  oled.setCursor(0, 49);
  if (!brainOnline) {
    oled.print("НЕТ СВЯЗИ");
  } else if (r_purging()) {
    oled.print(">>> ПРОДУВКА <<<");
  } else if (!r_sysOn()) {
    oled.print("ВЫКЛ | РЕЖИМ:");
    oled.print(remTargetBar);
    oled.print("b");
  } else if (r_constGas()) {
    oled.print("ПОСТ.ГАЗ ");
    oled.print(remTargetBar);
    oled.print("b");
  } else if (r_manual()) {
    oled.print("РУЧНАЯ ПОДАЧА");
  } else if (r_locked()) {
    oled.print("ЗАБЛОКИРОВАНО");
  } else {
    oled.print("АВТО ");
    oled.print(remTargetBar);
    oled.print("b  >");
    oled.print(lastCmdLabel);
  }

  oled.display();
}

// ======================== SETUP ========================
void setup() {
  Serial.begin(115200);
  Serial.println("\n--- H2 Control Remote v3.0 ---");

  // Кнопки
  for (uint8_t i = 0; i < NUM_BTNS; i++) {
    pinMode(BTN_PINS[i], INPUT_PULLUP);
    btnLastRaw[i] = HIGH;
    btnStable[i]  = HIGH;
    btnDebTime[i] = 0;
  }

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
    oled.setCursor(18, 24);
    oled.print("H2 REMOTE v3.0");
    oled.display();
    Serial.println("OLED: OK");
  }

  // NRF24L01
  SPI.begin(18, 19, 23, PIN_NRF_CSN);
  if (!radio.begin()) {
    Serial.println("NRF24: FAIL! Проверь подключение и 3.3V!");
    // 3 мигания — ошибка
    for (int i = 0; i < 3; i++) {
      digitalWrite(PIN_LED_TX, HIGH); delay(200);
      digitalWrite(PIN_LED_TX, LOW);  delay(200);
    }
  } else {
    radio.openWritingPipe(PIPE_R2B);       // Команды → мозгу
    radio.openReadingPipe(1, PIPE_B2R);    // Статус ← от мозга
    radio.setPALevel(RF24_PA_MAX);
    radio.setDataRate(RF24_250KBPS);
    radio.setCRCLength(RF24_CRC_16);
    radio.startListening();
    Serial.println("NRF24: OK");
    // 2 быстрых мигания — готов
    for (int i = 0; i < 2; i++) {
      digitalWrite(PIN_LED_TX, HIGH); delay(80);
      digitalWrite(PIN_LED_TX, LOW);  delay(80);
    }
  }

  Serial.println("Пульт готов.");
  for (uint8_t i = 0; i < NUM_BTNS; i++) {
    Serial.print("  GPIO"); Serial.print(BTN_PINS[i]);
    Serial.print(" -> "); Serial.println(BTN_LABELS[i]);
  }
}

// ======================== LOOP ========================
void loop() {
  unsigned long now = millis();

  // ---- Приём статуса от мозга ----
  if (radio.available()) {
    StatusPacket pkt;
    radio.read(&pkt, sizeof(pkt));
    if (pkt.type == 0xAA) {
      remPressure  = pkt.pressure;
      remFlags     = pkt.flags;
      remTargetBar = pkt.targetBar;
      lastRxTime   = now;
    }
  }
  brainOnline = (now - lastRxTime < T_RF_TIMEOUT) && (lastRxTime > 0);

  // ---- Кнопки с антидребезгом ----
  for (uint8_t i = 0; i < NUM_BTNS; i++) {
    bool raw = digitalRead(BTN_PINS[i]);
    if (raw != btnLastRaw[i]) btnDebTime[i] = now;
    if ((now - btnDebTime[i]) > T_DEBOUNCE && raw != btnStable[i]) {
      btnStable[i] = raw;
      if (btnStable[i] == LOW) {  // Фронт нажатия
        sendCmd(BTN_CMDS[i], BTN_LABELS[i]);
      }
    }
    btnLastRaw[i] = raw;
  }

  // ---- Выключение LED TX ----
  if (ledTxOff > 0 && now >= ledTxOff) {
    digitalWrite(PIN_LED_TX, LOW);
    ledTxOff = 0;
  }

  // ---- Обновление дисплея ----
  if (oledOk && (now - lastDispUpd >= T_DISPLAY_UPD)) {
    updateDisplay();
    lastDispUpd = now;
  }
}
