#include "../../H2_Remote_v5/remote_game.h"
char* strncpy(char* d,const char* s,unsigned n){unsigned i=0;for(;i<n&&s[i];++i)d[i]=s[i];for(;i<n;++i)d[i]=0;return d;}

namespace old {

uint32_t clockMs = 0;
uint32_t millis() { return clockMs; }
const bool HIGH=true, LOW=false;
bool pins[64];
bool digitalRead(int pin) { return pins[pin]; }
const uint8_t NUM_SIMPLE_BTNS=7;
const int SIMPLE_BTN_PINS[]={17,25,26,27,32,33,34};
const uint8_t SIMPLE_BTN_CMDS[]={2,3,7,4,8,9,11};
const char* SIMPLE_BTN_LABELS[]={"LOCK","REP","MAN","FIRE","UP","DOWN","MUSIC"};
const int PIN_BTN_POWER=16, PIN_BTN_LOCK=17;
const uint32_t T_DEBOUNCE=50,T_DOUBLE_CLICK=400,T_LONG_PRESS=1000,T_DISPLAY_UPD=200;
const uint32_t T_LED_BLINK=60,T_LED_FAIL_ON=80,T_LED_FAIL_OFF=60;
const uint8_t PKT_CMD=0x55,CMD_SILENCE_ALARM=12;
struct CmdPacket { uint8_t type,cmd; };
int sent[64], sentCount=0;
struct Radio {
 void stopListening() {}
 bool write(const void* packet, unsigned) { sent[sentCount++]=((const CmdPacket*)packet)->cmd; return true; }
 void startListening() {}
} radio;
struct SerialStub { void println(const char*) {} template<class... T> void printf(const char*,T...) {} } Serial;
bool simpleLastRaw[7],simpleStable[7];
uint32_t simpleDebTime[7];
bool pwrLastRaw=HIGH,pwrStable=HIGH;
uint32_t pwrDebTime=0,pwrLastClick=0,pwrPressMs=0;
bool pwrLongSent=false,pwrPurgeSent=false;
uint32_t lockPressMs=0;
bool lockLongSent=false;
uint32_t lastActivityMs=0,lastDispCheck=0,lastDispDraw=0,lastGameDraw=0;
char lastCmdLabel[16];
bool boomerPlaying=false,oledOk=true,brainOnline=false,alarm=false;
int toggles=0,stops=0;
RemoteDino dino;
MelodyClicks melodyClicks;
void toggleBoomer(uint32_t) { boomerPlaying=!boomerPlaying; ++toggles; }
const int MEL_BOOMER[]={1};
const int* sndSeq=nullptr;
void soundStop() { ++stops; sndSeq=nullptr; }
bool s_clicks() { return false; }
const int MEL_CLICK=0,MEL_CLICK_LEN=1,SPRIO_CLICK=0;
void soundPlayLocal(int,int,int) {}
void ledTxBlink(int,uint32_t,uint32_t) {}
void leaveGame(unsigned long);
bool r_tiltLock() { return alarm; }
bool r_ovLock() { return alarm; }
bool r_psFault() { return alarm; }
bool r_leakLock() { return alarm; }
bool r_overheat() { return alarm; }
bool r_watchdog() { return alarm; }
bool r_ovWarn() { return alarm; }
bool r_leakWarn() { return alarm; }
bool r_tempFault() { return alarm; }
bool r_tiltFault() { return alarm; }
bool r_tiltCal() { return alarm; }
bool r_purging() { return alarm; }
bool r_purgeShort() { return alarm; }
bool r_repulsor() { return alarm; }
bool r_manual() { return alarm; }
bool r_piezo() { return alarm; }
void sendCmd(uint8_t cmd, const char* label) {
  lastActivityMs = millis();   // ⚡ v5.0: любое нажатие сбрасывает таймер сна
  // ⚡ v6.0: щелчок через неблокирующий мини-плеер — отзвучит сам в loop
  if (s_clicks()) soundPlayLocal(MEL_CLICK, MEL_CLICK_LEN, SPRIO_CLICK);

  CmdPacket pkt = { PKT_CMD, cmd };
  radio.stopListening();
  bool ok = radio.write(&pkt, sizeof(pkt));
  radio.startListening();

  strncpy(lastCmdLabel, label, sizeof(lastCmdLabel) - 1);
  lastCmdLabel[sizeof(lastCmdLabel) - 1] = '\0';

  // ⚡ v6.0: индикация без delay(): OK — одна вспышка, ошибка — две
  if (ok) ledTxBlink(1, T_LED_BLINK, 0);
  else    ledTxBlink(2, T_LED_FAIL_ON, T_LED_FAIL_OFF);
  Serial.printf("[TX] %s (%d) -> %s\n", label, cmd, ok ? "OK" : "FAIL");
}
void handlePowerBtn(unsigned long now) {
  bool raw = digitalRead(PIN_BTN_POWER);
  if (raw != pwrLastRaw) pwrDebTime = now;
  if ((now - pwrDebTime) > T_DEBOUNCE && raw != pwrStable) {
    pwrStable = raw;
    if (pwrStable == LOW) {
      pwrPressMs  = now;
      pwrLongSent = false;
      // ⚡ v6.3: pwrLastClick == 0 — нажатий ещё не было (не «двойной клик» от нулевой метки на старте)
      pwrPurgeSent = pwrLastClick != 0 && (now - pwrLastClick) < T_DOUBLE_CLICK;
      if (pwrPurgeSent) sendCmd(99, "SAFE OFF");
      else              sendCmd(1,  "POWER");
      pwrLastClick = now;
    } else {
      pwrPressMs = 0;
    }
  }
  if (pwrStable == LOW && pwrPressMs && !pwrLongSent && now - pwrPressMs >= T_LONG_PRESS) {
    pwrLongSent = true;
    if (!pwrPurgeSent) sendCmd(99, "SAFE OFF");
  }
  pwrLastRaw = raw;
}
void handleSimpleBtns(unsigned long now) {
  for (uint8_t i = 0; i < NUM_SIMPLE_BTNS; i++) {
    bool raw = digitalRead(SIMPLE_BTN_PINS[i]);
    if (raw != simpleLastRaw[i]) simpleDebTime[i] = now;
    if ((now - simpleDebTime[i]) > T_DEBOUNCE && raw != simpleStable[i]) {
      simpleStable[i] = raw;
      bool isLock = SIMPLE_BTN_PINS[i] == PIN_BTN_LOCK;
      if (simpleStable[i] == LOW) {
        if (SIMPLE_BTN_PINS[i] == 34) toggleBoomer(now);
        else if (isLock) { lockPressMs = now; lockLongSent = false; lastActivityMs = now; }
        else sendCmd(SIMPLE_BTN_CMDS[i], SIMPLE_BTN_LABELS[i]);
      } else if (isLock && lockPressMs) {
        if (!lockLongSent) sendCmd(SIMPLE_BTN_CMDS[i], SIMPLE_BTN_LABELS[i]);   // короткое — LOCK
        lockPressMs = 0;
      }
    }
    simpleLastRaw[i] = raw;
  }
  // Удержание LOCK ≥ 1 с — заглушить тревогу (один раз за нажатие)
  if (lockPressMs && !lockLongSent && now - lockPressMs >= T_LONG_PRESS) {
    lockLongSent = true;
    soundStop();                       // ⚡ v6.3: сначала глушим свой звук пульта («связь потеряна» / нота мозга),
    sendCmd(CMD_SILENCE_ALARM, "MUTE ALARM");   // затем команда — её щелчок (если включён) станет подтверждением удержания
  }
}

void reset() {
  clockMs=0; sentCount=0; toggles=stops=0;
  for(int i=0;i<64;++i) pins[i]=HIGH;
  for(int i=0;i<7;++i) { simpleLastRaw[i]=simpleStable[i]=HIGH; simpleDebTime[i]=0; }
  pwrLastRaw=pwrStable=HIGH; pwrDebTime=pwrLastClick=pwrPressMs=0;
  pwrLongSent=pwrPurgeSent=false; lockPressMs=0; lockLongSent=false;
  lastActivityMs=0; boomerPlaying=false; oledOk=true; brainOnline=alarm=false;
  dino.stop(); melodyClicks=MelodyClicks(); sndSeq=nullptr;
}
void advance(uint32_t ms) {
  for(uint32_t n=0;n<ms;++n) {
    ++clockMs;
    handlePowerBtn(clockMs); handleSimpleBtns(clockMs);
    
  }
}
void hold(int pin, uint32_t ms) { pins[pin]=LOW; advance(ms); pins[pin]=HIGH; advance(70); }
}

