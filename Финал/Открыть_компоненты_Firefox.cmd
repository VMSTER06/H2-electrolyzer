@echo off
setlocal
rem Opens the component shortlist and alternative offers in one Firefox window.
rem This file only opens web pages; it does not place orders.

set "ELECTRO_FIREFOX="
if exist "%ProgramFiles%\Mozilla Firefox\firefox.exe" set "ELECTRO_FIREFOX=%ProgramFiles%\Mozilla Firefox\firefox.exe"
if not defined ELECTRO_FIREFOX if exist "%ProgramFiles(x86)%\Mozilla Firefox\firefox.exe" set "ELECTRO_FIREFOX=%ProgramFiles(x86)%\Mozilla Firefox\firefox.exe"
if not defined ELECTRO_FIREFOX if exist "%LOCALAPPDATA%\Mozilla Firefox\firefox.exe" set "ELECTRO_FIREFOX=%LOCALAPPDATA%\Mozilla Firefox\firefox.exe"
if not defined ELECTRO_FIREFOX for /f "delims=" %%F in ('where firefox.exe 2^>nul') do if not defined ELECTRO_FIREFOX set "ELECTRO_FIREFOX=%%F"

if not defined ELECTRO_FIREFOX (
    echo Firefox was not found. Install Firefox or add firefox.exe to PATH.
    pause
    exit /b 1
)

rem Main parts and alternatives. No thermal paste or insulating pads.
rem Next tabs: Ozon heatsinks and alternative suppliers / offers.
start "" "%ELECTRO_FIREFOX%" ^
 -new-window "https://belchip.by/product/?selected_product=s12_144066" ^
 -new-tab "https://belchip.by/product/?selected_product=54781" ^
 -new-tab "https://belchip.by/product/?selected_product=34424" ^
 -new-tab "https://www.ozon.ru/product/radiator-ohlazhdeniya-alyuminievyy-95h13h100-mm-teplootvod-ohlazhdenie-svetodiodov-1546744326/" ^
 -new-tab "https://www.ozon.ru/product/radiator-ohlazhdeniya-alyuminievyy-95h13h200-mm-teplootvod-ohlazhdenie-svetodiodov-1546745058/" ^
 -new-tab "https://www.kufar.by/item/1084744943?listName=kufar-ml&r_block=Recommendation+widget" ^
 -new-tab "https://www.chipdip.by/product/irfb4110pbf" ^
 -new-tab "https://radiodetali.by/poluprovodnikovye-pribory/tranzistory/195744.html" ^
 -new-tab "https://radiodetali.by/poluprovodnikovye-pribory/tranzistory/211591.html" ^
 -new-tab "https://radiodetali.by/ustanovochnye-izdelija/ohladiteli/46045.html" ^
 -new-tab "https://radiodetali.by/product/6-46e0ebde102c.html" ^
 -new-tab "https://belchip.by/product/?selected_product=s20_03091" ^
 -new-tab "https://belchip.by/product/?selected_product=s21_UT-00099966" ^
 -new-tab "https://belchip.by/product/?selected_product=s2_SM8S33A" ^
 -new-tab "https://belchip.by/product/?selected_product=s20_04564" ^
 -new-tab "https://belchip.by/product/?selected_product=46939"

endlocal
exit /b 0
