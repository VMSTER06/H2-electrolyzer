// Тесты приёма мелодий пультом (remote_rmel.h) и формата H2M1 (h2_melody.h) на ПК.
// Сборка и запуск: см. run_tests.sh
#include <cstdio>
#include <vector>
#include <string>
#include "../H2_Remote_v5/remote_rmel.h"

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static std::vector<uint8_t> h2m(const std::string& name, const std::vector<Note>& notes) {
  std::vector<uint8_t> b = {'H', '2', 'M', '1', (uint8_t)name.size()};
  b.insert(b.end(), name.begin(), name.end());
  b.push_back(notes.size() & 0xFF); b.push_back(notes.size() >> 8);
  for (auto n : notes) { b.push_back(n.f & 0xFF); b.push_back(n.f >> 8); b.push_back(n.d & 0xFF); b.push_back(n.d >> 8); }
  return b;
}

static void sendAll(RmelReceiver& rx, const std::vector<uint8_t>& f, uint8_t xfer, bool dupEach = false) {
  RmelBegin b = { PKT_RMEL, RMEL_BEGIN, xfer, RMEL_AUTO_SLOT, (uint16_t)f.size(), melCrc32(f.data(), f.size()) };
  rx.begin(b);
  for (size_t off = 0; off < f.size(); off += RMEL_CHUNK) {
    RmelData d = {};
    d.type = PKT_RMEL; d.op = RMEL_DATA; d.xfer = xfer; d.off = (uint16_t)off;
    d.n = (uint8_t)std::min<size_t>(RMEL_CHUNK, f.size() - off);
    memcpy(d.data, f.data() + off, d.n);
    rx.data(d);
    if (dupEach) rx.data(d);            // потерянный ACK — мозг повторил кусок
  }
}

int main() {
  // CRC-32 — как у zlib
  CHECK(melCrc32((const uint8_t*)"123456789", 9) == 0xCBF43926u);

  // Корректный файл с русским названием
  std::vector<Note> notes;
  for (int i = 0; i < 300; i++) notes.push_back({(uint16_t)(i % 3 ? 1000 + i : 0), 40});
  auto f = h2m("Мелодия тест", notes);
  MelInfo mi;
  CHECK(melValidate(f.data(), f.size(), &mi) == nullptr && mi.n == 300 && mi.ms == 12000);
  Note out[MEL_NOTES_MAX];
  CHECK(melNotesFrom(f.data(), out) == 300 && out[1].f == 1001 && out[299].d == 40);

  // Передача целиком, с дублями кусков
  RmelReceiver rx;
  sendAll(rx, f, 7, true);
  RmelEnd e = { PKT_RMEL, RMEL_END, 7 };
  CHECK(rx.end(e) == RmelReceiver::STORE);
  CHECK(rx.size() == f.size() && memcmp(rx.file(), f.data(), f.size()) == 0);
  rx.finish(RMEL_OK, 3);
  RmelResult r = rx.result();
  CHECK(r.xfer == 7 && r.code == RMEL_OK && r.slot == 3);
  // Повтор END — тот же результат, без повторной записи
  CHECK(rx.end(e) == RmelReceiver::REPLY && rx.result().slot == 3);

  // Испорченный байт → CRC
  {
    RmelReceiver q;
    auto g = f;
    RmelBegin b = { PKT_RMEL, RMEL_BEGIN, 8, RMEL_AUTO_SLOT, (uint16_t)g.size(), melCrc32(g.data(), g.size()) };
    q.begin(b);
    g[40] ^= 0x55;
    for (size_t off = 0; off < g.size(); off += RMEL_CHUNK) {
      RmelData d = {}; d.type = PKT_RMEL; d.op = RMEL_DATA; d.xfer = 8; d.off = (uint16_t)off;
      d.n = (uint8_t)std::min<size_t>(RMEL_CHUNK, g.size() - off); memcpy(d.data, g.data() + off, d.n); q.data(d);
    }
    RmelEnd e2 = { PKT_RMEL, RMEL_END, 8 };
    CHECK(q.end(e2) == RmelReceiver::REPLY && q.result().code == RMEL_E_CRC);
  }
  // Пропущенный кусок → SEQ
  {
    RmelReceiver q;
    RmelBegin b = { PKT_RMEL, RMEL_BEGIN, 9, RMEL_AUTO_SLOT, (uint16_t)f.size(), melCrc32(f.data(), f.size()) };
    q.begin(b);
    for (size_t off = 0; off < f.size(); off += RMEL_CHUNK) {
      if (off == RMEL_CHUNK * 2) continue;
      RmelData d = {}; d.type = PKT_RMEL; d.op = RMEL_DATA; d.xfer = 9; d.off = (uint16_t)off;
      d.n = (uint8_t)std::min<size_t>(RMEL_CHUNK, f.size() - off); memcpy(d.data, f.data() + off, d.n); q.data(d);
    }
    RmelEnd e2 = { PKT_RMEL, RMEL_END, 9 };
    CHECK(q.end(e2) == RmelReceiver::REPLY && q.result().code == RMEL_E_SEQ);
  }
  // END без BEGIN / чужая передача → SEQ
  {
    RmelReceiver q;
    RmelEnd e2 = { PKT_RMEL, RMEL_END, 1 };
    CHECK(q.end(e2) == RmelReceiver::REPLY && q.result().code == RMEL_E_SEQ);
  }
  // Битый формат (частота 20 Гц) при верном CRC → FORMAT
  {
    auto g = h2m("x", {{20, 100}});
    RmelReceiver q; sendAll(q, g, 10);
    RmelEnd e2 = { PKT_RMEL, RMEL_END, 10 };
    CHECK(q.end(e2) == RmelReceiver::REPLY && q.result().code == RMEL_E_FORMAT);
  }
  // Кусок за пределами файла игнорируется как ошибка
  {
    RmelReceiver q;
    RmelBegin b = { PKT_RMEL, RMEL_BEGIN, 11, RMEL_AUTO_SLOT, 10, 0 };
    q.begin(b);
    RmelData d = {}; d.type = PKT_RMEL; d.op = RMEL_DATA; d.xfer = 11; d.off = 0; d.n = 26; q.data(d);
    RmelEnd e2 = { PKT_RMEL, RMEL_END, 11 };
    CHECK(q.end(e2) == RmelReceiver::REPLY && q.result().code == RMEL_E_SEQ);
  }
  // Свободный слот и укорочение названия по границе UTF-8
  CHECK(rmelFreeSlot(0x00) == 0 && rmelFreeSlot(0x0B) == 2 && rmelFreeSlot(0xFF) == 0xFF);
  const char* ru = "ЖЖЖЖЖЖЖЖЖЖЖЖ";   // 24 байта
  CHECK(melUtf8Fit(ru, 24, 22) == 22);
  CHECK(melUtf8Fit(ru, 24, 21) == 20);
  CHECK(melUtf8Fit("abc", 3, 22) == 3);

  printf(fails ? "rmel_test: %d FAIL\n" : "rmel_test: PASS\n", fails);
  return fails ? 1 : 0;
}
