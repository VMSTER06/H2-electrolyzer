// =====================================================================================
//  ⚡ v6.7: ФОРМАТ МЕЛОДИЙ H2M1 — общий для мозга и пульта
// -------------------------------------------------------------------------------------
//  ИДЕНТИЧНАЯ копия лежит в H2_Brain_v5 и H2_Remote_v5 (Arduino IDE видит только файлы
//  папки скетча) — править обе. Без зависимостей от Arduino: проверяется тестами на ПК.
//
//  Формат H2M1 (little-endian, общий с веб-конвертером и приложением):
//      0..3  'H','2','M','1'
//      4     nameLen (uint8, ≤ 48)
//      5..   name (UTF-8, nameLen байт)
//      затем uint16 noteCount (1..1200)
//      затем noteCount × { uint16 freqHz (0 или 50..10000), uint16 durMs (1..10000) }
//  Суммарная длительность ≤ 120 с, максимальный размер файла 4855 байт.
// =====================================================================================
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

struct Note { uint16_t f; uint16_t d; };   // f — Гц (0 = пауза), d — мс
static_assert(sizeof(Note) == 4, "Note: 4 байта, как в файле H2M1");

const uint8_t  MEL_NAME_MAX     = 48;       // байт UTF-8 в названии
const uint16_t MEL_NOTES_MAX    = 1200;     // нот в мелодии
const uint16_t MEL_FREQ_MIN     = 50;       // Гц
const uint16_t MEL_FREQ_MAX     = 10000;    // Гц
const uint16_t MEL_DUR_MAX      = 10000;    // мс на одну ноту
const uint32_t MEL_TOTAL_MS_MAX = 120000;   // мс на всю мелодию
const size_t   MEL_FILE_MAX     = 4 + 1 + MEL_NAME_MAX + 2 + (size_t)MEL_NOTES_MAX * 4;  // 4855

struct MelInfo {
  bool     used;                     // слот занят корректным файлом
  uint16_t n;                        // число нот
  uint32_t ms;                       // общая длительность, мс
  char     name[MEL_NAME_MAX + 1];   // название (UTF-8, с завершающим нулём)
};

inline uint16_t melRdLE16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }

// Корректный UTF-8 без нулевых байтов
inline bool melNameValid(const uint8_t* s, uint8_t len) {
  uint8_t i = 0;
  while (i < len) {
    uint8_t c = s[i], extra;
    if (c == 0)                               return false;
    if (c < 0x80)                             extra = 0;
    else if ((c & 0xE0) == 0xC0 && c >= 0xC2) extra = 1;
    else if ((c & 0xF0) == 0xE0)              extra = 2;
    else if ((c & 0xF8) == 0xF0 && c <= 0xF4) extra = 3;
    else                                      return false;
    if ((uint16_t)i + extra >= len)           return false;
    for (uint8_t k = 1; k <= extra; k++)
      if ((s[i + k] & 0xC0) != 0x80)          return false;
    i += 1 + extra;
  }
  return true;
}

// Длина префикса UTF-8-строки не длиннее max байт, не разрезающая символ
inline uint8_t melUtf8Fit(const char* s, uint8_t len, uint8_t max) {
  if (len <= max) return len;
  uint8_t n = max;
  while (n > 0 && ((uint8_t)s[n] & 0xC0) == 0x80) n--;
  return n;
}

// Строгая проверка образа H2M1 (магия, длины, ТОЧНЫЙ размер, диапазоны, общая
// длительность). nullptr — файл корректен (info заполнен), иначе текст ошибки.
inline const char* melValidate(const uint8_t* b, size_t len, MelInfo* info) {
  if (len > MEL_FILE_MAX)                      return "слишком большой файл";
  if (len < 4 + 1 + 2 || memcmp(b, "H2M1", 4) != 0)
                                               return "плохой формат: нет сигнатуры H2M1";
  uint8_t nameLen = b[4];
  if (nameLen > MEL_NAME_MAX)                  return "плохой формат: длинное название";
  size_t hdr = 5 + (size_t)nameLen + 2;
  if (len < hdr)                               return "плохой формат: файл обрезан";
  if (!melNameValid(b + 5, nameLen))           return "плохой формат: название не UTF-8";
  uint16_t cnt = melRdLE16(b + 5 + nameLen);
  if (cnt < 1 || cnt > MEL_NOTES_MAX)          return "плохой формат: число нот";
  if (len != hdr + (size_t)cnt * 4)            return "плохой формат: размер не совпадает";
  uint32_t total = 0;
  const uint8_t* p = b + hdr;
  for (uint16_t i = 0; i < cnt; i++, p += 4) {
    uint16_t f = melRdLE16(p), d = melRdLE16(p + 2);
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

// Ноты корректного образа (после melValidate): копия в out, возвращает их число
inline uint16_t melNotesFrom(const uint8_t* b, Note* out) {
  size_t   hdr = 5 + (size_t)b[4];
  uint16_t cnt = melRdLE16(b + hdr);
  const uint8_t* p = b + hdr + 2;
  for (uint16_t i = 0; i < cnt; i++, p += 4) { out[i].f = melRdLE16(p); out[i].d = melRdLE16(p + 2); }
  return cnt;
}

// CRC-32 (IEEE 802.3, как zlib) — контроль файла, переданного по радио
inline uint32_t melCrc32(const uint8_t* p, size_t len, uint32_t crc = 0) {
  crc = ~crc;
  while (len--) {
    crc ^= *p++;
    for (uint8_t k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}