namespace updated {

uint32_t clockMs = 0;
uint32_t millis() { return clockMs; }
const bool HIGH=true, LOW=false;
bool pins[64];
bool digitalRead(int pin) { return pins[pin]; }
const uint8_t NUM_SIMPLE_BTNS=7;
const int SIMPLE_BTN_PINS[]={17,25,26,27,32,33,34};
const uint8_t SIMPLE_BTN_CMDS[]={2,3,7,4,8,9,11};
const char* SIMPLE_BTN_LABELS[]={"LOCK","REP","MAN","FIRE","UP","DOWN","MUSIC"};
const int PIN_BTN_POWER=16, PIN_BTN_LOCK=17;
const uint32_t T_DEBOUNCE=50,T_DOUBLE_CLICK=400,T_LONG_PRESS=1000,T_DISPLAY_UPD=200;
const uint32_t T_LED_BLINK=60,T_LED_FAIL_ON=80,T_LED_FAIL_OFF=60;
const uint8_t PKT_CMD=0x55,CMD_SILENCE_ALARM=12;
struct CmdPacket { uint8_t type,cmd; };
int sent[64], sentCount=0;
struct Radio {
 void stopListening() {}
 bool write(const void* packet, unsigned) { sent[sentCount++]=((const CmdPacket*)packet)->cmd; return true; }
 void startListening() {}
} radio;
struct SerialStub { void println(const char*) {} template<class... T> void printf(const char*,T...) {} } Serial;
bool simpleLastRaw[7],simpleStable[7];
uint32_t simpleDebTime[7];
bool pwrLastRaw=HIGH,pwrStable=HIGH;
uint32_t pwrDebTime=0,pwrLastClick=0,pwrPressMs=0;
bool pwrLongSent=false,pwrPurgeSent=false;
uint32_t lockPressMs=0;
bool lockLongSent=false;
uint32_t lastActivityMs=0,lastDispCheck=0,lastDispDraw=0,lastGameDraw=0;
char lastCmdLabel[16];
bool boomerPlaying=false,oledOk=true,brainOnline=false,alarm=false;
int toggles=0,stops=0;
RemoteDino dino;
MelodyClicks melodyClicks;
void toggleBoomer(uint32_t) { boomerPlaying=!boomerPlaying; ++toggles; }
const int MEL_BOOMER[]={1};
const int* sndSeq=nullptr;
void soundStop() { ++stops; sndSeq=nullptr; }
bool s_clicks() { return false; }
const int MEL_CLICK=0,MEL_CLICK_LEN=1,SPRIO_CLICK=0;
void soundPlayLocal(int,int,int) {}
void ledTxBlink(int,uint32_t,uint32_t) {}
void leaveGame(unsigned long);
bool r_tiltLock() { return alarm; }
bool r_ovLock() { return alarm; }
bool r_psFault() { return alarm; }
bool r_leakLock() { return alarm; }
bool r_overheat() { return alarm; }
bool r_watchdog() { return alarm; }
bool r_ovWarn() { return alarm; }
bool r_leakWarn() { return alarm; }
bool r_tempFault() { return alarm; }
bool r_tiltFault() { return alarm; }
bool r_tiltCal() { return alarm; }
bool r_purging() { return alarm; }
bool r_purgeShort() { return alarm; }
bool r_repulsor() { return alarm; }
bool r_manual() { return alarm; }
bool r_piezo() { return alarm; }
void leaveGame(unsigned long now) {
  melodyClicks.cancel();
  if (!dino.active) return;
  boomerPlaying = false;
  if (sndSeq == MEL_BOOMER) soundStop();  // Brain tones/alarms retain their priority.
  melodyClicks.suppressAfterExit(now);
  dino.stop();
  lastActivityMs = now;
  lastDispCheck = now - T_DISPLAY_UPD;
  lastDispDraw = now - 1000;
  Serial.println("[GAME] EXIT: controls/telemetry");
}
bool gameAlarmActive() {
  return brainOnline && (r_tiltLock() || r_ovLock() || r_psFault() ||
    r_leakLock() || r_overheat() || r_watchdog() || r_ovWarn() ||
    r_leakWarn() || r_tempFault() || r_tiltFault() || r_tiltCal() ||
    r_purging() || r_purgeShort() || r_repulsor() || r_manual() || r_piezo());
}
void handleMelodyEvent(MelodyClicks::Event event, unsigned long now) {
  if (event == MelodyClicks::DOUBLE) {
    if (dino.active) {
      leaveGame(now);
    } else if (oledOk && !gameAlarmActive()) {
      if (boomerPlaying) toggleBoomer(now);
      dino.start(now);
      lastGameDraw = now - 80;
      Serial.println("[GAME] DINO: MELODY=jump/restart, double=exit");
    } else {
      Serial.println("[GAME] unavailable: OLED missing or priority status");
    }
  } else if (event == MelodyClicks::SINGLE) {
    if (dino.active) dino.jump(now);
    else toggleBoomer(now);
  }
}
void handleMelodyPress(unsigned long now) {
  lastActivityMs = now;
  handleMelodyEvent(melodyClicks.poll(now), now);
  handleMelodyEvent(melodyClicks.press(now, !dino.active), now);
}
void sendCmd(uint8_t cmd, const char* label) {
  leaveGame(millis());
  lastActivityMs = millis();   // ⚡ v5.0: любое нажатие сбрасывает таймер сна
  // ⚡ v6.0: щелчок через неблокирующий мини-плеер — отзвучит сам в loop
  if (s_clicks()) soundPlayLocal(MEL_CLICK, MEL_CLICK_LEN, SPRIO_CLICK);

  CmdPacket pkt = { PKT_CMD, cmd };
  radio.stopListening();
  bool ok = radio.write(&pkt, sizeof(pkt));
  radio.startListening();

  strncpy(lastCmdLabel, label, sizeof(lastCmdLabel) - 1);
  lastCmdLabel[sizeof(lastCmdLabel) - 1] = '\0';

  // ⚡ v6.0: индикация без delay(): OK — одна вспышка, ошибка — две
  if (ok) ledTxBlink(1, T_LED_BLINK, 0);
  else    ledTxBlink(2, T_LED_FAIL_ON, T_LED_FAIL_OFF);
  Serial.printf("[TX] %s (%d) -> %s\n", label, cmd, ok ? "OK" : "FAIL");
}
void handlePowerBtn(unsigned long now) {
  bool raw = digitalRead(PIN_BTN_POWER);
  if (raw != pwrLastRaw) pwrDebTime = now;
  if ((now - pwrDebTime) > T_DEBOUNCE && raw != pwrStable) {
    pwrStable = raw;
    if (pwrStable == LOW) {
      pwrPressMs  = now;
      pwrLongSent = false;
      // ⚡ v6.3: pwrLastClick == 0 — нажатий ещё не было (не «двойной клик» от нулевой метки на старте)
      pwrPurgeSent = pwrLastClick != 0 && (now - pwrLastClick) < T_DOUBLE_CLICK;
      if (pwrPurgeSent) sendCmd(99, "SAFE OFF");
      else              sendCmd(1,  "POWER");
      pwrLastClick = now;
    } else {
      pwrPressMs = 0;
    }
  }
  if (pwrStable == LOW && pwrPressMs && !pwrLongSent && now - pwrPressMs >= T_LONG_PRESS) {
    pwrLongSent = true;
    if (!pwrPurgeSent) sendCmd(99, "SAFE OFF");
  }
  pwrLastRaw = raw;
}
void handleSimpleBtns(unsigned long now) {
  for (uint8_t i = 0; i < NUM_SIMPLE_BTNS; i++) {
    bool raw = digitalRead(SIMPLE_BTN_PINS[i]);
    if (raw != simpleLastRaw[i]) simpleDebTime[i] = now;
    if ((now - simpleDebTime[i]) > T_DEBOUNCE && raw != simpleStable[i]) {
      simpleStable[i] = raw;
      bool isLock = SIMPLE_BTN_PINS[i] == PIN_BTN_LOCK;
      if (simpleStable[i] == LOW) {
        if (SIMPLE_BTN_PINS[i] == 34) handleMelodyPress(now);
        else if (isLock) { leaveGame(now); lockPressMs = now; lockLongSent = false; lastActivityMs = now; }
        else sendCmd(SIMPLE_BTN_CMDS[i], SIMPLE_BTN_LABELS[i]);
      } else if (isLock && lockPressMs) {
        if (!lockLongSent) sendCmd(SIMPLE_BTN_CMDS[i], SIMPLE_BTN_LABELS[i]);   // короткое — LOCK
        lockPressMs = 0;
      }
    }
    simpleLastRaw[i] = raw;
  }
  // Удержание LOCK ≥ 1 с — заглушить тревогу (один раз за нажатие)
  if (lockPressMs && !lockLongSent && now - lockPressMs >= T_LONG_PRESS) {
    lockLongSent = true;
    soundStop();                       // ⚡ v6.3: сначала глушим свой звук пульта («связь потеряна» / нота мозга),
    sendCmd(CMD_SILENCE_ALARM, "MUTE ALARM");   // затем команда — её щелчок (если включён) станет подтверждением удержания
  }
}

void reset() {
  clockMs=0; sentCount=0; toggles=stops=0;
  for(int i=0;i<64;++i) pins[i]=HIGH;
  for(int i=0;i<7;++i) { simpleLastRaw[i]=simpleStable[i]=HIGH; simpleDebTime[i]=0; }
  pwrLastRaw=pwrStable=HIGH; pwrDebTime=pwrLastClick=pwrPressMs=0;
  pwrLongSent=pwrPurgeSent=false; lockPressMs=0; lockLongSent=false;
  lastActivityMs=0; boomerPlaying=false; oledOk=true; brainOnline=alarm=false;
  dino.stop(); melodyClicks=MelodyClicks(); sndSeq=nullptr;
}
void advance(uint32_t ms) {
  for(uint32_t n=0;n<ms;++n) {
    ++clockMs;
    handlePowerBtn(clockMs); handleSimpleBtns(clockMs);
    handleMelodyEvent(melodyClicks.poll(clockMs),clockMs);
  }
}
void hold(int pin, uint32_t ms) { pins[pin]=LOW; advance(ms); pins[pin]=HIGH; advance(70); }
}

