// =====================================================================================
//  ⚡ v6.8: ПУЛЬТ — заряд батареи и доля принятых пакетов (без Arduino, тестируется на ПК)
// =====================================================================================
#pragma once
#include <stdint.h>

// Заряд Li-ion / LiPo 1S по напряжению покоя, % (кусочно-линейная кривая разряда).
// Под нагрузкой (передача NRF24) напряжение ниже — поэтому замер сглаживается.
inline uint8_t liionPercent(uint16_t mv) {
  static const uint16_t V[] = { 3300, 3500, 3600, 3700, 3750, 3800, 3900, 4000, 4100, 4200 };
  static const uint8_t  P[] = {    0,    5,   10,   20,   30,   45,   60,   75,   90,  100 };
  const uint8_t n = sizeof(V) / sizeof(V[0]);
  if (mv <= V[0])     return 0;
  if (mv >= V[n - 1]) return 100;
  for (uint8_t i = 1; i < n; i++)
    if (mv <= V[i]) return (uint8_t)(P[i - 1] + (uint32_t)(P[i] - P[i - 1]) * (mv - V[i - 1]) / (V[i] - V[i - 1]));
  return 100;
}

// Похоже ли напряжение на подключённый аккумулятор (иначе — вход без делителя / USB)
inline bool liionPresent(uint16_t mv) { return mv >= 2800 && mv <= 4500; }

// Доля принятых пакетов статуса мозга (10 Гц) за последние RX_SECS полных секунд.
// Секунды без единого пакета тоже считаются (0 %), текущая неполная — нет.
class RxMeter {
 public:
  static const uint8_t  RX_SECS = 5;
  static const uint8_t  PER_SEC = 10;   // мозг шлёт статус каждые 100 мс

  void onPacket(uint32_t now) { roll(now); if (cnt_[slot(now)] < 255) cnt_[slot(now)]++; }
  // 0..100; нет ни одной полной секунды наблюдения — 0
  uint8_t pct(uint32_t now) {
    roll(now);
    uint8_t full = seen_ < RX_SECS ? seen_ : RX_SECS - 1;   // полных секунд в окне (без текущей)
    if (full == 0) return 0;
    uint16_t sum = 0;
    uint32_t sec = now / 1000;
    for (uint8_t k = 1; k <= full; k++) sum += cnt_[(sec - k) % RX_SECS];
    uint16_t p = (uint32_t)sum * 100 / ((uint16_t)full * PER_SEC);
    return p > 100 ? 100 : (uint8_t)p;
  }
  void reset(uint32_t now) { for (uint8_t i = 0; i < RX_SECS; i++) cnt_[i] = 0; sec_ = now / 1000; seen_ = 0; started_ = true; }

 private:
  uint8_t slot(uint32_t now) const { return (now / 1000) % RX_SECS; }
  // Новые секунды: обнулить их ячейки (не больше RX_SECS)
  void roll(uint32_t now) {
    uint32_t sec = now / 1000;
    if (!started_) { reset(now); return; }
    uint32_t d = sec - sec_;
    if (d == 0) return;
    for (uint32_t k = 1; k <= d && k <= RX_SECS; k++) cnt_[(sec_ + k) % RX_SECS] = 0;
    seen_ = (seen_ + d > 250) ? 250 : (uint8_t)(seen_ + d);
    sec_ = sec;
  }
  uint8_t  cnt_[RX_SECS] = {};
  uint32_t sec_ = 0;
  uint8_t  seen_ = 0;
  bool     started_ = false;
};

// Время до сна (меню и приложение): значения по кругу, 0 — никогда
const uint8_t SLEEP_CHOICES[] = { 1, 2, 5, 10, 30, 0 };
inline uint8_t nextSleepChoice(uint8_t cur) {
  const uint8_t n = sizeof(SLEEP_CHOICES);
  for (uint8_t i = 0; i < n; i++) if (SLEEP_CHOICES[i] == cur) return SLEEP_CHOICES[(i + 1) % n];
  return SLEEP_CHOICES[1];
}
