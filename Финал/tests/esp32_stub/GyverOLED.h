// Заглушка GyverOLED для проверки синтаксиса прошивок на ПК (API, которым пользуется h2_oled.h)
#pragma once
#include <Arduino.h>
#define SSD1306_128x64 1
#define OLED_BUFFER 1
#define OLED_CLEAR 0
#define OLED_FILL 1
#define BUF_ADD 0
#define BUF_SUBTRACT 1
#define BUF_REPLACE 2
template<int T, int B> struct GyverOLED {
  explicit GyverOLED(uint8_t) {}
  void init() {} void clear() {} void update() {} void setCursorXY(int, int) {} void setScale(uint8_t) {}
  void invertText(bool) {} void textMode(uint8_t) {} void dot(int, int, uint8_t = 1) {} void line(int, int, int, int, uint8_t = 1) {}
  void fastLineH(int, int, int, uint8_t = 1) {} void fastLineV(int, int, int, uint8_t = 1) {} void rect(int, int, int, int, uint8_t = 1) {}
  void setPower(bool) {} void sendCommand(uint8_t) {} size_t write(uint8_t) { return 1; }
};
