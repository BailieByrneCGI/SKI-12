param(
    [Parameter(Mandatory = $true)][string]$Url,
    [Parameter(Mandatory = $true)][string]$Name,
    [string]$Format = ''
)

$ErrorActionPreference = 'Stop'
$uri = [Uri]$Url
$allowedHosts = @('legislation.gov.uk', 'www.legislation.gov.uk', 'gov.uk', 'www.gov.uk', 'ico.org.uk', 'www.ico.org.uk')
if ($uri.Scheme -ne 'https' -or $allowedHosts -notcontains $uri.Host) {
    throw 'Only HTTPS URLs from legislation.gov.uk, gov.uk or ico.org.uk are accepted.'
}
if ($Name -notmatch '^[a-zA-Z0-9][a-zA-Z0-9_-]{0,79}$') {
    throw 'Name must contain only letters, digits, underscores or hyphens.'
}
if (-not $Format) {
    $extension = [IO.Path]::GetExtension($uri.AbsolutePath).TrimStart('.').ToLowerInvariant()
    $Format = if ($extension -in @('pdf', 'txt', 'json', 'html', 'xml')) { $extension } else { 'html' }
}
if ($Format -notin @('pdf', 'txt', 'json', 'html', 'xml')) {
    throw 'Format must be pdf, txt, json, html or xml.'
}

$workspace = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$targetDir = [IO.Path]::GetFullPath((Join-Path $workspace 'Policy\sources'))
$targetFile = [IO.Path]::GetFullPath((Join-Path $targetDir "$Name.$Format"))
$metadataFile = [IO.Path]::GetFullPath((Join-Path $targetDir "$Name.metadata.json"))
$prefix = $targetDir.TrimEnd('\') + '\'
if (-not $targetFile.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase) -or
    -not $metadataFile.StartsWith($prefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Destination outside Policy/sources is not allowed.'
}
if ((Test-Path -LiteralPath $targetFile) -or (Test-Path -LiteralPath $metadataFile)) {
    throw "A source named $Name already exists; use a new name to preserve the earlier version."
}

New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
$temporaryFile = Join-Path $targetDir "$Name.download.tmp"
if (Test-Path -LiteralPath $temporaryFile) { throw 'Temporary download path already exists.' }
try {
    $response = Invoke-WebRequest -Uri $uri -UseBasicParsing -MaximumRedirection 3 -OutFile $temporaryFile -PassThru
    $finalUri = [Uri]$response.BaseResponse.ResponseUri.AbsoluteUri
    if ($allowedHosts -notcontains $finalUri.Host -or $finalUri.Scheme -ne 'https') {
        throw 'Redirect left the approved HTTPS official hosts.'
    }
    $bytes = (Get-Item -LiteralPath $temporaryFile).Length
    if ($bytes -eq 0) { throw 'The downloaded file is empty.' }
    if ($Format -eq 'pdf') {
        $stream = [IO.File]::OpenRead($temporaryFile)
        try {
            $headerBytes = New-Object byte[] 5
            if ($stream.Read($headerBytes, 0, 5) -ne 5 -or
                [Text.Encoding]::ASCII.GetString($headerBytes) -ne '%PDF-') {
                throw 'The response is not a PDF.'
            }
        } finally { $stream.Dispose() }
    }
    $hash = (Get-FileHash -LiteralPath $temporaryFile -Algorithm SHA256).Hash.ToLowerInvariant()
    Move-Item -LiteralPath $temporaryFile -Destination $targetFile
    $metadata = [ordered]@{
        schema = 'legislation_source_v1'
        name = $Name
        requested_url = $Url
        final_url = $finalUri.AbsoluteUri
        format = $Format
        retrieved_at_utc = [DateTime]::UtcNow.ToString('o')
        sha256 = $hash
        bytes = $bytes
        status = 'raw source; clause review required before use by harness'
    }
    $metadata | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $metadataFile -Encoding UTF8
    Write-Output "Saved $targetFile ($bytes bytes, SHA-256 $hash)"
} catch {
    if (Test-Path -LiteralPath $temporaryFile) { Remove-Item -LiteralPath $temporaryFile }
    throw
}
