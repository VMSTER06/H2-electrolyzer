#!/bin/bash
cd "$(dirname "$0")"
W=$(cygpath -w "$PWD")
py -3 insert_sheets.py kp2_raw.docx kp2_s.docx "А=e1.png:A3" "Б=e2.png:A3" || exit 1
powershell -NoProfile -ExecutionPolicy Bypass -File upd.ps1 -Doc "$W/kp2_s.docx" -OutDoc "$W/kp2.docx" -Pdf "$W/kp2.pdf" || exit 1
py -3 insert_sheets.py full_raw.docx full_s.docx "А=e1.png:A3" "Б=e2.png:A3" "В=e3_1.png;e3_2.png:A3" "Г=pe_1.png;pe_2.png:A4" "Ж=d1.png:A4" || exit 1
powershell -NoProfile -ExecutionPolicy Bypass -File upd.ps1 -Doc "$W/full_s.docx" -OutDoc "$W/full.docx" -Pdf "$W/full.pdf" || exit 1
echo FINISHED
