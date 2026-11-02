param([string]$Spec, [string]$Out)
# Draws an ESKD sheet (A3/A4) with frame, title block (GOST 2.104 form 1) and a block diagram from JSON (mm) to 300-dpi PNG.
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
function Fnt([double]$pt, [bool]$bold) { $st = if ($bold) { [System.Drawing.FontStyle]::Bold } else { [System.Drawing.FontStyle]::Regular }; return New-Object System.Drawing.Font ($fam, [float]$pt, $st, [System.Drawing.GraphicsUnit]::Point) }
$fmtC = New-Object System.Drawing.StringFormat; $fmtC.Alignment = 'Center'; $fmtC.LineAlignment = 'Center'
$fmtL = New-Object System.Drawing.StringFormat; $fmtL.Alignment = 'Near';   $fmtL.LineAlignment = 'Center'
$fmtLT = New-Object System.Drawing.StringFormat; $fmtLT.Alignment = 'Near'; $fmtLT.LineAlignment = 'Near'
function Txt([string]$t, [double]$x, [double]$y, [double]$w, [double]$h, [double]$pt, $fmt, [bool]$bold) {
  $r = New-Object System.Drawing.RectangleF (P $x), (P $y), (P $w), (P $h)
  $g.DrawString(($t -replace '\\n', "`n"), (Fnt $pt $bold), $black, $r, $fmt)
}
function Rect([double]$x, [double]$y, [double]$w, [double]$h, [double]$lw) { $g.DrawRectangle((Pen $lw), (P $x), (P $y), (P $w), (P $h)) }
function Line([double]$x1, [double]$y1, [double]$x2, [double]$y2, [double]$lw) { $g.DrawLine((Pen $lw), (P $x1), (P $y1), (P $x2), (P $y2)) }
function Arrow([double]$x1,[double]$y1,[double]$x2,[double]$y2,[double]$len) {
  $ang = [math]::Atan2($y2 - $y1, $x2 - $x1); $L = $len * $k
  $a1 = $ang + [math]::PI - 0.5236; $a2 = $ang + [math]::PI + 0.5236
  $tip = New-Object System.Drawing.PointF (P $x2), (P $y2)
  $p1 = New-Object System.Drawing.PointF ([float]((P $x2) + $L*[math]::Cos($a1))), ([float]((P $y2) + $L*[math]::Sin($a1)))
  $p2 = New-Object System.Drawing.PointF ([float]((P $x2) + $L*[math]::Cos($a2))), ([float]((P $y2) + $L*[math]::Sin($a2)))
  $g.FillPolygon($black, [System.Drawing.PointF[]]@($tip, $p1, $p2))
}
$lwMain = [double]$s.lw_main      # frame / title block outer lines
$lwThin = [double]$s.lw_thin      # title block inner lines
$lwLine = [double]$s.lw_line      # diagram lines and boxes
$arrowLen = if ($s.arrow) { [double]$s.arrow } else { 4.0 }

# ---------- frame (5 mm top/right/bottom, 20 mm left) ----------
$fx = 20.0; $fy = 5.0; $fw = $SW - 25.0; $fh = $SH - 10.0
Rect $fx $fy $fw $fh $lwMain

