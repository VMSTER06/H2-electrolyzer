#!/bin/sh
# Тесты общих заголовков прошивок на ПК: g++ (Linux/macOS/MSYS2). Без ESP32 и Arduino IDE.
set -e
cd "$(dirname "$0")"
out="${TMPDIR:-/tmp}/h2_tests"; mkdir -p "$out"
g++ -std=gnu++17 -Wall -Wextra -Istub rmel_test.cpp -o "$out/rmel_test" && "$out/rmel_test"
g++ -std=gnu++17 -Wall -Wextra ui_test.cpp -o "$out/ui_test" && "$out/ui_test"
g++ -std=gnu++17 -Wall -Wextra ../Remote_game_2026-10-29/tests/game_test.cpp game_main.cpp -o "$out/game_test" && "$out/game_test"
