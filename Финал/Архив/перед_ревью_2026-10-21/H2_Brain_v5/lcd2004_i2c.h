// =====================================================================================
//  LCD 2004 (HD44780) через I²C-переходник PCF8574 — минимальный драйвер
// -------------------------------------------------------------------------------------
//  Написан вместо библиотеки LiquidCrystal_I2C по двум причинам:
//    1) та библиотека объявляет architectures=avr, и arduino-cli на ESP32 обрабатывает
//       её нештатно;
//    2) её файлы в OneDrive оказались облачными заглушками, которые не читаются, а
//       сервер библиотек отдаёт 403 — сборка от этого не должна зависеть.
//
//  Протокол: 4-битный режим HD44780. Раскладка бит переходника — самая ходовая
//  (99% модулей на PCF8574/PCF8574A):
//      P0 = RS   P1 = RW   P2 = EN   P3 = подсветка   P4..P7 = D4..D7
//  Если у вас редкий переходник с иной раскладкой — на экране будет мусор; тогда
//  поменяйте константы LCD_RS/LCD_RW/LCD_EN/LCD_BL ниже.
//
//  Адрес: 0x27 (PCF8574) или 0x3F (PCF8574A). Подбирается сканом шины в setup().
//  Совместимо с Wire, уже инициализированной в скетче (SDA=21, SCL=22).
// =====================================================================================
#pragma once
#include <Arduino.h>
#include <Wire.h>

class LiquidCrystal_I2C {
public:
  LiquidCrystal_I2C(uint8_t addr, uint8_t cols, uint8_t rows)
    : _addr(addr), _cols(cols), _rows(rows), _bl(LCD_BL) {}

  // Полная инициализация по даташиту HD44780 (последовательность входа в 4-бит).
  void init() {
    delay(50);                       // после подачи питания контроллеру нужно >40 мс
    expanderWrite(_bl);              // всё в 0, подсветка по текущему состоянию
    delay(1000);
    // Трижды 0x03 — принудительный сброс в 8-битный режим, затем 0x02 — в 4-битный.
    write4bits(0x03 << 4); delayMicroseconds(4500);
    write4bits(0x03 << 4); delayMicroseconds(4500);
    write4bits(0x03 << 4); delayMicroseconds(150);
    write4bits(0x02 << 4);
    command(0x28);                   // Function set: 4 бита, 2 строки, 5×8
    command(0x0C);                   // Display ON, курсор OFF, мигание OFF
    clear();
    command(0x06);                   // Entry mode: инкремент, без сдвига
    home();
  }

  void clear()        { command(0x01); delayMicroseconds(2000); }
  void home()         { command(0x02); delayMicroseconds(2000); }
  void backlight()    { _bl = LCD_BL; expanderWrite(0); }
  void noBacklight()  { _bl = 0;      expanderWrite(0); }
  void display()      { command(0x0C); }
  void noDisplay()    { command(0x08); }

  void setCursor(uint8_t col, uint8_t row) {
    static const uint8_t offs[] = { 0x00, 0x40, 0x14, 0x54 };   // 20×4
    if (row >= _rows) row = _rows - 1;
    command(0x80 | (col + offs[row]));
  }

  size_t print(const char* s) {
    size_t n = 0;
    while (*s) { write((uint8_t)*s++); n++; }
    return n;
  }
  size_t print(char c) { write((uint8_t)c); return 1; }

  // Символ 0xDF в ROM HD44780 — знак градуса; 0xFF — заполненная клетка (для шкал).
  void write(uint8_t value) { send(value, LCD_RS); }

private:
  static const uint8_t LCD_RS = 0x01;   // P0
  static const uint8_t LCD_RW = 0x02;   // P1 (не используем, всегда запись)
  static const uint8_t LCD_EN = 0x04;   // P2
  static const uint8_t LCD_BL = 0x08;   // P3 подсветка

  uint8_t _addr, _cols, _rows, _bl;

  void command(uint8_t value) { send(value, 0); }

  // Байт данных/команды — ОДНОЙ транзакцией I2C (6 байт: для каждого полубайта
  // «данные, данные+EN↑, данные+EN↓»). Раньше было 6 отдельных транзакций ≈ 2 мс на
  // знак, и полная смена экрана тормозила loop (давление, репульсор) на ~160 мс;
  // теперь ≈ 0,6 мс на знак. Длительность байта на 100 кГц (~90 мкс) с запасом
  // перекрывает требования HD44780 к ширине EN (>450 нс) и установке данных.
  void send(uint8_t value, uint8_t mode) {
    uint8_t hi = (value & 0xF0) | mode | _bl;
    uint8_t lo = ((value << 4) & 0xF0) | mode | _bl;
    Wire.beginTransmission(_addr);
    Wire.write(hi); Wire.write(hi | LCD_EN); Wire.write(hi & ~LCD_EN);
    Wire.write(lo); Wire.write(lo | LCD_EN); Wire.write(lo & ~LCD_EN);
    Wire.endTransmission();
    delayMicroseconds(40);           // выполнение команды/записи знака: >37 мкс
  }

  void write4bits(uint8_t data) {
    expanderWrite(data);
    pulseEnable(data);
  }

  void pulseEnable(uint8_t data) {
    expanderWrite(data | LCD_EN);  delayMicroseconds(1);    // EN ↑ (нужно >450 нс)
    expanderWrite(data & ~LCD_EN); delayMicroseconds(50);   // EN ↓, команде нужно >37 мкс
  }

  void expanderWrite(uint8_t data) {
    Wire.beginTransmission(_addr);
    Wire.write(data | _bl);
    Wire.endTransmission();
  }
};
