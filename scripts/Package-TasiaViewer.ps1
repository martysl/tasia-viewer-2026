#!/usr/bin/env pwsh
# Strict Tasia package staging. Does not launch the viewer, install, or delete old packages.
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$BuildDir,
    [Parameter(Mandatory)][string]$Destination,
    [ValidateSet('Release','Debug','RelWithDebInfo')][string]$Configuration = 'Release',
    [string]$DependencyDir = '',
    [string]$RuntimeBase = ''
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
if ([string]::IsNullOrWhiteSpace($DependencyDir)) { $DependencyDir = $repo }
$runtime = Join-Path $BuildDir "newview/$Configuration"
if (Test-Path $Destination) { throw 'Use a new destination to avoid stale package contents.' }
$required = @('TasiaViewer.exe', 'tasia-ffmpeg.exe', 'tasia-ffmpeg-license.txt', 'tasia-ffmpeg-source.json')
foreach ($file in $required) {
    if (!(Test-Path (Join-Path $runtime $file) -PathType Leaf)) { throw "Required runtime file missing: $file" }
}
$provenance = Get-Content (Join-Path $runtime 'tasia-ffmpeg-source.json') -Raw | ConvertFrom-Json
if ((Get-FileHash (Join-Path $runtime 'tasia-ffmpeg.exe') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $provenance.binarySHA256) {
    throw 'Converter hash does not match the verified dependency receipt.'
}
New-Item -ItemType Directory -Path $Destination | Out-Null
# Optionally reuse only runtime DLLs from a known working installation. Never
# copy its executable, saved accounts, settings, logs or modify that installation.
if (![string]::IsNullOrWhiteSpace($RuntimeBase)) {
    if (!(Test-Path $RuntimeBase -PathType Container)) { throw 'Runtime base directory missing.' }
    Get-ChildItem $RuntimeBase -Filter '*.dll' -File | Copy-Item -Destination $Destination
}
# Runtime is build output, never an installed viewer/user-data directory.
Get-ChildItem $runtime -File | Where-Object {
    $_.Extension -in @('.exe','.dll','.crt','.pak','.bin','.dat') -and $_.Name -notlike 'Kirstens*'
} | Copy-Item -Destination $Destination
foreach ($file in @('tasia-ffmpeg-license.txt','tasia-ffmpeg-source.json')) {
    Copy-Item (Join-Path $runtime $file) $Destination
}
foreach ($folder in @('llplugin','locales','resources')) {
    $path = Join-Path $runtime $folder
    if (Test-Path $path -PathType Container) { Copy-Item $path $Destination -Recurse }
}
# The Kirsten-derived dependency layout places CEF/VLC runtimes under bin/release.
$pluginDependencies = Join-Path $DependencyDir 'bin/release'
if (Test-Path $pluginDependencies -PathType Container) {
    $plugins = Join-Path $Destination 'llplugin'
    New-Item -ItemType Directory $plugins -Force | Out-Null
    Get-ChildItem $pluginDependencies | Where-Object { $_.Name -ne 'SLVoice.exe' } |
        Copy-Item -Destination $plugins -Recurse -Force
}
$runtimeLibraries = Join-Path $DependencyDir 'lib/release'
if (Test-Path $runtimeLibraries -PathType Container) {
    Get-ChildItem $runtimeLibraries -Filter '*.dll' -File | Copy-Item -Destination $Destination -Force
}
# The viewer is built with FMOD Studio. Stage its runtime loader and the Windows
# C++/CX runtime from the checked-in viewer runtime set, plus the current MSVC
# redistributables from indra/Misc (newview also contains older copies).
$viewerRuntime = Join-Path $repo 'indra/newview'
foreach ($file in @('fmod.dll','vccorlib140.dll')) {
    $source = Join-Path $viewerRuntime $file
    if (!(Test-Path $source -PathType Leaf)) { throw "Required viewer runtime DLL missing: $file" }
    Copy-Item $source $Destination -Force
}
$vcRuntime = Join-Path $repo 'indra/Misc'
$vcRuntimeFiles = @('msvcp140.dll','msvcp140_1.dll','msvcp140_2.dll',
    'msvcp140_atomic_wait.dll','msvcp140_codecvt_ids.dll','vcruntime140.dll',
    'vcruntime140_1.dll','vcruntime140_threads.dll')
foreach ($file in $vcRuntimeFiles) {
    $source = Join-Path $vcRuntime $file
    if (!(Test-Path $source -PathType Leaf)) { throw "Required MSVC runtime DLL missing: $file" }
    Copy-Item $source $Destination -Force
}
$cefResources = Join-Path $DependencyDir 'resources'
if (Test-Path $cefResources -PathType Container) {
    $plugins = Join-Path $Destination 'llplugin'
    New-Item -ItemType Directory $plugins -Force | Out-Null
    Get-ChildItem $cefResources | Copy-Item -Destination $plugins -Recurse -Force
}
$webrtc = Join-Path $BuildDir "llwebrtc/$Configuration/llwebrtc.dll"
if (Test-Path $webrtc -PathType Leaf) { Copy-Item $webrtc $Destination -Force }
foreach ($folder in @('app_settings','character','fonts','skins')) {
    $path = Join-Path $repo "indra/newview/$folder"
    if (!(Test-Path $path -PathType Container)) { throw "Required source asset directory missing: $folder" }
    Copy-Item $path $Destination -Recurse
}
foreach ($file in @('featuretable.txt','gpu_table.txt')) {
    $path = Join-Path $repo "indra/newview/$file"
    if (Test-Path $path) { Copy-Item $path $Destination }
}
Copy-Item (Join-Path $repo 'LICENSE') $Destination
# Marker is resolved relative to the executable, not to the current directory.
Set-Content -Path (Join-Path $Destination 'TasiaViewer.portable') -Value 'TasiaViewerData' -Encoding ASCII
Copy-Item (Join-Path $repo 'docs/PORTABLE.md') $Destination
$mediaDir = Join-Path $Destination 'skins/default/html/en/tasia_media'
foreach ($name in @('index.html','picker.css','picker.bundle.js','preview.html','preview.css','preview.bundle.js','sdk-manifest.json','THIRD_PARTY_LICENSES.txt')) {
    if (!(Test-Path (Join-Path $mediaDir $name) -PathType Leaf)) { throw "Required media SDK asset missing: $name" }
}
$mediaManifest = Get-Content (Join-Path $mediaDir 'sdk-manifest.json') -Raw | ConvertFrom-Json
if ((Get-FileHash (Join-Path $mediaDir 'preview.bundle.js') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $mediaManifest.preview_bundle_sha256) {
    throw 'Chat media preview bundle hash mismatch.'
}
if ((Get-FileHash (Join-Path $mediaDir 'picker.bundle.js') -Algorithm SHA256).Hash.ToLowerInvariant() -ne $mediaManifest.bundle_sha256) {
    throw 'GIPHY Web SDK bundle hash mismatch.'
}
Copy-Item (Join-Path $repo 'indra/newview/res/tasia_icon.ico') (Join-Path $Destination 'TasiaViewer.ico')
$brandManifestFile = Join-Path $repo 'indra/newview/res/tasia_brand_assets.json'
$brandManifest = Get-Content $brandManifestFile -Raw | ConvertFrom-Json
foreach ($asset in $brandManifest.outputs.PSObject.Properties) {
    $relative = $asset.Name
    if ($relative -match '(^/|\x5c|:|(^|/)\.\.(/|$))') { throw 'Unsafe branding asset path.' }
    $sourceAsset = Join-Path (Join-Path $repo 'indra/newview') $relative
    $targetAsset = if ($relative -eq 'res/tasia_icon.ico') { Join-Path $Destination 'TasiaViewer.ico' } else { Join-Path $Destination $relative }
    foreach ($path in @($sourceAsset, $targetAsset)) {
        if (!(Test-Path $path -PathType Leaf) -or
            (Get-FileHash $path -Algorithm SHA256).Hash.ToLowerInvariant() -ne $asset.Value.sha256) {
            throw "Approved branding asset missing or mismatched: $relative"
        }
    }
}
Copy-Item $brandManifestFile (Join-Path $Destination 'tasia-brand-assets.json')
foreach ($name in @('quic_on.png','quic_off.png','quic_unavailable.png')) {
    $relative = "skins/default/textures/icons/$name"
    $source = Join-Path (Join-Path $repo 'indra/newview') $relative
    $target = Join-Path $Destination $relative
    if (!(Test-Path $target -PathType Leaf) -or
        (Get-FileHash $source -Algorithm SHA256).Hash -ne (Get-FileHash $target -Algorithm SHA256).Hash) {
        throw "QUIC status asset missing or mismatched: $name"
    }
}
Copy-Item (Join-Path $repo 'ca-bundle.crt') $Destination -Force
Copy-Item (Join-Path $repo 'scripts/messages/message_template.msg') (Join-Path $Destination 'app_settings') -Force
Copy-Item (Join-Path $repo 'etc/message.xml') (Join-Path $Destination 'app_settings') -Force
# Prefer the actual built plugin host if it was not staged beside the viewer.
$pluginHost = Join-Path $BuildDir "llplugin/slplugin/$Configuration/SLPlugin.exe"
if (Test-Path $pluginHost) { Copy-Item $pluginHost $Destination -Force }
$cefPlugin = Join-Path $BuildDir "media_plugins/cef/$Configuration/media_plugin_cef.dll"
if (Test-Path $cefPlugin) {
    New-Item -ItemType Directory (Join-Path $Destination 'llplugin') -Force | Out-Null
    Copy-Item $cefPlugin (Join-Path $Destination 'llplugin') -Force
}
foreach ($file in @('SLPlugin.exe','llplugin/media_plugin_cef.dll','llplugin/libcef.dll',
    'fmod.dll','vccorlib140.dll','msvcp140.dll','msvcp140_1.dll','msvcp140_2.dll',
    'msvcp140_atomic_wait.dll','msvcp140_codecvt_ids.dll','vcruntime140.dll',
    'vcruntime140_1.dll','vcruntime140_threads.dll',
    'app_settings/commands.xml','app_settings/toolbars.xml','app_settings/tasia_default_grids.xml',
    'skins/default/textures/toolbar_icons/tasia_chat.png',
    'skins/default/textures/world/tasia_planet.png',
    'skins/default/textures/icons/tasiafeed.png')) {
    if (!(Test-Path (Join-Path $Destination $file) -PathType Leaf)) { throw "Package incomplete: $file" }
}
if (!(Get-ChildItem (Join-Path $Destination 'llplugin') -Recurse -Filter '*.pak')) {
    throw 'Package incomplete: CEF resource packs are missing.'
}
# A shortcut is local package output, never a desktop/start-menu installation.
if ($IsWindows -or $env:OS -eq 'Windows_NT') {
    $shell = New-Object -ComObject WScript.Shell
    $shortcut = $shell.CreateShortcut((Join-Path $Destination 'Tasia Viewer.lnk'))
    $shortcut.TargetPath = Join-Path (Resolve-Path $Destination).Path 'TasiaViewer.exe'
    $shortcut.WorkingDirectory = (Resolve-Path $Destination).Path
    $shortcut.IconLocation = (Join-Path (Resolve-Path $Destination).Path 'TasiaViewer.ico') + ',0'
    $shortcut.Save()
}
$root = (Resolve-Path $Destination).Path
Get-ChildItem $Destination -Recurse -File | Where-Object { $_.Extension -ne '.lnk' } | ForEach-Object {
    [pscustomobject]@{ Path = $_.FullName.Substring($root.Length + 1); SHA256 = (Get-FileHash $_.FullName -Algorithm SHA256).Hash }
} | ConvertTo-Json | Set-Content (Join-Path $Destination 'package-sha256.json') -Encoding utf8
Write-Host 'Tasia package staged with file hashes. Runtime verification is still required.'
