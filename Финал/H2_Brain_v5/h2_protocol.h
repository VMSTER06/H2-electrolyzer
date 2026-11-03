#ifndef H2_PROTOCOL_H
#define H2_PROTOCOL_H
// =====================================================================================
//  ОБЩИЙ ПРОТОКОЛ ОБМЕНА H2  (мозг ↔ пульт)   — v6.0
// -------------------------------------------------------------------------------------
//  Единственный источник описания радиопакетов и параметров канала. Файл включается
//  ОБЕИМИ прошивками (H2_Brain_v5 и H2_Remote_v5), поэтому структуры пакетов
//  физически не могут разойтись между прошивками.
//
//  ВАЖНО (особенность Arduino IDE): каждый скетч компилируется отдельно и видит только
//  файлы своей папки, поэтому КОПИЯ этого файла лежит в каждой папке скетча
//  (H2_Brain_v5\ и H2_Remote_v5\). Обе копии ОБЯЗАНЫ быть идентичными — при правке
//  протокола меняйте оба файла одновременно (или держите их синхронизированными).
//
//  ⚡ v6.5: TonePacket ходит в обе стороны (пульт → мозг — звуки пульта на зуммер
//  мозга); размер и раскладка пакетов не менялись.
//  ⚡ v6.3: биты ext 5/6 — EXT_TEMP_FAULT / EXT_TILT_FAULT (совместимо: старый пульт их
//  просто не читает; размер и раскладка пакетов не менялись).
//  ⚡ v6.2: байт hystX10 стал ext (флаги EXT_*), добавлена команда 12.
//  ⚡ v6.0: прошивать мозг и пульт ОДНОВРЕМЕННО — пакет статуса стал 16 байт
//  (поле fanDuty заменено битами вентиляторов, добавлен байт настроек звука),
//  добавлен пакет TonePacket (ноты для зуммера пульта).
//
//  Модуль nRF24 работает со СТАТИЧЕСКОЙ длиной полезной нагрузки (по умолчанию
//  32 байта), поэтому несколько типов пакетов идут по одной трубе: приёмник читает
//  весь буфер RF_PAYLOAD_MAX и различает пакет по первому байту (type).
// =====================================================================================
#include <Arduino.h>

// ── Трубы (адреса) nRF24 ───────────────────────────────────────────────────
const uint8_t PIPE_R2B[6] = "CMD01";   // пульт → мозг (команды)
const uint8_t PIPE_B2R[6] = "STS01";   // мозг → пульт (статус + ноты)

// ── ⚡ v5.0: Резервные каналы NRF24 (channel hopping) ──────────────────────
const uint8_t RF_CHANNELS[]  = {76, 100, 120};                          // список каналов
const uint8_t RF_CHANNEL_CNT = sizeof(RF_CHANNELS) / sizeof(RF_CHANNELS[0]);

// Максимальная длина полезной нагрузки nRF24 (статический режим библиотеки RF24).
const uint8_t RF_PAYLOAD_MAX = 32;

// ── Диапазон целевого давления (для индикации и регулирования) ─────────────
const float MIN_TARGET_PRESSURE = 0.5f;   // минимум регулировки
const float MAX_TARGET_PRESSURE = 6.5f;   // максимум регулировки

// ── Типы пакетов (первый байт) ─────────────────────────────────────────────
const uint8_t PKT_CMD    = 0x55;   // пульт → мозг: команда
const uint8_t PKT_STATUS = 0xAA;   // мозг → пульт: статус (10 Гц)
const uint8_t PKT_TONE   = 0xB5;   // ⚡ v6.0 мозг → пульт: одна нота для зуммера пульта
                                   // ⚡ v6.5 и пульт → мозг: нота звука пульта для зуммера мозга

// ── Вывод звука (поле sound статуса, настройка в веб-интерфейсе) ───────────
const uint8_t SND_OUT_REMOTE = 0;   // только пульт
const uint8_t SND_OUT_BRAIN  = 1;   // только мозг
const uint8_t SND_OUT_BOTH   = 2;   // пульт и мозг одновременно
const uint8_t SND_OUT_OFF    = 3;   // без звука

// ── ⚡ v6.2: биты байта ext статуса ─────────────────────────────────────────
const uint8_t EXT_PSENS_FAULT = 1 << 0;   // датчик давления неисправен (обрыв/замыкание)
const uint8_t EXT_LEAK_LOCK   = 1 << 1;   // сильная утечка — система выключена, нужен LOCK
const uint8_t EXT_LEAK_WARN   = 1 << 2;   // давление падает при закрытом клапане
const uint8_t EXT_MAINT_DUE   = 1 << 3;   // пора обслуживать (доливка)
const uint8_t EXT_ALARM_MUTED = 1 << 4;   // повторы тревоги заглушены
const uint8_t EXT_TEMP_FAULT  = 1 << 5;   // ⚡ v6.3: датчик температуры неисправен / нет замера (temp недостоверна)
const uint8_t EXT_TILT_FAULT  = 1 << 6;   // ⚡ v6.3: MPU6050 не отвечает / не найден — угла нет (tiltAngle недостоверен)

