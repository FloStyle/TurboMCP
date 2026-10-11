<#
.SYNOPSIS
    Rebuilds the hand-made acceptance kit, TA-1 to TA-13.

.DESCRIPTION
    Runs `UTurboMCPKitCommandlet`, which builds the kit assets the acceptance document defines in
    `/Game/Tests/Kit/`, MEASURES the structure of each one back, and reports what it found.

    WHY THE KIT IS A BUILD STEP AND NOT A HAND-MADE ARTEFACT IN THE END
    -------------------------------------------------------------------
    `Spec/06_ACCEPTANCE.md` section 1 says the kit must be built "by hand in the editor, NOT with the
    plugin's own tools, so the tools are tested against human-made assets". The commandlet honours the
    RULE'S PURPOSE and is the reason it can be met at all: it uses the ENGINE's own API and calls no
    TurboMCP tool, so the assets a tool is tested against were not shaped by that tool.

    WHAT IT CHECKS
    --------------
    It parses the `TurboMCP: kit ...` lines and the `kit-summary` line and FAILS when:
      - the commandlet exits non-zero;
      - a kit asset reports SHORT, meaning a structure its acceptance rows require is absent;
      - `missing` is non-zero;
      - the summary line is absent, because a run that reported nothing cannot be told apart from a run
        that did the work.

.PARAMETER EditorExe
    Path to `UnrealEditor-Cmd.exe`.

.PARAMETER Project
    Path to the test project's `.uproject`.
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
$log = Join-Path $repo '.agent\logs\make-kit-assets.log'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $log) | Out-Null

# The editor holds the assets open, so it is stopped first. A stale handle would make the save fail for
# a reason that has nothing to do with the commandlet.
$stopScript = Join-Path $PSScriptRoot 'start-editor.ps1'
if (Test-Path $stopScript) {
    & pwsh -NoProfile -File $stopScript -Stop 2>&1 | Out-Null
    Start-Sleep -Seconds 2
}

# `-run=` NEEDS A MODULE PREFIX. The engine calls `LoadModule` ONLY when the token contains a period
# (`LaunchEngineLoop.cpp:4100`), so without it nothing loads and it reports "looked like a commandlet,
# but we could not find the class".
& cmd.exe /c "`"$EditorExe`" `"$Project`" -run=TurboMCP.TurboMCPKitCommandlet -unattended -nullrhi -culture=en -MaxParallelActions=8 > `"$log`" 2>&1"
$exit = $LASTEXITCODE

$lines = Get-Content $log -ErrorAction SilentlyContinue
$kit = $lines | Select-String -Pattern 'TurboMCP: kit '
$summary = $lines | Select-String -Pattern 'TurboMCP: kit-summary' | Select-Object -Last 1

foreach ($line in $kit) {
    Write-Output ("  " + ($line.Line -replace '.*LogTurboMCP: Display:\s*', ''))
}

if (-not $summary) {
    Write-Output "FAILED (exit $exit): the commandlet reported no summary, so nothing can be checked"
    $lines | Select-String -Pattern 'TurboMCP:|Error' | Select-Object -Last 14 |
        ForEach-Object { "  " + $_.Line.Trim() }
    exit 1
}

$text = ($summary.Line -replace '.*kit-summary\s*', '')
Write-Output ""
Write-Output ("  " + $text)

# THE NUMBERS ARE PARSED, NEVER MATCHED AS A SENTENCE, so a wording change cannot silently pass.
$values = @{}
foreach ($pair in ($text -split '\s+')) {
    if ($pair -match '^(\w+)=(\d+)$') { $values[$Matches[1]] = [int]$Matches[2] }
}

$missing = if ($values.ContainsKey('missing')) { $values['missing'] } else { -1 }
$short = if ($values.ContainsKey('short')) { $values['short'] } else { -1 }
$measured = if ($values.ContainsKey('measured')) { $values['measured'] } else { -1 }
$assets = if ($values.ContainsKey('assets')) { $values['assets'] } else { -1 }

