param([string]$Doc, [string]$OutDoc, [string]$Pdf)
$ErrorActionPreference = 'Stop'
Get-Process WINWORD -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
$w = New-Object -ComObject Word.Application
Start-Sleep -Seconds 3
$w.Visible = $false; $w.DisplayAlerts = 0
$d = $w.Documents.Open($Doc, $false, $false)
Start-Sleep -Seconds 1
foreach ($t in $d.TablesOfContents) { $t.Update() }
$d.Fields.Update() | Out-Null
foreach ($t in $d.TablesOfContents) { $t.Update() }
$d.Repaginate()
"pages: " + $d.ComputeStatistics(2)
$n = $d.Paragraphs.Count
for ($i = 1; $i -le $n; $i++) {
  $p = $d.Paragraphs.Item($i)
  $t = ($p.Range.Text -replace "[\r\n\a\t]+", ' ').Trim()
  if ($p.OutlineLevel -le 2 -or $t -like 'ПРИЛОЖЕНИЕ*' -or $t -like 'Таблица*') {
    if ($t.Length -gt 0) { "{0,3}  {1}" -f $p.Range.Information(3), $t.Substring(0, [math]::Min(70, $t.Length)) }
  }
}
"--- TOC text ---"
foreach ($t in $d.TablesOfContents) { ($t.Range.Text -replace "\r", "`n") }
$d.SaveAs2($OutDoc, 16)
$d.ExportAsFixedFormat($Pdf, 17)
$d.Close(0); $w.Quit()
"saved $OutDoc / $Pdf"
