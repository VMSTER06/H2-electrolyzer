// Тесты remote_ui.h: заряд Li-ion, доля принятых пакетов, выбор времени сна
#include <cstdio>
#include "../H2_Remote_v5/remote_ui.h"
static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
int main() {
  CHECK(liionPercent(4200) == 100 && liionPercent(4300) == 100);
  CHECK(liionPercent(3300) == 0 && liionPercent(3000) == 0);
  CHECK(liionPercent(3700) == 20 && liionPercent(3850) == 52);
  for (uint16_t mv = 3300; mv < 4200; mv += 10) CHECK(liionPercent(mv) <= liionPercent(mv + 10));
  CHECK(liionPresent(3700) && !liionPresent(0) && !liionPresent(5000));

  RxMeter m;
  CHECK(m.pct(0) == 0);
  for (uint32_t t = 0; t < 6000; t += 100) m.onPacket(t);      // 10 Гц, все дошли
  CHECK(m.pct(6000) == 100);
  for (uint32_t t = 6000; t < 11000; t += 200) m.onPacket(t);  // 5 Гц — половина
  CHECK(m.pct(11000) == 50);
  CHECK(m.pct(20000) == 0);                                    // тишина 9 с
  RxMeter q; q.reset(0);
  for (uint32_t t = 0; t < 2000; t += 100) q.onPacket(t);      // 2 полные секунды
  CHECK(q.pct(2050) == 100);
  RxMeter w; w.reset(0xFFFFF000u);                             // переполнение millis
  for (uint32_t t = 0xFFFFF000u, k = 0; k < 80; t += 100, k++) w.onPacket(t);
  CHECK(w.pct(0xFFFFF000u + 8000) >= 95);

  CHECK(nextSleepChoice(2) == 5 && nextSleepChoice(0) == 1 && nextSleepChoice(7) == 2);
  printf(fails ? "ui_test: %d FAIL\n" : "ui_test: PASS\n", fails);
  return fails ? 1 : 0;
}
