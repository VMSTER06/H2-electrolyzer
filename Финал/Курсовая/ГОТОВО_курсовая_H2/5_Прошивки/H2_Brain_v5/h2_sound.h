// =====================================================================================
//  ⚡ v6.0: ЗВУК МОЗГА — зуммер, мелодии, плеер, библиотека мелодий (LittleFS)
// -------------------------------------------------------------------------------------
//  Подключается из H2_Brain_v5.ino ПОСЛЕ объявления глобальных переменных скетча:
//  использует radio, radioOk, radioMutex, remoteAckMs, otaInProgress, wsStateChanged,
//  server, а также пины PIN_BUZZER / BUZZER_PASSIVE из раздела «ПИНЫ».
//
//  Состав:
//    • зуммер мозга: ПАССИВНЫЙ — ШИМ-тон через LEDC (громкость = скважность),
//      АКТИВНЫЙ (BUZZER_PASSIVE false) — просто «вкл на длительность ноты»;
//    • встроенные мелодии — только собственные короткие сигналы (без чужой музыки);
//    • пользовательские мелодии в LittleFS: /mel/NN.h2m (NN = 00..15), формат H2M1,
//      готовятся (конвертируются под зуммер) в браузере и загружаются через веб;
//    • задача FreeRTOS soundTask — плеер с точным таймингом: очередь запросов,
//      вытеснение по приоритету, ноты дублируются на пульт пакетами TonePacket;
//    • HTTP-эндпоинты /snd/* (настройки, библиотека, загрузка, проигрывание).
//
//  Формат H2M1 (little-endian, общий с веб-конвертером):
//      0..3  'H','2','M','1'
//      4     nameLen (uint8, ≤ 48)
//      5..   name (UTF-8, nameLen байт)
//      затем uint16 noteCount (1..1200)
//      затем noteCount × { uint16 freqHz (0 или 50..10000), uint16 durMs (1..10000) }
//  Суммарная длительность ≤ 120 с, максимальный размер файла 4855 байт.
// =====================================================================================
#pragma once
#include <LittleFS.h>

// ── Идентификаторы мелодий ─────────────────────────────────────────────────
//   0..15  — пользовательские (слоты LittleFS)
//   -1..-3 — встроенные, доступны для выбора и проигрывания из веба
//   -4..-7 — встроенные служебные (в список выбора не входят)
//   -9     — «нет звука» (для настроек событий)
const int8_t MEL_BOOT   = -1;   // «H2 PRO» — стандартная мелодия загрузки (оригинальная)
const int8_t MEL_ON     = -2;   // сигнал включения системы
const int8_t MEL_ALARM  = -3;   // тревога
const int8_t MEL_OFF    = -4;   // выключение системы
const int8_t MEL_WARN   = -5;   // предупреждение
const int8_t MEL_PURGED = -6;   // продувка завершена
const int8_t MEL_CLICK  = -7;   // щелчок кнопки
const int8_t MEL_NONE   = -9;   // нет звука

const uint8_t  MEL_SLOTS        = 16;       // слотов пользовательских мелодий
const uint8_t  MEL_NAME_MAX     = 48;       // байт UTF-8 в названии
const uint16_t MEL_NOTES_MAX    = 1200;     // нот в мелодии
const uint16_t MEL_FREQ_MIN     = 50;       // Гц
const uint16_t MEL_FREQ_MAX     = 10000;    // Гц
const uint16_t MEL_DUR_MAX      = 10000;    // мс на одну ноту
const uint32_t MEL_TOTAL_MS_MAX = 120000;   // мс на всю мелодию
const size_t   MEL_FILE_MAX     = 4 + 1 + MEL_NAME_MAX + 2 + (size_t)MEL_NOTES_MAX * 4;  // 4855

// ── Тайминги звуковых событий ──────────────────────────────────────────────
const unsigned long SND_BOOT_DELAY_MS   = 2500;    // мелодия загрузки через 2.5 с после старта
                                                   // (пульт успевает найти мозг при одновременном включении)
const unsigned long SND_ALARM_REPEAT_MS = 15000;   // повтор тревоги, пока она активна
const unsigned long REMOTE_LISTEN_MS    = 1500;    // пульт «слушает», если ACK статуса был не позже
const float         SND_WARN_REARM_BAR  = 0.10f;   // гистерезис повторного предупреждения по давлению

// ── Нота ───────────────────────────────────────────────────────────────────
struct Note { uint16_t f; uint16_t d; };   // f — Гц (0 = пауза), d — мс
static_assert(sizeof(Note) == 4, "Note: 4 байта, как в файле H2M1");

// ── Встроенные мелодии (константы во флеше) ────────────────────────────────
const Note MEL_BOOT_NOTES[]   = { {1047,90},{0,25},{1319,90},{0,25},{1568,90},{0,25},
                                  {2093,140},{0,60},{1568,90},{0,25},{2093,260} };
const Note MEL_ON_NOTES[]     = { {1500,80},{0,30},{2000,80},{0,30},{2600,140} };
const Note MEL_ALARM_NOTES[]  = { {3000,150},{0,90},{3000,150},{0,90},{3000,150},{0,90} };
const Note MEL_OFF_NOTES[]    = { {2600,80},{0,30},{2000,80},{0,30},{1500,140} };
const Note MEL_WARN_NOTES[]   = { {2000,200},{0,150},{2000,200} };
const Note MEL_PURGED_NOTES[] = { {2200,120},{0,60},{2900,180} };
const Note MEL_CLICK_NOTES[]  = { {4000,15} };

