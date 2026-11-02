// =====================================================================================
//  ⚡ v6.7: МЕЛОДИИ ПУЛЬТА — передача по радио и HTTP /rmel/*
// -------------------------------------------------------------------------------------
//  Подключается из H2_Brain_v5.ino ПОСЛЕ h2_sound.h (использует radio, radioOk,
//  RadioLock, server, wsStateChanged, melUpBuf/melUploadChunk/melUploadTake, melCache,
//  melPath, melJsonEscape, sndParamInt, remoteListening).
//
//  Потоки:
//    • HTTP-обработчики (ядро 0) кладут запрос в очередь rmelCtlQ (служебные пакеты)
//      или готовят файл в rmelBuf и взводят rmelStartReq;
//    • rmelLoop() из loop (ядро 1) — единственный, кто передаёт по радио: шлёт служебные
//      пакеты, ведёт передачу файла кусками, ждёт результат, повторяет END;
//    • rmelOnPacket() из loop — разбор ответов пульта (опись, результат).
//  Общие поля состояния защищены rmelMux (короткие критические секции).
// =====================================================================================
#pragma once

const unsigned long RMEL_RESULT_WAIT_MS = 700;    // ожидание результата после END
const uint8_t       RMEL_END_TRIES      = 5;      // повторов END без ответа
const unsigned long RMEL_NOACK_MS       = 4000;   // столько без ACK пульта → «пульт не отвечает»
const uint8_t       RMEL_CHUNKS_PER_LOOP = 6;     // кусков за один проход loop
const unsigned long RMEL_QUERY_GAP_MS   = 1500;   // не чаще — запрос описи

// ── Опись пульта (по пакетам SUMMARY/SLOT) ─────────────────────────────────
struct RmelSlotInfo { uint16_t n; uint32_t ms; char name[23]; };
struct RmelInventory {
  bool          known;      // опись хоть раз получена
  unsigned long atMs;       // когда пришла последняя SUMMARY
  int8_t        act, boot;
  bool          fsOk;
  uint8_t       used;       // маска занятых слотов
  uint8_t       pending;    // слоты из маски, чьё описание ещё не пришло
  RmelSlotInfo  slot[RMEL_SLOTS];
};

// ── Передача файла ─────────────────────────────────────────────────────────
enum : uint8_t { RMEL_JOB_IDLE, RMEL_JOB_BEGIN, RMEL_JOB_DATA, RMEL_JOB_END, RMEL_JOB_WAIT, RMEL_JOB_DONE, RMEL_JOB_ERR };
struct RmelJob {
  uint8_t       phase;
  uint8_t       xfer;       // номер текущей передачи
  uint16_t      size, off;
  uint32_t      crc;
  unsigned long lastAck;    // последний ACK пульта в этой передаче
  uint8_t       endTries;
  unsigned long waitFrom;
  int8_t        slot;       // слот на пульте (результат)
  const char*   err;        // текст ошибки (RMEL_JOB_ERR)
  char          name[MEL_NAME_MAX + 1];
};

static portMUX_TYPE  rmelMux = portMUX_INITIALIZER_UNLOCKED;
static RmelInventory rmelInv = {};
static RmelJob       rmelJob = {};             // phase = RMEL_JOB_IDLE
static uint8_t       rmelBuf[MEL_FILE_MAX];   // файл передачи — пишет только HTTP при IDLE/DONE/ERR
static volatile bool rmelStartReq = false;    // HTTP подготовил rmelBuf — loop начинает передачу
static uint16_t      rmelReqSize  = 0;
static QueueHandle_t rmelCtlQ     = nullptr;  // служебные пакеты мозг → пульт (32 байта)
static unsigned long rmelLastQuery = 0;
static volatile bool rmelQueryReq  = false;

static const char* rmelErrText(uint8_t code) {
  switch (code) {
    case RMEL_E_SEQ:    return "пульт потерял начало передачи — повторите";
    case RMEL_E_CRC:    return "файл повреждён при передаче — повторите";
    case RMEL_E_FORMAT: return "пульт не принял формат мелодии";
    case RMEL_E_FULL:   return "на пульте нет свободных слотов (максимум 8)";
    case RMEL_E_FS:     return "ошибка памяти пульта (LittleFS)";
    default:            return "неизвестная ошибка пульта";
  }
}

