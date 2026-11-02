param([string]$File,[string]$AuditDir)
$ErrorActionPreference='Stop'
$w=New-Object -ComObject Word.Application
Start-Sleep -Seconds 3
$w.Visible=$false
$w.DisplayAlerts=0
$doc=$null
try {
 $doc=$w.Documents.Open($File,$false,$false)
 foreach($toc in $doc.TablesOfContents) { $toc.Update() }
 foreach($toc in $doc.TablesOfContents) {
  $toc.Range.Font.Name='Times New Roman'
  $toc.Range.Font.NameAscii='Times New Roman'
  $toc.Range.Font.NameOther='Times New Roman'
  $toc.Range.Font.Size=14
  $toc.Range.Font.Bold=0
  $toc.Range.Font.Italic=0
  foreach($p in $toc.Range.Paragraphs) {
   $txt=$p.Range.Text.Trim()
   $fmt=$p.Format
   $fmt.Alignment=0
   $fmt.LineSpacingRule=4
   $fmt.LineSpacing=18
   $fmt.SpaceBefore=0
   $fmt.SpaceAfter=0
   $fmt.KeepWithNext=0
   $fmt.KeepTogether=-1
   $fmt.Hyphenation=0
   $fmt.RightIndent=$w.MillimetersToPoints(8)
   $fmt.TabStops.ClearAll()
   if($txt -match '^\d+\.\d+') {
    $fmt.LeftIndent=$w.MillimetersToPoints(20)
    $fmt.FirstLineIndent=$w.MillimetersToPoints(-13)
    $null=$fmt.TabStops.Add($w.MillimetersToPoints(20),0,0)
   } elseif($txt -match '^\d+\s') {
    $fmt.LeftIndent=$w.MillimetersToPoints(7)
    $fmt.FirstLineIndent=$w.MillimetersToPoints(-7)
    $null=$fmt.TabStops.Add($w.MillimetersToPoints(7),0,0)
   } else {
    $fmt.LeftIndent=0
    $fmt.FirstLineIndent=0
   }
   $null=$fmt.TabStops.Add($w.MillimetersToPoints(165),2,1)
  }
 }
 $doc.Repaginate()
 foreach($toc in $doc.TablesOfContents) { $toc.UpdatePageNumbers(); $toc.Range.Font.Name='Times New Roman';$toc.Range.Font.Size=14 }
 $doc.Repaginate()
 $doc.Save()
 $pdf=[IO.Path]::ChangeExtension($File,'.pdf')
 $doc.ExportAsFixedFormat($pdf,17)
 Write-Output ('Exported pages='+$doc.ComputeStatistics(2))
 $tables=@()
 $i=0
 foreach($t in $doc.Tables) {
  $i++
  $rows=@()
  foreach($row in $t.Rows) {
   $rng=$row.Range.Duplicate;$rng.Collapse(1)
   $start=$rng.Information(3)
   $rng=$row.Range.Duplicate;$rng.End=$rng.End-1;$rng.Collapse(0)
   $end=$rng.Information(3)
   $rows+=@{start=$start;end=$end}
  }
  $tables+=@{index=$i;rows=$rows}
 }
 $tables | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $AuditDir 'word_table_pages.json') -Encoding UTF8
 Write-Output 'Table page audit saved'
} finally {
 if($null -ne $doc){$doc.Close(0)}
 $w.Quit()
}