template <size_t N> constexpr uint16_t melCount(const Note (&)[N]) { return (uint16_t)N; }

struct BuiltinMelody { int8_t id; const char* name; const Note* notes; uint16_t n; };
const BuiltinMelody BUILTIN_MELODIES[] = {
  { MEL_BOOT,   "H2 PRO",             MEL_BOOT_NOTES,   melCount(MEL_BOOT_NOTES)   },
  { MEL_ON,     "Сигнал включения",   MEL_ON_NOTES,     melCount(MEL_ON_NOTES)     },
  { MEL_ALARM,  "Тревога",            MEL_ALARM_NOTES,  melCount(MEL_ALARM_NOTES)  },
  { MEL_OFF,    "Выключение",         MEL_OFF_NOTES,    melCount(MEL_OFF_NOTES)    },
  { MEL_WARN,   "Предупреждение",     MEL_WARN_NOTES,   melCount(MEL_WARN_NOTES)   },
  { MEL_PURGED, "Продувка завершена", MEL_PURGED_NOTES, melCount(MEL_PURGED_NOTES) },
  { MEL_CLICK,  "Щелчок",             MEL_CLICK_NOTES,  melCount(MEL_CLICK_NOTES)  },
};

const BuiltinMelody* melBuiltin(int8_t id) {
  for (const BuiltinMelody& b : BUILTIN_MELODIES) if (b.id == id) return &b;
  return nullptr;
}

// ── Настройки звука (NVS "sout","svol","salm","sclk","sboot","son") ────────
// Однобайтовые volatile — задача плеера читает их без блокировок.
volatile uint8_t soundOut  = SND_OUT_BOTH;   // куда играть: SND_OUT_REMOTE/BRAIN/BOTH/OFF
volatile uint8_t soundVol  = 10;             // громкость 1..10
volatile bool    sndAlarms = true;           // звуки тревог и предупреждений
volatile bool    sndClicks = true;           // щелчки кнопок
volatile int8_t  sndBoot   = MEL_BOOT;       // мелодия загрузки (-9 = нет)
volatile int8_t  sndOn     = MEL_ON;         // мелодия включения системы (-9 = нет)

// Байт sound для StatusPacket: bit0..1 вывод, bit2 щелчки, bit4..7 громкость
uint8_t soundStatusByte() {
  return (uint8_t)((soundOut & 0x03) | (sndClicks ? 0x04 : 0) | ((soundVol & 0x0F) << 4));
}

void soundLoadSettings(Preferences& p) {   // вызывается при открытом пространстве "h2cfg"
  uint8_t o = p.getUChar("sout", SND_OUT_BOTH);
  uint8_t v = p.getUChar("svol", 10);
  int8_t  b = p.getChar("sboot", MEL_BOOT);
  int8_t  n = p.getChar("son",   MEL_ON);
  soundOut  = (o <= SND_OUT_OFF) ? o : SND_OUT_BOTH;
  soundVol  = (v >= 1 && v <= 10) ? v : 10;
  sndAlarms = p.getUChar("salm", 1) != 0;
  sndClicks = p.getUChar("sclk", 1) != 0;
  // Слоты здесь проверяем только по диапазону — наличие файла сверяется после
  // чтения библиотеки (melFixEventSlots).
  sndBoot = (b == MEL_NONE || b == MEL_BOOT || b == MEL_ON || (b >= 0 && b < MEL_SLOTS)) ? b : MEL_BOOT;
  sndOn   = (n == MEL_NONE || n == MEL_BOOT || n == MEL_ON || (n >= 0 && n < MEL_SLOTS)) ? n : MEL_ON;
}

// Сохранение сразу (вызывается из веб-обработчиков). Отдельный экземпляр Preferences —
// глобальный prefs параллельно может использоваться из loop на другом ядре.
void soundSaveSettings() {
  Preferences p;
  p.begin("h2cfg", false);
  p.putUChar("sout",  soundOut);
  p.putUChar("svol",  soundVol);
  p.putUChar("salm",  sndAlarms ? 1 : 0);
  p.putUChar("sclk",  sndClicks ? 1 : 0);
  p.putChar ("sboot", sndBoot);
  p.putChar ("son",   sndOn);
  p.end();
}


// =====================================================================================
//  ЗУММЕР МОЗГА (PIN_BUZZER, через NPN-ключ)
// =====================================================================================
// Громкость пассивного зуммера = скважность ШИМ (10 бит, 512 = 50% = максимум).
const uint16_t BUZZER_VOL_DUTY[11] = { 0, 6, 12, 24, 40, 64, 96, 144, 220, 340, 512 };
inline uint16_t dutyForVolume(uint8_t vol) { return BUZZER_VOL_DUTY[vol > 10 ? 10 : vol]; }
#if ESP_ARDUINO_VERSION_MAJOR < 3
const uint8_t BUZZER_LEDC_CHANNEL = 0;   // ядро 2.x: LEDC адресуется каналом
#endif

