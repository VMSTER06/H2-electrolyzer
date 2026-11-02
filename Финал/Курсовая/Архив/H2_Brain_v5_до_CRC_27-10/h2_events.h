// =====================================================================================
//  ⚡ v6.2: ЖУРНАЛ АВАРИЙ И СОБЫТИЙ МОЗГА (LittleFS /events.bin)
// -------------------------------------------------------------------------------------
//  Подключается из H2_Brain_v5.ino ПОСЛЕ h2_sound.h (использует fsOk, server,
//  timeSynced, bootCount, wsStateChanged).
//
//  Кольцевой буфер на EVT_MAX записей хранится в RAM и целиком дублируется в
//  файл при каждом событии (события редкие — износ flash ничтожен). Время:
//  epoch (если часы синхронизированы с телефона/браузера) и uptime. Записи,
//  сделанные в этой загрузке до синхронизации, получают время задним числом.
//
//  Писать (evtLog) — только из loop (ядро 1); читать JSON (/log) — веб (ядро 0):
//  общий буфер защищён мьютексом evtMutex.
// =====================================================================================
#pragma once
#include <LittleFS.h>
#include <sys/time.h>
#include <esp_system.h>

enum EvtCode : uint8_t {
  EVT_BOOT = 1,        // загрузка; value = причина сброса (esp_reset_reason)
  EVT_OVERPRESSURE,    // аварийное давление; value = бар×100
  EVT_TILT,            // наклон; value = градусы
  EVT_OVERHEAT,        // перегрев; value = °C
  EVT_NO_RISE,         // бак не набирает давление; value = бар×100
  EVT_PSENS_FAULT,     // датчик давления неисправен; value = мВ на пине
  EVT_PSENS_OK,        // датчик давления снова в норме; value = мВ
  EVT_TEMP_FAULT,      // датчик температуры не отвечает
  EVT_LEAK_WARN,       // утечка / клапан не держит; value = падение бар×100 за окно
  EVT_LEAK_CRIT,       // сильная утечка — система выключена; value = падение бар×100
  EVT_TIMER_END,       // таймер работы выключил систему; value = минут
  EVT_MAINT_DUE,       // пора обслуживать (доливка); value = часов наработки
  EVT_MAINT_DONE,      // обслуживание отмечено
  EVT_TIME_SYNC,       // часы синхронизированы
  EVT_LEAK_HOLD,       // при удержании цели MOSFET подкачивает слишком часто; value = % времени
  // ⚡ v6.3: новые коды — ТОЛЬКО в конец (коды хранятся в /events.bin и не должны сдвигаться)
  EVT_TILT_FAULT,      // 16: MPU6050 перестал отвечать в работе (TILT без данных, MOSFET запрещён)
  EVT_PURGE_FAIL,      // 17: продувка до нуля НЕ завершена (TILT или тайм-аут); value = бар×100
};
static_assert(EVT_TILT_FAULT == 16 && EVT_PURGE_FAIL == 17, "коды событий сдвинулись");

struct __attribute__((packed)) EvtRec {
  uint32_t epoch;      // unix-время, 0 — неизвестно
  uint32_t uptime;     // секунд от загрузки
  uint16_t boot;       // номер загрузки (младшие 16 бит bootCount)
  uint8_t  code;       // EvtCode
  uint8_t  pad;
  int32_t  value;
};
static_assert(sizeof(EvtRec) == 16, "EvtRec 16 байт");

const uint8_t EVT_MAX = 50;
EvtRec            evtBuf[EVT_MAX];
uint8_t           evtHead  = 0;      // куда пишется следующая запись
uint8_t           evtCount = 0;
SemaphoreHandle_t evtMutex = nullptr;
const char*       EVT_PATH = "/events.bin";
volatile uint32_t evtSeq   = 0;      // счётчик событий этой загрузки — по нему приложение
                                     // узнаёт о новых записях журнала (поле "evs" статуса)

uint32_t nowEpoch() {
  if (!timeSynced) return 0;
  struct timeval tv; gettimeofday(&tv, nullptr);
  return (uint32_t)tv.tv_sec;
}

static void evtSave() {   // под evtMutex
  if (!fsOk) return;
  File f = LittleFS.open(EVT_PATH, FILE_WRITE, true);
  if (!f) return;
  uint8_t hdr[2] = { evtHead, evtCount };
  f.write(hdr, 2);
  f.write((const uint8_t*)evtBuf, sizeof(evtBuf));
  f.close();
}