# ---------- title block, GOST 2.104 form 1 (185 x 55), bottom-right inside the frame ----------
$tb = $s.stamp
$sx = $fx + $fw - 185.0; $sy = $fy + $fh - 55.0
Rect $sx $sy 185 55 $lwMain
# left block: columns 7,10,23,15,10 in rows 0-25; columns 17,23,15,10 in rows 25-55
Line ($sx+65) $sy ($sx+65) ($sy+55) $lwMain
foreach ($yy in @(5,10,15,20,25)) { Line $sx ($sy+$yy) ($sx+65) ($sy+$yy) $lwThin }
Line $sx ($sy+25) ($sx+65) ($sy+25) $lwMain
foreach ($xx in @(7,17,40,55)) { Line ($sx+$xx) $sy ($sx+$xx) ($sy+25) $lwThin }
foreach ($yy in @(30,35,40,45,50)) { Line $sx ($sy+$yy) ($sx+65) ($sy+$yy) $lwThin }
foreach ($xx in @(17,40,55)) { Line ($sx+$xx) ($sy+25) ($sx+$xx) ($sy+55) $lwThin }
Txt 'Изм.' ($sx) ($sy+20) 7 5 7 $fmtC $false
Txt 'Лист' ($sx+7) ($sy+20) 10 5 7 $fmtC $false
Txt '№ докум.' ($sx+17) ($sy+20) 23 5 7 $fmtC $false
Txt 'Подп.' ($sx+40) ($sy+20) 15 5 7 $fmtC $false
Txt 'Дата' ($sx+55) ($sy+20) 10 5 7 $fmtC $false
$roles = @('Разраб.','Пров.','Т.контр.','','Н.контр.','Утв.')
$names = @([string]$tb.developer, [string]$tb.checker, '', '', [string]$tb.normcontrol, [string]$tb.approved)
for ($i = 0; $i -lt 6; $i++) {
  Txt $roles[$i] ($sx+0.8) ($sy+25+5*$i) 16 5 8 $fmtL $false
  Txt $names[$i] ($sx+17.8) ($sy+25+5*$i) 22 5 8 $fmtL $false
}
# right block: designation (120x15), name (70x40), lit/mass/scale (50x10), sheet/sheets (50x5), org (50x25)
Line ($sx+65) ($sy+15) ($sx+185) ($sy+15) $lwMain
Line ($sx+135) ($sy+15) ($sx+135) ($sy+55) $lwMain
Line ($sx+135) ($sy+25) ($sx+185) ($sy+25) $lwMain
Line ($sx+135) ($sy+30) ($sx+185) ($sy+30) $lwMain
Line ($sx+135) ($sy+20) ($sx+185) ($sy+20) $lwThin
foreach ($xx in @(140,145,150,165)) { Line ($sx+$xx) ($sy+15) ($sx+$xx) ($sy+25) $lwThin }
Line ($sx+155) ($sy+25) ($sx+155) ($sy+30) $lwThin
Txt 'Лит.' ($sx+135) ($sy+15) 15 5 7 $fmtC $false
Txt 'Масса' ($sx+150) ($sy+15) 15 5 7 $fmtC $false
Txt 'Масштаб' ($sx+165) ($sy+15) 20 5 7 $fmtC $false
Txt ([string]$tb.liter) ($sx+135) ($sy+20) 5 5 8 $fmtC $false
Txt ('Лист ' + [string]$tb.sheet) ($sx+135) ($sy+25) 20 5 7 $fmtC $false
Txt ('Листов ' + [string]$tb.sheets) ($sx+155) ($sy+25) 30 5 7 $fmtC $false
Txt ([string]$tb.designation) ($sx+65) ($sy) 120 15 20 $fmtC $false
Txt ([string]$tb.name) ($sx+66) ($sy+15) 68 40 10 $fmtC $false
Txt ([string]$tb.org) ($sx+135) ($sy+30) 50 25 10 $fmtC $false
# rotated designation box (70 x 14) at top-left along the frame
$g.TranslateTransform((P ($fx + 35)), (P ($fy + 7)))
$g.RotateTransform(180)
$g.DrawRectangle((Pen $lwMain), (P -35), (P -7), (P 70), (P 14))
$rr = New-Object System.Drawing.RectangleF (P -35), (P -7), (P 70), (P 14)
$g.DrawString([string]$tb.designation, (Fnt 14 $false), $black, $rr, $fmtC)
$g.ResetTransform()

# ---------- diagram items ----------
foreach ($it in $s.items) {
  switch ($it.t) {
    'box'  { Rect $it.x $it.y $it.w $it.h $lwLine
             if ($it.text) { $pt = if ($it.size) { [double]$it.size } else { 12 }; Txt $it.text $it.x $it.y $it.w $it.h $pt $fmtC ([bool]$it.bold) } }
    'dash' { $p = Pen $lwLine; $p.DashPattern = [float[]]@(8.0, 2.5, 1.5, 2.5); $g.DrawRectangle($p, (P $it.x), (P $it.y), (P $it.w), (P $it.h)) }
    'line' { $pts = @(); foreach ($q in $it.pts) { $pts += (New-Object System.Drawing.PointF (P $q[0]), (P $q[1])) }
             for ($i = 1; $i -lt $pts.Count; $i++) { $g.DrawLine((Pen $lwLine), $pts[$i-1], $pts[$i]) }
             $n = $pts.Count
             if ($it.arrow) { Arrow $it.pts[$n-2][0] $it.pts[$n-2][1] $it.pts[$n-1][0] $it.pts[$n-1][1] $arrowLen }
             if ($it.arrow0) { Arrow $it.pts[1][0] $it.pts[1][1] $it.pts[0][0] $it.pts[0][1] $arrowLen }
             if ($it.dot) { $d = 2.0 * $k; $g.FillEllipse($black, $pts[0].X - $d/2, $pts[0].Y - $d/2, $d, $d) } }
    'text' { $pt = if ($it.size) { [double]$it.size } else { 12 }; $f = if ($it.a -eq 'l') { $fmtLT } else { $fmtC }; Txt $it.text $it.x $it.y $it.w $it.h $pt $f ([bool]$it.bold) }
  }
}
$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
"saved $Out"
