param([string]$Spec, [string]$Out)
# ESKD sheet (A3 landscape or A4 portrait): frame, title block as in the department sample
# (GOST 2.104 form 1 without mass/scale, italic Arial), vertical designation box at the top-right,
# block diagram from JSON (all sizes in mm) -> 300 dpi PNG.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$json = [System.IO.File]::ReadAllText($Spec, [System.Text.Encoding]::UTF8)
$s = $json | ConvertFrom-Json
$dpi = 300.0; $k = $dpi / 25.4
$SW = [double]$s.sheet_w; $SH = [double]$s.sheet_h
$bmp = New-Object System.Drawing.Bitmap ([int][math]::Ceiling($SW*$k)), ([int][math]::Ceiling($SH*$k))
$bmp.SetResolution($dpi, $dpi)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::White)
$g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAliasGridFit'
$fam = if ($s.font_family) { [string]$s.font_family } else { 'Arial' }
$black = [System.Drawing.Brushes]::Black
function P([double]$mm) { return [float]($mm * $k) }
function Pen([double]$mm) { $p = New-Object System.Drawing.Pen ([System.Drawing.Color]::Black), ([float]($mm * $k)); $p.LineJoin = 'Miter'; return $p }
function Fnt([double]$pt, [bool]$bold, [bool]$italic) {
  $st = [System.Drawing.FontStyle]::Regular
  if ($bold) { $st = $st -bor [System.Drawing.FontStyle]::Bold }
  if ($italic) { $st = $st -bor [System.Drawing.FontStyle]::Italic }
  return New-Object System.Drawing.Font ($fam, [float]$pt, $st, [System.Drawing.GraphicsUnit]::Point)
}
$fmtC = New-Object System.Drawing.StringFormat; $fmtC.Alignment = 'Center'; $fmtC.LineAlignment = 'Center'
$fmtL = New-Object System.Drawing.StringFormat; $fmtL.Alignment = 'Near';   $fmtL.LineAlignment = 'Center'
$fmtLT = New-Object System.Drawing.StringFormat; $fmtLT.Alignment = 'Near'; $fmtLT.LineAlignment = 'Near'
function Txt([string]$t, [double]$x, [double]$y, [double]$w, [double]$h, [double]$pt, $fmt, [bool]$bold, [bool]$italic) {
  $r = New-Object System.Drawing.RectangleF (P $x), (P $y), (P $w), (P $h)
  $g.DrawString(($t -replace '\\n', "`n"), (Fnt $pt $bold $italic), $black, $r, $fmt)
}
function Rect([double]$x, [double]$y, [double]$w, [double]$h, [double]$lw) { $g.DrawRectangle((Pen $lw), (P $x), (P $y), (P $w), (P $h)) }
function Line([double]$x1, [double]$y1, [double]$x2, [double]$y2, [double]$lw) { $g.DrawLine((Pen $lw), (P $x1), (P $y1), (P $x2), (P $y2)) }
function OpenArrow([double]$x1,[double]$y1,[double]$x2,[double]$y2,[double]$len,[double]$lw) {
  $ang = [math]::Atan2($y2 - $y1, $x2 - $x1); $L = $len * $k
  $a1 = $ang + [math]::PI - 0.4; $a2 = $ang + [math]::PI + 0.4
  $tx = (P $x2); $ty = (P $y2)
  $g.DrawLine((Pen $lw), $tx, $ty, [float]($tx + $L*[math]::Cos($a1)), [float]($ty + $L*[math]::Sin($a1)))
  $g.DrawLine((Pen $lw), $tx, $ty, [float]($tx + $L*[math]::Cos($a2)), [float]($ty + $L*[math]::Sin($a2)))
}
$lwMain = [double]$s.lw_main; $lwThin = [double]$s.lw_thin; $lwLine = [double]$s.lw_line
$arrowLen = if ($s.arrow) { [double]$s.arrow } else { 3.0 }

# ---------- frame ----------
$fx = 20.0; $fy = 5.0; $fw = $SW - 25.0; $fh = $SH - 10.0
Rect $fx $fy $fw $fh $lwMain

