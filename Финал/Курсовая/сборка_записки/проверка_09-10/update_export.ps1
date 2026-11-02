param([string]$Folder)
$ErrorActionPreference = 'Stop'
$word = New-Object -ComObject Word.Application
Start-Sleep -Seconds 3
$word.Visible = $false
$word.DisplayAlerts = 0
$doc = $null
try {
    $items = Get-ChildItem -LiteralPath $Folder -Filter '*.docx' | Where-Object { $_.Length -gt 20000 }
    foreach ($item in $items) {
        $doc = $word.Documents.Open($item.FullName, $false, $false)
        foreach ($toc in $doc.TablesOfContents) { $toc.Update() }
        $doc.Repaginate()
        foreach ($toc in $doc.TablesOfContents) { $toc.UpdatePageNumbers() }
        $doc.Save()
        $pdf = [System.IO.Path]::ChangeExtension($item.FullName, '.pdf')
        $doc.ExportAsFixedFormat($pdf, 17)
        Write-Output ($item.Name + ': pages=' + $doc.ComputeStatistics(2))
        $doc.Close(0)
        $doc = $null
    }
} finally {
    if ($null -ne $doc) { $doc.Close(0) }
    $word.Quit()
}
