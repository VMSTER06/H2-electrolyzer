"""Compile actual old/new button handlers against fake GPIO/RF, without actuating hardware."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[2]
out = Path(__file__).resolve().parent
before = (root / 'Remote_game_2026-10-29/backup/H2_Remote_v5.ino').read_text(encoding='utf-8-sig')
after = (root / 'H2_Remote_v5/H2_Remote_v5.ino').read_text(encoding='utf-8-sig')

def function(text, name):
    m = re.search(r'^(?:void|bool) ' + name + r'\([^\n]*\) \{', text, re.M)
    assert m, name
    start = m.start()
    p = text.index('{', start)
    level = 1
    end = p + 1
    while level:
        level += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

stub = r'''
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
'''

names = ('r_tiltLock','r_ovLock','r_psFault','r_leakLock','r_overheat','r_watchdog',
         'r_ovWarn','r_leakWarn','r_tempFault','r_tiltFault','r_tiltCal','r_purging',
         'r_purgeShort','r_repulsor','r_manual','r_piezo')
stub += '\n'.join(f'bool {n}() {{ return alarm; }}' for n in names)
reset = r'''
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
    EXTRA_POLL
  }
}
void hold(int pin, uint32_t ms) { pins[pin]=LOW; advance(ms); pins[pin]=HIGH; advance(70); }
'''
cpp = '#include "../../H2_Remote_v5/remote_game.h"\n'
cpp += 'char* strncpy(char* d,const char* s,unsigned n){unsigned i=0;for(;i<n&&s[i];++i)d[i]=s[i];for(;i<n;++i)d[i]=0;return d;}\n'
for ns, src in [('old', before), ('updated', after)]:
    cpp += '\nnamespace ' + ns + ' {\n' + stub + '\n'
    if ns == 'updated':
        for n in ('leaveGame','gameAlarmActive','handleMelodyEvent','handleMelodyPress'):
            cpp += function(src,n) + '\n'
    for n in ('sendCmd','handlePowerBtn','handleSimpleBtns'):
        cpp += function(src,n) + '\n'
    cpp += reset.replace('EXTRA_POLL', 'handleMelodyEvent(melodyClicks.poll(clockMs),clockMs);' if ns=='updated' else '')
    cpp += '}\n'

cpp += r'''
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
'''
(out / 'controls_test.cpp').write_text(cpp, encoding='utf-8')
assert (root / 'H2_Remote_v5/h2_protocol.h').read_bytes() == (root / 'Remote_game_2026-10-29/backup/h2_protocol.h').read_bytes()
print('Generated tests from actual before/after handlers; protocol unchanged.')