# ---------- title block 185 x 55, bottom-right ----------
$tb = $s.stamp
$sx = $fx + $fw - 185.0; $sy = $fy + $fh - 55.0
Rect $sx $sy 185 55 $lwMain
Line ($sx+65) $sy ($sx+65) ($sy+55) $lwMain
Line ($sx+7) $sy ($sx+7) ($sy+25) $lwMain
foreach ($xx in @(17,40,55)) { Line ($sx+$xx) $sy ($sx+$xx) ($sy+55) $lwMain }
foreach ($yy in @(5,10,15,20)) { Line $sx ($sy+$yy) ($sx+65) ($sy+$yy) $lwThin }
Line $sx ($sy+25) ($sx+65) ($sy+25) $lwMain
foreach ($yy in @(30,35,40,45,50)) { Line $sx ($sy+$yy) ($sx+65) ($sy+$yy) $lwThin }
Txt 'Изм.' ($sx) ($sy+20) 7 5 6.5 $fmtC $false $true
Txt 'Лист' ($sx+7) ($sy+20) 10 5 6.5 $fmtC $false $true
Txt '№ докум.' ($sx+17) ($sy+20) 23 5 6.5 $fmtC $false $true
Txt 'Подп.' ($sx+40) ($sy+20) 15 5 6.5 $fmtC $false $true
Txt 'Дата' ($sx+55) ($sy+20) 10 5 6.5 $fmtC $false $true
$roles = @('Разраб.','Пров.','','','Н.контр.','Утв.')
$names = @([string]$tb.developer, [string]$tb.checker, '', '', [string]$tb.normcontrol, [string]$tb.approved)
for ($i = 0; $i -lt 6; $i++) {
  Txt $roles[$i] ($sx+0.6) ($sy+25+5*$i) 16.4 5 8 $fmtL $false $true
  Txt $names[$i] ($sx+17.6) ($sy+25+5*$i) 22.4 5 8 $fmtL $false $true
}
# right part: designation 120x15, name 70x40, lit/sheet/sheets 50x(5+5), org 50x30
Line ($sx+65) ($sy+15) ($sx+185) ($sy+15) $lwMain
Line ($sx+135) ($sy+15) ($sx+135) ($sy+55) $lwMain
Line ($sx+135) ($sy+20) ($sx+185) ($sy+20) $lwThin
Line ($sx+135) ($sy+25) ($sx+185) ($sy+25) $lwMain
Line ($sx+150) ($sy+15) ($sx+150) ($sy+25) $lwMain
Line ($sx+165) ($sy+15) ($sx+165) ($sy+25) $lwMain
Line ($sx+140) ($sy+20) ($sx+140) ($sy+25) $lwThin
Line ($sx+145) ($sy+20) ($sx+145) ($sy+25) $lwThin
Txt 'Лит.' ($sx+135) ($sy+15) 15 5 6.5 $fmtC $false $true
Txt 'Лист' ($sx+150) ($sy+15) 15 5 6.5 $fmtC $false $true
Txt 'Листов' ($sx+165) ($sy+15) 20 5 6.5 $fmtC $false $true
Txt ([string]$tb.liter) ($sx+135) ($sy+20) 5 5 7 $fmtC $false $true
Txt ([string]$tb.sheet) ($sx+150) ($sy+20) 15 5 8 $fmtC $false $true
Txt ([string]$tb.sheets) ($sx+165) ($sy+20) 20 5 8 $fmtC $false $true
Txt ([string]$tb.designation) ($sx+65) ($sy) 120 15 20 $fmtC $false $true
Txt ([string]$tb.name) ($sx+66) ($sy+15) 68 40 11 $fmtC $false $true
Txt ([string]$tb.org) ($sx+135) ($sy+25) 50 30 12 $fmtC $false $true

# ---------- vertical designation box 14 x 70 at the top-right inside the frame (as in the sample) ----------
$vx = $fx + $fw - 14.0; $vy = $fy
Rect $vx $vy 14 70 $lwMain
$g.TranslateTransform((P ($vx + 7)), (P ($vy + 35)))
$g.RotateTransform(-90)
$rr = New-Object System.Drawing.RectangleF (P -35), (P -7), (P 70), (P 14)
$fmtN = New-Object System.Drawing.StringFormat; $fmtN.Alignment = 'Center'; $fmtN.LineAlignment = 'Center'; $fmtN.FormatFlags = [System.Drawing.StringFormatFlags]::NoWrap
$g.DrawString([string]$tb.designation, (Fnt 13.5 $false $true), $black, $rr, $fmtN)
$g.ResetTransform()

# ---------- diagram items ----------
foreach ($it in $s.items) {
  switch ($it.t) {
    'box'  { Rect $it.x $it.y $it.w $it.h $lwLine
             if ($it.text) { $pt = if ($it.size) { [double]$it.size } else { 12 }; Txt $it.text $it.x $it.y $it.w $it.h $pt $fmtC ([bool]$it.bold) $false } }
    'line' { $pen = Pen $lwLine
             if ($it.dashed) { $pen.DashPattern = [float[]]@(6.0, 3.0) }
             $pts = @(); foreach ($q in $it.pts) { $pts += (New-Object System.Drawing.PointF (P $q[0]), (P $q[1])) }
             for ($i = 1; $i -lt $pts.Count; $i++) { $g.DrawLine($pen, $pts[$i-1], $pts[$i]) }
             $n = $pts.Count
             if ($it.arrow) { OpenArrow $it.pts[$n-2][0] $it.pts[$n-2][1] $it.pts[$n-1][0] $it.pts[$n-1][1] $arrowLen $lwLine }
             if ($it.arrow0) { OpenArrow $it.pts[1][0] $it.pts[1][1] $it.pts[0][0] $it.pts[0][1] $arrowLen $lwLine } }
    'dash' { $p = Pen $lwLine; $p.DashPattern = [float[]]@(10.0, 3.0, 1.5, 3.0); $g.DrawRectangle($p, (P $it.x), (P $it.y), (P $it.w), (P $it.h)) }
    'text' { $pt = if ($it.size) { [double]$it.size } else { 12 }; $f = if ($it.a -eq 'l') { $fmtLT } else { $fmtC }; Txt $it.text $it.x $it.y $it.w $it.h $pt $f ([bool]$it.bold) $false }
  }
}
$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
"saved $Out"
