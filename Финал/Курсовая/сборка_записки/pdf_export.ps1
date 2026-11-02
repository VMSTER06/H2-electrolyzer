param([string]$Doc, [string]$Pdf)
$ErrorActionPreference = 'Stop'
$w = New-Object -ComObject Word.Application
Start-Sleep -Seconds 3
$w.Visible = $false; $w.DisplayAlerts = 0
$d = $w.Documents.Open($Doc, $false, $true)
$d.ExportAsFixedFormat($Pdf, 17)
"pages: " + $d.ComputeStatistics(2)
$d.Close(0); $w.Quit()