// ── ⚡ v6.2: команды пульта (CmdPacket.cmd), добавленные к прежним 1..9, 99 ─
const uint8_t CMD_SILENCE_ALARM = 12;     // заглушить тревогу (долгий LOCK)

struct __attribute__((packed)) CmdPacket {
  uint8_t type;   // PKT_CMD (0x55)
  uint8_t cmd;
};

// Статус мозг → пульт — v6.0: 16 байт
struct __attribute__((packed)) StatusPacket {
  uint8_t type;       // PKT_STATUS (0xAA)
  float   pressure;
  float   target;
  uint8_t flags;      // bit0=sysOn    bit1=locked  bit2=purging(full)
                      // bit3=repulsor bit4=manual  bit5=electro
                      // bit6=valve    bit7=piezo
  uint8_t flags2;     // bit0=ovWarn  bit1=ovLock  bit2=purgeShort
                      // bit3=tiltLock bit4=tiltCal bit5=(резерв, 0; в v5 был driver)
                      // bit6=watchdog bit7=overheat
  uint8_t tiltAngle;  // 0..180 градусов (округлённое)
  uint8_t ext;        // ⚡ v6.2: доп. состояния EXT_* (в v6.0–6.1 здесь был гистерезис ×10)
  int8_t  temp;       // температура °C (округлённая, -40..125)
  uint8_t fans;       // ⚡ v6.0: bit0 = вентилятор корпуса ВКЛ, bit1 = выводной вентилятор ВКЛ
  uint8_t sound;      // ⚡ v6.0: bit0..1 = вывод звука (SND_OUT_*),
                      //          bit2 = щелчки кнопок разрешены,
                      //          bit3 = резерв (0),
                      //          bit4..7 = громкость 1..10 (0 — не задана, считать 10)
};

// ⚡ v6.0: Нота мозг → пульт. Мозг сам ведёт мелодию по времени и шлёт каждую
// ноту в момент её начала; пульт сразу играет её и САМ глушит через durMs (защита
// от «зависшего» писка при потере следующего пакета). freq=0 — пауза/стоп.
// ⚡ v6.5: тот же пакет идёт и пульт → мозг (труба PIPE_R2B, рядом с CmdPacket):
// ноты локальных звуков пульта (щелчки, «Бумер») при выводе «Мозг»/«Оба». Мозг
// играет их только на своём зуммере и тоже сам глушит через durMs.
struct __attribute__((packed)) TonePacket {
  uint8_t  type;      // PKT_TONE (0xB5)
  uint8_t  seq;       // счётчик пакетов (для отладки/отбрасывания дублей)
  uint16_t freq;      // Гц, 0 = тишина
  uint16_t durMs;     // длительность звучания, мс (0 при freq=0 — немедленный стоп)
  uint8_t  volume;    // 1..10
};

// ── ⚡ v6.7: мелодии пульта (PKT_RMEL) ──────────────────────────────────────
// Свои мелодии пульта хранятся в его LittleFS (/rmel/N.h2m, формат H2M1). Файл
// приходит из веба/приложения в мозг и передаётся мозгом по радио кусками; пульт
// проверяет CRC-32 и формат, пишет файл и отвечает результатом и описью. Подтверждение
// доставки каждого куска — аппаратный ACK nRF24 (мозг повторяет кусок, пока ACK нет).
// Повтор END безопасен: пульт помнит результат последней передачи.
const uint8_t PKT_RMEL   = 0xC3;
const uint8_t RMEL_SLOTS = 8;      // слотов мелодий на пульте
const uint8_t RMEL_CHUNK = 26;     // байт файла в одном пакете DATA
const int8_t  RMEL_BOOMER = -1;    // активная мелодия кнопки МЕЛОДИЯ — встроенный «Бумер»
const int8_t  RMEL_NONE   = -9;    // звук загрузки пульта — нет
const uint8_t RMEL_AUTO_SLOT = 0xFF;   // BEGIN: первый свободный слот

enum : uint8_t {
  RMEL_BEGIN   = 1,     // мозг → пульт: начать приём файла
  RMEL_DATA    = 2,     // мозг → пульт: кусок файла
  RMEL_END     = 3,     // мозг → пульт: файл передан — проверить и сохранить
  RMEL_DEL     = 4,     // мозг → пульт: удалить слот
  RMEL_CFG     = 5,     // мозг → пульт: активная мелодия кнопки и звук загрузки
  RMEL_QUERY   = 6,     // мозг → пульт: прислать опись
  RMEL_SUMMARY = 0x80,  // пульт → мозг: опись (общая часть)
  RMEL_SLOT    = 0x81,  // пульт → мозг: опись одного занятого слота
  RMEL_RESULT  = 0x82,  // пульт → мозг: результат передачи
};
enum : uint8_t {          // RmelResult.code
  RMEL_OK = 0, RMEL_E_SEQ = 1, RMEL_E_CRC = 2, RMEL_E_FORMAT = 3, RMEL_E_FULL = 4, RMEL_E_FS = 5,
};

