param([string]$Doc, [string]$OutDoc, [string]$Pdf)
$ErrorActionPreference = 'Stop'
# do not kill Word: other Word sessions may belong to the user
Start-Sleep -Seconds 2
# wait until a previous (closing) automation instance of Word has exited
$t = 0; while ((Get-Process WINWORD -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -eq '' }) -and $t -lt 60) { Start-Sleep -Seconds 1; $t++ }
$w = New-Object -ComObject Word.Application
Start-Sleep -Seconds 3
$w.Visible = $false; $w.DisplayAlerts = 0
$d = $w.Documents.Open($Doc, $false, $false)
Start-Sleep -Seconds 1
# TOC level 2: one tab stop after the number so that 1.1 and 3.10 align (1,75 cm)
$ts = $d.Styles.Item(-21).ParagraphFormat.TabStops
$ts.ClearAll(); $ts.Add(49.6, 0, 0) | Out-Null; $ts.Add(467.7, 2, 1) | Out-Null
$d.Repaginate()
foreach ($t in $d.TablesOfContents) { $t.Update() }
$d.Repaginate()
foreach ($t in $d.TablesOfContents) { $t.Update() }
"pages: " + $d.ComputeStatistics(2)
$d.SaveAs2($OutDoc, 16)
$d.ExportAsFixedFormat($Pdf, 17)
$d.Close(0); $w.Quit()
"done"
