param([string]$Spec, [string]$Out, [string]$FigDir, [switch]$Visible)
# Builds the explanatory note (.docx) from the markup spec via Word COM. ASCII-only script.
$ErrorActionPreference = 'Stop'
$CM = 28.3465
$TEXT_W = 595.3 - 3.0*$CM - 1.5*$CM      # A4 width minus margins = 467.7 pt
$lines = [System.IO.File]::ReadAllLines($Spec, [System.Text.Encoding]::UTF8)

# wait until a previous (closing) automation instance of Word has exited
$t = 0; while ((Get-Process WINWORD -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -eq '' }) -and $t -lt 60) { Start-Sleep -Seconds 1; $t++ }
$w = New-Object -ComObject Word.Application
Start-Sleep -Seconds 3
$w.Visible = [bool]$Visible
$w.DisplayAlerts = 0
$w.Options.CheckGrammarAsYouType = $false
$w.Options.CheckSpellingAsYouType = $false
try { $w.Options.AutoFormatAsYouTypeReplaceQuotes = $false } catch {}
try { $w.Options.AutoFormatAsYouTypeReplaceHyphens = $false } catch {}
$doc = $w.Documents.Add()
Start-Sleep -Seconds 1
if ($null -eq $doc) { $doc = $w.ActiveDocument }
if ($null -eq $doc.PageSetup) { Start-Sleep -Seconds 3 }

# ---------- page setup ----------
$ps = $doc.PageSetup
$ps.PaperSize = 7          # wdPaperA4
$ps.Orientation = 0
$ps.LeftMargin = 3.0*$CM; $ps.RightMargin = 1.5*$CM; $ps.TopMargin = 2.0*$CM; $ps.BottomMargin = 2.0*$CM
$ps.FooterDistance = 1.0*$CM; $ps.HeaderDistance = 1.0*$CM
$ps.DifferentFirstPageHeaderFooter = 0

# ---------- hyphenation ----------
$doc.AutoHyphenation = -1
$doc.HyphenateCaps = 0
$doc.HyphenationZone = 0.63*$CM
$doc.ConsecutiveHyphensLimit = 3

# ---------- styles ----------
function SetFont($f) { $f.Name = 'Times New Roman'; $f.Size = 14; $f.Bold = 0; $f.Italic = 0; $f.Color = -16777216; $f.AllCaps = 0 }
$script:stN = $doc.Styles.Item(-1)      # Normal
SetFont $stN.Font
$pf = $stN.ParagraphFormat
$pf.Alignment = 3; $pf.FirstLineIndent = 0; $pf.LeftIndent = 0; $pf.RightIndent = 0
$pf.LineSpacingRule = 4; $pf.LineSpacing = 18; $pf.SpaceBefore = 0; $pf.SpaceAfter = 0; $pf.WidowControl = -1
$script:stH1 = $doc.Styles.Item(-2)     # Heading 1
SetFont $stH1.Font; $stH1.Font.Bold = -1; $stH1.Font.AllCaps = -1
$pf = $stH1.ParagraphFormat; $pf.Hyphenation = 0; $pf.Alignment = 0; $pf.LineSpacingRule = 4; $pf.LineSpacing = 18; $pf.SpaceBefore = 0; $pf.SpaceAfter = 0; $pf.KeepWithNext = -1; $pf.PageBreakBefore = -1; $pf.FirstLineIndent = 0; $pf.LeftIndent = 0
$stH1.NextParagraphStyle = $stN
$script:stH2 = $doc.Styles.Item(-3)     # Heading 2
SetFont $stH2.Font; $stH2.Font.Bold = -1
$pf = $stH2.ParagraphFormat; $pf.Hyphenation = 0; $pf.Alignment = 0; $pf.LineSpacingRule = 4; $pf.LineSpacing = 18; $pf.SpaceBefore = 0; $pf.SpaceAfter = 0; $pf.KeepWithNext = -1; $pf.PageBreakBefore = 0; $pf.FirstLineIndent = 0; $pf.LeftIndent = 0
$stH2.NextParagraphStyle = $stN
foreach ($sid in @(-20, -21)) {  # TOC 1, TOC 2
  $st = $doc.Styles.Item($sid); SetFont $st.Font
  $pf = $st.ParagraphFormat; $pf.Alignment = 0; $pf.LineSpacingRule = 4; $pf.LineSpacing = 18; $pf.SpaceBefore = 0; $pf.SpaceAfter = 0
  $pf.FirstLineIndent = 0; $pf.LeftIndent = $(if ($sid -eq -21) { 14 } else { 0 })
}
# body paragraph style (first-line indent 1.25 cm)
$script:stB = $doc.Styles.Add('PZ Body', 1); $stB.BaseStyle = $stN; $stB.NextParagraphStyle = $stB
SetFont $stB.Font
$pf = $stB.ParagraphFormat; $pf.Alignment = 3; $pf.FirstLineIndent = 1.25*$CM; $pf.LeftIndent = 0; $pf.LineSpacingRule = 4; $pf.LineSpacing = 18; $pf.SpaceBefore = 0; $pf.SpaceAfter = 0
# code style
$script:stC = $doc.Styles.Add('PZ Code', 1); $stC.BaseStyle = $stN; $stC.NextParagraphStyle = $stC
$stC.Font.Name = 'Courier New'; $stC.Font.Size = 10; $stC.Font.Bold = 0; $stC.Font.Color = -16777216
$pf = $stC.ParagraphFormat; $pf.Alignment = 0; $pf.FirstLineIndent = 0; $pf.LeftIndent = 0; $pf.LineSpacingRule = 0; $pf.SpaceBefore = 0; $pf.SpaceAfter = 0; $pf.WidowControl = 0
$stC.NoSpaceBetweenParagraphsOfSameStyle = -1

