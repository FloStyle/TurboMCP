<#
.SYNOPSIS
    Rebuilds the material fixture that the four material readers need.

.DESCRIPTION
    `TurboMCPMaterialFixture` is a BUILD ARTIFACT, produced by
    `UTurboMCPMaterialFixtureCommandlet` from `Source/TurboMCP/TurboMCPMaterialFixtureCommandlet.cpp`.

    WHY IT EXISTS
    -------------
    `R-MAT-LABELS`, `R-MAT-LOAD`, `R-MAT-TRACE` and `R-MAT-BLOCKS` compile and answer, but the test
    project held NO MATERIAL, so the only thing a suite could prove about them is that they REFUSE a
    non-material asset. `Tools/test-batch-tools.py` records that limitation in its own docstring rather
    than pretending the refusal is a test of the reading.

    WHY A COMMANDLET RATHER THAN A HAND-MADE ASSET
    ----------------------------------------------
    The structure is legible in the repository: a reader can see exactly which expressions were made
    and where they were placed. That matters more here than for the Blueprint fixture, because
    `R-MAT-BLOCKS` decides box membership GEOMETRICALLY - by comparing each expression's editor
    position against each box rectangle and giving it to the SMALLEST box containing it. A fixture
    whose nodes all sat at (0, 0) would put every node in every box, and the nesting check would pass
    while testing nothing.

    THE `-run=` TOKEN NEEDS A MODULE PREFIX. The engine calls `LoadModule` only when the token contains
    a period (`LaunchEngineLoop.cpp:4100`), so `-run=TurboMCP.TurboMCPMaterialFixtureCommandlet` is what
    loads this plugin. Without the prefix the run answers "looked like a commandlet, but we could not
    find the class" having loaded nothing.

.PARAMETER EditorExe
    Path to `UnrealEditor-Cmd.exe`. Defaults to the source-engine build this project uses.

.PARAMETER Project
    Path to the test project's `.uproject`.

.EXAMPLE
    pwsh -File Tools\make-material-asset.ps1
#>
[CmdletBinding()]
param(
    [string]$EditorExe = 'C:\ExampleUnrealengine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [string]$Project = '<REPO>\TestProject\TurboMCPTest.uproject'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $EditorExe)) { throw "the editor is not at $EditorExe" }
if (-not (Test-Path $Project))   { throw "the project is not at $Project" }

$repo = Split-Path -Parent $PSScriptRoot
$log = Join-Path $repo '.agent\logs\make-material-asset.log'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $log) | Out-Null

# The editor holds the asset open, so it is stopped first. A stale handle would make the save fail for
# a reason that has nothing to do with the commandlet.
$stopScript = Join-Path $PSScriptRoot 'start-editor.ps1'
if (Test-Path $stopScript) {
    & pwsh -NoProfile -File $stopScript -Stop 2>&1 | Out-Null
    Start-Sleep -Seconds 2
}

& cmd.exe /c "`"$EditorExe`" `"$Project`" -run=TurboMCP.TurboMCPMaterialFixtureCommandlet -unattended -nullrhi -culture=en -MaxParallelActions=8 > `"$log`" 2>&1"
$exit = $LASTEXITCODE

# THE EXIT CODE IS NOT THE EVIDENCE, and neither is the commandlet's own summary. The line it prints
# about the FILE is read back, because a commandlet has reported success while writing nothing and the
# difference matters to every suite that then reads the asset.
$wrote = Select-String -Path $log -Pattern 'TurboMCP: wrote' -EA SilentlyContinue | Select-Object -Last 1
$structure = Select-String -Path $log -Pattern 'TurboMCP: (material|instance|function|labels|boxes|usage)' -EA SilentlyContinue

if ($exit -ne 0 -or -not $wrote) {
    Write-Output "FAILED (exit $exit)"
    Select-String -Path $log -Pattern 'TurboMCP:|Error' -EA SilentlyContinue |
        Select-Object -Last 14 | ForEach-Object { "  " + $_.Line.Trim() }
    exit 1
}

Write-Output "PASS  the material fixture was rebuilt"
foreach ($line in $structure) {
    Write-Output ("  " + ($line.Line -replace '.*LogTurboMCP: Display:\s*', ''))
}
