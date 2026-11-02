#ifndef H2_PROTOCOL_H
#define H2_PROTOCOL_H
// =====================================================================================
//  ОБЩИЙ ПРОТОКОЛ ОБМЕНА H2  (мозг ↔ пульт)
// -------------------------------------------------------------------------------------
//  Единственный источник описания радиопакетов и параметров канала. Файл включается
//  ОБЕИМИ прошивками (H2_Brain_v5 и H2_Remote_v5), поэтому структуры CmdPacket и
//  StatusPacket физически не могут разойтись между прошивками.
//
//  ВАЖНО (особенность Arduino IDE): каждый скетч компилируется отдельно и видит только
//  файлы своей папки, поэтому КОПИЯ этого файла лежит в каждой папке скетча
//  (H2_Brain_v5\ и H2_Remote_v5\). Обе копии ОБЯЗАНЫ быть идентичными — при правке
//  протокола меняйте оба файла одновременно (или держите их синхронизированными).
// =====================================================================================
#include <Arduino.h>

// ── Трубы (адреса) nRF24 ───────────────────────────────────────────────────
const uint8_t PIPE_R2B[6] = "CMD01";   // пульт → мозг (команды)
const uint8_t PIPE_B2R[6] = "STS01";   // мозг → пульт (статус)

// ── ⚡ v5.0: Резервные каналы NRF24 (channel hopping) ──────────────────────
const uint8_t RF_CHANNELS[]  = {76, 100, 120};                          // список каналов
const uint8_t RF_CHANNEL_CNT = sizeof(RF_CHANNELS) / sizeof(RF_CHANNELS[0]);

// ── Диапазон целевого давления (для индикации и регулирования) ─────────────
const float MIN_TARGET_PRESSURE = 0.5f;   // минимум регулировки
const float MAX_TARGET_PRESSURE = 6.5f;   // максимум регулировки

struct __attribute__((packed)) CmdPacket {
  uint8_t type;   // 0x55
  uint8_t cmd;
};

// Статус мозг → пульт — v5.0: 15 байт (добавлены temp и fanDuty)
struct __attribute__((packed)) StatusPacket {
  uint8_t type;       // 0xAA
  float   pressure;
  float   target;
  uint8_t flags;      // bit0=sysOn    bit1=locked  bit2=purging(full)
                      // bit3=repulsor bit4=manual  bit5=electro
                      // bit6=valve    bit7=piezo
  uint8_t flags2;     // bit0=ovWarn  bit1=ovLock  bit2=purgeShort
                      // bit3=tiltLock bit4=tiltCal bit5=driver(0=MOS,1=REL)
                      // bit6=watchdog bit7=overheat
  uint8_t tiltAngle;  // 0..180 градусов (округлённое)
  uint8_t hystX10;    // гистерезис × 10 (0..10 → 0.0..1.0 бар)
  int8_t  temp;       // ⚡ v5.0: температура °C (округлённая, -40..125)
  uint8_t fanDuty;    // ⚡ v5.0: обороты вентилятора 0..100 %
};

#endif // H2_PROTOCOL_H
