param([string]$Doc)
$ErrorActionPreference = 'Stop'
Get-Process WINWORD -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
$w = New-Object -ComObject Word.Application
Start-Sleep -Seconds 3
$w.Visible = $false; $w.DisplayAlerts = 0
$d = $w.Documents.Open($Doc, $false, $true)
Start-Sleep -Seconds 1
$d.Repaginate()
$n = $d.Paragraphs.Count
$prev = $null
$stop = $false
for ($i = 1; $i -le $n -and -not $stop; $i++) {
  $p = $d.Paragraphs.Item($i)
  if ($p.OutlineLevel -eq 1) {
    $t = ($p.Range.Text -replace "[\r\n\a\t]+", ' ').Trim()
    $pg = $p.Range.Information(3)
    if ($null -ne $prev) {
      $pp = $d.Paragraphs.Item($i - 1)
      # walk back over empty paragraphs
      $k = $i - 1
      while ($k -gt 1 -and ($d.Paragraphs.Item($k).Range.Text.Trim().Length -eq 0)) { $k-- }
      $pp = $d.Paragraphs.Item($k)
      $ppg = $pp.Range.Information(3)
      $ypos = [math]::Round($pp.Range.Information(6) / 28.3465, 1)   # cm from top of page
      "  ... previous section ends on page $ppg at $ypos cm from top (page text bottom = 27.7 cm)"
    }
    "$pg  $t"
    $prev = $p
    if ($t -like '*ИСТОЧНИКОВ*') { $stop = $true }
  }
}
$d.Close(0); $w.Quit()
