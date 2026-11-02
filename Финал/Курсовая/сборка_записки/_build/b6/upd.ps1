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
# СТП: таблица, не уместившаяся на странице, продолжается на следующей с надписью
# «Продолжение таблицы N.M» и повтором головки. Таблицу режем там, где строка ушла на новую страницу.
$i = 1; $splits = 0
while ($i -le $d.Tables.Count) {
  $tb = $d.Tables.Item($i)
  $ps = [math]::Max(0, $tb.Range.Start - 1)
  $cap = $d.Range($ps, $ps).Paragraphs.Item(1)
  if ($cap.Range.Text -match '^(Таблица|Продолжение таблицы) ([А-Я]?\d+(\.\d+)?)') {
    $num = $Matches[2]
    $p0 = $d.Range($tb.Range.Start, $tb.Range.Start).Information(3)
    $nr = $tb.Rows.Count; $k = 0
    try {
      for ($r = 2; $r -le $nr; $r++) {
        $st = $tb.Rows.Item($r).Range.Start
        if ($d.Range($st, $st).Information(3) -gt $p0) { $k = $r; break }
      }
    } catch { $k = 0 }   # вертикально объединенные ячейки: такие таблицы не режутся
    if ($k -ge 3) {
      $nt = $tb.Split($k)
      $mid = $d.Range($tb.Range.End, $tb.Range.End).Paragraphs.Item(1)
      $mid.Range.InsertBefore('Продолжение таблицы ' + $num)
      $pf = $mid.Range.ParagraphFormat
      $pf.Alignment = 0; $pf.FirstLineIndent = 0; $pf.LeftIndent = $cap.Range.ParagraphFormat.LeftIndent
      $pf.KeepWithNext = -1; $pf.PageBreakBefore = -1; $pf.LineSpacingRule = 4; $pf.LineSpacing = 18
      $nt.Rows.Add($nt.Rows.Item(1)) | Out-Null
      for ($c = 1; $c -le $tb.Rows.Item(1).Cells.Count; $c++) {
        $src = $tb.Cell(1, $c).Range; $src.MoveEnd(1, -1) | Out-Null
        $dst = $nt.Cell(1, $c).Range; $dst.MoveEnd(1, -1) | Out-Null
        $dst.FormattedText = $src.FormattedText
      }
      $nt.Rows.Item(1).HeadingFormat = -1
      $splits++
      $d.Repaginate()
    }
  }
  $i++
}
"table continuations: " + $splits
$d.Repaginate()
foreach ($t in $d.TablesOfContents) { $t.Update() }
$d.Repaginate()
foreach ($t in $d.TablesOfContents) { $t.Update() }
"pages: " + $d.ComputeStatistics(2)
$d.SaveAs2($OutDoc, 16)
$d.ExportAsFixedFormat($Pdf, 17)
$d.Close(0); $w.Quit()
"done"
