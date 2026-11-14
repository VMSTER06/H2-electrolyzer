#!/bin/sh
# Проверка синтаксиса прошивок мозга и пульта на ПК (g++ + заглушки ESP32/Arduino в esp32_stub/).
# Ловит опечатки, несовпадение типов и забытые объявления. НЕ заменяет сборку в Arduino IDE:
# заглушки повторяют только используемую часть API библиотек.
set -e
cd "$(dirname "$0")"
out="${TMPDIR:-/tmp}/h2_compile"; rm -rf "$out"; mkdir -p "$out"
for sk in H2_Brain_v5 H2_Remote_v5; do
  mkdir -p "$out/$sk"
  cp ../$sk/*.h "$out/$sk/"
  rm -f "$out/$sk/h2_web_index.h"          # исходник страницы, в сборку не входит
  { cat ../$sk/$sk.ino; printf '\nSerialC Serial; TwoWire Wire; SPIc SPI; LittleFSc LittleFS;\n'; } > "$out/$sk/main.cpp"
  [ "$sk" = H2_Brain_v5 ] && printf 'WiFiC WiFi; UpdateC Update; EspC ESP;\n' >> "$out/$sk/main.cpp"
  g++ -std=gnu++17 -w -Iesp32_stub -I"$out/$sk" -fsyntax-only "$out/$sk/main.cpp"
  echo "$sk: синтаксис OK"
done
