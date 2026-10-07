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
if ($scripts.Count -ne 1) {
    throw "Expected exactly 1 O3DE probe Python script, found $($scripts.Count): $source"
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
Copy-Item -LiteralPath $scripts[0].FullName -Destination $target -Force
Copy-Item -Path (Join-Path $gemSource "*") -Destination $gemTarget -Recurse -Force

$gemFiles = @(Get-ChildItem -LiteralPath $gemTarget -File -Recurse)

Write-Host "SANSA_O3DE_BUILD|SCRIPT_COUNT|$($scripts.Count)"
Write-Host "SANSA_O3DE_BUILD|DEPLOY_TARGET|$target"
Write-Host "SANSA_O3DE_BUILD|GEM_FILE_COUNT|$($gemFiles.Count)"
Write-Host "SANSA_O3DE_BUILD|GEM_DEPLOY_TARGET|$gemTarget"
Write-Host "SANSA_O3DE_BUILD|RESULT|PASS"
