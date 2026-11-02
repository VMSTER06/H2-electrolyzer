Чертёж Э1 (лист А3 по ЕСКД: рамка 5/20 мм, основная надпись 185x55 по ГОСТ 2.104 форма 1).
Данные штампа и все блоки схемы - в e1_sheet.json (поля "stamp": designation, developer, checker, org ...).
Перерисовать (PowerShell, скрипт сохранять с BOM UTF-8):
  powershell -ExecutionPolicy Bypass -File draw_sheet.ps1 -Spec e1_sheet.json -Out E1_A3.png
  powershell -ExecutionPolicy Bypass -File png_to_pdf_a3.ps1 -Png E1_A3.png -Pdf E1_A3.pdf
Координаты в JSON - миллиметры листа; шрифт Arial (ГОСТ 2.304 в системе нет; при наличии
шрифта GOST type B поменяйте "font_family").
