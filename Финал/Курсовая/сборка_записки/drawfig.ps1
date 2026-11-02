param([string]$Spec, [string]$Out)
# Draws a block/flow diagram from a JSON spec (mm coordinates) to a 300-dpi PNG using GDI+.
# Spec: { w, h (mm), font (pt), items: [ {t:"box"|"rbox"|"diamond"|"line"|"text"|"res", ...} ] }
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$json = [System.IO.File]::ReadAllText($Spec, [System.Text.Encoding]::UTF8)
$s = $json | ConvertFrom-Json
$dpi = 300.0
$k = $dpi / 25.4
$W = [int][math]::Ceiling($s.w * $k); $H = [int][math]::Ceiling($s.h * $k)
$bmp = New-Object System.Drawing.Bitmap $W, $H
$bmp.SetResolution($dpi, $dpi)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.Clear([System.Drawing.Color]::White)
$g.SmoothingMode = 'AntiAlias'
$g.TextRenderingHint = 'AntiAliasGridFit'
$lw = [float](0.4 * $k)
$pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::Black), $lw
$pen.LineJoin = 'Miter'
$brush = [System.Drawing.Brushes]::Black
$fontPt = if ($s.font) { [float]$s.font } else { 12.0 }
$font = New-Object System.Drawing.Font ("Times New Roman", $fontPt, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Point)
$fmtC = New-Object System.Drawing.StringFormat
$fmtC.Alignment = 'Center'; $fmtC.LineAlignment = 'Center'
$fmtL = New-Object System.Drawing.StringFormat
$fmtL.Alignment = 'Near'; $fmtL.LineAlignment = 'Center'
function P([double]$mm) { return [float]($mm * $k) }
function Arrow([float]$x1,[float]$y1,[float]$x2,[float]$y2) {
  # arrowhead at (x2,y2), 60 deg spread (30 deg each side), 3 mm long, filled
  $ang = [math]::Atan2($y2 - $y1, $x2 - $x1)
  $len = 3.0 * $k
  $a1 = $ang + [math]::PI - 0.5236; $a2 = $ang + [math]::PI + 0.5236
  $p1 = New-Object System.Drawing.PointF ([float]($x2 + $len * [math]::Cos($a1))), ([float]($y2 + $len * [math]::Sin($a1)))
  $p2 = New-Object System.Drawing.PointF ([float]($x2 + $len * [math]::Cos($a2))), ([float]($y2 + $len * [math]::Sin($a2)))
  $tip = New-Object System.Drawing.PointF $x2, $y2
  $g.FillPolygon($brush, [System.Drawing.PointF[]]@($tip, $p1, $p2))
}
foreach ($it in $s.items) {
  switch ($it.t) {
    'box' {
      $r = New-Object System.Drawing.RectangleF (P $it.x), (P $it.y), (P $it.w), (P $it.h)
      $g.DrawRectangle($pen, $r.X, $r.Y, $r.Width, $r.Height)
      if ($it.text) { $g.DrawString(($it.text -replace '\\n', "`n"), $font, $brush, $r, $fmtC) }
    }
    'rbox' {   # terminator: rounded rectangle
      $x = P $it.x; $y = P $it.y; $w = P $it.w; $h = P $it.h; $rad = $h / 2
      $path = New-Object System.Drawing.Drawing2D.GraphicsPath
      $path.AddArc($x, $y, $h, $h, 90, 180)
      $path.AddArc($x + $w - $h, $y, $h, $h, 270, 180)
      $path.CloseFigure()
      $g.DrawPath($pen, $path)
      $r = New-Object System.Drawing.RectangleF $x, $y, $w, $h
      if ($it.text) { $g.DrawString(($it.text -replace '\\n', "`n"), $font, $brush, $r, $fmtC) }
    }
    'diamond' {
      $x = P $it.x; $y = P $it.y; $w = P $it.w; $h = P $it.h
      $pts = [System.Drawing.PointF[]]@(
        (New-Object System.Drawing.PointF ($x + $w/2), $y),
        (New-Object System.Drawing.PointF ($x + $w), ($y + $h/2)),
        (New-Object System.Drawing.PointF ($x + $w/2), ($y + $h)),
        (New-Object System.Drawing.PointF $x, ($y + $h/2)))
      $g.DrawPolygon($pen, $pts)
      $r = New-Object System.Drawing.RectangleF $x, $y, $w, $h
      if ($it.text) { $g.DrawString(($it.text -replace '\\n', "`n"), $font, $brush, $r, $fmtC) }
    }
    'line' {   # polyline through pts [[x,y],...], optional arrow at end
      $pts = @()
      foreach ($q in $it.pts) { $pts += (New-Object System.Drawing.PointF (P $q[0]), (P $q[1])) }
      for ($i = 1; $i -lt $pts.Count; $i++) { $g.DrawLine($pen, $pts[$i-1], $pts[$i]) }
      if ($it.arrow) { $n = $pts.Count; Arrow $pts[$n-2].X $pts[$n-2].Y $pts[$n-1].X $pts[$n-1].Y }
      if ($it.dot) { $d = 1.6 * $k; $g.FillEllipse($brush, $pts[0].X - $d/2, $pts[0].Y - $d/2, $d, $d) }
    }
    'text' {   # free text; anchor: c (center) or l (left)
      $r = New-Object System.Drawing.RectangleF (P $it.x), (P $it.y), (P $it.w), (P $it.h)
      $f = if ($it.a -eq 'l') { $fmtL } else { $fmtC }
      $fn = $font
      if ($it.size) { $fn = New-Object System.Drawing.Font ("Times New Roman", [float]$it.size, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Point) }
      $g.DrawString(($it.text -replace '\\n', "`n"), $fn, $brush, $r, $f)
    }
    'res' {    # resistor per GOST 2.728: rectangle 10x4 mm, horizontal (dir h) or vertical (dir v), centered at x,y
      $cx = P $it.x; $cy = P $it.y
      if ($it.dir -eq 'v') { $w = P 4; $h = P 10 } else { $w = P 10; $h = P 4 }
      $g.DrawRectangle($pen, $cx - $w/2, $cy - $h/2, $w, $h)
    }
    'gnd' {    # ground symbol at x,y (top of symbol)
      $cx = P $it.x; $cy = P $it.y
      $g.DrawLine($pen, $cx, $cy, $cx, $cy + (P 2))
      $g.DrawLine($pen, $cx - (P 3), $cy + (P 2), $cx + (P 3), $cy + (P 2))
      $g.DrawLine($pen, $cx - (P 2), $cy + (P 3.2), $cx + (P 2), $cy + (P 3.2))
      $g.DrawLine($pen, $cx - (P 1), $cy + (P 4.4), $cx + (P 1), $cy + (P 4.4))
    }
    'dash' {   # dash-dot rectangle (functional group boundary)
      $p2 = New-Object System.Drawing.Pen ([System.Drawing.Color]::Black), $lw
      $p2.DashPattern = [float[]]@(6.0, 2.5, 1.0, 2.5)
      $g.DrawRectangle($p2, (P $it.x), (P $it.y), (P $it.w), (P $it.h))
    }
  }
}
$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
"saved $Out ($W x $H px)"
