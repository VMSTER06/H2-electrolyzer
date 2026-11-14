// =====================================================================================
//  ⚡ v6.7: ПРИЁМ МЕЛОДИЙ ПУЛЬТОМ — разбор пакетов PKT_RMEL (без Arduino, тестируется на ПК)
// -------------------------------------------------------------------------------------
//  RmelReceiver собирает файл из пакетов BEGIN/DATA/END и проверяет его (CRC-32,
//  формат H2M1). Запись в LittleFS, радио и настройки — в H2_Remote_v5.ino.
//  Повтор END после завершённой передачи даёт тот же результат без повторной записи
//  (мозг повторяет END, если потерял ответ).
// =====================================================================================
#pragma once
#include "h2_protocol.h"
#include "h2_melody.h"

class RmelReceiver {
 public:
  enum Action { NONE, STORE, REPLY };   // что сделать после пакета

  // BEGIN: новая передача (прежняя незавершённая отбрасывается)
  void begin(const RmelBegin& p) {
    active_  = p.size >= 7 && p.size <= MEL_FILE_MAX;
    xfer_    = p.xfer;
    slot_    = p.slot;
    size_    = p.size;
    crc_     = p.crc;
    next_    = 0;
    gap_     = false;
    doneXfer_ = false;
  }

  // DATA: куски идут по порядку; повтор (ACK потерялся) — пропускаем, дыра — ошибка
  void data(const RmelData& p) {
    if (!active_ || p.xfer != xfer_ || p.n == 0 || p.n > RMEL_CHUNK) return;
    if (p.off < next_) return;
    if (p.off > next_ || (size_t)p.off + p.n > size_) { gap_ = true; return; }
    memcpy(buf_ + p.off, p.data, p.n);
    next_ += p.n;
  }

  // END: STORE — файл цел, сохранить его (затем finish()); REPLY — ответить result()
  Action end(const RmelEnd& p) {
    if (doneXfer_ && p.xfer == lastXfer_) return REPLY;          // повтор END
    if (!active_ || p.xfer != xfer_) { setResult(p.xfer, RMEL_E_SEQ, 0xFF); return REPLY; }
    active_ = false;
    if (gap_ || next_ != size_)               { setResult(p.xfer, RMEL_E_SEQ, 0xFF);    return REPLY; }
    if (melCrc32(buf_, size_) != crc_)        { setResult(p.xfer, RMEL_E_CRC, 0xFF);    return REPLY; }
    if (melValidate(buf_, size_, &info_))     { setResult(p.xfer, RMEL_E_FORMAT, 0xFF); return REPLY; }
    return STORE;
  }
  // Итог записи: code RMEL_OK и слот, либо ошибка
  void finish(uint8_t code, uint8_t slot) { setResult(xfer_, code, slot); }

  const uint8_t* file() const     { return buf_; }
  uint16_t       size() const     { return size_; }
  uint8_t        wantSlot() const { return slot_; }
  const MelInfo& info() const     { return info_; }
  RmelResult     result() const   { return { PKT_RMEL, RMEL_RESULT, lastXfer_, lastCode_, lastSlot_ }; }

 private:
  void setResult(uint8_t xfer, uint8_t code, uint8_t slot) {
    doneXfer_ = true; lastXfer_ = xfer; lastCode_ = code; lastSlot_ = slot;
  }
  uint8_t  buf_[MEL_FILE_MAX];
  MelInfo  info_ = {};
  bool     active_ = false, gap_ = false, doneXfer_ = false;
  uint8_t  xfer_ = 0, slot_ = 0, lastXfer_ = 0, lastCode_ = 0, lastSlot_ = 0;
  uint16_t size_ = 0, next_ = 0;
  uint32_t crc_ = 0;
};

// Первый свободный слот по маске занятых (0xFF — нет)
inline uint8_t rmelFreeSlot(uint8_t used) {
  for (uint8_t i = 0; i < RMEL_SLOTS; i++) if (!(used & (1u << i))) return i;
  return 0xFF;
}
