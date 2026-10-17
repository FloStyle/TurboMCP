<#
.SYNOPSIS
    Builds the PIE test map: a small level containing a placed actor whose code runs during Play-In-Editor.

.DESCRIPTION
    `TurboMCPTestMap` is a BUILD ARTIFACT, not a hand-made file: it is produced by
    `UTurboMCPTestMapCommandlet` from `Source/TurboMCP/TurboMCPTestMapCommandlet.cpp`. Running that
    commandlet is what makes the map reproducible and reviewable, in the same way as the acceptance asset.

    WHY THIS ONE FIXTURE IS WORTH ITS OWN SCRIPT
    -------------------------------------------
    `TestProject/Content` contained NO `.umap` AT ALL - measured, not assumed. Play-In-Editor therefore ran the
    engine's empty `/Temp/UEDPIE_0_Untitled_1` world, NOTHING IN IT EXECUTED ANY CODE, and TWO capabilities
    were blocked by that single missing thing:

      Q-17  `R-PLAY-CHECK`'s suite could not prove that its warning and error COUNT is MEASURED rather than
            merely shaped, because a forged `0` is indistinguishable from a true `0` when nothing runs to
            raise one. It was also measured that `UKismetSystemLibrary::PrintString` logs at `Log` severity
            UNCONDITIONALLY with NO parameter that can raise it, so no Blueprint node can produce a Warning -
            which is why the map places a C++ actor from the TEST project's module rather than a Blueprint.

      Q-18  the `E-TRACE` subscriber to `FBlueprintCoreDelegates::OnScriptException` registers and had NEVER
            been observed to fire, because the VM never reaches an `EX_Tracepoint` in an empty world.

    ONE FIXTURE, TWO HOLES.

    THE EXIT CODE CARRIES THE VERDICT AND IS READ AS SUCH. The commandlet writes the map and then answers 0
    for "a map with the actor in it", 4 for "a map that raises nothing", 1 for "the world or package could not
    be made", and 3 for "the save failed". Those are four different failures that would all look like a
    missing file if only the file were checked, so the code is reported rather than collapsed.

    THE `-run=` TOKEN NEEDS A MODULE PREFIX. The engine calls `LoadModule` only when the token contains a
    period (`LaunchEngineLoop.cpp:4100`), so `-run=TurboMCP.TurboMCPTestMapCommandlet` is what loads this
    plugin. Without the prefix the run answers "looked like a commandlet, but we could not find the class"
    having loaded nothing.

.PARAMETER EditorExe
    Path to `UnrealEditor-Cmd.exe`. Defaults to the source-engine build this project uses.

.PARAMETER Project
    Path to the test project's `.uproject`.

.EXAMPLE
    pwsh -File Tools\make-test-map.ps1
#>
[CmdletBinding()]
param(
    # EMPTY ON PURPOSE: $repo, $Project and $EditorExe are DERIVED below by Tools/common.ps1, so a
    # clone on any machine works without editing this file. See that file for the resolution order.
    [string]$EditorExe = '',
    [string]$Project = ''
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------------------------
# THE PATHS ARE DERIVED, NOT HARD-CODED. A clone on any machine has to work without editing this
# file, and an absolute default only ever worked on the machine it was written on.
# ---------------------------------------------------------------------------------------------
. (Join-Path $PSScriptRoot 'common.ps1')
if (-not $Project)   { $Project   = Get-TurboMCPProject }
$repo = Get-TurboMCPRepoRoot
if (-not $EditorExe) { $EditorExe = Join-Path (Resolve-TurboMCPEngine -Project $Project) 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe' }


$executionRepo = Split-Path -Parent $PSScriptRoot
if ([System.IO.Path]::GetFullPath($Project) -ne [System.IO.Path]::GetFullPath((Join-Path $executionRepo 'TestProject/TurboMCPTest.uproject'))) { throw 'Only the repository test project may be changed by this fixture command' }
$executionEngine = [System.IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $EditorExe) '..\..\..'))
& python (Join-Path $PSScriptRoot 'execution_runtime.py') --authorize $executionRepo $executionEngine
if ($LASTEXITCODE -ne 0) { throw 'Owned execution context required' }

if (-not (Test-Path $EditorExe)) { throw "the editor is not at $EditorExe" }
if (-not (Test-Path $Project))   { throw "the project is not at $Project" }

$repo = Split-Path -Parent $PSScriptRoot
$log = Join-Path $repo '.agent\logs\make-test-map.log'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $log) | Out-Null

# The editor holds the asset open, so it is stopped first. A stale handle would make the save fail for a
# reason that has nothing to do with the commandlet.
$stopScript = Join-Path $PSScriptRoot 'start-editor.ps1'
if (Test-Path $stopScript) {
    & pwsh -NoProfile -File $stopScript -Stop 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Ownership check refused editor cleanup; operation stopped' }
    Start-Sleep -Seconds 2
}

& cmd.exe /c "`"$EditorExe`" `"$Project`" -run=TurboMCP.TurboMCPTestMapCommandlet -unattended -nullrhi -culture=en -MaxParallelActions=8 > `"$log`" 2>&1"
$exit = $LASTEXITCODE

# THE EXIT CODE IS NOT THE EVIDENCE. The commandlet reports what it placed, and that line is read back rather
# than trusted, because this project has seen a commandlet report success while writing nothing.
$placed = Select-String -Path $log -Pattern 'TurboMCP: testMap placedActor=' -EA SilentlyContinue | Select-Object -Last 1
$wrote  = Select-String -Path $log -Pattern 'TurboMCP: wrote ' -EA SilentlyContinue | Select-Object -Last 1
$module = Select-String -Path $log -Pattern 'TurboMCP: test module ' -EA SilentlyContinue | Select-Object -Last 1

if ($module) { Write-Output ("  " + ($module.Line -replace '.*LogTurboMCP: Display:\s+', '')) }
if ($wrote)  { Write-Output ("  wrote " + ($wrote.Line -replace '.*TurboMCP: wrote\s+', '')) }
if ($placed) { Write-Output ("  " + ($placed.Line -replace '.*LogTurboMCP: Display:\s+', '')) }

switch ($exit) {
    0 { Write-Output "PASS  the PIE test map was built with a log actor placed in it"; exit 0 }
    4 {
        Write-Output "FAIL  the map was written but NO LOG ACTOR WAS PLACED, so PIE would raise nothing"
        Select-String -Path $log -Pattern 'TurboMCP: Error' -EA SilentlyContinue |
            Select-Object -Last 6 | ForEach-Object { "  " + ($_.Line -replace '.*LogTurboMCP: Error:\s+', '') }
        exit 1
    }
    default {
        Write-Output "FAILED (exit $exit)"
        Select-String -Path $log -Pattern 'TurboMCP:|Error' -EA SilentlyContinue |
            Select-Object -Last 12 | ForEach-Object { "  " + $_.Line.Trim() }
        exit 1
    }
}