void evtInit() {
  evtMutex = xSemaphoreCreateMutex();
  memset(evtBuf, 0, sizeof(evtBuf));
  if (!fsOk || !LittleFS.exists(EVT_PATH)) return;
  File f = LittleFS.open(EVT_PATH, FILE_READ);
  if (!f) return;
  uint8_t hdr[2];
  if (f.size() == 2 + sizeof(evtBuf) && f.read(hdr, 2) == 2 && hdr[0] < EVT_MAX && hdr[1] <= EVT_MAX &&
      f.read((uint8_t*)evtBuf, sizeof(evtBuf)) == sizeof(evtBuf)) {
    evtHead = hdr[0]; evtCount = hdr[1];
  } else {
    memset(evtBuf, 0, sizeof(evtBuf));   // битый файл — журнал с нуля
  }
  f.close();
}

// Записать событие (только из loop / setup).
void evtLog(EvtCode code, int32_t value = 0) {
  EvtRec r;
  r.epoch  = nowEpoch();
  r.uptime = millis() / 1000UL;
  r.boot   = (uint16_t)bootCount;
  r.code   = code;
  r.pad    = 0;
  r.value  = value;
  if (evtMutex) xSemaphoreTake(evtMutex, portMAX_DELAY);
  evtBuf[evtHead] = r;
  evtHead = (evtHead + 1) % EVT_MAX;
  if (evtCount < EVT_MAX) evtCount++;
  evtSave();
  evtSeq = evtSeq + 1;
  if (evtMutex) xSemaphoreGive(evtMutex);
  wsStateChanged = true;
  Serial.printf("[EVT] code=%u value=%ld\n", code, (long)value);
}

// Часы только что синхронизированы: проставить время записям этой загрузки.
void evtBackfillTime() {
  uint32_t ep = nowEpoch();
  if (!ep) return;
  uint32_t up = millis() / 1000UL;
  if (evtMutex) xSemaphoreTake(evtMutex, portMAX_DELAY);
  bool changed = false;
  for (uint8_t i = 0; i < EVT_MAX; i++) {
    EvtRec& r = evtBuf[i];
    if (r.code && r.epoch == 0 && r.boot == (uint16_t)bootCount && r.uptime <= up) {
      r.epoch = ep - (up - r.uptime);
      changed = true;
    }
  }
  if (changed) evtSave();
  if (evtMutex) xSemaphoreGive(evtMutex);
}

void evtRegisterHttp() {
  // ⚡ v6.3: /log/clear регистрируется ДО /log — в ESPAsyncWebServer 3.x "/log"
  // (режим BackwardCompatible) перехватывает и /log/* по порядку регистрации, и
  // кнопка «Очистить журнал» молча получала JSON журнала.
  server.on("/log/clear", HTTP_GET, [](AsyncWebServerRequest* req) {
    if (evtMutex) xSemaphoreTake(evtMutex, portMAX_DELAY);
    memset(evtBuf, 0, sizeof(evtBuf));
    evtHead = 0; evtCount = 0;
    evtSave();
    if (evtMutex) xSemaphoreGive(evtMutex);
    wsStateChanged = true;
    req->send(200, "text/plain", "OK");
  });
  // Журнал: новые записи первыми
  server.on("/log", HTTP_GET, [](AsyncWebServerRequest* req) {
    AsyncResponseStream* rs = req->beginResponseStream("application/json");
    rs->printf("{\"boot\":%lu,\"up\":%lu,\"now\":%lu,\"list\":[",
               (unsigned long)bootCount, millis() / 1000UL, (unsigned long)nowEpoch());
    if (evtMutex) xSemaphoreTake(evtMutex, portMAX_DELAY);
    for (uint8_t k = 0; k < evtCount; k++) {
      const EvtRec& r = evtBuf[(evtHead + EVT_MAX - 1 - k) % EVT_MAX];
      rs->printf("%s{\"c\":%u,\"v\":%ld,\"e\":%lu,\"u\":%lu,\"b\":%u}", k ? "," : "",
                 r.code, (long)r.value, (unsigned long)r.epoch, (unsigned long)r.uptime, r.boot);
    }
    if (evtMutex) xSemaphoreGive(evtMutex);
    rs->print("]}");
    req->send(rs);
  });
}
