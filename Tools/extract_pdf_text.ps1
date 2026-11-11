param([Parameter(Mandatory = $true)][string]$Name)

$ErrorActionPreference = 'Stop'
if ($Name -notmatch '^[a-zA-Z0-9][a-zA-Z0-9_-]{0,79}$') {
    throw 'Name must contain only letters, digits, underscores or hyphens.'
}
$sourceDir = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\Policy\sources'))
$source = Join-Path $sourceDir "$Name.pdf"
$textFile = Join-Path $sourceDir "$Name.txt"
$metadataFile = Join-Path $sourceDir "$Name.text.metadata.json"
if (-not (Test-Path -LiteralPath $source)) { throw "PDF source missing: $source" }
if ((Test-Path -LiteralPath $textFile) -or (Test-Path -LiteralPath $metadataFile)) {
    throw 'Extracted text already exists; preserve the earlier extraction.'
}
$tool = Get-Command pdftotext.exe -ErrorAction Stop
& $tool.Source -enc UTF-8 -layout $source $textFile
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $textFile)) {
    throw 'pdftotext extraction failed.'
}
$metadata = [ordered]@{
    schema = 'legislation_text_extraction_v1'
    source_pdf = "$Name.pdf"
    source_sha256 = (Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant()
    text_sha256 = (Get-FileHash -LiteralPath $textFile -Algorithm SHA256).Hash.ToLowerInvariant()
    tool = $tool.Source
    extracted_at_utc = [DateTime]::UtcNow.ToString('o')
    status = 'unreviewed extraction; not loaded by harness'
}
$metadata | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $metadataFile -Encoding UTF8
Write-Output "Extracted $textFile"
