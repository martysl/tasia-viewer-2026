param([switch]$ValidateOnly)
$ErrorActionPreference = 'Stop'
$bytes = $null
$keys = $null
$psi = $null
try {
    $exe = Join-Path $PSScriptRoot 'TasiaViewer.exe'
    $private = Join-Path $PSScriptRoot 'TasiaViewerData\secrets'
    $expected = [IO.File]::ReadAllText((Join-Path $private 'viewer.sha256')).Trim()
    if ($expected -notmatch '^[0-9a-fA-F]{64}$' -or
        (Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ne $expected) {
        throw 'Viewer identity mismatch'
    }
    Add-Type -AssemblyName System.Security
    $bytes = [Security.Cryptography.ProtectedData]::Unprotect(
        [IO.File]::ReadAllBytes((Join-Path $private 'media-keys.dpapi')),
        $null, [Security.Cryptography.DataProtectionScope]::CurrentUser)
    $keys = [Text.Encoding]::UTF8.GetString($bytes) | ConvertFrom-Json
    if ([string]::IsNullOrWhiteSpace($keys.giphy) -or [string]::IsNullOrWhiteSpace($keys.klipy)) {
        throw 'Encrypted media keys unavailable'
    }
    $psi = New-Object Diagnostics.ProcessStartInfo
    $psi.FileName = $exe
    $psi.WorkingDirectory = $PSScriptRoot
    $psi.UseShellExecute = $false
    $psi.EnvironmentVariables['TASIA_GIPHY_KEY'] = $keys.giphy
    $psi.EnvironmentVariables['TASIA_KLIPY_KEY'] = $keys.klipy
    if ($ValidateOnly) {
        Write-Output 'VALIDATION OK: executable hash and both encrypted provider keys verified; viewer not started.'
    } else {
        # Do not open two processes against the same portable profile.
        $running = @(Get-CimInstance Win32_Process -Filter "Name='TasiaViewer.exe'" |
            Where-Object { $_.ExecutablePath -eq $exe })
        if ($running.Count) { throw 'This portable viewer is already running' }
        $child = [Diagnostics.Process]::Start($psi)
        if ($null -eq $child) { throw 'Viewer process was not created' }
        $child.Dispose()
    }
} catch {
    # Never include decrypted data or exception details in errors or log files.
    [Console]::Error.WriteLine('Viewer launch validation failed. Check executable hash, encrypted media file and whether this copy is already running. No credentials were displayed.')
    exit 1
} finally {
    if ($bytes) { [Array]::Clear($bytes, 0, $bytes.Length) }
    if ($psi) {
        $psi.EnvironmentVariables.Remove('TASIA_GIPHY_KEY')
        $psi.EnvironmentVariables.Remove('TASIA_KLIPY_KEY')
    }
    $keys = $null
    $psi = $null
}