struct __attribute__((packed)) RmelBegin {
  uint8_t  type, op;      // PKT_RMEL, RMEL_BEGIN
  uint8_t  xfer;          // номер передачи (меняется с каждой новой)
  uint8_t  slot;          // 0..RMEL_SLOTS-1 или RMEL_AUTO_SLOT
  uint16_t size;          // размер файла H2M1
  uint32_t crc;           // CRC-32 файла
};
struct __attribute__((packed)) RmelData {
  uint8_t  type, op;      // PKT_RMEL, RMEL_DATA
  uint8_t  xfer;
  uint16_t off;           // смещение куска в файле
  uint8_t  n;             // 1..RMEL_CHUNK
  uint8_t  data[RMEL_CHUNK];
};
struct __attribute__((packed)) RmelEnd    { uint8_t type, op, xfer; };
struct __attribute__((packed)) RmelDel    { uint8_t type, op, slot; };
struct __attribute__((packed)) RmelCfg    { uint8_t type, op; int8_t act, boot; };
struct __attribute__((packed)) RmelQuery  { uint8_t type, op; };
struct __attribute__((packed)) RmelSummary {
  uint8_t type, op;       // PKT_RMEL, RMEL_SUMMARY
  int8_t  act;            // RMEL_BOOMER или слот
  int8_t  boot;           // RMEL_NONE или слот
  uint8_t fsOk;           // LittleFS пульта смонтирована
  uint8_t used;           // битовая маска занятых слотов
};
struct __attribute__((packed)) RmelSlot {
  uint8_t  type, op;      // PKT_RMEL, RMEL_SLOT
  uint8_t  slot;
  uint16_t n;             // нот
  uint32_t ms;            // длительность
  uint8_t  nameLen;       // ≤ 22 (название укорочено по границе символа UTF-8)
  char     name[22];
};
struct __attribute__((packed)) RmelResult { uint8_t type, op, xfer, code, slot; };

static_assert(sizeof(RmelData) == 32 && sizeof(RmelSlot) == 32, "RMEL payload");

// ── ⚡ v6.8: служебный канал пульта (PKT_RSVC) ───────────────────────────────
// Настройки пульта из веба/приложения (через мозг), телеметрия пульта (батарея, доля
// принятых пакетов статуса) и отчёт о каждом нажатии кнопки пульта — для режима отладки.
const uint8_t PKT_RSVC  = 0xC4;
const uint8_t RSVC_KEEP = 0xFF;    // RsvcSet: поле не менять
enum : uint8_t {
  RSVC_SET  = 1,      // мозг → пульт: настройки
  RSVC_INFO = 0x81,   // пульт → мозг: телеметрия и настройки (раз в 5 с и после изменения)
  RSVC_BTN  = 0x82,   // пульт → мозг: нажатие кнопки
};
// Кнопки пульта (RsvcBtn.btn)
enum : uint8_t { RBTN_POWER, RBTN_LOCK, RBTN_REPULSOR, RBTN_MANUAL, RBTN_PIEZO, RBTN_TGT_UP, RBTN_TGT_DOWN, RBTN_MELODY };
// Действие (RsvcBtn.act и журнал мозга)
enum : uint8_t {
  BACT_PRESS = 1,     // короткое нажатие
  BACT_DOUBLE = 2,    // двойное
  BACT_HOLD = 3,      // удержание ≥ 1 с
  BACT_MENU = 4,      // нажатие поглощено меню пульта
  BACT_GAME = 5,      // нажатие поглощено игрой
};
struct __attribute__((packed)) RsvcSet {
  uint8_t type, op;   // PKT_RSVC, RSVC_SET
  uint8_t bat;        // 0/1 — измерять и показывать батарею, RSVC_KEEP — не менять
  uint8_t sleepMin;   // минут до сна, 0 — никогда, RSVC_KEEP
  uint8_t contrast;   // яркость OLED 1..10, RSVC_KEEP
};
struct __attribute__((packed)) RsvcInfo {
  uint8_t  type, op;  // PKT_RSVC, RSVC_INFO
  uint8_t  fw;        // версия прошивки ×10 (68 = v6.8)
  uint16_t batMv;     // напряжение батареи, мВ (0 — не измеряется / нет батареи)
  uint8_t  batPct;    // заряд 0..100, RSVC_KEEP — нет данных
  uint8_t  rxPct;     // доля принятых пакетов статуса за 5 с, 0..100
  uint8_t  bat, sleepMin, contrast;   // текущие настройки
};
struct __attribute__((packed)) RsvcBtn {
  uint8_t type, op;   // PKT_RSVC, RSVC_BTN
  uint8_t seq;        // счётчик нажатий (повтор пакета — тот же seq)
  uint8_t btn;        // RBTN_*
  uint8_t act;        // BACT_*
  uint8_t cmd;        // отправленная команда (0 — локальное действие пульта)
};

static_assert(sizeof(CmdPacket)    == 2,  "CmdPacket size");
static_assert(sizeof(StatusPacket) == 16, "StatusPacket size");
static_assert(sizeof(TonePacket)   == 7,  "TonePacket size");
static_assert(sizeof(StatusPacket) <= RF_PAYLOAD_MAX, "payload too big");

#endif // H2_PROTOCOL_H
