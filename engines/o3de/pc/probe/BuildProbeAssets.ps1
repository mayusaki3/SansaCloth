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
    "sansacloth_fixture_exchange_import_probe.py",
    "sansacloth_fixture_exchange_matrix_probe.py",
    "sansacloth_handoff_string_probe.py"
)
foreach ($requiredScript in $requiredScripts) {
    if (-not (Test-Path -LiteralPath (Join-Path $source $requiredScript) -PathType Leaf)) {
        throw "Required O3DE probe Python script missing: $requiredScript"
    }
}
if ($scripts.Count -ne $requiredScripts.Count) {
    throw "Expected $($requiredScripts.Count) O3DE probe Python scripts, found $($scripts.Count): $source"
}

$fixtureSourceDir = [System.IO.Path]::GetFullPath((Join-Path $probeRoot "..\..\..\..\reference\validation\fixture-exchange\basic-v1"))
$expectedCaseIds = @(
    foreach ($scenario in 1..5) {
        foreach ($conformity in @("0", "05", "1")) {
            foreach ($gravity in @("0", "1")) {
                "SR-{0:D3}-C{1}-G{2}" -f $scenario, $conformity, $gravity
            }
        }
    }
)
if ($expectedCaseIds.Count -ne 30 -or -not (Test-Path -LiteralPath $fixtureSourceDir -PathType Container)) { throw "Reference 30-case Fixture Exchange directory missing: $fixtureSourceDir" }
foreach ($caseId in $expectedCaseIds) {
    if (-not (Test-Path -LiteralPath (Join-Path $fixtureSourceDir "$caseId.json") -PathType Leaf)) { throw "Reference Fixture Exchange baseline missing: $caseId" }
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
foreach ($caseId in $expectedCaseIds) {
    $fixtureSource = Join-Path $fixtureSourceDir "$caseId.json"
    $fixtureTarget = Join-Path $fixtureTargetDir "$caseId.json"
    Copy-Item -LiteralPath $fixtureSource -Destination $fixtureTarget -Force
    $sourceHash = (Get-FileHash -LiteralPath $fixtureSource -Algorithm SHA256).Hash
    $targetHash = (Get-FileHash -LiteralPath $fixtureTarget -Algorithm SHA256).Hash
    if ($sourceHash -ne $targetHash) { throw "Reference Fixture Exchange SHA256 mismatch after deploy: $caseId" }
    if ($caseId -eq "SR-001-C0-G0") { $singleFixtureHash = $targetHash; $singleFixtureTarget = $fixtureTarget }
    Write-Host "SANSA_O3DE_BUILD|MATRIX_CASE|$caseId|$targetHash|PASS"
}

$gemFiles = @(Get-ChildItem -LiteralPath $gemTarget -File -Recurse)

Write-Host "SANSA_O3DE_BUILD|SCRIPT_COUNT|$($scripts.Count)"
Write-Host "SANSA_O3DE_BUILD|MATRIX_CASE_COUNT|$($expectedCaseIds.Count)"
Write-Host "SANSA_O3DE_BUILD|FIXTURE_CASE_ID|SR-001-C0-G0"
Write-Host "SANSA_O3DE_BUILD|FIXTURE_SHA256|$singleFixtureHash"
Write-Host "SANSA_O3DE_BUILD|FIXTURE_DEPLOY_TARGET|$singleFixtureTarget"
Write-Host "SANSA_O3DE_BUILD|DEPLOY_TARGET|$target"
Write-Host "SANSA_O3DE_BUILD|GEM_FILE_COUNT|$($gemFiles.Count)"
Write-Host "SANSA_O3DE_BUILD|GEM_DEPLOY_TARGET|$gemTarget"
Write-Host "SANSA_O3DE_BUILD|RESULT|PASS"
