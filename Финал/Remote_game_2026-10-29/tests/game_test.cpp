#include "../../H2_Remote_v5/remote_game.h"
#define CHECK(condition) do { if (!(condition)) return __LINE__; } while (0)

extern "C" int test_clicks() {
  MelodyClicks c;
  CHECK(c.poll(0) == MelodyClicks::NONE);
  CHECK(c.press(0, true) == MelodyClicks::NONE);
  CHECK(c.poll(400) == MelodyClicks::NONE);
  CHECK(c.poll(401) == MelodyClicks::SINGLE);
  CHECK(c.poll(402) == MelodyClicks::NONE);
  c.press(1000, true);
  CHECK(c.press(1400, true) == MelodyClicks::DOUBLE);
  CHECK(c.poll(2000) == MelodyClicks::NONE);
  c.press(2200, true);
  c.cancel();
  CHECK(c.poll(3000) == MelodyClicks::NONE);
  CHECK(c.press(3100, false) == MelodyClicks::SINGLE);
  CHECK(c.poll(3501) == MelodyClicks::NONE);
  CHECK(c.press(4000, false) == MelodyClicks::SINGLE);
  CHECK(c.press(4200, false) == MelodyClicks::DOUBLE);
  c.press(0xffffff00UL, true);
  CHECK(c.press(0x80, true) == MelodyClicks::DOUBLE);
  c.press(0xffffff00UL, true);
  CHECK(c.poll(0x100) == MelodyClicks::SINGLE);
  c.suppressAfterExit(1000);
  CHECK(c.press(1100,true)==MelodyClicks::NONE);
  CHECK(c.press(1600,true)==MelodyClicks::NONE);
  CHECK(c.poll(2001)==MelodyClicks::NONE);
  CHECK(c.poll(2400)==MelodyClicks::NONE);
  CHECK(c.press(2500,true)==MelodyClicks::NONE);
  CHECK(c.poll(2901)==MelodyClicks::SINGLE);
  c.suppressAfterExit(0xffffff00UL);
  CHECK(c.press(0x100,true)==MelodyClicks::NONE);
  CHECK(c.poll(0x300)==MelodyClicks::NONE);
  CHECK(c.press(0x500,true)==MelodyClicks::NONE);
  CHECK(c.poll(0x700)==MelodyClicks::SINGLE);
  return 0;
}

struct FakeNVS {
  uint16_t value=0;
  int writes=0,opens=0,closes=0;
  bool failOpen=false,failWrite=false;
  bool begin(const char*,bool) { ++opens; return !failOpen; }
  uint16_t getUShort(const char*,uint16_t) { return value; }
  unsigned putUShort(const char*,uint16_t v) {
    ++writes;
    if(failWrite) return 0;
    value=v; return 2;
  }
  void end() { ++closes; }
};

extern "C" int test_persistent_record() {
  FakeNVS nvs;
  DinoBestRecord first;
  uint16_t best=100;
  CHECK(first.load(nvs,best) && best==0 && first.saved()==0);
  CHECK(first.save(nvs,123) && nvs.writes==1 && nvs.value==123);
  const int opens=nvs.opens;
  CHECK(first.save(nvs,123) && first.save(nvs,100));
  CHECK(nvs.writes==1 && nvs.opens==opens);
  // New firmware object after reboot sees the committed record.
  DinoBestRecord reboot;
  CHECK(reboot.load(nvs,best) && best==123);
  nvs.failWrite=true;
  CHECK(!reboot.save(nvs,200) && reboot.saved()==123 && nvs.value==123);
  nvs.failWrite=false;
  CHECK(reboot.save(nvs,200) && reboot.saved()==200 && nvs.value==200);
  nvs.failOpen=true;
  CHECK(!reboot.save(nvs,300) && reboot.saved()==200);
  nvs.failOpen=false;
  CHECK(reboot.save(nvs,300) && nvs.value==300);
  nvs.value=65535;
  DinoBestRecord bad;
  CHECK(bad.load(nvs,best) && best==0);
  CHECK(bad.save(nvs,42) && nvs.value==42);
  return 0;
}

extern "C" int test_dino() {
  RemoteDino d;
  d.jump(0); CHECK(!d.active);
  d.start(1000); CHECK(d.active && !d.dead && d.height() == 0);
  d.jump(1000);
  d.update(1019); CHECK(d.height() == 0);
  d.update(1020); CHECK(d.height() > 0);
  for (uint32_t t = 1040; t <= 1460; t += 20) d.update(t);
  CHECK(d.height() == 0 && !d.dead);
  for (uint32_t t = 1480; t <= 2500; t += 20) d.update(t);
  CHECK(d.dead && d.best > 0);
  const uint16_t best = d.best;
  d.jump(3000); CHECK(!d.dead && d.score == 0 && d.best == best);
  d.stop(); CHECK(!d.active);
  const int16_t oldX = d.obstacleX;
  d.update(4000); CHECK(d.obstacleX == oldX);
  // One step only after a long pause: no unbounded catch-up loop.
  d.start(5000); d.update(1000000); CHECK(d.obstacleX == 126);
  d.start(0xfffffff0UL); d.update(4); CHECK(d.obstacleX == 126);
  // A slow loop (display redraw) is caught up: 60 ms = 3 steps, remainder kept.
  d.start(1000); d.update(1060); CHECK(d.obstacleX == 122);
  d.update(1070); CHECK(d.obstacleX == 122);
  d.update(1090); CHECK(d.obstacleX == 120);
  // A correctly timed jump clears a cactus and returns to the ground.
  d.start(0);
  for (uint32_t t = 20; t <= 900; t += 20) d.update(t);
  d.jump(900);
  for (uint32_t t = 920; t <= 1380; t += 20) d.update(t);
  CHECK(!d.dead && d.height() == 0);
  return 0;
}