// Отправить один пакет пульту (вызывается только из loop). true — ACK получен.
static bool rmelSend(const void* pkt, uint8_t len) {
  if (!radioOk) return false;
  RadioLock rl(pdMS_TO_TICKS(5));
  if (!rl.ok) return false;
  radio.stopListening();
  radio.setRetries(3, 5);          // кусок файла: несколько повторов на уровне радио
  bool ok = radio.write(pkt, len);
  radio.setRetries(0, 1);          // вернуть режим статуса (см. setup)
  radio.startListening();
  if (ok) { rfLastActivity = millis(); remoteAckMs = rfLastActivity; }
  return ok;
}

// Служебный пакет — в очередь (из HTTP, ядро 0). false — очередь полна.
static bool rmelQueueCtl(const void* pkt, uint8_t len) {
  if (!rmelCtlQ) return false;
  uint8_t b[RF_PAYLOAD_MAX] = {};
  memcpy(b, pkt, len);
  return xQueueSend(rmelCtlQ, b, 0) == pdTRUE;
}

static void rmelFail(const char* err) {
  portENTER_CRITICAL(&rmelMux);
  rmelJob.phase = RMEL_JOB_ERR;
  rmelJob.err   = err;
  portEXIT_CRITICAL(&rmelMux);
  wsStateChanged = true;
  Serial.printf("[RMEL] Ошибка передачи: %s\n", err);
}

// Ответы пульта (loop)
void rmelOnPacket(const uint8_t* b, uint8_t len) {
  if (len < 2) return;
  const uint8_t op = b[1];
  if (op == RMEL_SUMMARY && len >= sizeof(RmelSummary)) {
    RmelSummary s; memcpy(&s, b, sizeof(s));
    portENTER_CRITICAL(&rmelMux);
    rmelInv.known   = true;
    rmelInv.atMs    = millis();
    rmelInv.act     = s.act;
    rmelInv.boot    = s.boot;
    rmelInv.fsOk    = s.fsOk != 0;
    rmelInv.used    = s.used;
    rmelInv.pending = s.used;
    portEXIT_CRITICAL(&rmelMux);
    wsStateChanged = true;
  } else if (op == RMEL_SLOT && len >= sizeof(RmelSlot)) {
    RmelSlot s; memcpy(&s, b, sizeof(s));
    if (s.slot >= RMEL_SLOTS) return;
    uint8_t nl = s.nameLen <= sizeof(s.name) ? s.nameLen : 0;
    if (!melNameValid((const uint8_t*)s.name, nl)) nl = 0;
    portENTER_CRITICAL(&rmelMux);
    RmelSlotInfo& si = rmelInv.slot[s.slot];
    si.n = s.n; si.ms = s.ms;
    memcpy(si.name, s.name, nl); si.name[nl] = '\0';
    rmelInv.pending &= (uint8_t)~(1u << s.slot);
    portEXIT_CRITICAL(&rmelMux);
  } else if (op == RMEL_RESULT && len >= sizeof(RmelResult)) {
    RmelResult r; memcpy(&r, b, sizeof(r));
    bool mine = false;
    portENTER_CRITICAL(&rmelMux);
    if ((rmelJob.phase == RMEL_JOB_WAIT || rmelJob.phase == RMEL_JOB_END) && r.xfer == rmelJob.xfer) {
      mine = true;
      if (r.code == RMEL_OK) { rmelJob.phase = RMEL_JOB_DONE; rmelJob.slot = (int8_t)r.slot; }
      else                   { rmelJob.phase = RMEL_JOB_ERR;  rmelJob.err  = rmelErrText(r.code); }
    }
    portEXIT_CRITICAL(&rmelMux);
    if (mine) {
      wsStateChanged = true;
      Serial.printf("[RMEL] Результат: %s (слот %u)\n", r.code == RMEL_OK ? "OK" : rmelErrText(r.code), r.slot);
    }
  }
}