void buzzerInit() {
#if BUZZER_PASSIVE
  // ESP32 Arduino core 3.x сменил LEDC API: ledcSetup+ledcAttachPin → ledcAttach,
  // ledcWrite/ledcWriteTone принимают ПИН вместо канала. Поддерживаем обе версии.
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (!ledcAttach(PIN_BUZZER, 2000, 10))                  // core 3.x
    Serial.println("LEDC: FAIL — зуммер мозга не работает (нет канала)!");
  ledcWrite(PIN_BUZZER, 0);
#else
  ledcSetup(BUZZER_LEDC_CHANNEL, 2000, 10);               // core 2.x
  ledcAttachPin(PIN_BUZZER, BUZZER_LEDC_CHANNEL);
  ledcWrite(BUZZER_LEDC_CHANNEL, 0);
#endif
#else
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
#endif
}

void buzzerOff() {
#if BUZZER_PASSIVE
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(PIN_BUZZER, 0);
#else
  ledcWrite(BUZZER_LEDC_CHANNEL, 0);
#endif
#else
  digitalWrite(PIN_BUZZER, LOW);
#endif
}

// Начать тон f Гц с громкостью vol (1..10). f = 0 — тишина.
// Пассивный: ledcWriteTone задаёт частоту (и 10 бит, 50%), затем скважность громкости.
// Активный: частота игнорируется — зуммер просто включается на время ноты.
void buzzerTone(uint16_t f, uint8_t vol) {
  if (f == 0 || vol == 0) { buzzerOff(); return; }
#if BUZZER_PASSIVE
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteTone(PIN_BUZZER, f);
  ledcWrite(PIN_BUZZER, dutyForVolume(vol));
#else
  ledcWriteTone(BUZZER_LEDC_CHANNEL, f);
  ledcWrite(BUZZER_LEDC_CHANNEL, dutyForVolume(vol));
#endif
#else
  digitalWrite(PIN_BUZZER, HIGH);
#endif
}


// =====================================================================================
//  БИБЛИОТЕКА МЕЛОДИЙ (LittleFS, раздел "spiffs" стандартной схемы разделов)
// =====================================================================================
struct MelInfo {
  bool     used;                     // слот занят корректным файлом
  uint16_t n;                        // число нот
  uint32_t ms;                       // общая длительность, мс
  char     name[MEL_NAME_MAX + 1];   // название (UTF-8, с завершающим нулём)
};
MelInfo melCache[MEL_SLOTS];         // кэш списка в RAM (старт / загрузка / удаление)
bool    fsOk = false;                // LittleFS смонтирована

void melPath(uint8_t slot, char* out, size_t len) { snprintf(out, len, "/mel/%02u.h2m", slot); }

bool melSlotUsed(int id) { return id >= 0 && id < MEL_SLOTS && melCache[id].used; }

// Допустима ли мелодия для события (загрузка/включение): нет, -1, -2 или занятый слот
bool melEventIdValid(int id) { return id == MEL_NONE || id == MEL_BOOT || id == MEL_ON || melSlotUsed(id); }

// Мелодия для события: пользовательский слот, если он доступен, иначе встроенная
// (например, если LittleFS не поднялась — встроенные продолжают работать).
int8_t melEventId(int8_t id, int8_t fallback) {
  if (id >= 0 && !melSlotUsed(id)) return fallback;
  return id;
}

// Проверка, что название — корректный UTF-8 без нулевых байтов
static bool melNameValid(const uint8_t* s, uint8_t len) {
  uint8_t i = 0;
  while (i < len) {
    uint8_t c = s[i], extra;
    if (c == 0)                              return false;
    if (c < 0x80)                            extra = 0;
    else if ((c & 0xE0) == 0xC0 && c >= 0xC2) extra = 1;
    else if ((c & 0xF0) == 0xE0)             extra = 2;
    else if ((c & 0xF8) == 0xF0 && c <= 0xF4) extra = 3;
    else                                     return false;
    if ((uint16_t)i + extra >= len)          return false;
    for (uint8_t k = 1; k <= extra; k++)
      if ((s[i + k] & 0xC0) != 0x80)         return false;
    i += 1 + extra;
  }
  return true;
}

static uint16_t rdLE16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

// Строгая проверка образа H2M1 (магия, длины, ТОЧНЫЙ размер, диапазоны, общая
// длительность). nullptr — файл корректен (info заполнен), иначе текст ошибки.
const char* melValidate(const uint8_t* b, size_t len, MelInfo* info) {
  if (len > MEL_FILE_MAX)                      return "слишком большой файл";
  if (len < 4 + 1 + 2 || memcmp(b, "H2M1", 4) != 0)
                                               return "плохой формат: нет сигнатуры H2M1";
  uint8_t nameLen = b[4];
  if (nameLen > MEL_NAME_MAX)                  return "плохой формат: длинное название";
  size_t hdr = 5 + (size_t)nameLen + 2;
  if (len < hdr)                               return "плохой формат: файл обрезан";
  if (!melNameValid(b + 5, nameLen))           return "плохой формат: название не UTF-8";
  uint16_t cnt = rdLE16(b + 5 + nameLen);
  if (cnt < 1 || cnt > MEL_NOTES_MAX)          return "плохой формат: число нот";
  if (len != hdr + (size_t)cnt * 4)            return "плохой формат: размер не совпадает";
  uint32_t total = 0;
  const uint8_t* p = b + hdr;
  for (uint16_t i = 0; i < cnt; i++, p += 4) {
    uint16_t f = rdLE16(p), d = rdLE16(p + 2);
    if (f != 0 && (f < MEL_FREQ_MIN || f > MEL_FREQ_MAX)) return "плохой формат: частота вне 50..10000 Гц";
    if (d < 1 || d > MEL_DUR_MAX)                         return "плохой формат: длительность ноты";
    total += d;
  }
  if (total > MEL_TOTAL_MS_MAX)                return "мелодия длиннее 120 с";
  if (info) {
    info->used = true;
    info->n    = cnt;
    info->ms   = total;
    memcpy(info->name, b + 5, nameLen);
    info->name[nameLen] = '\0';
  }
  return nullptr;
}

