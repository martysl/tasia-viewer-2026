#!/usr/bin/env pwsh
# Stage a dated, SHA256-pinned converter from the old viewer's upstream supplier.
[CmdletBinding()]
param([Parameter(Mandatory)][string]$Destination)
$ErrorActionPreference = 'Stop'
$tag = 'autobuild-2026-10-18-13-01'
$asset = 'ffmpeg-n9.0.2-28-g669f5ab083-win64-gpl-9.0.zip'
$expected = 'a38198cc130ed3fa02097d0baebcea9c1b8a9d9d3d1d8161d691d1f03709158b'
$url = "https://github.com/BtbN/FFmpeg-Builds/releases/download/$tag/$asset"
if (Test-Path $Destination) { throw 'Use a new destination; existing dependency files are never overwritten.' }
$work = Join-Path ([IO.Path]::GetTempPath()) ('tasia-ffmpeg-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $work | Out-Null
try {
    $archive = Join-Path $work 'ffmpeg.zip'
    Invoke-WebRequest -Uri $url -OutFile $archive
    $actual = (Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expected) { throw 'FFmpeg archive SHA256 mismatch.' }
    $unpack = Join-Path $work 'unpack'
    Expand-Archive -Path $archive -DestinationPath $unpack
    $roots = @(Get-ChildItem $unpack -Directory)
    if ($roots.Count -ne 1) { throw 'Unexpected FFmpeg archive layout.' }
    $exe = Join-Path $roots[0].FullName 'bin/ffmpeg.exe'
    $license = Join-Path $roots[0].FullName 'LICENSE.txt'
    if (!(Test-Path $exe -PathType Leaf) -or !(Test-Path $license -PathType Leaf)) {
        throw 'Converter or license missing from archive.'
    }
    New-Item -ItemType Directory -Path $Destination | Out-Null
    Copy-Item $exe (Join-Path $Destination 'tasia-ffmpeg.exe')
    Copy-Item $license (Join-Path $Destination 'tasia-ffmpeg-license.txt')
    @{
        release = $tag; asset = $asset; archiveUrl = $url; archiveSHA256 = $actual
        binarySHA256 = (Get-FileHash $exe -Algorithm SHA256).Hash.ToLowerInvariant()
        buildScripts = 'https://github.com/BtbN/FFmpeg-Builds'
        note = 'Separate GPLv3 executable. Preserve the license and establish corresponding-source distribution obligations before public release.'
    } | ConvertTo-Json | Set-Content (Join-Path $Destination 'tasia-ffmpeg-source.json') -Encoding utf8
    Write-Host 'Verified converter, license and provenance staged. No executable was launched.'
}
finally {
    # Only this script's newly allocated work directory is removed.
    Remove-Item -LiteralPath $work -Recurse -Force
}
