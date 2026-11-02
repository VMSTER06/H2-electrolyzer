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

// ── Вывод звука (поле sound статуса, настройка в веб-интерфейсе) ───────────
const uint8_t SND_OUT_REMOTE = 0;   // только пульт
const uint8_t SND_OUT_BRAIN  = 1;   // только мозг
const uint8_t SND_OUT_BOTH   = 2;   // пульт и мозг одновременно
const uint8_t SND_OUT_OFF    = 3;   // без звука

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
  uint8_t hystX10;    // гистерезис × 10 (0..10 → 0.0..1.0 бар)
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
struct __attribute__((packed)) TonePacket {
  uint8_t  type;      // PKT_TONE (0xB5)
  uint8_t  seq;       // счётчик пакетов (для отладки/отбрасывания дублей)
  uint16_t freq;      // Гц, 0 = тишина
  uint16_t durMs;     // длительность звучания, мс (0 при freq=0 — немедленный стоп)
  uint8_t  volume;    // 1..10
};

static_assert(sizeof(CmdPacket)    == 2,  "CmdPacket size");
static_assert(sizeof(StatusPacket) == 16, "StatusPacket size");
static_assert(sizeof(TonePacket)   == 7,  "TonePacket size");
static_assert(sizeof(StatusPacket) <= RF_PAYLOAD_MAX, "payload too big");

#endif // H2_PROTOCOL_H
