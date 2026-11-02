#!/bin/bash
# ПОЛНЫЙ ЦИКЛ ОДНОЙ КОМАНДОЙ: листинг → схемы → записки (полная и КП3) → Д1 с верным числом
# страниц → листы задания в PDF → раскладка по папкам и zip → полная автопроверка.
# Usage: bash make_all.sh [папка_сборки=_build/cur]
# Word не убивается (могут быть открыты документы пользователя).
set -e
cd "$(dirname "$0")"
HERE="$PWD"; B="${1:-_build/cur}"; S=_sheets
POP="/c/Users/vladm/AppData/Local/Microsoft/WinGet/Packages/oschwartz10612.Poppler_Microsoft.Winget.Source_8wekyb3d8bbwe/poppler-25.07.0/Library/bin"
step() { echo "== $*"; }

step 'листинг без веба'; PYTHONIOENCODING=utf-8 py -3 make_listing_src.py | grep -E 'ОСТАЛОСЬ|ok' | grep -v sndSeq || true
step 'схемы'
( cd чертежи
  py -3 draw_e12.py e1_a3.json e1 ../$S/E1n >/dev/null
  py -3 draw_e12.py e2_a3.json e2 ../$S/E2n >/dev/null && py -3 json_to_drawio2.py e2_a3.json e2 ../$S/E2.drawio >/dev/null
  py -3 draw_e3.py ../$S/E3 >/dev/null
  py -3 draw_pd1.py ../$S/pd1 >/dev/null
  py -3 draw_docs.py ../$S/doc "$(cat ../$S/doc_Д1.pages 2>/dev/null)" >/dev/null
  py -3 draw_stickers.py ../$S/stickers >/dev/null )
step 'спецификация КП3'; py -3 make_kp_stage.py pz_spec.txt pz_spec_kp3.txt 4 А,Б,В,Г >/dev/null

mkdir -p "$B"; cd "$B"
cp "$HERE"/{pz_spec.txt,pz_spec_kp3.txt,build_pz.ps1,upd.ps1,insert_sheets.py} .
rm -rf ./рисунки; cp -r "$HERE/рисунки" .
cp ../../$S/E1n.png e1.png; cp ../../$S/E2n.png e2.png; cp ../../$S/E3_1.png e3_1.png; cp ../../$S/E3_2.png e3_2.png
cp ../../$S/pd1.png pd1.png; cp ../../$S/doc_ПЭ3_1.png pe_1.png; cp ../../$S/doc_ПЭ3_2.png pe_2.png; cp ../../$S/doc_Д1.png d1.png
W=$(cygpath -w "$PWD")
build() { powershell -NoProfile -ExecutionPolicy Bypass -File build_pz.ps1 -Spec "$W/$1" -Out "$W/$2_raw.docx" -FigDir "$W/рисунки" 2>&1 | grep -E 'TOTAL|FAIL' ; sleep 6; }
step 'Word: полная записка'; build pz_spec.txt full
step 'Word: КП3'; build pz_spec_kp3.txt kp3
FULLSHEETS=("А=e1.png:A3" "Б=e2.png:A3" "В=e3_1.png;e3_2.png:A3" "Г=pe_1.png;pe_2.png:A4" "Д=pd1.png:A3" "Ж=d1.png:A4")
finish_full() { py -3 insert_sheets.py full_raw.docx full_s.docx "${FULLSHEETS[@]}" >/dev/null
  powershell -NoProfile -ExecutionPolicy Bypass -File upd.ps1 -Doc "$W/full_s.docx" -OutDoc "$W/full.docx" -Pdf "$W/full.pdf" | grep -E 'continuations|pages'; sleep 4; }
step 'листы в полную записку'; finish_full
PAGES=$(( $("$POP/pdfinfo" full.pdf | awk '/^Pages:/{print $2}') + 2 ))
if [ "$PAGES" != "$(cat ../../$S/doc_Д1.pages 2>/dev/null)" ]; then
  step "Д1: записка $PAGES с. – перерисовка и повторная вставка"
  ( cd "$HERE/чертежи" && py -3 draw_docs.py ../$S/doc "$PAGES" >/dev/null )
  cp ../../$S/doc_Д1.png d1.png; finish_full
fi
step 'листы в КП3'
py -3 insert_sheets.py kp3_raw.docx kp3_s.docx "А=e1.png:A3" "Б=e2.png:A3" "В=e3_1.png;e3_2.png:A3" "Г=pe_1.png;pe_2.png:A4" >/dev/null
powershell -NoProfile -ExecutionPolicy Bypass -File upd.ps1 -Doc "$W/kp3_s.docx" -OutDoc "$W/kp3.docx" -Pdf "$W/kp3.pdf" | grep -E 'continuations|pages'
cd "$HERE"

step 'листы задания в PDF (если docx новее)'
for f in "../ГОТОВО_курсовая_H2/4_Лист задания" "../ГОТОВО_курсовая_H2/3_КП3_сдача/434701_Мурачев_ВК_КП3/Лист задания" "../434701_Мурачев_ВК_КП3/Лист задания"; do
  if [ "$f.docx" -nt "$f.pdf" ]; then sleep 4; powershell -NoProfile -ExecutionPolicy Bypass -File pdf_export.ps1 -Doc "$(cygpath -w "$f.docx")" -Pdf "$(cygpath -w "$f.pdf")"; fi
done
step 'раскладка и zip'; py -3 deliver.py "$B"
sed -i -E "s/по СТП, [0-9]+ с\./по СТП, $(cat $S/doc_Д1.pages) с./" ../ГОТОВО_курсовая_H2/ПРОЧТИ.txt
VOL=$(py -3 stp_pdf_check.py "$B/full.pdf" | sed -nE 's/.*введения \(с\.([0-9]+)\) до заключения \(с\.([0-9]+)\): ([0-9]+) с\..*/\1 \2 \3/p')
set -- $VOL
[ -n "$3" ] && sed -i -E "s/От введения \(с\. [0-9]+\) до заключения \(с\. [0-9]+\) — [0-9]+ страниц/От введения (с. $1) до заключения (с. $2) — $3 страниц/" ../ГОТОВО_курсовая_H2/ПРОЧТИ.txt
step 'ПРОВЕРКА'; py -3 check_all.py "$B"
