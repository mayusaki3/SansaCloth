param(
    [Parameter(Mandatory = $true)]
    [string]$O3DEProjectDirectory
)

$ErrorActionPreference = "Stop"

$probeRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$source = Join-Path $probeRoot "Editor\Scripts\SansaCloth"
$projectRoot = [System.IO.Path]::GetFullPath($O3DEProjectDirectory)
$target = Join-Path $projectRoot "Editor\Scripts\SansaCloth"

if (-not (Test-Path -LiteralPath $projectRoot -PathType Container)) {
    throw "O3DE project directory not found: $projectRoot"
}

$scripts = @(Get-ChildItem -LiteralPath $source -Filter "*.py" -File)
if ($scripts.Count -ne 1) {
    throw "Expected exactly 1 O3DE probe Python script, found $($scripts.Count): $source"
}

if (Test-Path -LiteralPath $target) {
    Remove-Item -LiteralPath $target -Recurse -Force
}

New-Item -ItemType Directory -Path $target -Force | Out-Null
Copy-Item -LiteralPath $scripts[0].FullName -Destination $target -Force

Write-Host "SANSA_O3DE_BUILD|SCRIPT_COUNT|$($scripts.Count)"
Write-Host "SANSA_O3DE_BUILD|DEPLOY_TARGET|$target"
Write-Host "SANSA_O3DE_BUILD|RESULT|PASS"
