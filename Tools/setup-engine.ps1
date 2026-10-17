<#
.SYNOPSIS
Validate an existing pinned engine and optionally build the test project.
.DESCRIPTION
Engine installation is separate from plugin setup. No moving release branch,
credential reader, engine-wide junction, or automatic engine source edit is used.
#>
[CmdletBinding()]
param([string]$EngineDir='', [switch]$Build)
$ErrorActionPreference='Stop'
. "$PSScriptRoot/common.ps1"
$repo = Get-TurboMCPRepoRoot
if (-not $EngineDir) { $EngineDir = Resolve-TurboMCPEngine }
$previousEngine = $env:TURBOMCP_ENGINE
try {
    $env:TURBOMCP_ENGINE=$EngineDir
    & python "$PSScriptRoot/doctor.py"
    if ($LASTEXITCODE -ne 0) { throw 'Engine or project validation failed' }
    if ($Build) {
        & "$PSScriptRoot/build.ps1" -Engine $EngineDir
        if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    }
} finally { $env:TURBOMCP_ENGINE=$previousEngine }
