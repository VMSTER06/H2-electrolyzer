#ifndef H2_REMOTE_GAME_H
#define H2_REMOTE_GAME_H

#include <stdint.h>

// Only the melody button enters this local UI. No game action sends RF commands.
class MelodyClicks {
 public:
  enum Event { NONE, SINGLE, DOUBLE };
  static const uint32_t WINDOW_MS = 400;
  static const uint32_t EXIT_QUIET_MS = 800;

  Event poll(uint32_t now) {
    if (suppressed(now)) return NONE;
    if (!pending_ || uint32_t(now - pressedAt_) <= WINDOW_MS) return NONE;
    pending_ = false;
    return deferred_ ? SINGLE : NONE;
  }
  // Caller polls before press(). In-game singles are immediate (jump/restart).
  Event press(uint32_t now, bool deferSingle) {
    if (suppressed(now)) {
      suppressedAt_ = now;  // Swallow the whole trailing click burst after exit.
      return NONE;
    }
    if (pending_ && uint32_t(now - pressedAt_) <= WINDOW_MS) {
      pending_ = false;
      return DOUBLE;
    }
    pending_ = true;
    pressedAt_ = now;
    deferred_ = deferSingle;
    return deferSingle ? NONE : SINGLE;
  }
  void cancel() { pending_ = false; }
  void suppressAfterExit(uint32_t now) {
    pending_ = false;
    suppress_ = true;
    suppressedAt_ = now;
  }

 private:
  bool pending_ = false;
  bool deferred_ = false;
  uint32_t pressedAt_ = 0;
  bool suppress_ = false;
  uint32_t suppressedAt_ = 0;
  bool suppressed(uint32_t now) {
    if (suppress_ && uint32_t(now - suppressedAt_) >= EXIT_QUIET_MS) suppress_ = false;
    return suppress_;
  }
};

class RemoteDino {
 public:
  static const int GROUND = 49;
  static const int PLAYER_X = 14;
  static const int PLAYER_W = 10;
  static const int PLAYER_H = 12;
  bool active = false;
  bool dead = false;
  uint16_t score = 0;
  uint16_t best = 0;  // Loaded from NVS; committed on game over, exit or sleep.
  int16_t obstacleX = 128;
  uint8_t obstacleW = 6;
  uint8_t obstacleH = 12;

  void start(uint32_t now) {
    active = true;
    dead = false;
    score = 0;
    y10_ = velocity10_ = 0;
    ticks_ = 0;
    obstacleX = 128;
    obstacleW = 6;
    obstacleH = 12;
    seed_ = now ^ 0x72e4a93bUL;
    lastStep_ = now;
  }
  void stop() { active = false; }
  void jump(uint32_t now) {
    if (!active) return;
    if (dead) start(now);
    if (y10_ == 0) velocity10_ = 33;
  }
  int height() const { return y10_ / 10; }
  static const uint32_t STEP_MS = 20;
  static const uint32_t MAX_CATCHUP = 4;  // Longer gap = pause: one step, no burst.

  // Fixed 20 ms steps: a slow loop (OLED redraw) is caught up, so speed does not
  // depend on display time. Work per call stays bounded.
  void update(uint32_t now) {
    if (!active || dead) return;
    uint32_t due = uint32_t(now - lastStep_) / STEP_MS;
    if (due == 0) return;
    if (due > MAX_CATCHUP) {
      lastStep_ = now;
      step();
      return;
    }
    lastStep_ += due * STEP_MS;
    while (due-- && !dead) step();
  }

 private:
  void step() {
    if (velocity10_ || y10_) {
      y10_ += velocity10_;
      velocity10_ -= 3;
      if (y10_ <= 0) y10_ = velocity10_ = 0;
    }
    obstacleX -= score < 150 ? 2 : 3;
    if (obstacleX + obstacleW < 0) {
      seed_ = seed_ * 1664525UL + 1013904223UL;
      obstacleX = 128 + (seed_ >> 16) % 48;
      obstacleW = 6 + (seed_ >> 8) % 4;
      obstacleH = 10 + seed_ % 7;
    }
    if (PLAYER_X + PLAYER_W > obstacleX &&
        PLAYER_X < obstacleX + obstacleW && height() < obstacleH) {
      dead = true;
    }
    if (++ticks_ == 5) {
      ticks_ = 0;
      if (!dead && score < 9999) ++score;
    }
    if (score > best) best = score;
  }

  int16_t y10_ = 0;
  int16_t velocity10_ = 0;
  uint8_t ticks_ = 0;
  uint32_t lastStep_ = 0;
  uint32_t seed_ = 1;
};

// Storage is ESP32 Preferences in firmware; tests use a persistent fake backend.
class DinoBestRecord {
 public:
  uint16_t saved() const { return saved_; }
  template<class Storage> bool load(Storage& storage, uint16_t& best) {
    if (!storage.begin("h2-dino", false)) return false;
    const uint16_t value = storage.getUShort("best", 0);
    storage.end();
    saved_ = value <= 9999 ? value : 0;
    best = saved_;
    return true;
  }
  template<class Storage> bool save(Storage& storage, uint16_t best) {
    if (best <= saved_ || best > 9999) return true;
    if (!storage.begin("h2-dino", false)) return false;
    const bool ok = storage.putUShort("best", best) == sizeof(uint16_t);
    storage.end();
    if (ok) saved_ = best;
    return ok;
  }

 private:
  uint16_t saved_ = 0;
};

#endif