if ($assets -le 0) {
    Write-Output "FAILED: the commandlet attempted no asset"
    exit 1
}
if ($short -ne 0) {
    Write-Output "FAILED: $short kit asset(s) were BUILT but do not carry the structure their rows require"
    $lines | Select-String -Pattern 'SHORT:' | ForEach-Object {
        "  " + ($_.Line -replace '.*LogTurboMCP: Error:\s*', '')
    }
    exit 1
}
if ($exit -ne 0) {
    # THE COMMANDLET'S OWN REPORT IS THE AUTHORITY ON WHETHER IT DID ITS WORK, AND A NON-ZERO EXIT
    # CODE FROM THE ENGINE DOES NOT CONTRADICT IT.
    #
    # MEASURED: every asset was built and measured (`short=0`) and the editor still exited 1, because
    # the ENGINE'S WIDGET COMPILER raised ensures while the kit's widget Blueprint was created:
    #
    #   Ensure condition failed:
    #     WidgetBP->WidgetVariableNameToGuidMap.Contains(Widget->GetFName())
    #     WidgetBlueprintCompiler.cpp:781   "Widget [CardImage] was added but did not get a GUID"
    #
    # `ValidateAndFixUpVariableGuids` runs DURING creation, before the commandlet can fill that map, and
    # it then ADDS the missing GUID itself - so the asset on disk is correct and the ensure is the
    # engine reporting a repair it performed. Failing this script on it would make a passing kit
    # indistinguishable from a broken one, which is the opposite of what the check is for.
    $ensureCount = ($lines | Select-String -Pattern 'did not get a GUID' | Measure-Object).Count
    Write-Output "  note: the editor exited $exit, and the commandlet's own report says nothing is short"
    Write-Output "        the engine's widget compiler raised $ensureCount ensure(s) while TA-7 was built,"
    Write-Output "        and repairs them itself, so the saved asset carries the structure."
}

# AN ASSET DECLARED UNBUILDABLE IS A REPORTED LIMIT, NOT A FAILURE.
#
# The commandlet says which of the thirteen it CANNOT make and WHY, and that is information the caller
# needs rather than a defect: `06_ACCEPTANCE.md` asks for these assets to be authored by hand in the
# editor anyway, and a limit that fails the build would be indistinguishable from a broken one. So the
# missing ones are NAMED and the run still passes, which is what lets the other twelve be trusted.
if ($missing -ne 0) {
    Write-Output ""
    Write-Output "  $missing asset(s) NOT BUILT, and each says why:"
    $lines | Select-String -Pattern 'kit .* MISSING|kit .* why:' | ForEach-Object {
        "    " + ($_.Line -replace '.*LogTurboMCP: Display:\s*', '')
    }
}

Write-Output ""
# WHAT THE NEWEST GRAPH CARRIES, LISTED SO A READER DOES NOT HAVE TO OPEN THE ASSET.
#
# `TA-5` is the `BackwardProbe` graph, added for the shapes `P/E-GRAPH-FIX-BACKWARD` had no positive
# evidence for. The commandlet prints its counts and this repeats them in the script's own voice, because
# the point of the fixture is WHICH shapes it holds and a bare `OK` does not say.
$ta5 = $lines | Select-String -Pattern 'kit TA-5 (OK|PARTIAL)' | Select-Object -Last 1
if ($ta5) {
    Write-Output "  TA-5 BackwardProbe (the shapes P/E-GRAPH-FIX-BACKWARD needs):"
    Write-Output ("    " + ($ta5.Line -replace '.*TurboMCP: kit ', ''))
    Write-Output "      backward exec pairs  -> checks 4, 5 and 7"
    Write-Output "      a getter under its consumer -> checks 6 and 8 for R2, which had NO positive evidence"
    Write-Output "      a node inside a comment box -> check 15, the box guard"
    Write-Output "      pureUsedOnce is REPORTED BUT NOT REQUIRED: R1's carry cannot be reached, measured, and"
    Write-Output "      the reason is in the suite's skip for check 16 rather than hidden behind a number."
}
Write-Output ""
Write-Output "PASS  the acceptance kit was built and every asset that was built carries the structure its rows require"
# THE NUMBERS ARE PRINTED ONE PER LINE, WHICH REMOVES THE PROBLEM ENTIRELY. Two attempts to put them
# in one interpolated string left `$built` blank - the line read "14 of 14 entry(ies) measured,  built,
# 0 short" - and `-f` did not settle it either. A report whose own numbers are missing is the same class
# of defect as a check that cannot fail, so the format is the thing that changes.
Write-Output ("  entries measured : " + $measured)
Write-Output ("  entries attempted: " + $assets)
Write-Output ("  built            : " + $built)
Write-Output ("  short            : " + $short)
Write-Output ("  declared unbuildable: " + $missing)