// Передача и служебные пакеты (loop, ядро 1)
void rmelLoop(unsigned long now) {
  if (!radioOk) return;
  // Новая передача, подготовленная HTTP-обработчиком
  if (rmelStartReq) {
    portENTER_CRITICAL(&rmelMux);
    rmelStartReq     = false;
    rmelJob.phase    = RMEL_JOB_BEGIN;
    rmelJob.xfer++;
    rmelJob.size     = rmelReqSize;
    rmelJob.off      = 0;
    rmelJob.lastAck  = now;
    rmelJob.endTries = 0;
    rmelJob.slot     = -1;
    rmelJob.err      = nullptr;
    portEXIT_CRITICAL(&rmelMux);
    rmelJob.crc = melCrc32(rmelBuf, rmelJob.size);
    Serial.printf("[RMEL] Передача «%s» на пульт: %u байт\n", rmelJob.name, rmelJob.size);
  }
  // Служебные пакеты — между кусками файла (не дольше одного за проход)
  uint8_t ctl[RF_PAYLOAD_MAX];
  if (rmelCtlQ && xQueuePeek(rmelCtlQ, ctl, 0) == pdTRUE) {
    if (rmelSend(ctl, RF_PAYLOAD_MAX) || !remoteListening()) xQueueReceive(rmelCtlQ, ctl, 0);
  }
  if (rmelQueryReq && now - rmelLastQuery >= RMEL_QUERY_GAP_MS) {
    rmelQueryReq  = false;
    rmelLastQuery = now;
    RmelQuery q = { PKT_RMEL, RMEL_QUERY };
    rmelQueueCtl(&q, sizeof(q));
  }

  switch (rmelJob.phase) {
    case RMEL_JOB_BEGIN: {
      RmelBegin p = { PKT_RMEL, RMEL_BEGIN, rmelJob.xfer, RMEL_AUTO_SLOT, rmelJob.size, rmelJob.crc };
      if (rmelSend(&p, sizeof(p))) { rmelJob.lastAck = now; rmelJob.phase = RMEL_JOB_DATA; }
      else if (now - rmelJob.lastAck >= RMEL_NOACK_MS) rmelFail("пульт не отвечает — он включён и рядом? Нажмите на нём любую кнопку");
      break;
    }
    case RMEL_JOB_DATA:
      for (uint8_t k = 0; k < RMEL_CHUNKS_PER_LOOP && rmelJob.phase == RMEL_JOB_DATA; k++) {
        RmelData p = {};
        p.type = PKT_RMEL; p.op = RMEL_DATA; p.xfer = rmelJob.xfer; p.off = rmelJob.off;
        uint16_t left = rmelJob.size - rmelJob.off;
        p.n = left < RMEL_CHUNK ? (uint8_t)left : RMEL_CHUNK;
        memcpy(p.data, rmelBuf + rmelJob.off, p.n);
        if (!rmelSend(&p, sizeof(p))) {
          if (now - rmelJob.lastAck >= RMEL_NOACK_MS) rmelFail("связь с пультом пропала во время передачи");
          break;                       // пульт занят (дисплей) — следующий проход
        }
        rmelJob.lastAck = now;
        rmelJob.off  += p.n;
        if (rmelJob.off >= rmelJob.size) rmelJob.phase = RMEL_JOB_END;
      }
      break;
    case RMEL_JOB_END: {
      RmelEnd p = { PKT_RMEL, RMEL_END, rmelJob.xfer };
      if (rmelSend(&p, sizeof(p))) {
        rmelJob.lastAck = now;
        rmelJob.endTries++;
        rmelJob.waitFrom = now;
        portENTER_CRITICAL(&rmelMux);
        if (rmelJob.phase == RMEL_JOB_END) rmelJob.phase = RMEL_JOB_WAIT;   // результат мог уже прийти
        portEXIT_CRITICAL(&rmelMux);
      } else if (now - rmelJob.lastAck >= RMEL_NOACK_MS) rmelFail("пульт не подтвердил конец передачи");
      break;
    }
    case RMEL_JOB_WAIT:
      if (now - rmelJob.waitFrom >= RMEL_RESULT_WAIT_MS) {
        if (rmelJob.endTries >= RMEL_END_TRIES) rmelFail("пульт не прислал результат");
        else rmelJob.phase = RMEL_JOB_END;   // повтор END: пульт ответит результатом ещё раз
      }
      break;
    default:
      break;
  }
}

