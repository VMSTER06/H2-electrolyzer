// Запуск тестов игры пульта (Remote_game_2026-10-29/tests/game_test.cpp) на ПК
#include <cstdio>
extern "C" int test_clicks(); extern "C" int test_dino(); extern "C" int test_persistent_record();
int main() {
  int a = test_clicks(), b = test_dino(), c = test_persistent_record();
  printf("game_test: clicks %s, dino %s, record %s\n", a ? "FAIL" : "PASS", b ? "FAIL" : "PASS", c ? "FAIL" : "PASS");
  return a || b || c;
}
