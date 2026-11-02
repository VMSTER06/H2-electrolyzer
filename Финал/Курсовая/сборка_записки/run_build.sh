#!/bin/bash
cd "$(dirname "$0")"
W=$(cygpath -w "$PWD")
taskkill //F //IM WINWORD.EXE >/dev/null 2>&1
for n in kp2 full; do
  if [ $n = full ]; then spec=pz_spec.txt; else spec=pz_spec_kp2.txt; fi
  powershell -NoProfile -ExecutionPolicy Bypass -File build_pz.ps1 -Spec "$W/$spec" -Out "$W/${n}_raw.docx" -FigDir "$W/рисунки" || exit 1
  echo "${n}RAW"
done
