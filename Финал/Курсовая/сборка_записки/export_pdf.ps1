param([string]$Doc, [string]$Pdf)
$ErrorActionPreference = 'Stop'
Get-Process WINWORD -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
$w = New-Object -ComObject Word.Application
Start-Sleep -Seconds 3
$w.Visible = $false; $w.DisplayAlerts = 0
$d = $w.Documents.Open($Doc, $false, $true)
Start-Sleep -Seconds 1
$d.ExportAsFixedFormat($Pdf, 17)   # wdExportFormatPDF
"pages: " + $d.ComputeStatistics(2)
$d.Close(0)
$w.Quit()
"exported $Pdf"
