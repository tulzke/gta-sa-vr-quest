# Extract the supplied HD IMG's DFFs and copy its loose PNG textures for authoring.
# No conversion, game changes, or device access. Keep output outside tracked assets.
param(
    [Parameter(Mandatory=$true)][string]$Img,
    [Parameter(Mandatory=$true)][string]$Textures,
    [string]$Out = "$PSScriptRoot/../build/task20-hdweapons-dff"
)
$ErrorActionPreference = 'Stop'
$bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Img).Path)
if ($bytes.Length -lt 8 -or [Text.Encoding]::ASCII.GetString($bytes, 0, 4) -ne 'VER2') {
    throw 'Expected a VER2 IMG archive'
}
$count = [BitConverter]::ToUInt32($bytes, 4)
if (8L + 32L * $count -gt $bytes.Length) { throw 'Truncated IMG directory' }
$models = Join-Path $Out 'models'
New-Item -ItemType Directory -Force -Path $models | Out-Null
$exported = @()
for ($i = 0; $i -lt $count; $i++) {
    $entry = 8 + 32 * $i
    $name = [Text.Encoding]::ASCII.GetString($bytes, $entry + 8, 24).Trim([char]0)
    if (-not $name.EndsWith('.dff', [StringComparison]::OrdinalIgnoreCase)) { continue }
    if ($name -notmatch '^[a-zA-Z0-9_-]+\.dff$') { throw "Unsafe IMG name: $name" }
    $offset = 2048L * [BitConverter]::ToUInt32($bytes, $entry)
    $length = 2048L * [BitConverter]::ToUInt16($bytes, $entry + 4)
    if ($length -lt 12 -or $offset + $length -gt $bytes.Length) { throw "Truncated DFF: $name" }
    $chunkType = [BitConverter]::ToUInt32($bytes, [int]$offset)
    $chunkLength = 12L + [BitConverter]::ToUInt32($bytes, [int]$offset + 4)
    if ($chunkType -ne 16 -or $chunkLength -gt $length) { throw "Invalid DFF clump: $name" }
    # Remove only IMG sector padding, preserve the original RenderWare stream.
    $blob = New-Object byte[] $chunkLength
    [Array]::Copy($bytes, $offset, $blob, 0L, $chunkLength)
    [IO.File]::WriteAllBytes((Join-Path $models $name), $blob)
    $exported += $name
}
$texOut = Join-Path $Out 'textures'
New-Item -ItemType Directory -Force -Path $texOut | Out-Null
Get-ChildItem -LiteralPath $Textures -Filter '*.png' -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $texOut -Force
}
$exported | Set-Content -LiteralPath (Join-Path $Out 'models.txt') -Encoding utf8
Write-Output "Exported $($exported.Count) DFFs to $([IO.Path]::GetFullPath($Out))"
