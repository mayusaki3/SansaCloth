param(
    [Parameter(Mandatory = $true)]
    [string]$O3DEProjectDirectory
)

$ErrorActionPreference = "Stop"

$probeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $probeRoot "Editor\Scripts\SansaCloth"
$gemSource = Join-Path $probeRoot "Gem\SansaClothBackendProbeValidation"
$projectRoot = [System.IO.Path]::GetFullPath($O3DEProjectDirectory)
$target = Join-Path $projectRoot "Editor\Scripts\SansaCloth"
$gemTarget = Join-Path $projectRoot "Gems\SansaClothBackendProbeValidation"

if (-not (Test-Path -LiteralPath $projectRoot -PathType Container)) {
    throw "O3DE project directory not found: $projectRoot"
}

$scripts = @(Get-ChildItem -LiteralPath $source -Filter "*.py" -File)
$requiredScripts = @(
    "sansacloth_coordinate_probe.py",
    "sansacloth_surface_reference_probe.py",
    "sansacloth_surface_query_probe.py",
    "sansacloth_sr001_boundary_probe.py",
    "sansacloth_basic_fixture_probe.py",
    "sansacloth_validation_capture_probe.py",
    "sansacloth_fixture_exchange_import_probe.py"
)
foreach ($requiredScript in $requiredScripts) {
    if (-not (Test-Path -LiteralPath (Join-Path $source $requiredScript) -PathType Leaf)) {
        throw "Required O3DE probe Python script missing: $requiredScript"
    }
}
if ($scripts.Count -ne $requiredScripts.Count) {
    throw "Expected $($requiredScripts.Count) O3DE probe Python scripts, found $($scripts.Count): $source"
}

$fixtureSource = [System.IO.Path]::GetFullPath(
    (Join-Path $probeRoot "..\..\..\..\reference\validation\fixture-exchange\basic-v1\SR-001-C0-G0.json")
)
if (-not (Test-Path -LiteralPath $fixtureSource -PathType Leaf)) {
    throw "Reference Fixture Exchange baseline not found: $fixtureSource"
}

$gemManifest = Join-Path $gemSource "gem.json"
if (-not (Test-Path -LiteralPath $gemManifest -PathType Leaf)) {
    throw "O3DE validation Gem manifest not found: $gemManifest"
}

if (Test-Path -LiteralPath $target) {
    Remove-Item -LiteralPath $target -Recurse -Force
}
if (Test-Path -LiteralPath $gemTarget) {
    Remove-Item -LiteralPath $gemTarget -Recurse -Force
}

New-Item -ItemType Directory -Path $target -Force | Out-Null
New-Item -ItemType Directory -Path $gemTarget -Force | Out-Null
foreach ($script in $scripts) {
    Copy-Item -LiteralPath $script.FullName -Destination $target -Force
}
Copy-Item -Path (Join-Path $gemSource "*") -Destination $gemTarget -Recurse -Force

$fixtureTargetDir = Join-Path $target "Fixtures"
New-Item -ItemType Directory -Path $fixtureTargetDir -Force | Out-Null
$fixtureTarget = Join-Path $fixtureTargetDir "SR-001-C0-G0.json"
Copy-Item -LiteralPath $fixtureSource -Destination $fixtureTarget -Force
$sourceHash = (Get-FileHash -LiteralPath $fixtureSource -Algorithm SHA256).Hash
$targetHash = (Get-FileHash -LiteralPath $fixtureTarget -Algorithm SHA256).Hash
if ($sourceHash -ne $targetHash) {
    throw "Reference Fixture Exchange SHA256 mismatch after deploy"
}

$gemFiles = @(Get-ChildItem -LiteralPath $gemTarget -File -Recurse)

Write-Host "SANSA_O3DE_BUILD|SCRIPT_COUNT|$($scripts.Count)"
Write-Host "SANSA_O3DE_BUILD|FIXTURE_CASE_ID|SR-001-C0-G0"
Write-Host "SANSA_O3DE_BUILD|FIXTURE_SHA256|$targetHash"
Write-Host "SANSA_O3DE_BUILD|FIXTURE_DEPLOY_TARGET|$fixtureTarget"
Write-Host "SANSA_O3DE_BUILD|DEPLOY_TARGET|$target"
Write-Host "SANSA_O3DE_BUILD|GEM_FILE_COUNT|$($gemFiles.Count)"
Write-Host "SANSA_O3DE_BUILD|GEM_DEPLOY_TARGET|$gemTarget"
Write-Host "SANSA_O3DE_BUILD|RESULT|PASS"