# ---------- footer with page number, numbering starts at 3 ----------
$sec = $doc.Sections.Item(1)
$ft = $sec.Footers.Item(1)
$ft.PageNumbers.RestartNumberingAtSection = -1
$ft.PageNumbers.StartingNumber = 3
$ft.PageNumbers.Add(2, -1) | Out-Null   # wdAlignPageNumberRight, first page too
$bfSet = [System.Reflection.BindingFlags]::SetProperty
[System.__ComObject].InvokeMember('RestartNumberingAtSection', $bfSet, $null, $ft.PageNumbers, [object[]]@($true))
[System.__ComObject].InvokeMember('StartingNumber', $bfSet, $null, $ft.PageNumbers, [object[]]@([int]3))
$ft.Range.Font.Name = 'Times New Roman'; $ft.Range.Font.Size = 14; $ft.Range.ParagraphFormat.Alignment = 2

# ---------- helpers ----------
$script:tocRange = $null
function LastPara { return $script:doc.Paragraphs.Item($script:doc.Paragraphs.Count) }
function NewPara { $script:doc.Content.InsertParagraphAfter() }

# Parse inline markup: _{sub} ^{sup} *{italic}. Returns plain text and segments.
function ParseInline([string]$s) {
  $sb = New-Object System.Text.StringBuilder
  $segs = New-Object System.Collections.ArrayList
  $i = 0
  while ($i -lt $s.Length) {
    $c = $s[$i]
    if (($c -eq '_' -or $c -eq '^' -or $c -eq '*') -and ($i + 1 -lt $s.Length) -and $s[$i+1] -eq '{') {
      $j = $s.IndexOf('}', $i + 2)
      if ($j -gt 0) {
        $inner = $s.Substring($i + 2, $j - $i - 2)
        [void]$segs.Add(@{ start = $sb.Length; len = $inner.Length; kind = [string]$c })
        [void]$sb.Append($inner)
        $i = $j + 1
        continue
      }
    }
    [void]$sb.Append($c); $i++
  }
  return @{ text = $sb.ToString(); segs = $segs }
}
function ApplySegs($para, $segs) {
  if ($segs.Count -eq 0) { return }
  $base = $para.Range.Start
  foreach ($sg in $segs) {
    $r = $script:doc.Range($base + $sg.start, $base + $sg.start + $sg.len)
    switch ($sg.kind) {
      '_' { $r.Font.Subscript = -1 }
      '^' { $r.Font.Superscript = -1 }
      '*' { $r.Font.Italic = -1 }
    }
  }
}
# Write text into the current (last, empty) paragraph, apply style + formatting, then open a new paragraph.
function EmitPara([string]$text, $style, [scriptblock]$fmt) {
  $pi = ParseInline ($text -replace '<TAB>', "`t")
  $p = LastPara
  $p.Range.InsertBefore($pi.text)
  $p = LastPara
  if ($null -ne $style) { $p.Style = $style } else { $p.Style = $script:stN }
  if ($fmt) { & $fmt $p }
  ApplySegs $p $pi.segs
  NewPara
}
$fmtBodyNone = $null
$fmtH1num = { param($p) $p.Alignment = 0; $p.LeftIndent = 50.5; $p.FirstLineIndent = -15 }
$fmtH1c   = { param($p) $p.Alignment = 1; $p.LeftIndent = 0; $p.FirstLineIndent = 0 }
$fmtH2    = { param($p) $p.Alignment = 0; $p.LeftIndent = 63.5; $p.FirstLineIndent = -28 }
$fmtCenter = { param($p) $p.Alignment = 1; $p.FirstLineIndent = 0; $p.LeftIndent = 0; $p.Hyphenation = 0 }
$fmtLeft0  = { param($p) $p.Alignment = 0; $p.FirstLineIndent = 0; $p.LeftIndent = 0 }
$fmtTblCap = { param($p) $p.Alignment = 0; $p.FirstLineIndent = 0; $p.LeftIndent = 0; $p.Hyphenation = 0; $p.KeepWithNext = -1 }
$fmtFormula = { param($p) $p.Alignment = 0; $p.FirstLineIndent = 0; $p.LeftIndent = 0; $p.TabStops.ClearAll(); $p.TabStops.Add($script:TEXT_W / 2, 1) | Out-Null; $p.TabStops.Add($script:TEXT_W, 2) | Out-Null }
$fmtWhere  = { param($p) $p.Alignment = 3; $p.LeftIndent = 1.9*$script:CM; $p.FirstLineIndent = -1.9*$script:CM }
$fmtWhere2 = { param($p) $p.Alignment = 3; $p.LeftIndent = 1.9*$script:CM; $p.FirstLineIndent = -1.0*$script:CM }
$fmtAppTitle = { param($p) $p.Alignment = 1; $p.FirstLineIndent = 0; $p.LeftIndent = 0; $p.KeepWithNext = -1 }