// Буфер приёма файла (/snd/up). Чтение библиотеки на старте использует его же —
// оно выполняется в setup() до запуска веб-сервера.
static uint8_t melUpBuf[MEL_FILE_MAX];

// Чтение библиотеки на старте: битые файлы пропускаются (слот считается свободным
// и будет перезаписан следующей загрузкой).
void melScan() {
  for (uint8_t i = 0; i < MEL_SLOTS; i++) melCache[i].used = false;
  if (!fsOk) return;
  for (uint8_t i = 0; i < MEL_SLOTS; i++) {
    char path[20]; melPath(i, path, sizeof(path));
    if (!LittleFS.exists(path)) continue;
    File f = LittleFS.open(path, FILE_READ);
    if (!f) continue;
    size_t sz = f.size();
    bool ok = false;
    if (sz <= MEL_FILE_MAX && f.read(melUpBuf, sz) == sz)
      ok = (melValidate(melUpBuf, sz, &melCache[i]) == nullptr);
    f.close();
    if (!ok) { melCache[i].used = false; Serial.printf("[SND] %s повреждён — слот свободен\n", path); }
  }
}

// После чтения библиотеки: мелодия события ссылается на отсутствующий слот → сброс
// на встроенную по умолчанию (только при рабочей ФС, иначе настройку не трогаем).
void melFixEventSlots() {
  if (!fsOk) return;
  bool changed = false;
  if (sndBoot >= 0 && !melSlotUsed(sndBoot)) { sndBoot = MEL_BOOT; changed = true; }
  if (sndOn   >= 0 && !melSlotUsed(sndOn))   { sndOn   = MEL_ON;   changed = true; }
  if (changed) soundSaveSettings();
}

// Сохранение принятого файла в первый свободный слот. nullptr — успех (slotOut).
const char* melStore(const uint8_t* buf, size_t len, int& slotOut) {
  MelInfo info;
  const char* err = melValidate(buf, len, &info);
  if (err) return err;
  int slot = -1;
  for (uint8_t i = 0; i < MEL_SLOTS; i++) if (!melCache[i].used) { slot = i; break; }
  if (slot < 0) return "нет свободных слотов (максимум 16)";
  // Запас на метаданные LittleFS — два блока по 4 КБ
  size_t total = LittleFS.totalBytes(), used = LittleFS.usedBytes();
  if (used + len + 2 * 4096 > total) return "нет места во флеш-памяти";
  char path[20]; melPath(slot, path, sizeof(path));
  File f = LittleFS.open(path, FILE_WRITE, true);
  if (!f) return "нет места во флеш-памяти";
  size_t w = f.write(buf, len);
  f.close();
  if (w != len) { LittleFS.remove(path); return "нет места во флеш-памяти"; }
  melCache[slot] = info;
  slotOut = slot;
  Serial.printf("[SND] Мелодия «%s» сохранена в слот %d (%u нот, %lu мс)\n",
                info.name, slot, info.n, (unsigned long)info.ms);
  return nullptr;
}

// Загрузка пользовательской мелодии в буфер плеера (вызывается ТОЛЬКО из soundTask).
// Возвращает число нот, 0 — ошибка (буфер мог быть частично перезаписан).
static uint16_t melLoadUser(int8_t slot, Note* buf) {
  if (!fsOk || slot < 0 || slot >= MEL_SLOTS) return 0;
  char path[20]; melPath(slot, path, sizeof(path));
  if (!LittleFS.exists(path)) return 0;
  File f = LittleFS.open(path, FILE_READ);
  if (!f) return 0;
  uint16_t result = 0;
  size_t   sz = f.size();
  uint8_t  hdr[5], c[2];
  if (sz >= 7 && sz <= MEL_FILE_MAX && f.read(hdr, 5) == 5 && memcmp(hdr, "H2M1", 4) == 0 &&
      hdr[4] <= MEL_NAME_MAX && f.seek(5 + hdr[4]) && f.read(c, 2) == 2) {
    uint16_t cnt   = rdLE16(c);
    size_t   bytes = (size_t)cnt * sizeof(Note);
    if (cnt >= 1 && cnt <= MEL_NOTES_MAX && sz == 7u + hdr[4] + bytes &&
        f.read((uint8_t*)buf, bytes) == bytes) {
      // Note {uint16 f; uint16 d} в памяти ESP32 (little-endian) совпадает с файлом
      uint32_t total = 0;
      bool ok = true;
      for (uint16_t i = 0; i < cnt && ok; i++) {
        ok = (buf[i].f == 0 || (buf[i].f >= MEL_FREQ_MIN && buf[i].f <= MEL_FREQ_MAX)) &&
             buf[i].d >= 1 && buf[i].d <= MEL_DUR_MAX;
        total += buf[i].d;
      }
      if (ok && total <= MEL_TOTAL_MS_MAX) result = cnt;
    }
  }
  f.close();
  return result;
}