#define CHECK(c) do { if(!(c)) return __LINE__; } while(0)
extern "C" int test_controls_regression() {
  const int pins[]={16,17,25,26,27,32,33};
  for(int pin:pins) {
    for(int mode=0;mode<4;++mode) {
      old::reset(); updated::reset();
      if(mode==3) updated::dino.start(0);
      old::advance(1000); updated::advance(1000);
      old::hold(pin,mode==1?1200:90); updated::hold(pin,mode==1?1200:90);
      if(mode==2) { old::hold(pin,90); updated::hold(pin,90); }
      old::advance(1300); updated::advance(1300);
      CHECK(old::sentCount==updated::sentCount && old::sentCount>0);
      for(int i=0;i<old::sentCount;++i) CHECK(old::sent[i]==updated::sent[i]);
      CHECK(old::stops==updated::stops);
      if(mode==3) CHECK(!updated::dino.active);
    }
  }
  // Debounce: a short bounce must not issue even one control command.
  for(int pin:pins) {
    updated::reset(); updated::hold(pin,20); CHECK(updated::sentCount==0);
  }
  return 0;
}
extern "C" int test_music_game_integration() {
  using namespace updated;
  reset(); advance(1000);
  hold(34,90); CHECK(toggles==0 && sentCount==0);
  advance(401); CHECK(toggles==1 && boomerPlaying && !dino.active);
  hold(34,90); advance(401); CHECK(toggles==2 && !boomerPlaying);
  reset(); advance(1000);
  hold(34,90); hold(34,90);
  CHECK(dino.active && toggles==0 && sentCount==0);
  advance(500); hold(34,90); dino.update(clockMs);
  CHECK(dino.height()>0 && sentCount==0);
  hold(34,90); CHECK(!dino.active && !boomerPlaying && sentCount==0);
  // Trailing third/fourth clicks of the exit gesture cannot start music.
  hold(34,90); advance(100); hold(34,90);
  advance(1000); CHECK(toggles==0);
  hold(34,90); advance(401); CHECK(boomerPlaying && toggles==1);
  // A priority/control exit between the two melody clicks must stay quiet.
  reset(); advance(1000); dino.start(clockMs); hold(34,90);
  leaveGame(clockMs); hold(34,90); advance(1000);
  CHECK(!boomerPlaying && toggles==0);
  // Stop a stale local melody, but retain a brain tone/alarm.
  reset(); dino.start(1000); boomerPlaying=true; sndSeq=MEL_BOOMER;
  leaveGame(1000); CHECK(!boomerPlaying && stops==1 && sndSeq==nullptr);
  reset(); dino.start(1000); leaveGame(1000); CHECK(stops==0);
  // An ordinary control cancels a pending melody click, preserving its command.
  reset(); advance(1000); hold(34,90); hold(25,90); advance(500);
  CHECK(toggles==0 && sentCount==1 && sent[0]==3);
  reset(); oledOk=false; advance(1000); hold(34,90); hold(34,90);
  CHECK(!dino.active && toggles==0 && sentCount==0);
  reset(); brainOnline=alarm=true; advance(1000); hold(34,90); hold(34,90);
  CHECK(!dino.active && toggles==0 && sentCount==0);
  // Starting the game silences only the local looping tune.
  reset(); boomerPlaying=true; advance(1000); hold(34,90); hold(34,90);
  CHECK(dino.active && !boomerPlaying && sentCount==0);
  return 0;
}
