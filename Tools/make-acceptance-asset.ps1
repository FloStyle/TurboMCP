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

if (-not (Test-Path $EditorExe)) { throw "the editor is not at $EditorExe" }
if (-not (Test-Path $Project))   { throw "the project is not at $Project" }

$repo = Split-Path -Parent $PSScriptRoot
$log = Join-Path $repo '.agent\logs\make-acceptance-asset.log'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $log) | Out-Null

# ---------------------------------------------------------------------------------------------
# ONE REBUILD AT A TIME, ACROSS PROCESSES, AND THIS IS THE MISSING PIECE.
#
# MEASURED: a full run reported `31 of 39` and the failures were EXACTLY the suites that rebuild this asset -
# `test-graph-dead`, `test-graph-knots`, `check-probe-graphs` - INCLUDING two new failures whose text is about
# the SECOND rebuild ("regenerated at the end", "rebuilt at the end"). Meanwhile calling this script TWICE IN A
# ROW by hand returns exit 0 and prints PASS BOTH TIMES, which is what proves the fault is in the INTERACTION
# rather than in the script.
#
# `Tools/run-suites.ps1` runs the suites ONE AT A TIME, but a suite that rebuilds this asset STOPS THE EDITOR
# while doing it - so the harness's own editor lifecycle and the script's are both driving the same editor and
# the same `.uasset` at once. Every one of these suites had been passing alone, which is the signature this
# project has now recorded four times.
#
# A NAMED MUTEX MAKES THE REBUILD MUTUALLY EXCLUSIVE ACROSS PROCESSES, which is the only kind of lock that helps
# here: the competitors are separate `pwsh` invocations launched by Python, not threads in one runspace. The
# name is fixed rather than derived, because the resource is fixed - there is exactly one acceptance asset.
# ---------------------------------------------------------------------------------------------
$mutexName = 'Global\TurboMCP.AcceptanceAsset'
$owned = $false
$buildMutex = New-Object System.Threading.Mutex($false, $mutexName)
try {
    # A generous timeout: a rebuild runs a commandlet, and waiting is always better than two commandlets racing
    # for one `.uasset`. If the wait expires the script FAILS LOUDLY rather than proceeding unlocked, because
    # proceeding unlocked is the defect this exists to prevent.
    try {
        $owned = $buildMutex.WaitOne([TimeSpan]::FromSeconds(300))
    } catch [System.Threading.AbandonedMutexException] {
        # A previous holder died without releasing. Windows hands ownership to us, which is exactly right.
        $owned = $true
    }
    if (-not $owned) {
        Write-Output "FAILED (another rebuild held the acceptance-asset lock for 300s)"
        exit 1
    }

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
    # NOT `exit 1` HERE: AN `exit` INSIDE A `try` DOES NOT RUN THE `finally`, SO THE MUTEX WOULD STAY HELD AND
    # EVERY LATER REBUILD WOULD WAIT ITS FULL TIMEOUT BEFORE FAILING TOO - turning one failure into what looks
    # like a hang. The code is recorded and the script leaves through the `finally` below.
    $script:FailCode = 1
} else {
    Write-Output "PASS  the acceptance asset was rebuilt"
    Write-Output ("  " + ($wrote.Line -replace '.*wrote\s+', ''))
    foreach ($line in $counts) { Write-Output ("  " + ($line.Line -replace '.*LogTurboMCP: Display:\s+', '')) }
}

} finally {
    # RELEASED IN A `finally` SO A FAILED REBUILD STILL LETS THE NEXT ONE RUN.
    if ($owned) {
        try { $buildMutex.ReleaseMutex() } catch { }
    }
    $buildMutex.Dispose()
}

if ($script:FailCode -ne 0) { exit $script:FailCode }
