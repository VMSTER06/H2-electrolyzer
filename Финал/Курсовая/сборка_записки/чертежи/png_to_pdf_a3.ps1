param([string]$Png, [string]$Pdf, [double]$Wmm = 420, [double]$Hmm = 297)
$ErrorActionPreference = 'Stop'
Get-Process WINWORD -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 2
$w = New-Object -ComObject Word.Application
Start-Sleep -Seconds 3
$w.Visible = $false; $w.DisplayAlerts = 0
$d = $w.Documents.Add()
Start-Sleep -Seconds 1
$CM = 28.3465
$ps = $null
for ($try = 0; $try -lt 10 -and $null -eq $ps; $try++) { try { $ps = $d.PageSetup; $null = $ps.Orientation } catch { $ps = $null; Start-Sleep -Seconds 2 } }
$ps.Orientation = $(if ($Wmm -gt $Hmm) { 1 } else { 0 })
$ps.PageWidth = $Wmm / 10 * $CM; $ps.PageHeight = $Hmm / 10 * $CM
$ps.LeftMargin = 0; $ps.RightMargin = 0; $ps.TopMargin = 0; $ps.BottomMargin = 0
$ps.HeaderDistance = 0; $ps.FooterDistance = 0
$p = $d.Paragraphs.Item(1)
$p.SpaceBefore = 0; $p.SpaceAfter = 0; $p.LineSpacingRule = 0; $p.Alignment = 0; $p.LeftIndent = 0; $p.FirstLineIndent = 0
$shp = $d.InlineShapes.AddPicture($Png, $false, $true, $p.Range)
$shp.LockAspectRatio = 0
$shp.Width = $Wmm / 10 * $CM; $shp.Height = $Hmm / 10 * $CM
$d.ExportAsFixedFormat($Pdf, 17, $false, 0)   # wdExportFormatPDF, OptimizeFor print
"pages: " + $d.ComputeStatistics(2)
$d.Close(0); $w.Quit()
"exported $Pdf"
