param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Release',
    [string]$JucePath = '',
    [switch]$Install
)
$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
$Build = Join-Path $Root 'build/windows'
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}
Invoke-Checked 'python' @((Join-Path $Root 'scripts/generate-demo.py'))
Invoke-Checked 'python' @((Join-Path $Root 'scripts/verify-content.py'))
$Configure = @('-S', $Root, '-B', $Build, '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DBUILD_TESTING=ON')
if ($JucePath) { $Configure += "-DWEE_JUCE_PATH=$JucePath" }
Invoke-Checked 'cmake' $Configure
Invoke-Checked 'cmake' @('--build', $Build, '--config', $Configuration, '--parallel', '2')
Invoke-Checked 'ctest' @('--test-dir', $Build, '-C', $Configuration, '--output-on-failure')
$Plugin = Join-Path $Build "WEE_artefacts/$Configuration/VST3/WEE.vst3"
Write-Host "Plugin: $Plugin"
Write-Host "Standalone: $Build/WEE_artefacts/$Configuration/Standalone/WEE.exe"
if ($Install) {
    $Destination = Join-Path $env:CommonProgramFiles 'VST3'
    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Copy-Item -LiteralPath $Plugin -Destination $Destination -Recurse -Force
    Write-Host "Installed to $Destination (requires write permission, usually Administrator)."
}
