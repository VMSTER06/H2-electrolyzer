#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
H2 PRO — раздача общего модуля мелодий (⚡ v6.7).

Источник: Финал/web_shared/h2_mel_edit.js. Копии:
  • H2_Brain_v5/h2_web_index.h — между маркерами /*H2MEL-BEGIN*/ и /*H2MEL-END*/
    (страница мозга — один файл, внешних скриптов нет);
  • H2_Android_App/app/src/main/assets/h2_mel_edit.js — для приложения.

Запуск (из папки H2_Brain_v5):
    python tools/sync_shared_js.py          — обновить копии
    python tools/sync_shared_js.py --check  — только проверить (код 1 — устарели)
После обновления страницы мозга запустите tools/make_web_gz.py.

Только стандартная библиотека Python 3.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
FINAL = os.path.normpath(os.path.join(HERE, "..", ".."))
SRC = os.path.join(FINAL, "web_shared", "h2_mel_edit.js")
WEB = os.path.join(FINAL, "H2_Brain_v5", "h2_web_index.h")
APP = os.path.join(FINAL, "H2_Android_App", "app", "src", "main", "assets", "h2_mel_edit.js")
BEGIN, END = "/*H2MEL-BEGIN*/", "/*H2MEL-END*/"


def read(path):
    with open(path, "r", encoding="utf-8", newline="") as f:
        return f.read()


def web_with(js):
    page = read(WEB)
    i, j = page.find(BEGIN), page.find(END)
    if i < 0 or j < i:
        sys.exit("ОШИБКА: в h2_web_index.h нет маркеров %s … %s" % (BEGIN, END))
    if ')rawliteral"' in js:
        sys.exit("ОШИБКА: модуль содержит конец raw-литерала )rawliteral\"")
    return page[: i + len(BEGIN)] + "\n" + js.rstrip("\n") + "\n" + page[j:]


def main():
    check = "--check" in sys.argv[1:]
    js = read(SRC)
    targets = [(WEB, web_with(js)), (APP, js)]
    stale = [p for p, want in targets if not os.path.exists(p) or read(p) != want]
    if check:
        for p in stale:
            print("УСТАРЕЛ: %s" % os.path.relpath(p, FINAL))
        sys.exit(1 if stale else 0)
    for p, want in targets:
        if p in stale:
            with open(p, "w", encoding="utf-8", newline="") as f:
                f.write(want)
            print("обновлён: %s" % os.path.relpath(p, FINAL))
    if not stale:
        print("копии актуальны")


if __name__ == "__main__":
    main()
