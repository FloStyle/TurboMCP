<#
.SYNOPSIS
    Runs every acceptance suite in sequence, keeping the editor alive per suite.

.DESCRIPTION
    WHY THIS EXISTS. Verification has been the binding constraint on this project for several rounds, not
    code. Four separate workstreams reported being unable to run their tests, and the cause is structural:

      - an UNREAL EDITOR LOADS THE PLUGIN DLL, so a build that replaces it kills the editor mid-test;
      - a `-run=` COMMANDLET EXITS THE EDITOR when it finishes, so a fixture rebuild and a suite cannot
        overlap;
      - several suites THEMSELVES stop the editor to rebuild a fixture, and must then start it again.

    Run naively, `foreach ($suite) { python $suite }` fails most suites with "the editor could not be
    started" or "the MCP server never answered" - NOT because of a defect in them, and that distinction
    matters, because a failure that is really an environment problem reads as a failing capability.

    So this script OWNS THE EDITOR for its whole run. For each suite it:
      1. ensures no editor is running, so a stale DLL handle cannot make a save fail;
      2. starts the editor and WAITS FOR THE MCP SERVER TO ANSWER, rather than assuming a fixed delay;
      3. runs the one suite, capturing its RESULT line;
      4. stops the editor again, so the next suite starts from a clean process.

    A SUITE THAT KILLS THE EDITOR (a fixture rebuild) is handled: the script detects that the server has
    gone and restarts it before the next suite.

    IT DOES NOT BUILD. A build takes a mutex and replaces the DLL; doing it here would reintroduce exactly
    the collision this script exists to remove. Run the build FIRST, then this.

.PARAMETER Suites
    Suite names without the `.py`, or `all` for every `test-*.py` in Tools/ that is not excluded.

.PARAMETER SkipSlow
    Skip `test-capture`, which renders a frame and is slow on a cold shader cache.

.EXAMPLE
    pwsh -File Tools\run-suites.ps1
    pwsh -File Tools\run-suites.ps1 -Suites test-graph-knots,test-graph-tidy
#>
[CmdletBinding()]
param(
    [string[]] $Suites = @('all'),
    [switch] $SkipSlow
)

$ErrorActionPreference = 'Stop'
$Repo = Split-Path -Parent $PSScriptRoot
$StartEditor = Join-Path $PSScriptRoot 'start-editor.ps1'

# Suites that need no editor: they are static readers and running them through a restart would waste a
# minute each for no benefit.
$StaticSuites = @('audit-tool-coverage', 'audit-suite-quality', 'check-includes')
# check-mcp-server NEEDS the editor: it asserts the MCP server answers. Listing it as static made the
# runner report a FAILING suite on a machine where no editor was up, which is an environment fact rather
# than a defect - the same misreading as above, one level up.

# A suite that is slow and whose value does not change per run.
$SlowSuites = @('test-capture')