static bool rmelBusy() {
  uint8_t ph = rmelJob.phase;
  return rmelStartReq || (ph != RMEL_JOB_IDLE && ph != RMEL_JOB_DONE && ph != RMEL_JOB_ERR);
}

// Начать передачу файла (HTTP, ядро 0). nullptr — передача поставлена.
static const char* rmelStart(const uint8_t* file, size_t len) {
  MelInfo info;
  const char* err = melValidate(file, len, &info);
  if (err) return err;
  if (!radioOk)           return "радиомодуль мозга не работает";
  if (!remoteListening()) return "пульт не на связи — включите его или нажмите любую кнопку";
  if (rmelBusy())         return "идёт другая передача — дождитесь окончания";
  if (rmelInv.known && rmelInv.used == (uint8_t)((1u << RMEL_SLOTS) - 1)) return "на пульте все 8 слотов заняты — удалите ненужную";
  if (file != rmelBuf) memcpy(rmelBuf, file, len);   // /rmel/send читает файл прямо в rmelBuf
  portENTER_CRITICAL(&rmelMux);
  memcpy(rmelJob.name, info.name, sizeof(rmelJob.name));
  rmelReqSize  = (uint16_t)len;
  rmelStartReq = true;
  portEXIT_CRITICAL(&rmelMux);
  return nullptr;
}

// Запросить опись, если она неполная или устарела (вызывается при /rmel/cfg)
static void rmelMaybeQuery() {
  bool need;
  portENTER_CRITICAL(&rmelMux);
  need = !rmelInv.known || rmelInv.pending || millis() - rmelInv.atMs > 10000;
  portEXIT_CRITICAL(&rmelMux);
  if (need && remoteListening()) rmelQueryReq = true;
}

void rmelInit() {
  rmelCtlQ = xQueueCreate(6, RF_PAYLOAD_MAX);
}

static void rmelJsonReply(AsyncWebServerRequest* req, const char* err) {
  char out[200];
  if (err) snprintf(out, sizeof(out), "{\"ok\":0,\"err\":\"%s\"}", err);
  else     snprintf(out, sizeof(out), "{\"ok\":1}");
  req->send(200, "application/json", out);
}