// =====================================================================================
//  ПЛЕЕР — задача FreeRTOS soundTask
// =====================================================================================
enum : uint8_t { SND_REQ_PLAY = 0, SND_REQ_STOP = 1 };
// ⚡ v6.3: предупреждение отдельным приоритетом — не обрывает звучащую тревогу
// (раньше MEL_WARN шёл с SND_PRIO_ALARM и вытеснял тревогу при r.prio >= prio)
enum : uint8_t { SND_PRIO_CLICK = 0, SND_PRIO_MELODY = 1, SND_PRIO_WARN = 2, SND_PRIO_ALARM = 3 };

struct SoundReq {
  uint8_t kind;   // SND_REQ_PLAY / SND_REQ_STOP
  int8_t  id;     // мелодия
  uint8_t prio;   // SND_PRIO_*
  int8_t  out;    // SND_OUT_* или -1 = текущая настройка soundOut
};

QueueHandle_t soundQueue       = nullptr;
volatile bool soundPlayingFlag = false;

// Пульт «слушает»: последний статус подтверждён ACK не позже REMOTE_LISTEN_MS назад
static bool remoteListening() {
  unsigned long a = remoteAckMs;
  return a != 0 && (millis() - a) <= REMOTE_LISTEN_MS;
}

// Отправка ноты на пульт (freq=0, durMs=0 — немедленный стоп). Радио общее с loop,
// поэтому строго под radioMutex; если радио занято дольше 10 мс — ноту пропускаем.
static void soundSendTone(uint16_t f, uint16_t d, uint8_t vol) {
  static uint8_t seq = 0;
  if (!radioOk || radioMutex == nullptr) return;
  TonePacket tp;
  tp.type   = PKT_TONE;
  tp.seq    = seq++;
  tp.freq   = f;
  tp.durMs  = d;
  tp.volume = vol;
  if (xSemaphoreTake(radioMutex, pdMS_TO_TICKS(10)) != pdTRUE) return;
  radio.stopListening();
  radio.setRetries(1, 3);          // нота важнее статуса — пара повторов
  radio.write(&tp, sizeof(tp));
  radio.setRetries(0, 1);          // вернуть режим статуса (см. setup)
  radio.startListening();
  xSemaphoreGive(radioMutex);
}

static void soundTask(void*) {
  static Note userBuf[MEL_NOTES_MAX];   // буфер пользовательской мелодии — принадлежит задаче
  const Note* mel       = nullptr;      // текущая мелодия (встроенная или userBuf)
  uint16_t    count     = 0, pos = 0;
  uint8_t     prio      = 0;
  int8_t      reqOut    = -1;
  bool        playing   = false;
  TickType_t  noteEnd   = 0;            // тик окончания текущей ноты (граница)
  TickType_t  remoteEnd = 0;            // до какого тика может звучать нота на пульте
  bool        remoteOn  = false;        // на пульт отправлена нота, которая ещё может звучать

  // Заглушить мозг и (если нота ещё звучит) пульт
  auto silence = [&]() {
    buzzerOff();
    if (remoteOn && (int32_t)(remoteEnd - xTaskGetTickCount()) > 0) soundSendTone(0, 0, soundVol);
    remoteOn = false;
  };
  auto finish = [&]() {
    playing = false;
    soundPlayingFlag = false;
    wsStateChanged = true;
  };
  // Начать ноту mel[pos] от границы noteEnd. false — вывод звука выключили (стоп).
  auto startNote = [&]() -> bool {
    uint8_t out = (reqOut >= 0) ? (uint8_t)reqOut : soundOut;   // маршрут — на каждую ноту
    if (out >= SND_OUT_OFF) return false;
    const Note nt  = mel[pos];
    uint8_t    vol = soundVol;
    noteEnd += pdMS_TO_TICKS(nt.d);
    // ⚡ v6.3: в режиме «Пульт» тревоги/предупреждения при недоступном (спящем, вне
    // связи) пульте дублируются на зуммер мозга — иначе они звучали «в пустоту».
    // Мелодии, предпрослушивание и щелчки — как раньше, только по выбранному выводу.
    bool rl       = remoteListening();
    bool toRemote = (out == SND_OUT_REMOTE || out == SND_OUT_BOTH) && rl;
    bool toBrain  = out == SND_OUT_BRAIN || out == SND_OUT_BOTH ||
                    (out == SND_OUT_REMOTE && prio >= SND_PRIO_WARN && !rl);
    if (nt.f && toBrain) buzzerTone(nt.f, vol);
    else                 buzzerOff();
    // Паузы на пульт не шлём: пульт сам замолкает по durMs предыдущей ноты
    if (nt.f && toRemote) {
      soundSendTone(nt.f, nt.d, vol);
      remoteEnd = noteEnd;
      remoteOn  = true;
    }
    return true;
  };

  for (;;) {
    // Ждём запрос не дольше, чем до границы текущей ноты (точность ±1 тик)
    TickType_t wait = portMAX_DELAY;
    if (playing) {
      int32_t left = (int32_t)(noteEnd - xTaskGetTickCount());
      wait = (left > 0) ? (TickType_t)left : 0;
    }
    SoundReq r;
    if (xQueueReceive(soundQueue, &r, wait) == pdTRUE) {
      if (r.kind == SND_REQ_STOP) {               // STOP останавливает всегда
        if (playing) { silence(); finish(); }
        continue;
      }
      if (playing && r.prio < prio) continue;     // менее важный звук не перебивает текущий
      const Note* m = nullptr;
      uint16_t    n = 0;
      if (r.id >= 0) {
        n = melLoadUser(r.id, userBuf);
        if (n == 0) {                             // файла нет/битый: userBuf мог испортиться
          if (playing && mel == userBuf) { silence(); finish(); }
          continue;
        }
        m = userBuf;
      } else {
        const BuiltinMelody* b = melBuiltin(r.id);
        if (!b) continue;
        m = b->notes; n = b->n;
      }
      if (playing) silence();                     // вытеснение текущего звука
      mel = m; count = n; pos = 0; prio = r.prio; reqOut = r.out;
      if (!playing) { playing = true; soundPlayingFlag = true; wsStateChanged = true; }
      noteEnd = xTaskGetTickCount();
      if (!startNote()) { silence(); finish(); }
      continue;
    }
    if (!playing) continue;
    // Граница ноты — следующая нота или конец мелодии
    if (++pos >= count) {
      buzzerOff();
      remoteOn = false;                           // последняя нота пульта заканчивается сама
      finish();
      continue;
    }
    // ⚡ v6.3: плеер отстал от графика больше чем на 20 мс (короткие ноты + долгие
    // radio.write) — график сдвигаем на «сейчас», а не догоняем пачкой нот без пауз
    if ((int32_t)(xTaskGetTickCount() - noteEnd) > (int32_t)pdMS_TO_TICKS(20)) noteEnd = xTaskGetTickCount();
    if (!startNote()) { silence(); finish(); }
  }
}

