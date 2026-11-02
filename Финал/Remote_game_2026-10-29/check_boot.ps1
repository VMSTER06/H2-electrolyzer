param([int]$Seconds = 15, [string]$LogName = 'serial_after_upload.log')
$ErrorActionPreference = 'Stop'
$remotePort = [System.IO.Ports.SerialPort]::new('COM5', 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$remotePort.DtrEnable = $false
$remotePort.RtsEnable = $false
$remotePort.Encoding = [System.Text.UTF8Encoding]::new($false)
$remoteLog = [System.Text.StringBuilder]::new()
try {
    $remotePort.Open()
    # Normal boot: GPIO0 released, pulse EN via the USB bridge's RTS.
    $remotePort.DtrEnable = $false
    $remotePort.RtsEnable = $true
    Start-Sleep -Milliseconds 100
    $remotePort.RtsEnable = $false
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($watch.Elapsed.TotalSeconds -lt $Seconds) {
        $chunk = $remotePort.ReadExisting()
        if ($chunk) { [void]$remoteLog.Append($chunk) }
        Start-Sleep -Milliseconds 100
    }
} finally {
    if ($remotePort.IsOpen) { $remotePort.Close() }
    $remotePort.Dispose()
    $text = $remoteLog.ToString()
    [System.IO.File]::WriteAllText((Join-Path $PSScriptRoot $LogName), $text, [System.Text.UTF8Encoding]::new($false))
}
$text
if ($text -notmatch 'H2 PRO Remote v6\.3 \+ GAME') { throw 'Updated firmware boot banner was not observed.' }
if ($text -notmatch 'OLED: OK' -or $text -notmatch 'NRF24: OK' -or $text -notmatch 'Пульт готов:') { throw 'Peripheral initialization was not confirmed.' }
if ($text -match 'Guru Meditation|Brownout|panic|WDT: FAIL|Task watchdog') { throw 'A runtime fault was observed.' }
if ([regex]::Matches($text, 'H2 PRO Remote v6\.3 \+ GAME').Count -ne 1) { throw 'Unexpected repeated boot.' }
Write-Output 'PASS: v6.3 + GAME booted once; OLED/NRF24 ready; no runtime fault in observation window.'
