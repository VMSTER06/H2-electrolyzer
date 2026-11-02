#!/bin/bash
# Полная сборка: записка КП2 (разделы 1–3, приложения А, Б) и полная записка с листами чертежей.
# Usage: bash build_all.sh <папка_сборки> <папка_с_png_листов>
# В папке листов должны быть: E1n.png E2n.png E3_1.png E3_2.png pd1.png doc_ПЭ3_1.png doc_ПЭ3_2.png doc_Д1.png
set -e
SRC="$(cd "$(dirname "$0")" && pwd)"
B="$1"; IMG="$2"
mkdir -p "$B"; rm -f "$B"/*.docx "$B"/*.pdf
cp "$SRC/pz_spec.txt" "$SRC/pz_spec_kp2.txt" "$SRC/build_pz.ps1" "$SRC/upd.ps1" "$SRC/insert_sheets.py" "$B/"
rm -rf "$B/рисунки"; cp -r "$SRC/рисунки" "$B/"
cp "$IMG/E1n.png" "$B/e1.png"; cp "$IMG/E2n.png" "$B/e2.png"
cp "$IMG/E3_1.png" "$B/e3_1.png"; cp "$IMG/E3_2.png" "$B/e3_2.png"; cp "$IMG/pd1.png" "$B/pd1.png"
cp "$IMG/doc_ПЭ3_1.png" "$B/pe_1.png"; cp "$IMG/doc_ПЭ3_2.png" "$B/pe_2.png"; cp "$IMG/doc_Д1.png" "$B/d1.png"
cd "$B"
W=$(cygpath -w "$PWD")
for n in kp2 full; do
  if [ $n = full ]; then spec=pz_spec.txt; else spec=pz_spec_kp2.txt; fi
  sleep 10
  powershell -NoProfile -ExecutionPolicy Bypass -File build_pz.ps1 -Spec "$W/$spec" -Out "$W/${n}_raw.docx" -FigDir "$W/рисунки"
done
sleep 10
py -3 insert_sheets.py kp2_raw.docx kp2_s.docx "А=e1.png:A3" "Б=e2.png:A3"
powershell -NoProfile -ExecutionPolicy Bypass -File upd.ps1 -Doc "$W/kp2_s.docx" -OutDoc "$W/kp2.docx" -Pdf "$W/kp2.pdf"
sleep 10
py -3 insert_sheets.py full_raw.docx full_s.docx "А=e1.png:A3" "Б=e2.png:A3" "В=e3_1.png;e3_2.png:A3" "Г=pe_1.png;pe_2.png:A4" "Д=pd1.png:A3" "Ж=d1.png:A4"
powershell -NoProfile -ExecutionPolicy Bypass -File upd.ps1 -Doc "$W/full_s.docx" -OutDoc "$W/full.docx" -Pdf "$W/full.pdf"
echo FINISHED
