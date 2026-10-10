<#
.SYNOPSIS
    Rebuilds the acceptance asset from its commandlet.

.DESCRIPTION
    `TurboMCPAcceptance` is a BUILD ARTIFACT, not a hand-made file: it is produced by
    `UTurboMCPAcceptanceAssetCommandlet` from `Source/TurboMCP/TurboMCPAcceptanceAssetCommandlet.cpp`.
    This script runs that commandlet so the asset can be regenerated at any time, which is what makes
    it safe for a TEST to mutate it.

    WHY THAT MATTERS
    ----------------
    `P/E-GRAPH-DEAD`'s Edit REMOVES nodes and `P/E-LOCAL-VARS`'s Edit ADDS a local variable and a
    getter. Both are destructive, so a suite that runs them against the committed asset would corrupt
    the shared fixture for every other suite. Two options were considered:

      duplicate the asset, run the test on the copy, delete the copy;
      regenerate the asset.

    REGENERATION IS THE ONE CHOSEN, because it is the only one that is EXACT. A duplicate can drift
    from the original in ways nothing checks, and the value of this asset is that its structure is
    known precisely. Regenerating also means the asset on disk always matches the C++ that describes
    it, so a reader can trust either one.

    The cost is that the suites that mutate it cannot run concurrently with each other. Given they
    share one editor, that was never possible anyway.

    THE `-run=` TOKEN NEEDS A MODULE PREFIX. The engine calls `LoadModule` only when the token contains
    a period (`LaunchEngineLoop.cpp:4100`), so `-run=TurboMCP.TurboMCPAcceptanceAssetCommandlet` is
    what loads this plugin. Without the prefix the run answers "looked like a commandlet, but we could
    not find the class" having loaded nothing.

.PARAMETER EditorExe
    Path to `UnrealEditor-Cmd.exe`. Defaults to the source-engine build this project uses.

.PARAMETER Project
    Path to the test project's `.uproject`.

.EXAMPLE
    pwsh -File Tools\make-acceptance-asset.ps1
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
$log = Join-Path $repo '.agent\logs\make-acceptance-asset.log'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $log) | Out-Null

# The editor holds the asset open, so it is stopped first. A stale handle would make the save fail
# for a reason that has nothing to do with the commandlet.
#
# THE FIXED SLEEP WAS NOT ENOUGH, AND IT IS THE CAUSE OF A WHOLE CLASS OF INTERMITTENT FAILURE.
# This script runs INSIDE a suite as well as by hand: `Tools/test-graph-dead.py` calls it twice, and
# `Tools/check-probe-graphs.py` calls it too, so under the harness it always follows another editor.
# MEASURED: a full harness run reported `31 of 38` while EVERY failing suite passed in isolation, and the
# failing names were exactly the ones that REBUILD A FIXTURE - `check-probe-graphs`, `test-graph-dead`,
# `test-graph-knots`. `make-acceptance-asset.ps1` run DIRECTLY returns exit 0 and prints PASS, so the script
# was never wrong in itself; what failed is the SECOND call inside one suite, racing the editor the FIRST
# call's commandlet had just been holding.
#
# A FIXED SLEEP IS EITHER TOO SHORT ON A BUSY MACHINE OR WASTED TIME ON AN IDLE ONE - the same reason
# `start-editor.ps1` polls the server instead of sleeping. So the PROCESS is waited for, and a stubborn one
# is killed, because the commandlet's failure mode here is a locked `.uasset` that looks like a build error.
$stopScript = Join-Path $PSScriptRoot 'start-editor.ps1'
if (Test-Path $stopScript) {
    & pwsh -NoProfile -File $stopScript -Stop 2>&1 | Out-Null
}

$deadline = (Get-Date).AddSeconds(30)
while ((Get-Date) -lt $deadline) {
    if (-not (Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue)) { break }
    Start-Sleep -Seconds 1
}
$stubborn = Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue
if ($stubborn) {
    Write-Output ("  a previous editor (PID {0}) outlived the stop; killing it before the commandlet runs" -f `
        ($stubborn.Id -join ', '))
    $stubborn | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 3
}
# The launcher's own editor is a different process name and holds the same DLL, so it is cleared too.
Get-Process -Name UnrealEditor-Cmd -ErrorAction SilentlyContinue |
    Stop-Process -Force -ErrorAction SilentlyContinue

& cmd.exe /c "`"$EditorExe`" `"$Project`" -run=TurboMCP.TurboMCPAcceptanceAssetCommandlet -unattended -nullrhi -culture=en -MaxParallelActions=8 > `"$log`" 2>&1"
$exit = $LASTEXITCODE

# THE EXIT CODE IS NOT THE EVIDENCE. The commandlet has reported success while writing nothing, so the
# line it prints about the file is read back rather than trusted.
$wrote = Select-String -Path $log -Pattern 'TurboMCP: wrote' -EA SilentlyContinue | Select-Object -Last 1
$counts = Select-String -Path $log -Pattern 'TurboMCP: Display:\s+\w+: \d+ node' -EA SilentlyContinue

if ($exit -ne 0 -or -not $wrote) {
    Write-Output "FAILED (exit $exit)"
    Select-String -Path $log -Pattern 'TurboMCP:|Error' -EA SilentlyContinue |
        Select-Object -Last 12 | ForEach-Object { "  " + $_.Line.Trim() }
    exit 1
}

Write-Output "PASS  the acceptance asset was rebuilt"
Write-Output ("  " + ($wrote.Line -replace '.*wrote\s+', ''))
foreach ($line in $counts) { Write-Output ("  " + ($line.Line -replace '.*LogTurboMCP: Display:\s+', '')) }