// ── API (потокобезопасно: из loop и из веб-обработчиков на ядре 0) ─────────
bool soundPlay(int8_t id, uint8_t prio, int8_t out = -1) {
  if (soundQueue == nullptr || otaInProgress || id == MEL_NONE) return false;
  uint8_t eff = (out >= 0) ? (uint8_t)out : soundOut;
  if (eff >= SND_OUT_OFF) return false;           // «без звука» — нечего ставить в очередь
  SoundReq r = { SND_REQ_PLAY, id, prio, out };
  return xQueueSend(soundQueue, &r, 0) == pdTRUE;
}

void soundStop() {
  if (soundQueue == nullptr) return;
  // Всё, что ещё в очереди, STOP всё равно отменил бы — очищаем, чтобы стоп
  // гарантированно поместился даже в заполненную очередь.
  xQueueReset(soundQueue);
  SoundReq r = { SND_REQ_STOP, 0, 0, -1 };
  xQueueSend(soundQueue, &r, 0);
}

// Щелчок физической кнопки мозга — только на зуммер мозга и только если он включён
// в вывод (пульт щёлкает сам, команды с пульта и веба мозг не «щёлкает»).
void soundClick() {
  if (!sndClicks) return;
  uint8_t o = soundOut;
  if (o != SND_OUT_BRAIN && o != SND_OUT_BOTH) return;
  soundPlay(MEL_CLICK, SND_PRIO_CLICK, SND_OUT_BRAIN);
}

bool soundIsPlaying() { return soundPlayingFlag; }

// Инициализация: LittleFS + библиотека, очередь и задача плеера. Вызывать в setup()
// после создания radioMutex и инициализации радио.
void soundInit() {
  fsOk = LittleFS.begin(true);   // true — отформатировать раздел при первом запуске
  if (fsOk) {
    if (!LittleFS.exists("/mel")) LittleFS.mkdir("/mel");
    melScan();
    melFixEventSlots();
    uint8_t cnt = 0;
    for (uint8_t i = 0; i < MEL_SLOTS; i++) if (melCache[i].used) cnt++;
    Serial.printf("LittleFS: OK (%u из %u КБ занято), мелодий: %u\n",
                  (unsigned)(LittleFS.usedBytes() / 1024), (unsigned)(LittleFS.totalBytes() / 1024), cnt);
  } else {
    melScan();   // очистить кэш
    Serial.println("LittleFS: FAIL — пользовательские мелодии недоступны (встроенные работают)");
  }
  soundQueue = xQueueCreate(8, sizeof(SoundReq));
  // Стек 6 КБ: задача читает файлы LittleFS (VFS + littlefs заметно расходуют стек)
  // ⚡ v6.3: приоритет 1 = loop: при отставании от графика нот плеер не блокируется
  // (wait=0) и с приоритетом 2 мог до ~1.5 с не пускать loop (защиты) на ядро 1; при
  // равном приоритете FreeRTOS делит ядро по тикам (time slicing), дрожание нот ≤1 мс
  if (soundQueue == nullptr ||
      xTaskCreatePinnedToCore(soundTask, "sound", 6144, nullptr, 1, nullptr, ARDUINO_RUNNING_CORE) != pdPASS) {
    soundQueue = nullptr;
    Serial.println("SOUND: FAIL — не удалось запустить задачу плеера");
  }
}


// =====================================================================================
//  HTTP /snd/*
// =====================================================================================
// Экранирование строки для JSON: \" \\ и управляющие символы (< 0x20) как \u00XX.
// Байты UTF-8 (≥ 0x80) передаются как есть.
static void melJsonEscape(const char* s, char* out, size_t outLen) {
  size_t o = 0;
  for (; *s; s++) {
    uint8_t c = (uint8_t)*s;
    char    tmp[8];
    size_t  tl;
    if (c == '"' || c == '\\') { tmp[0] = '\\'; tmp[1] = (char)c; tl = 2; }
    else if (c < 0x20)         { snprintf(tmp, sizeof(tmp), "\\u%04x", c); tl = 6; }
    else                       { tmp[0] = (char)c; tl = 1; }
    if (o + tl >= outLen) break;
    memcpy(out + o, tmp, tl);
    o += tl;
  }
  out[o] = '\0';
}

