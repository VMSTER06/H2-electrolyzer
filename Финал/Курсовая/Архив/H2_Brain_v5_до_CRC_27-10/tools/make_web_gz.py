#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
H2 PRO — генератор сжатой главной страницы мозга (⚡ v6.3).

Читает сырой литерал INDEX_HTML из ../h2_web_index.h (исходник веб-интерфейса,
его и правим) и пишет ../h2_web_index_gz.h:

    const uint8_t INDEX_HTML_GZ[] PROGMEM = { ... };   // gzip-образ страницы
    const size_t  INDEX_HTML_GZ_LEN = N;

Скетч подключает ТОЛЬКО h2_web_index_gz.h и отдаёт страницу по GET / прямо из
флеша (без копии в кучу) с заголовком Content-Encoding: gzip.

После КАЖДОЙ правки h2_web_index.h запустите:
    python tools/make_web_gz.py            (из папки H2_Brain_v5)
Проверить, что сжатый файл соответствует исходнику (ничего не пишет):
    python tools/make_web_gz.py --check    (код возврата 1 — файл устарел)

Только стандартная библиотека Python 3. Результат детерминирован (mtime=0 в
заголовке gzip), поэтому повторный запуск без правок даёт тот же файл.
"""
import gzip
import hashlib
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SKETCH = os.path.normpath(os.path.join(HERE, ".."))
SRC = os.path.join(SKETCH, "h2_web_index.h")
DST = os.path.join(SKETCH, "h2_web_index_gz.h")

# const char INDEX_HTML[] PROGMEM = R"delim( ... )delim";
_START = re.compile(rb'INDEX_HTML\s*\[\s*\]\s*PROGMEM\s*=\s*R"([^()\\\s]{0,16})\(')


def read_literal(path):
    """Содержимое raw-литерала INDEX_HTML — ровно те байты, что видит компилятор."""
    with open(path, "rb") as f:
        text = f.read()
    m = _START.search(text)
    if not m:
        sys.exit("ОШИБКА: в %s не найден литерал INDEX_HTML[] PROGMEM = R\"...(" % path)
    delim = m.group(1)
    begin = m.end()
    end = text.find(b")" + delim + b'"', begin)
    if end < 0:
        sys.exit("ОШИБКА: не найден конец литерала )%s\"" % delim.decode("ascii", "replace"))
    body = text[begin:end]
    # Компилятор переводит конец строки исходника (CRLF) в \n и внутри raw-литерала
    body = body.replace(b"\r\n", b"\n")
    try:
        body.decode("utf-8")
    except UnicodeDecodeError as e:
        sys.exit("ОШИБКА: литерал не в UTF-8: %s" % e)
    return body


def build_header(html):
    gz = gzip.compress(html, compresslevel=9, mtime=0)
    if gzip.decompress(gz) != html:
        sys.exit("ОШИБКА: проверка распаковки не прошла")
    sha = hashlib.sha256(html).hexdigest()
    out = []
    out.append("// =====================================================================================")
    out.append("//  H2 PRO — главная страница веб-интерфейса, СЖАТАЯ gzip (⚡ v6.3)")
    out.append("//  АВТОМАТИЧЕСКИ СОЗДАН tools/make_web_gz.py из h2_web_index.h — НЕ ПРАВИТЬ ВРУЧНУЮ.")
    out.append("//  Правьте h2_web_index.h и запускайте:  python tools/make_web_gz.py")
    out.append("//  Исходник: %d байт, gzip: %d байт, sha256 исходника:" % (len(html), len(gz)))
    out.append("//  %s" % sha)
    out.append("// =====================================================================================")
    out.append("#pragma once")
    out.append("#include <Arduino.h>")
    out.append("")
    out.append("const size_t INDEX_HTML_GZ_LEN = %d;" % len(gz))
    out.append("const uint8_t INDEX_HTML_GZ[] PROGMEM = {")
    for i in range(0, len(gz), 16):
        out.append("  " + ",".join("0x%02x" % b for b in gz[i:i + 16]) + ",")
    out.append("};")
    out.append('static_assert(sizeof(INDEX_HTML_GZ) == INDEX_HTML_GZ_LEN, "INDEX_HTML_GZ_LEN");')
    out.append("")
    return "\n".join(out), len(gz), sha


def main():
    check = "--check" in sys.argv[1:]
    html = read_literal(SRC)
    text, gz_len, sha = build_header(html)
    data = text.encode("utf-8")              # UTF-8 без BOM, концы строк LF
    old = None
    if os.path.exists(DST):
        with open(DST, "rb") as f:
            old = f.read()
    if check:
        if old == data:
            print("OK: h2_web_index_gz.h соответствует h2_web_index.h (sha256 %s)" % sha[:16])
            return 0
        print("УСТАРЕЛ: h2_web_index_gz.h не соответствует h2_web_index.h — запустите tools/make_web_gz.py")
        return 1
    if old == data:
        print("Без изменений: %s (%d -> %d байт gzip)" % (os.path.basename(DST), len(html), gz_len))
        return 0
    with open(DST, "wb") as f:
        f.write(data)
    print("Записан %s: %d байт HTML -> %d байт gzip (%.1f%%)"
          % (os.path.basename(DST), len(html), gz_len, 100.0 * gz_len / len(html)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
