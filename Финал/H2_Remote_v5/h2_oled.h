// =====================================================================================
//  ⚡ v6.6: OLED 128×64 на GyverOLED с русским шрифтом (вместо Adafruit SSD1306 + GFX)
// -------------------------------------------------------------------------------------
//  Тонкий переходник: тот же набор вызовов, что раньше давал Adafruit_SSD1306
//  (begin / clearDisplay / display / setCursor в пикселях / setTextSize / setTextColor /
//  print / printf / drawLine / drawRect / fillRect / drawPixel / ssd1306_command), поэтому
//  код экранов мозга и пульта почти не меняется. Строки печатаются в UTF-8 — русские
//  буквы рисует шрифт GyverOLED. Байт 0xF8 (знак градуса из CP437, как было в старом
//  коде) рисуется отдельным глифом.
//  ИДЕНТИЧНАЯ копия лежит в H2_Brain_v5 и H2_Remote_v5 — править обе.
// =====================================================================================
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <GyverOLED.h>

#ifndef SSD1306_WHITE
#define SSD1306_BLACK          0
#define SSD1306_WHITE          1
#define SSD1306_SWITCHCAPVCC   0x02
#define SSD1306_DISPLAYOFF     0xAE
#define SSD1306_DISPLAYON      0xAF
#endif

class H2Oled : public Print {
 public:
  explicit H2Oled(uint8_t addr = 0x3C) : _addr(addr), _d(addr) {}

  // Есть ли дисплей на шине (I²C уже запущен в setup) — затем инициализация
  bool begin(uint8_t = SSD1306_SWITCHCAPVCC, uint8_t addr = 0) {
    if (addr) _addr = addr;
    Wire.beginTransmission(_addr);
    if (Wire.endTransmission() != 0) return false;
    _d.init();
    _d.clear();
    _d.update();
    return true;
  }

  void clearDisplay() { _d.clear(); setCursor(0, 0); }
  void display()      { _d.update(); }
  void cp437(bool = true) {}

  void setCursor(int16_t x, int16_t y) { _x = x; _y = y; _d.setCursorXY(x, y); }
  void setTextSize(uint8_t s) { _s = constrain(s, 1, 4); _d.setScale(_s); _d.setCursorXY(_x, _y); }
  void setTextColor(uint16_t c) { _d.invertText(c == SSD1306_BLACK); }
  void setTextColor(uint16_t c, uint16_t) { setTextColor(c); }

  void drawPixel(int16_t x, int16_t y, uint16_t c) { _d.dot(x, y, c ? 1 : 0); restore(); }
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t c) {
    _d.line(x0, y0, x1, y1, c ? 1 : 0); restore();
  }
  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
    if (w <= 0 || h <= 0) return;
    uint8_t f = c ? 1 : 0;
    _d.fastLineH(y, x, x + w - 1, f);
    _d.fastLineH(y + h - 1, x, x + w - 1, f);
    _d.fastLineV(x, y, y + h - 1, f);
    _d.fastLineV(x + w - 1, y, y + h - 1, f);
    restore();
  }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
    if (w <= 0 || h <= 0) return;
    _d.rect(x, y, x + w - 1, y + h - 1, c ? OLED_FILL : OLED_CLEAR);
    restore();
  }
  void ssd1306_command(uint8_t cmd) {
    if (cmd == SSD1306_DISPLAYOFF)     _d.setPower(false);
    else if (cmd == SSD1306_DISPLAYON) _d.setPower(true);
    else                               _d.sendCommand(cmd);
  }

  // Печать: UTF-8 передаём шрифту GyverOLED, сами только ведём курсор (графика
  // GyverOLED сбрасывает его внутренний курсор, поэтому после неё он восстанавливается)
  size_t write(uint8_t b) override {
    if (b == 0xF8) { degree(); return 1; }
    if (b == '\n') { _x = 0; _y += 8 * _s; _d.setCursorXY(_x, _y); return 1; }
    if (b == '\r') { _x = 0; _d.setCursorXY(_x, _y); return 1; }
    _d.write(b);
    if (b <= 191) _x += 6 * _s;     // ведущие байты UTF-8 (≥0xC0) места не занимают
    return 1;
  }
  using Print::write;

 private:
  uint8_t _addr;
  GyverOLED<SSD1306_128x64, OLED_BUFFER> _d;
  int16_t _x = 0, _y = 0;
  uint8_t _s = 1;

  void restore() { _d.setCursorXY(_x, _y); }

  // Знак градуса: кружок 3×3 (×масштаб) в верхней части знакоместа 6×8
  void degree() {
    static const uint8_t pts[8][2] = {{1,0},{2,0},{0,1},{3,1},{0,2},{3,2},{1,3},{2,3}};
    _d.rect(_x, _y, _x + 6 * _s - 1, _y + 8 * _s - 1, OLED_CLEAR);
    for (auto& p : pts)
      for (uint8_t i = 0; i < _s; i++)
        for (uint8_t j = 0; j < _s; j++) _d.dot(_x + (p[0] + 1) * _s + i, _y + p[1] * _s + j, 1);
    _x += 6 * _s;
    restore();
  }
};
