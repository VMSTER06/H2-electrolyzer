param([string]$Docx, [string]$Pdf)
$ErrorActionPreference = 'Stop'
$word = New-Object -ComObject Word.Application
Start-Sleep -Seconds 3
$word.Visible = $false
$word.DisplayAlerts = 0
try {
  $doc = $word.Documents.Open($Docx, $false, $true)
  $doc.Repaginate()
  $pages = $doc.ComputeStatistics(2)
  $doc.ExportAsFixedFormat($Pdf, 17)
  Write-Output "pages=$pages"
  $doc.Close(0)
} finally {
  $word.Quit()
}
