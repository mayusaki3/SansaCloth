param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot "build"),
    [string]$UnityProjectDirectory = ""
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $PSScriptRoot "..\..\..\..")
)
$assetsRoot = Join-Path $OutputDirectory "Assets"
$probeRoot = Join-Path $assetsRoot "SansaCloth\Probe"
$runtimeRoot = Join-Path $probeRoot "Runtime"
$editorRoot = Join-Path $probeRoot "Editor"
$fixtureRoot = Join-Path $probeRoot "Data\FixtureExchange\BasicV1"
$resultRoot = Join-Path $probeRoot "Data\SurfaceResponseResult\BasicV1"

if (Test-Path $OutputDirectory) {
    Remove-Item $OutputDirectory -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $runtimeRoot | Out-Null
New-Item -ItemType Directory -Force -Path $editorRoot | Out-Null
New-Item -ItemType Directory -Force -Path $fixtureRoot | Out-Null
New-Item -ItemType Directory -Force -Path $resultRoot | Out-Null

$runtimeFiles = Get-ChildItem -Path $PSScriptRoot -Filter "*.cs" -File
if ($runtimeFiles.Count -eq 0) {
    throw "No Unity probe runtime C# files found in $PSScriptRoot"
}
Copy-Item $runtimeFiles.FullName -Destination $runtimeRoot

$setupSource = Join-Path $PSScriptRoot "Editor\ProbeSetup.cs"
if (-not (Test-Path $setupSource)) {
    throw "ProbeSetup.cs not found: $setupSource"
}
Copy-Item $setupSource -Destination $editorRoot

$fixtureSource = Join-Path $repoRoot "reference\validation\fixture-exchange\basic-v1"
$resultSource = Join-Path $repoRoot "reference\validation\surface-response-result\basic-v1"

$fixtureFiles = @(Get-ChildItem -Path $fixtureSource -Filter "*.json" -File)
$resultFiles = @(Get-ChildItem -Path $resultSource -Filter "*.json" -File)

if ($fixtureFiles.Count -ne 30) {
    throw "Expected 30 Fixture Exchange JSON files, found $($fixtureFiles.Count)"
}
if ($resultFiles.Count -ne 30) {
    throw "Expected 30 SurfaceResponse result JSON files, found $($resultFiles.Count)"
}

Copy-Item $fixtureFiles.FullName -Destination $fixtureRoot
Copy-Item $resultFiles.FullName -Destination $resultRoot

Write-Host "SANSA_PROBE|BUILD.RUNTIME_CS_COUNT|$($runtimeFiles.Count)"
Write-Host "SANSA_PROBE|BUILD.FIXTURE_EXCHANGE_COUNT|$($fixtureFiles.Count)"
Write-Host "SANSA_PROBE|BUILD.SURFACE_RESPONSE_RESULT_COUNT|$($resultFiles.Count)"
Write-Host "SANSA_PROBE|BUILD.OUTPUT|$assetsRoot"

if (-not [string]::IsNullOrWhiteSpace($UnityProjectDirectory)) {
    $unityAssetsRoot = Join-Path $UnityProjectDirectory "Assets"
    if (-not (Test-Path $unityAssetsRoot)) {
        throw "Unity Project Assets directory not found: $unityAssetsRoot"
    }

    $unitySansaRoot = Join-Path $unityAssetsRoot "SansaCloth"
    $unityProbeRoot = Join-Path $unitySansaRoot "Probe"
    if (Test-Path $unityProbeRoot) {
        Remove-Item $unityProbeRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $unitySansaRoot | Out-Null
    Copy-Item $probeRoot -Destination $unitySansaRoot -Recurse

    Write-Host "SANSA_PROBE|DEPLOY.UNITY_PROJECT|$UnityProjectDirectory"
    Write-Host "SANSA_PROBE|DEPLOY.TARGET|$unityProbeRoot"
    Write-Host "SANSA_PROBE|DEPLOY.RESULT|PASS"
}

Write-Host "SANSA_PROBE|BUILD.RESULT|PASS"