// Строгое чтение целого параметра: false — параметр есть, но это не число.
// Отсутствующий параметр не меняет v и возвращает true.
static bool sndParamInt(AsyncWebServerRequest* req, const char* name, int& v) {
  if (!req->hasParam(name)) return true;
  const String& s = req->getParam(name)->value();
  char* end = nullptr;
  long  x   = strtol(s.c_str(), &end, 10);
  if (s.length() == 0 || end == nullptr || *end != '\0') return false;
  v = (int)x;
  return true;
}

// Состояние приёма файла /snd/up (один приём за раз; новая загрузка вытесняет
// незавершённую — прежний запрос получит ошибку «файл не получен»).
struct MelUpload {
  AsyncWebServerRequest* owner;    // чей файл сейчас в melUpBuf
  size_t                 len;      // принято байт
  const char*            err;      // ошибка приёма (nullptr — нет)
  bool                   active;   // идёт приём части "f"
  bool                   done;     // часть "f" принята целиком
};
static MelUpload melUp = { nullptr, 0, nullptr, false, false };

void soundRegisterHttp() {
  // Настройки + библиотека
  server.on("/snd/cfg", HTTP_GET, [](AsyncWebServerRequest* req) {
    AsyncResponseStream* rs = req->beginResponseStream("application/json");
    unsigned long total = fsOk ? (unsigned long)LittleFS.totalBytes() : 0;
    unsigned long used  = fsOk ? (unsigned long)LittleFS.usedBytes()  : 0;
    rs->printf("{\"out\":%u,\"vol\":%u,\"alm\":%d,\"clk\":%d,\"boot\":%d,\"on\":%d,"
               "\"fsok\":%d,\"free\":%lu,\"total\":%lu,\"play\":%d,\"list\":[",
               (unsigned)soundOut, (unsigned)soundVol, sndAlarms ? 1 : 0, sndClicks ? 1 : 0,
               (int)sndBoot, (int)sndOn, fsOk ? 1 : 0, total > used ? total - used : 0UL, total,
               soundIsPlaying() ? 1 : 0);
    bool first = true;
    for (uint8_t i = 0; i < MEL_SLOTS; i++) {
      if (!melCache[i].used) continue;
      char esc[MEL_NAME_MAX * 6 + 1];
      if (melCache[i].name[0]) melJsonEscape(melCache[i].name, esc, sizeof(esc));
      else                     snprintf(esc, sizeof(esc), "Мелодия %u", i);
      rs->printf("%s{\"id\":%u,\"name\":\"%s\",\"n\":%u,\"ms\":%lu}",
                 first ? "" : ",", i, esc, melCache[i].n, (unsigned long)melCache[i].ms);
      first = false;
    }
    rs->print("]}");
    req->send(rs);
  });

  // Изменение настроек (любое подмножество параметров), сохраняется сразу
  server.on("/snd/set", HTTP_GET, [](AsyncWebServerRequest* req) {
    int out = soundOut, vol = soundVol, alm = sndAlarms, clk = sndClicks, boot = sndBoot, on = sndOn;
    const char* bad = nullptr;
    if      (!sndParamInt(req, "out",  out)  || out < 0 || out > SND_OUT_OFF) bad = "bad out";
    else if (!sndParamInt(req, "vol",  vol)  || vol < 1 || vol > 10)          bad = "bad vol";
    else if (!sndParamInt(req, "alm",  alm)  || alm < 0 || alm > 1)           bad = "bad alm";
    else if (!sndParamInt(req, "clk",  clk)  || clk < 0 || clk > 1)           bad = "bad clk";
    // ⚡ v6.3: неприсланные boot/on не проверяем — при битой ФС сохранённый слот иначе
    // блокирует любой частичный /snd/set (Android шлёт только out/vol/alm/clk); при
    // воспроизведении melEventId() подменит недоступный слот встроенной мелодией
    else if (req->hasParam("boot") && (!sndParamInt(req, "boot", boot) || !melEventIdValid(boot))) bad = "bad boot";
    else if (req->hasParam("on")   && (!sndParamInt(req, "on",   on)   || !melEventIdValid(on)))   bad = "bad on";
    if (bad) { req->send(400, "text/plain", bad); return; }
    soundOut  = (uint8_t)out;
    soundVol  = (uint8_t)vol;
    sndAlarms = alm != 0;
    sndClicks = clk != 0;
    sndBoot   = (int8_t)boot;
    sndOn     = (int8_t)on;
    soundSaveSettings();
    if (soundOut == SND_OUT_OFF) soundStop();   // «без звука» — замолчать сразу
    wsStateChanged = true;
    req->send(200, "text/plain", "OK");
  });

  // Проиграть мелодию (встроенные -1/-2/-3 или слот) — приоритет MELODY
  server.on("/snd/play", HTTP_GET, [](AsyncWebServerRequest* req) {
    int id = MEL_NONE, out = -1;
    if (!req->hasParam("id") || !sndParamInt(req, "id", id)) { req->send(400, "text/plain", "bad id"); return; }
    if (!sndParamInt(req, "out", out) || out < -1 || out > SND_OUT_OFF) { req->send(400, "text/plain", "bad out"); return; }
    bool known = (id == MEL_BOOT || id == MEL_ON || id == MEL_ALARM) || melSlotUsed(id);
    if (!known) { req->send(404, "text/plain", "no melody"); return; }
    soundPlay((int8_t)id, SND_PRIO_MELODY, (int8_t)out);
    req->send(200, "text/plain", "OK");
  });

  server.on("/snd/stop", HTTP_GET, [](AsyncWebServerRequest* req) {
    soundStop();
    req->send(200, "text/plain", "OK");
  });

  // Удаление пользовательской мелодии
  server.on("/snd/del", HTTP_GET, [](AsyncWebServerRequest* req) {
    int id = MEL_NONE;
    if (!req->hasParam("id") || !sndParamInt(req, "id", id) || !melSlotUsed(id)) {
      req->send(404, "text/plain", "no melody");
      return;
    }
    char path[20]; melPath((uint8_t)id, path, sizeof(path));
    LittleFS.remove(path);
    melCache[id].used = false;
    // Удалили мелодию события — вернуть встроенную по умолчанию
    bool changed = false;
    if (sndBoot == id) { sndBoot = MEL_BOOT; changed = true; }
    if (sndOn   == id) { sndOn   = MEL_ON;   changed = true; }
    if (changed) soundSaveSettings();
    wsStateChanged = true;
    req->send(200, "text/plain", "OK");
  });

  // Бинарный H2M1 для предпрослушивания в браузере (встроенные сериализуются на лету)
  server.on("/snd/get", HTTP_GET, [](AsyncWebServerRequest* req) {
    int id = MEL_NONE;
    if (!req->hasParam("id") || !sndParamInt(req, "id", id)) { req->send(400, "text/plain", "bad id"); return; }
    if (id == MEL_BOOT || id == MEL_ON || id == MEL_ALARM) {
      const BuiltinMelody* b = melBuiltin((int8_t)id);
      uint8_t nameLen = (uint8_t)strlen(b->name);
      uint8_t hdr[5 + MEL_NAME_MAX + 2];
      memcpy(hdr, "H2M1", 4);
      hdr[4] = nameLen;
      memcpy(hdr + 5, b->name, nameLen);
      hdr[5 + nameLen] = (uint8_t)(b->n & 0xFF);
      hdr[6 + nameLen] = (uint8_t)(b->n >> 8);
      // AsyncResponseStream копирует данные (буфер на стеке после return недействителен)
      AsyncResponseStream* rs = req->beginResponseStream("application/octet-stream", 7 + nameLen + b->n * 4);
      rs->write(hdr, 7 + nameLen);
      for (uint16_t i = 0; i < b->n; i++) {
        uint8_t nb[4] = { (uint8_t)(b->notes[i].f & 0xFF), (uint8_t)(b->notes[i].f >> 8),
                          (uint8_t)(b->notes[i].d & 0xFF), (uint8_t)(b->notes[i].d >> 8) };
        rs->write(nb, 4);
      }
      req->send(rs);
      return;
    }
    if (!melSlotUsed(id)) { req->send(404, "text/plain", "no melody"); return; }
    char path[20]; melPath((uint8_t)id, path, sizeof(path));
    AsyncWebServerResponse* resp = req->beginResponse(LittleFS, path, "application/octet-stream");
    if (!resp) { req->send(404, "text/plain", "no melody"); return; }
    req->send(resp);
  });

  // Загрузка мелодии: multipart, поле файла "f" (готовый H2M1 из веб-конвертера).
  // Обработчик загрузки копит куски в melUpBuf, обработчик завершения проверяет
  // формат, пишет файл в первый свободный слот и отвечает JSON.
  server.on("/snd/up", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      const char* err  = nullptr;
      int         slot = -1;
      if (!fsOk)                                         err = "нет ФС (LittleFS не смонтирована)";
      else if (melUp.owner != req || !melUp.done)        err = "файл не получен";
      else if (melUp.err)                                err = melUp.err;
      else                                               err = melStore(melUpBuf, melUp.len, slot);
      if (melUp.owner == req) melUp.owner = nullptr;
      char out[160];
      if (err) snprintf(out, sizeof(out), "{\"ok\":0,\"err\":\"%s\"}", err);
      else     snprintf(out, sizeof(out), "{\"ok\":1,\"id\":%d}", slot);
      if (!err) wsStateChanged = true;
      req->send(200, "application/json", out);
    },
    [](AsyncWebServerRequest* req, const String& filename, size_t index, uint8_t* data, size_t len, bool final) {
      if (!fsOk) return;
      if (index == 0) {
        // Имя поля формы: библиотека кладёт его параметром "name" на время приёма части
        const AsyncWebParameter* fp = req->getParam("name", true, true);
        bool isF = (fp == nullptr) || (fp->value() == "f");
        if (melUp.owner != req) {
          if (!isF) return;                       // посторонний файл — приём не начинаем
          melUp.owner  = req;
          melUp.len    = 0;
          melUp.err    = nullptr;
          melUp.done   = false;
          melUp.active = true;
          // Клиент оборвал загрузку — освободить буфер для следующей
          req->onDisconnect([req]() { if (melUp.owner == req) melUp.owner = nullptr; });
        } else {
          melUp.active = isF && !melUp.done;      // нужна только первая часть "f"
        }
      }
      if (melUp.owner != req || !melUp.active) return;
      if (melUp.err == nullptr) {
        if (melUp.len + len > MEL_FILE_MAX) melUp.err = "слишком большой файл (максимум 4855 байт)";
        else { memcpy(melUpBuf + melUp.len, data, len); melUp.len += len; }
      }
      if (final) { melUp.done = true; melUp.active = false; }
    }
  );
}
