param (
    [string] $RepositoryRoot,
    [string] $InstallerPath,
    [string] $PluginPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $RepositoryRoot) {
    $RepositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
}
if (-not $InstallerPath) {
    $InstallerPath = Join-Path $RepositoryRoot 'build/tools/webview2/MicrosoftEdgeWebView2RuntimeInstallerX64.exe'
}
if (-not $PluginPath) {
    $PluginPath = Join-Path $RepositoryRoot 'build/windows-x64/HYPERREAL_artefacts/Release/VST3/HYPERREAL.vst3'
}

$manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'webview2-runtime.json') -Raw | ConvertFrom-Json
$installer = Get-Item -LiteralPath $InstallerPath -ErrorAction Stop
if ($installer.Name -ne $manifest.fileName -or $installer.Length -ne $manifest.bytes) {
    throw 'WebView2 installer filename or size does not match the recorded offline artifact.'
}
$hash = (Get-FileHash -LiteralPath $installer.FullName -Algorithm SHA256).Hash
if ($hash -ne $manifest.sha256) {
    throw 'WebView2 installer SHA-256 does not match the recorded offline artifact.'
}
$signature = Get-AuthenticodeSignature -LiteralPath $installer.FullName
if ($signature.Status -ne 'Valid' -or
    $signature.SignerCertificate.Subject -notmatch [regex]::Escape($manifest.signer)) {
    throw 'WebView2 installer does not have a valid Microsoft signature.'
}

$plugin = Get-Item -LiteralPath $PluginPath -ErrorAction Stop
$binary = Join-Path $plugin.FullName 'Contents/x86_64-win/HYPERREAL.vst3'
if (-not $plugin.PSIsContainer -or -not (Test-Path -LiteralPath $binary -PathType Leaf)) {
    throw 'Release VST3 bundle is missing its x64 binary.'
}

$uiRoot = Join-Path $RepositoryRoot 'packages/ui/dist'
$index = Join-Path $uiRoot 'index.html'
if (-not (Test-Path -LiteralPath $index -PathType Leaf)) {
    throw 'Local UI bundle is missing. Build packages/ui first.'
}
$html = Get-Content -LiteralPath $index -Raw
if ($html -match '(?i)https?://|//[^/]|@vite/client') {
    throw 'UI entry page contains a remote URL or development-server reference.'
}
foreach ($asset in @('assets/app.js', 'assets/style.css')) {
    if ($html -notlike "*./$asset*" -or
        -not (Test-Path -LiteralPath (Join-Path $uiRoot $asset) -PathType Leaf)) {
        throw "UI entry page does not reference a present local asset: $asset"
    }
}

$clientId = '{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}'
$registryPaths = @(
    "Registry::HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\$clientId",
    "Registry::HKEY_CURRENT_USER\Software\Microsoft\EdgeUpdate\Clients\$clientId"
)
$runtimeVersions = foreach ($path in $registryPaths) {
    $entry = Get-ItemProperty -Path $path -ErrorAction SilentlyContinue
    if ($entry -and $entry.pv -and $entry.pv -ne '0.0.0.0') {
        $entry.pv
    }
}

Write-Output "VST3 bundle: $($plugin.FullName)"
Write-Output 'Local UI: index.html, app.js and style.css present; entry page has no external URL.'
Write-Output "WebView2 offline installer: Microsoft signature valid; SHA-256 $hash"
if ($runtimeVersions) {
    Write-Output "Installed WebView2 Runtime: $($runtimeVersions -join ', ')"
} else {
    Write-Output 'Installed WebView2 Runtime: absent; run the verified standalone installer before launching the plugin.'
}
Write-Output 'Offline artifact readiness passed. This does not replace a disconnected clean-machine test.'