void rmelRegisterHttp() {
  // Опись пульта + состояние передачи
  server.on("/rmel/cfg", HTTP_GET, [](AsyncWebServerRequest* req) {
    rmelMaybeQuery();
    RmelInventory inv;
    RmelJob       job;
    portENTER_CRITICAL(&rmelMux);
    inv = rmelInv;
    job = rmelJob;
    portEXIT_CRITICAL(&rmelMux);
    const char* ph = "idle";
    switch (job.phase) {
      case RMEL_JOB_BEGIN: case RMEL_JOB_DATA: ph = "send"; break;
      case RMEL_JOB_END: case RMEL_JOB_WAIT:   ph = "check"; break;
      case RMEL_JOB_DONE: ph = "done"; break;
      case RMEL_JOB_ERR:  ph = "err";  break;
    }
    if (rmelStartReq) ph = "send";
    unsigned pct = job.size ? (unsigned)(100UL * job.off / job.size) : 0;
    AsyncResponseStream* rs = req->beginResponseStream("application/json");
    rs->printf("{\"online\":%d,\"known\":%d,\"complete\":%d,\"fsok\":%d,\"act\":%d,\"boot\":%d,\"slots\":%u,"
               "\"job\":{\"st\":\"%s\",\"pct\":%u,\"slot\":%d,\"err\":\"%s\"},\"list\":[",
               remoteListening() ? 1 : 0, inv.known ? 1 : 0, (inv.known && !inv.pending) ? 1 : 0,
               inv.fsOk ? 1 : 0, (int)inv.act, (int)inv.boot, (unsigned)RMEL_SLOTS,
               ph, pct, (int)job.slot, job.err ? job.err : "");
    bool first = true;
    for (uint8_t i = 0; i < RMEL_SLOTS; i++) {
      if (!(inv.used & (1u << i))) continue;
      char esc[sizeof(inv.slot[i].name) * 6 + 1];
      bool have = !(inv.pending & (1u << i));
      if (have && inv.slot[i].name[0]) melJsonEscape(inv.slot[i].name, esc, sizeof(esc));
      else snprintf(esc, sizeof(esc), "Мелодия %u", i + 1);
      rs->printf("%s{\"id\":%u,\"name\":\"%s\",\"n\":%u,\"ms\":%lu}", first ? "" : ",", i, esc,
                 have ? inv.slot[i].n : 0, have ? (unsigned long)inv.slot[i].ms : 0UL);
      first = false;
    }
    rs->print("]}");
    req->send(rs);
  });

  // Загрузить файл H2M1 прямо в пульт (multipart, поле "f")
  server.on("/rmel/up", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      const char* err = melUploadTake(req);
      if (!err) err = rmelStart(melUpBuf, melUp.len);
      rmelJsonReply(req, err);
    },
    melUploadChunk);

  // Отправить в пульт мелодию из библиотеки мозга
  server.on("/rmel/send", HTTP_GET, [](AsyncWebServerRequest* req) {
    int id = -1;
    if (!req->hasParam("id") || !sndParamInt(req, "id", id) || !melSlotUsed(id)) {
      req->send(404, "text/plain", "no melody");
      return;
    }
    if (rmelBusy()) { rmelJsonReply(req, "идёт другая передача — дождитесь окончания"); return; }
    char path[20]; melPath((uint8_t)id, path, sizeof(path));
    File f = LittleFS.open(path, FILE_READ);
    size_t sz = f ? f.size() : 0;
    // rmelBuf свободен (передачи нет) — читаем прямо в него
    bool ok = f && sz <= MEL_FILE_MAX && f.read(rmelBuf, sz) == sz;
    if (f) f.close();
    rmelJsonReply(req, ok ? rmelStart(rmelBuf, sz) : "не удалось прочитать мелодию мозга");
  });

  // Удалить слот на пульте
  server.on("/rmel/del", HTTP_GET, [](AsyncWebServerRequest* req) {
    int id = -1;
    if (!req->hasParam("id") || !sndParamInt(req, "id", id) || id < 0 || id >= RMEL_SLOTS) {
      req->send(400, "text/plain", "bad id");
      return;
    }
    if (!remoteListening()) { rmelJsonReply(req, "пульт не на связи"); return; }
    RmelDel p = { PKT_RMEL, RMEL_DEL, (uint8_t)id };
    rmelJsonReply(req, rmelQueueCtl(&p, sizeof(p)) ? nullptr : "очередь команд пульта занята — повторите");
  });

  // Активная мелодия кнопки МЕЛОДИЯ (act: -1 «Бумер» или слот) и звук загрузки (boot: -9 или слот)
  server.on("/rmel/set", HTTP_GET, [](AsyncWebServerRequest* req) {
    int act = rmelInv.act, boot = rmelInv.boot;
    if (!sndParamInt(req, "act", act) || !(act == RMEL_BOOMER || (act >= 0 && act < RMEL_SLOTS)) ||
        !sndParamInt(req, "boot", boot) || !(boot == RMEL_NONE || (boot >= 0 && boot < RMEL_SLOTS))) {
      req->send(400, "text/plain", "bad act/boot");
      return;
    }
    if (!remoteListening()) { rmelJsonReply(req, "пульт не на связи"); return; }
    RmelCfg p = { PKT_RMEL, RMEL_CFG, (int8_t)act, (int8_t)boot };
    rmelJsonReply(req, rmelQueueCtl(&p, sizeof(p)) ? nullptr : "очередь команд пульта занята — повторите");
  });
}