function Start-EditorAndWait {
    param([int] $TimeoutSeconds = 240)

    # A PROBE THAT CAN PASS ON A DYING PROCESS MEASURES THE WRONG THING, SO THE WAIT RUNS IN BOTH
    # DIRECTIONS: the previous server must be GONE, and then the new one must ANSWER.
    #
    # AND THE PROCESS MUST BE GONE TOO, WHICH IS A SEPARATE CONDITION AND WAS THE MISSING ONE.
    # `start-editor.ps1` REFUSES to start when it believes an editor is already running - correctly, because
    # two editors fight over the same `.uasset` - and it decides that from the PROCESS. But the server can
    # stop answering seconds before the process finally exits, so the down-probe passed, the launcher was
    # called while the old editor was still shutting down, the launcher REFUSED, and the up-probe then waited
    # the full timeout for a server nothing was ever going to start. That is `BLOCKED (the editor would not
    # start)` on a machine where the editor was fine - and it is why the failing suite names CHANGED between
    # runs, which is the signature of a race rather than of a tool.
    #
    # MEASURED CONSEQUENCE BEFORE THIS FIX: `passed 32, failed 4, blocked 1, of 37`, then `passed 31, failed
    # 6`, with `test-graph-check`, `test-layout`, `test-panel-reach`, `test-vars-add` and others each PASSING
    # when run alone or in a small group.
    $downDeadline = (Get-Date).AddSeconds(60)
    while ((Get-Date) -lt $downDeadline) {
        $probe = & python (Join-Path $PSScriptRoot 'check-mcp-server.py') 2>&1
        if ($probe -notmatch '^PASS') { break }
        Start-Sleep -Seconds 2
    }

    # THE PROCESS IS WAITED FOR SEPARATELY, because "the server stopped answering" and "the process exited"
    # are not the same event and only the second one lets the launcher start a new editor.
    $procDeadline = (Get-Date).AddSeconds(45)
    while ((Get-Date) -lt $procDeadline) {
        $alive = Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue
        if (-not $alive) { break }
        Start-Sleep -Seconds 1
    }
    # AND IF IT IS STILL THERE AFTER THAT, IT IS KILLED RATHER THAN WAITED FOR INDEFINITELY. A stubborn
    # editor holds the plugin DLL and the `.uasset` handles, so leaving it would make the next suite fail for
    # a reason that has nothing to do with the suite.
    $stubborn = Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue
    if ($stubborn) {
        Write-Output ("    a previous editor (PID {0}) outlived the stop; killing it before starting a new one" -f `
            ($stubborn.Id -join ', '))
        $stubborn | Stop-Process -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 3
    }

    # THE GHOST PROCESSES ARE CLEARED TOO. MEASURED: an `UnrealEditor-Cmd` left by an experiment keeps
    # `UnrealEditor-TurboMCP.dll` mapped and makes the NEXT BUILD fail with `LNK1104`, and `-Stop` does not
    # know about it because it is not the editor the launcher started.
    Get-Process -Name UnrealEditor-Cmd, UnrealTraceServer -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue

    # THE LAUNCH TIME IS RECORDED SO THE POLL CAN PROVE A *NEW* EDITOR ANSWERED.
    #
    # MEASURED, AND THIS IS THE FOURTH INSTANCE OF ONE SHAPE. A full run reported `31 of 39` while the failing
    # suites were EXACTLY the ones that rebuild a fixture, and EVERY ONE passed in isolation;
    # `make-acceptance-asset.ps1` run directly returns exit 0. There are now TWENTY invocations of fixture
    # scripts across the suites, each of which stops the editor and runs a commandlet, so the harness restarts
    # an editor far more often than before - and the probe below only asked whether SOMETHING answered on port
    # 8000. A server from an editor that is still SHUTTING DOWN answers for a few seconds, so the probe
    # returned PASS, the suite connected to a dying editor, and it died with NO RESULT LINE. The names changed
    # between runs and each passed alone, which is the signature this project has now recorded four times.
    #
    # A PROBE THAT CAN PASS ON A PROCESS THAT IS LEAVING MEASURES THE WRONG THING. So the poll requires BOTH:
    # an `UnrealEditor` THAT STARTED AFTER THIS LAUNCH, and a server that answers.
    $launchTime = Get-Date
    & pwsh -NoProfile -File $StartEditor 2>&1 | Out-Null

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        $fresh = Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue |
            Where-Object { $_.StartTime -ge $launchTime.AddSeconds(-2) }
        if ($fresh) {
            $probe = & python (Join-Path $PSScriptRoot 'check-mcp-server.py') 2>&1
            if ($probe -match '^PASS') { return $true }
        }
        Start-Sleep -Seconds 3
    }
    return $false
}

function Stop-Editor {
    & pwsh -NoProfile -File $StartEditor -Stop 2>&1 | Out-Null
    # A killed process can hold the DLL for a moment; the wait is for the handle, not for tidiness.
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        if (-not (Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue)) { return }
        Start-Sleep -Seconds 2
    }
}

# ---------------------------------------------------------------------------------------------
# The suite list
# ---------------------------------------------------------------------------------------------
$All = Get-ChildItem $PSScriptRoot -Filter '*.py' |
    Where-Object { $_.BaseName -match '^(test|check|audit)-' } |
    ForEach-Object { $_.BaseName } | Sort-Object

$Static = $All | Where-Object { $StaticSuites -contains $_ }
$NeedsEditor = $All | Where-Object { $StaticSuites -notcontains $_ }
if ($SkipSlow) { $NeedsEditor = $NeedsEditor | Where-Object { $SlowSuites -notcontains $_ } }

# `-contains` RATHER THAN `-in`, AND THAT IS A MEASURED FIX. Called as `-Suites 'a,b,c'` PowerShell binds the
# WHOLE STRING as one element, so `$_ -in $Suites` compared a bare name against a single long string and
# matched nothing - the runner then reported "3 static, 0 needing an editor" and ran no capability suite at
# all WHILE PRINTING A SUMMARY THAT LOOKED FINE. A filter that silently selects nothing is the same failure
# as a check that cannot fail, one level up.
if ($Suites -notcontains 'all' -and $Suites.Count -gt 0) {
    # A COMMA-SEPARATED STRING IS SPLIT HERE, so both call styles work and neither silently selects nothing.
    $Wanted = @()
    foreach ($entry in $Suites) { $Wanted += ($entry -split ',') }
    $Wanted = $Wanted | Where-Object { $_.Trim() } | ForEach-Object { $_.Trim() }
    $NeedsEditor = $NeedsEditor | Where-Object { $Wanted -contains $_ }
    if ($NeedsEditor.Count -eq 0) {
        Write-Output "WARNING: -Suites matched no suite that needs an editor. Requested: $($Wanted -join ', ')"
    }
}

# ---------------------------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------------------------
Write-Output "=== TurboMCP suite runner: $($Static.Count) static, $($NeedsEditor.Count) needing an editor ==="
Write-Output ""

$Results = [ordered]@{}

Write-Output "--- static suites, which need no editor ---"
foreach ($name in $Static) {
    $out = & python (Join-Path $PSScriptRoot "$name.py") 2>&1
    # TWO SUITES REPORT THEIR VERDICT WITHOUT A `RESULT:` LINE - `check-includes` and `check-mcp-server`
    # print `PASS <what>` or `FAIL <what>`. The first version of this runner looked only for `^RESULT`,
    # found nothing, and reported them as FAILING when they had both passed. A runner that MISREADS a
    # passing suite as a failing one is worse than no runner: it sends someone to fix working code.
    $result = ($out | Select-String -Pattern '^RESULT|^PASS|^FAIL' | Select-Object -Last 1).Line
    if (-not $result) { $result = 'RESULT: no result line' }
    $Results[$name] = $result
    Write-Output ("  {0,-26} {1}" -f $name, $result.Trim())
}

Write-Output ""
Write-Output "--- suites that need an editor, run ONE AT A TIME with the editor restarted between ---"

foreach ($name in $NeedsEditor) {
    Write-Output ""
    Write-Output "--- $name ---"
    Stop-Editor
    if (-not (Start-EditorAndWait)) {
        # RECORDED AS AN ENVIRONMENT FAILURE RATHER THAN A CAPABILITY ONE. This distinction is the whole
        # reason the script exists: "the editor could not be started" is not a statement about the tool.
        $Results[$name] = 'RESULT: BLOCKED (the editor would not start)'
        Write-Output "  BLOCKED: the editor would not start, so this suite was NOT run"
        continue
    }
    $out = & python (Join-Path $PSScriptRoot "$name.py") 2>&1
    $result = ($out | Select-String -Pattern '^RESULT|^PASS|^FAIL' | Select-Object -Last 1).Line
    if (-not $result) {
        # A suite that dies without a result line is worth seeing, so the tail is printed.
        $Results[$name] = 'RESULT: no result line'
        Write-Output "  no RESULT line; last lines:"
        $out | Select-Object -Last 4 | ForEach-Object { "    " + $_ }
    } else {
        $Results[$name] = $result
        Write-Output ("  " + $result.Trim())
    }
}

Stop-Editor

# ---------------------------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------------------------
Write-Output ""
Write-Output "=== SUMMARY ==="
$passed = 0; $failed = 0; $blocked = 0
foreach ($name in $Results.Keys) {
    $r = $Results[$name]
    if ($r -match 'RESULT: PASS|^PASS ') { $passed++ }
    elseif ($r -match 'BLOCKED') { $blocked++ }
    else { $failed++ }
}
foreach ($name in $Results.Keys) {
    $r = $Results[$name]
    $mark = if ($r -match 'RESULT: PASS|^PASS ') { 'PASS   ' } elseif ($r -match 'BLOCKED') { 'BLOCKED' } else { 'FAIL   ' }
    Write-Output ("  {0} {1,-28} {2}" -f $mark, $name, $r.Trim())
}
Write-Output ""
Write-Output ("  passed {0}, failed {1}, blocked {2}, of {3}" -f $passed, $failed, $blocked, $Results.Count)
Write-Output ""
Write-Output "BLOCKED MEANS THE ENVIRONMENT, NOT THE TOOL: the editor would not start, so the suite was not"
Write-Output "run at all. A blocked suite is NOT a passing one, and this summary never counts it as one."
if ($failed -gt 0) { exit 1 }
exit 0
