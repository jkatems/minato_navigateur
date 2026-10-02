#Requires -Version 5.1
[CmdletBinding()]
param(
    [string]$QtRoot = $env:QT_ROOT_DIR,
    [switch]$Installer,
    [switch]$RunBrowserTests,
    [ValidateRange(1, 64)][int]$Jobs = 2
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if ($env:OS -ne 'Windows_NT') { throw 'Ce script doit etre execute sous Windows x64.' }
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build-windows'

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program a echoue (code $LASTEXITCODE)." }
}

# Initialise MSVC in an ordinary PowerShell, without requiring a developer prompt.
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw 'Installer Visual Studio 2022 / Build Tools avec Desktop development with C++.' }
$vsRoot = & $vswhere -latest -version '[17.0,18.0)' -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsRoot) { throw 'MSVC 2022 x64 introuvable. Installer la charge C++ et le SDK Windows.' }
$devCmd = Join-Path $vsRoot 'Common7\Tools\VsDevCmd.bat'
$environmentLines = & $env:ComSpec /d /s /c "`"`"$devCmd`" -arch=x64 -host_arch=x64 >nul && set`""
if ($LASTEXITCODE -ne 0) { throw 'Initialisation MSVC impossible.' }
foreach ($line in $environmentLines) {
    if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
}

if (-not $QtRoot) { $QtRoot = $env:QTDIR }
if (-not $QtRoot) {
    $candidates = @(Get-ChildItem 'C:\Qt\*\msvc2022_64' -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Parent.Name -match '^6\.\d+\.\d+$' } |
        Sort-Object { [version]$_.Parent.Name } -Descending)
    if ($candidates.Count -gt 0) { $QtRoot = $candidates[0].FullName }
}
if (-not $QtRoot) { throw 'Qt introuvable. Relancer avec -QtRoot C:\Qt\6.x.y\msvc2022_64.' }
$QtRoot = (Resolve-Path $QtRoot).Path
foreach ($relative in @('bin\windeployqt.exe', 'lib\cmake\Qt6WebEngineWidgets\Qt6WebEngineWidgetsConfig.cmake')) {
    if (-not (Test-Path (Join-Path $QtRoot $relative))) { throw "Qt MSVC avec WebEngine requis : fichier absent $relative" }
}
$env:PATH = "$QtRoot\bin;$env:PATH"
$cmake = (Get-Command cmake -ErrorAction Stop).Source
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
$cpack = Join-Path (Split-Path $cmake) 'cpack.exe'
if ($Installer -and -not (Get-Command makensis -ErrorAction SilentlyContinue)) {
    $nsisBin = Join-Path ${env:ProgramFiles(x86)} 'NSIS'
    if (-not (Test-Path (Join-Path $nsisBin 'makensis.exe'))) { throw 'Installer NSIS pour utiliser -Installer, ou omettre cette option pour un ZIP.' }
    $env:PATH = "$nsisBin;$env:PATH"
}

Invoke-Checked $cmake @('-S', $projectRoot, '-B', $buildDir, '-G', 'Visual Studio 17 2022', '-A', 'x64', "-DCMAKE_PREFIX_PATH=$QtRoot", '-DBUILD_TESTING=ON')
Invoke-Checked $cmake @('--build', $buildDir, '--config', 'Release', '--parallel', "$Jobs")
$testPattern = if ($RunBrowserTests) { 'core|reports|smtp|sync|browser' } else { '^(core|reports|smtp|sync)$' }
Invoke-Checked $ctest @('--test-dir', $buildDir, '-C', 'Release', '-R', $testPattern, '--output-on-failure')

# A fresh output directory avoids including stale DLLs from an older Qt kit.
$stamp = (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0, 6)
$outputDir = Join-Path $projectRoot "dist\windows\$stamp"
$bundleDir = Join-Path $outputDir 'Minato'
Invoke-Checked $cmake @('--install', $buildDir, '--config', 'Release', '--prefix', $bundleDir)

foreach ($name in @('minato.exe', 'Qt6Core.dll', 'Qt6Widgets.dll', 'Qt6Network.dll', 'Qt6Sql.dll', 'Qt6WebEngineCore.dll', 'QtWebEngineProcess.exe', 'qwindows.dll', 'qsqlite.dll', 'qschannelbackend.dll', 'icudtl.dat', 'qtwebengine_resources.pak', 'vcruntime140.dll', 'msvcp140.dll')) {
    $found = @(Get-ChildItem $bundleDir -Recurse -File -Filter $name)
    if ($found.Count -eq 0) { throw "Distribution incomplete : $name absent." }
}
$locales = @(Get-ChildItem $bundleDir -Recurse -Directory -Filter 'qtwebengine_locales')
if ($locales.Count -eq 0 -or @(Get-ChildItem $locales[0].FullName -Filter '*.pak').Count -eq 0) {
    throw 'Locales WebEngine absentes.'
}

# Check DLL/platform loading without falling back to the Qt installation in PATH.
$originalPath = $env:PATH
$originalPlugins = $env:QT_PLUGIN_PATH
$originalPlatform = $env:QT_QPA_PLATFORM
try {
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $env:QT_PLUGIN_PATH = $null
    $env:QT_QPA_PLATFORM = 'windows'
    $process = Start-Process -FilePath (Join-Path $bundleDir 'bin\minato.exe') -ArgumentList '--version' -PassThru
    if (-not $process.WaitForExit(30000)) { $process.Kill(); throw 'Le test de lancement a depasse 30 secondes.' }
    if ($process.ExitCode -ne 0) { throw "Le programme distribue ne demarre pas (code $($process.ExitCode))." }
} finally {
    $env:PATH = $originalPath
    $env:QT_PLUGIN_PATH = $originalPlugins
    $env:QT_QPA_PLATFORM = $originalPlatform
}
Invoke-Checked $cpack @('--config', (Join-Path $buildDir 'CPackConfig.cmake'), '-C', 'Release', '-G', 'ZIP', '-B', $outputDir)
if ($Installer) {
    Invoke-Checked $cpack @('--config', (Join-Path $buildDir 'CPackConfig.cmake'), '-C', 'Release', '-G', 'NSIS', '-B', $outputDir)
}
Get-ChildItem $outputDir -File | Where-Object { $_.Extension -in '.zip', '.exe' } | ForEach-Object {
    $hash = (Get-FileHash $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $($_.Name)" | Set-Content -Encoding ascii ($_.FullName + '.sha256')
}
Write-Host "`nMinato est pret : $outputDir"
Write-Host 'Partager le ZIP complet ou le setup.exe. Ne pas partager minato.exe seul.'
