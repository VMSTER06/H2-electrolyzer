// =====================================================================================
//  ⚡ v6.8: РЕЖИМ ОТЛАДКИ — журнал нажатий и команд, телеметрия и настройки пульта
// -------------------------------------------------------------------------------------
//  Подключается из H2_Brain_v5.ino ПОСЛЕ h2_rmel.h (использует server, wsStateChanged,
//  rmelQueueCtl, sndParamInt, remoteListening).
//
//  • dbgLog() — кольцевой журнал из DBG_MAX записей «кто что нажал»: кнопки мозга,
//    кнопки пульта (пакет RSVC_BTN), команды, принятые по радио, команды веба и
//    приложения. Пишут loop (ядро 1) и веб (ядро 0) — под dbgMux.
//  • GET /dbg?since=N — записи с номером > N (приложение опрашивает в режиме отладки).
//  • Телеметрия пульта (RSVC_INFO): батарея, доля принятых пакетов, настройки — в /status.
//  • GET /rset?bat=&sleep=&con= — настройки пульта (уходят пакетом RSVC_SET).
// =====================================================================================
#pragma once

// Источник записи журнала
enum : uint8_t { DBG_SRC_BRAIN = 0, DBG_SRC_REMOTE = 1, DBG_SRC_WEB = 2, DBG_SRC_APP = 3, DBG_SRC_RADIO = 4 };
// Кнопки мозга (DBG_SRC_BRAIN)
enum : uint8_t { BBTN_POWER = 0, BBTN_LOCK = 1, BBTN_PURGE = 2 };
const uint8_t DBG_BTN_NONE = 0xFF;   // команда без кнопки (веб, приложение, радио)

struct DbgEv { uint32_t seq; uint32_t ms; uint8_t src, btn, act, cmd; };
const uint8_t DBG_MAX = 40;
static DbgEv        dbgRing[DBG_MAX];
static uint32_t     dbgSeq = 0;
static portMUX_TYPE dbgMux = portMUX_INITIALIZER_UNLOCKED;

void dbgLog(uint8_t src, uint8_t btn, uint8_t act, uint8_t cmd) {
  portENTER_CRITICAL(&dbgMux);
  dbgSeq++;
  dbgRing[dbgSeq % DBG_MAX] = { dbgSeq, (uint32_t)millis(), src, btn, act, cmd };
  portEXIT_CRITICAL(&dbgMux);
}

// ── Телеметрия пульта ──────────────────────────────────────────────────────
static RsvcInfo      rsvcInfo   = {};
static unsigned long rsvcInfoMs = 0;      // когда пришла (0 — ни разу)
static int16_t       rsvcBtnSeq = -1;     // последний принятый seq нажатия (повторы отбрасываем)
static portMUX_TYPE  rsvcMux    = portMUX_INITIALIZER_UNLOCKED;

// Пакет PKT_RSVC от пульта (loop)
void rsvcOnPacket(const uint8_t* b) {
  if (b[1] == RSVC_INFO) {
    portENTER_CRITICAL(&rsvcMux);
    memcpy(&rsvcInfo, b, sizeof(rsvcInfo));
    rsvcInfoMs = millis();
    portEXIT_CRITICAL(&rsvcMux);
  } else if (b[1] == RSVC_BTN) {
    RsvcBtn p; memcpy(&p, b, sizeof(p));
    if (p.seq == rsvcBtnSeq) return;     // ACK потерялся, пульт повторил пакет
    rsvcBtnSeq = p.seq;
    dbgLog(DBG_SRC_REMOTE, p.btn, p.act, p.cmd);
    wsStateChanged = true;
  }
}

// Поля телеметрии пульта для JSON состояния: "rinf" — возраст данных, с (−1 — нет)
int rsvcJson(char* out, size_t len) {
  RsvcInfo i; unsigned long at;
  portENTER_CRITICAL(&rsvcMux);
  i = rsvcInfo; at = rsvcInfoMs;
  portEXIT_CRITICAL(&rsvcMux);
  long age = at ? (long)((millis() - at) / 1000UL) : -1;
  return snprintf(out, len, "\"rinf\":%ld,\"rfw\":%u,\"rbmv\":%u,\"rbat\":%d,\"rrx\":%u,\"rbaten\":%u,\"rslp\":%u,\"rcon\":%u",
                  age, i.fw, i.batMv, i.batPct == RSVC_KEEP ? -1 : (int)i.batPct, i.rxPct, i.bat, i.sleepMin, i.contrast);
}

void dbgRegisterHttp() {
  // Журнал нажатий: записи с номером > since (не больше DBG_MAX последних)
  server.on("/dbg", HTTP_GET, [](AsyncWebServerRequest* req) {
    int since = 0;
    if (!sndParamInt(req, "since", since) || since < 0) { req->send(400, "text/plain", "bad since"); return; }
    DbgEv    copy[DBG_MAX];
    uint32_t last;
    portENTER_CRITICAL(&dbgMux);
    memcpy(copy, dbgRing, sizeof(copy));
    last = dbgSeq;
    portEXIT_CRITICAL(&dbgMux);
    uint32_t from = last > DBG_MAX ? last - DBG_MAX + 1 : 1;
    if ((uint32_t)since + 1 > from) from = (uint32_t)since + 1;
    uint32_t now = millis();
    AsyncResponseStream* rs = req->beginResponseStream("application/json");
    rs->printf("{\"seq\":%lu,\"ev\":[", (unsigned long)last);
    bool first = true;
    for (uint32_t s = from; s <= last; s++) {
      const DbgEv& e = copy[s % DBG_MAX];
      if (e.seq != s) continue;
      rs->printf("%s{\"s\":%lu,\"t\":%lu,\"src\":%u,\"b\":%d,\"a\":%u,\"c\":%u}", first ? "" : ",",
                 (unsigned long)e.seq, (unsigned long)(now - e.ms), e.src, e.btn == DBG_BTN_NONE ? -1 : (int)e.btn, e.act, e.cmd);
      first = false;
    }
    rs->print("]}");
    req->send(rs);
  });

  // Настройки пульта: bat=0|1, sleep=0..60 (мин, 0 — никогда), con=1..10 (любое подмножество)
  server.on("/rset", HTTP_GET, [](AsyncWebServerRequest* req) {
    int bat = RSVC_KEEP, slp = RSVC_KEEP, con = RSVC_KEEP;
    if (!sndParamInt(req, "bat", bat) || !(bat == RSVC_KEEP || bat == 0 || bat == 1) ||
        !sndParamInt(req, "sleep", slp) || !(slp == RSVC_KEEP || (slp >= 0 && slp <= 60)) ||
        !sndParamInt(req, "con", con) || !(con == RSVC_KEEP || (con >= 1 && con <= 10))) {
      req->send(400, "text/plain", "bad rset");
      return;
    }
    if (!remoteListening()) { req->send(200, "application/json", "{\"ok\":0,\"err\":\"пульт не на связи\"}"); return; }
    RsvcSet p = { PKT_RSVC, RSVC_SET, (uint8_t)bat, (uint8_t)slp, (uint8_t)con };
    req->send(200, "application/json", rmelQueueCtl(&p, sizeof(p)) ? "{\"ok\":1}" : "{\"ok\":0,\"err\":\"очередь команд пульта занята — повторите\"}");
  });
}