function EmitTable([string]$caption, [System.Collections.ArrayList]$rows) {
  # optional explicit column widths in percent: caption ends with {w:14,12,37,37}
  $widths = $null
  if ($caption -match '\s*\{w:([0-9,]+)\}\s*$') { $widths = $Matches[1] -split ','; $caption = $caption -replace '\s*\{w:[0-9,]+\}\s*$', '' }
  EmitPara $caption $null $fmtTblCap
  $lp = LastPara
  $nr = $rows.Count; $nc = ($rows[0] -split '\|').Count
  $t = $script:doc.Tables.Add($lp.Range, $nr, $nc)
  $t.Borders.Enable = -1
  $t.Range.Style = $script:stN
  $t.Range.Font.Name = 'Times New Roman'; $t.Range.Font.Size = 14
  $t.Range.ParagraphFormat.Alignment = 0; $t.Range.ParagraphFormat.FirstLineIndent = 0; $t.Range.ParagraphFormat.LeftIndent = 0
  $t.Range.ParagraphFormat.LineSpacingRule = 0; $t.Range.ParagraphFormat.SpaceBefore = 0; $t.Range.ParagraphFormat.SpaceAfter = 0
  for ($r = 0; $r -lt $nr; $r++) {
    $cells = $rows[$r] -split '\|'
    for ($c = 0; $c -lt $nc; $c++) {
      $txt = if ($c -lt $cells.Count) { $cells[$c] } else { '' }
      $pi = ParseInline $txt
      $cellR = $t.Cell($r + 1, $c + 1).Range
      $cellR.Text = $pi.text
      if ($pi.segs.Count -gt 0) {
        $base = $t.Cell($r + 1, $c + 1).Range.Start
        foreach ($sg in $pi.segs) { $rr = $script:doc.Range($base + $sg.start, $base + $sg.start + $sg.len); if ($sg.kind -eq '_') { $rr.Font.Subscript = -1 } elseif ($sg.kind -eq '^') { $rr.Font.Superscript = -1 } else { $rr.Font.Italic = -1 } }
      }
    }
  }
  $t.Rows.Item(1).HeadingFormat = -1
  $t.Rows.AllowBreakAcrossPages = 0
  $t.AutoFitBehavior(1)   # fit contents
  $t.AutoFitBehavior(2)   # then fit window
  $t.PreferredWidthType = 2; $t.PreferredWidth = 100
  $t.Range.ParagraphFormat.Hyphenation = 0
  if ($nc -eq 2) { $w1 = 42 } elseif ($nc -ge 5) { $w1 = 40 } else { $w1 = 30 }
  $rest = [int][math]::Floor((100 - $w1) / ($nc - 1))
  for ($c = 1; $c -le $nc; $c++) { $col = $t.Columns.Item($c); $col.PreferredWidthType = 2; $col.PreferredWidth = [float]$(if ($widths) { $widths[$c - 1] } elseif ($c -eq 1) { $w1 } else { $rest }) }
  # cursor is now the paragraph after the table (Word keeps one)
}

