#ifndef H2_REMOTE_GAME_H
#define H2_REMOTE_GAME_H

#include <stdint.h>

// Only the melody button enters this local UI. No game action sends RF commands.
class MelodyClicks {
 public:
  enum Event { NONE, SINGLE, DOUBLE };
  static const uint32_t WINDOW_MS = 400;

  Event poll(uint32_t now) {
    if (!pending_ || uint32_t(now - pressedAt_) <= WINDOW_MS) return NONE;
    pending_ = false;
    return deferred_ ? SINGLE : NONE;
  }
  // Caller polls before press(). In-game singles are immediate (jump/restart).
  Event press(uint32_t now, bool deferSingle) {
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

 private:
  bool pending_ = false;
  bool deferred_ = false;
  uint32_t pressedAt_ = 0;
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
  uint16_t best = 0;  // RAM only: no flash writes while playing.
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
  void update(uint32_t now) {
    if (!active || dead || uint32_t(now - lastStep_) < 20) return;
    // At most one bounded step per loop, including after a long pause.
    lastStep_ = now;
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

 private:
  int16_t y10_ = 0;
  int16_t velocity10_ = 0;
  uint8_t ticks_ = 0;
  uint32_t lastStep_ = 0;
  uint32_t seed_ = 1;
};

#endif
