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
    "sansacloth_basic_fixture_probe.py"
)
foreach ($requiredScript in $requiredScripts) {
    if (-not (Test-Path -LiteralPath (Join-Path $source $requiredScript) -PathType Leaf)) {
        throw "Required O3DE probe Python script missing: $requiredScript"
    }
}
if ($scripts.Count -ne $requiredScripts.Count) {
    throw "Expected $($requiredScripts.Count) O3DE probe Python scripts, found $($scripts.Count): $source"
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

$gemFiles = @(Get-ChildItem -LiteralPath $gemTarget -File -Recurse)

Write-Host "SANSA_O3DE_BUILD|SCRIPT_COUNT|$($scripts.Count)"
Write-Host "SANSA_O3DE_BUILD|DEPLOY_TARGET|$target"
Write-Host "SANSA_O3DE_BUILD|GEM_FILE_COUNT|$($gemFiles.Count)"
Write-Host "SANSA_O3DE_BUILD|GEM_DEPLOY_TARGET|$gemTarget"
Write-Host "SANSA_O3DE_BUILD|RESULT|PASS"