function EmitFigure([string]$file, [string]$caption, [double]$wmm) {
  $p = LastPara
  $p.Style = $script:stN
  $p.Alignment = 1; $p.FirstLineIndent = 0; $p.LeftIndent = 0; $p.KeepWithNext = -1; $p.LineSpacingRule = 0
  $path = Join-Path $script:FigDir $file
  $shp = $script:doc.InlineShapes.AddPicture($path, $false, $true, $p.Range)
  $shp.LockAspectRatio = -1
  $maxW = $wmm * $script:CM / 10; $maxH = 841.9 - 4.0*$script:CM - 40
  if ($shp.Width -gt $maxW) { $shp.Width = $maxW }
  if ($shp.Height -gt $maxH) { $shp.Height = $maxH }
  NewPara
  EmitPara $caption $null $fmtCenter
}

function EmitListing([string]$path) {
  $src = [System.IO.File]::ReadAllLines($path, [System.Text.Encoding]::UTF8)
  $txt = ($src | ForEach-Object { ($_ -replace "`t", '  ').TrimEnd() }) -join "`r"
  $p = LastPara
  $startPos = $p.Range.Start
  $p.Range.InsertBefore($txt)
  $endPos = (LastPara).Range.End
  $r = $script:doc.Range($startPos, $endPos)
  $r.Style = $script:stC
  $r.Font.Name = 'Courier New'; $r.Font.Size = 8
  $r.ParagraphFormat.Alignment = 0; $r.ParagraphFormat.FirstLineIndent = 0; $r.ParagraphFormat.LeftIndent = 0
  $r.ParagraphFormat.LineSpacingRule = 0; $r.ParagraphFormat.SpaceBefore = 0; $r.ParagraphFormat.SpaceAfter = 0
  NewPara
}

