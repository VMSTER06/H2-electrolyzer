$ErrorActionPreference = 'Continue'
$here = $PSScriptRoot
try { chcp 65001 > $null } catch {}
function Step($t){ Write-Host "`n==================  $t  ==================" -ForegroundColor Cyan }

Write-Host "H2 PRO - установка среды для сборки и прошивки" -ForegroundColor Green
Write-Host "Рабочая папка: $here`n"

# ---------- 1. Драйвер USB (CP210x) ----------
Step "1/4  Драйвер USB-чипа платы (CP210x)"
$inf = Join-Path $here 'drivers\CP210x\silabser.inf'
if (Test-Path $inf) {
  & pnputil /add-driver "$inf" /install
  & pnputil /scan-devices | Out-Null
  Write-Host "  Драйвер CP210x установлен." -ForegroundColor Green
} else { Write-Host "  ! silabser.inf не найден в drivers\CP210x" -ForegroundColor Yellow }

# ---------- 2. Arduino IDE ----------
Step "2/4  Arduino IDE"
$ide = Get-ChildItem (Join-Path $here 'tools') -Filter 'arduino-ide_*_Windows_64bit.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
if ($ide) {
  Write-Host "  Установка $($ide.Name) (тихий режим, подождите)..."
  Start-Process -FilePath $ide.FullName -ArgumentList '/S' -Wait
  Write-Host "  Arduino IDE установлена." -ForegroundColor Green
} else { Write-Host "  ! установщик Arduino IDE не найден в tools\" -ForegroundColor Yellow }

# ---------- 3. ESP32 ядро + библиотеки ----------
Step "3/4  ESP32 ядро и библиотеки модулей"
$acli = Join-Path $here 'tools\arduino-cli.exe'
& $acli config init --overwrite | Out-Null
& $acli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
& $acli config set library.enable_unsafe_install true
Write-Host "  Обновление индексов (нужен интернет)..."
& $acli core update-index
Write-Host "  Установка ESP32 ядра (~несколько минут, большой объём)..."
& $acli core install esp32:esp32
Write-Host "  Установка библиотек из tools\libs (офлайн):"
Get-ChildItem (Join-Path $here 'tools\libs') -Filter *.zip -ErrorAction SilentlyContinue | Sort-Object Name | ForEach-Object {
  Write-Host ("    + " + ($_.BaseName -replace '^\d+_',''))
  & $acli lib install --zip-path $_.FullName 2>&1 | Out-Null
}
Write-Host "  Библиотеки установлены." -ForegroundColor Green

# ---------- 4. Проверка ----------
Step "4/4  Проверка"
Write-Host "Установленные ядра:" -ForegroundColor Cyan; & $acli core list
Write-Host "`nУстановленные библиотеки:" -ForegroundColor Cyan; & $acli lib list

Write-Host "`n=================================================" -ForegroundColor Green
Write-Host " ГОТОВО! Что дальше:" -ForegroundColor Green
Write-Host " 1. Откройте Arduino IDE." -ForegroundColor Green
Write-Host " 2. Откройте прошивку: Первый запуск\Проект\H2_Brain_v5\H2_Brain_v5.ino" -ForegroundColor Green
Write-Host " 3. Инструменты -> Плата -> ESP32 Dev Module." -ForegroundColor Green
Write-Host " 4. Подключите плату, выберите Порт (COMx), нажмите Загрузить." -ForegroundColor Green
Write-Host " 5. То же для пульта: Проект\H2_Remote_v5\H2_Remote_v5.ino" -ForegroundColor Green
Write-Host "=================================================" -ForegroundColor Green