# ---------- main pass ----------
$tblCaption = $null; $tblRows = $null; $inCode = $false; $script:lastTag = ''
$i = 0
while ($i -lt $lines.Count) {
  $ln = $lines[$i]; $i++
  if ($ln.Length -eq 0) { continue }
  $sp = $ln.IndexOf(' ')
  $tag = if ($sp -gt 0) { $ln.Substring(0, $sp) } else { $ln }
  $arg = if ($sp -gt 0) { $ln.Substring($sp + 1) } else { '' }
  switch ($tag) {
    '#TITLE' { EmitPara $arg $null { param($p) $p.Alignment = 1; $p.FirstLineIndent = 0; $p.Range.Font.Bold = -1; $p.Range.Font.AllCaps = 0 }; EmitPara '' $null $fmtLeft0 }
    '#TOC'   { $script:tocRange = (LastPara).Range; $toc = $script:doc.TablesOfContents.Add($script:tocRange, -1, 1, 2, -1); NewPara }
    '#H1C'   { EmitPara $arg $script:stH1 $fmtH1c }
    '#H1'    { EmitPara $arg $script:stH1 $fmtH1num }
    '#H2'    { EmitPara $arg $script:stH2 $fmtH2 }
    '#P'     { EmitPara $arg $script:stB $null }
    '#L'     { EmitPara $arg $script:stB $null }
    '#N'     { EmitPara $arg $script:stB $null }
    '#PC'    { EmitPara $arg $null $fmtCenter }
    '#PL'    { EmitPara $arg $script:stB { param($p) $p.Alignment = 3 } }
    '#E'     { if ($script:lastTag -eq '#H1' -or $script:lastTag -eq '#H1C' -or $script:lastTag -eq '#H2') { EmitPara '' $null { param($p) $p.Alignment = 0; $p.FirstLineIndent = 0; $p.KeepWithNext = -1 } } else { EmitPara '' $null $fmtLeft0 } }
    '#F'     { $parts = $arg -split '<TAB>'; EmitPara ("<TAB>" + $parts[0] + "<TAB>" + $parts[1]) $null $fmtFormula }
    '#W'     { EmitPara $arg $null $fmtWhere }
    '#W2'    { EmitPara $arg $null $fmtWhere2 }
    '#T'     { $tblCaption = $arg; $tblRows = New-Object System.Collections.ArrayList }
    '#R'     { [void]$tblRows.Add($arg) }
    '#TEND'  { EmitTable $tblCaption $tblRows; $tblCaption = $null; $tblRows = $null }
    '#FIG'   { $parts = $arg -split '<TAB>'; $wm = 100.0; if ($parts.Count -gt 2) { $wm = [double]$parts[2] }; EmitFigure $parts[0] $parts[1] $wm }
    '#CODEBLOCK' { $inCode = $true }
    '#CODEEND'   { $inCode = $false }
    '#CODE'  { EmitPara $arg $script:stC { param($p) $p.Range.Font.Size = 11 } }
    '#LISTING' { EmitListing $arg }
    '#APP'   {
      $parts = $arg -split '\|'
      $letter = $parts[0]; $kind = $parts[1]; $title = $parts[2]
      $p = LastPara; $p.PageBreakBefore = -1
      EmitPara ("ПРИЛОЖЕНИЕ " + $letter) $null { param($p) $p.Alignment = 1; $p.FirstLineIndent = 0; $p.LeftIndent = 0; $p.PageBreakBefore = -1; $p.KeepWithNext = -1; $p.Range.Font.Bold = -1 }
      EmitPara ("(" + $kind + ")") $null $fmtAppTitle
      EmitPara $title $null { param($p) $p.Alignment = 1; $p.FirstLineIndent = 0; $p.LeftIndent = 0; $p.KeepWithNext = -1; $p.Range.Font.Bold = -1 }
      # TOC entry field for the appendix
      $tcText = 'TC "' + ('Приложение ' + $letter + ' (' + $kind + ') ' + $title) + '" \l 1'
      $tp = $script:doc.Paragraphs.Item($script:doc.Paragraphs.Count - 1)
      $fr = $script:doc.Range($tp.Range.End - 1, $tp.Range.End - 1)
      $script:doc.Fields.Add($fr, -1, $tcText, $false) | Out-Null
      EmitPara '' $null $fmtLeft0
    }
    default { }
  }
  $script:lastTag = $tag
}
# remove trailing empty paragraph
$lp = LastPara
if ($lp.Range.Text.Trim().Length -eq 0 -and $script:doc.Paragraphs.Count -gt 1) { $lp.Range.Delete() | Out-Null }

# ---------- finalize ----------
$pn = $doc.Sections.Item(1).Footers.Item(1).PageNumbers
[System.__ComObject].InvokeMember('RestartNumberingAtSection', $bfSet, $null, $pn, [object[]]@($true))
[System.__ComObject].InvokeMember('StartingNumber', $bfSet, $null, $pn, [object[]]@([int]3))
"footer start number: " + $doc.Sections.Item(1).Footers.Item(1).PageNumbers.StartingNumber
$doc.TablesOfContents.Item(1).Update()
$doc.Repaginate()
$doc.SaveAs2($Out, 16)
$total = $doc.ComputeStatistics(2)
"TOTAL PAGES: $total"
$doc.Close(-1)
$w.Quit()
"saved: $Out"
