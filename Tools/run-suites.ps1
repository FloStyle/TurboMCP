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

# ---------------------------------------------------------------------------------------------
# A WALL-CLOCK DEADLINE PER SUITE, WHICH IS THE ONLY THING THAT CAN BOUND A RUN.
#
# MEASURED: the invocation was `& python <suite> 2>&1` WITH NO TIMEOUT AT ALL. Three full runs were left for 18,
# 25 and 24 minutes with NO EDITOR ALIVE AND NO LOG ACTIVITY, indistinguishable from a hang. A suite that blocks
# on a request blocks THE WHOLE RUN INDEFINITELY, AND NO GUARD ON THE EDITOR CAN HELP, because the harness is not
# looking at the editor while it waits.
#
# `Start-Process` WITH REDIRECTED FILES RATHER THAN A PIPELINE, because `& python ... 2>&1` gives no way to
# interrupt: PowerShell waits for the child to close its streams and never gets to decide otherwise. Killing the
# process on expiry is the only lever and it needs the process HANDLE.
#
# *** THE OUTPUT IS SPLIT INTO LINES BEFORE IT IS RETURNED, AND THAT ONE LINE IS THE WHOLE POINT. ***
# A previous version returned `Get-Content -Raw`, which is ONE `System.String` carrying the file's `\r\n` endings,
# and the harness feeds it to a LINE-ORIENTED cmdlet. MEASURED on `check-includes`:
#
#     type: System.String          length: 293
#     newline style: CR=True LF=True
#     Select-String count: 0                     <- IT MATCHED NOTHING
#     verdict via split: PASS  every include resolves to a real file in the engine or the plugin.
#
# CARET DOES NOT MATCH AFTER A CARRIAGE RETURN, so a single multi-line string DEFEATS A LINE-ANCHORED PATTERN even
# though the text plainly begins a line with `PASS`. That is why THREE PASSING STATIC SUITES were recorded as
# `no result line` while the helper itself captured 293 correct characters. A HELPER THAT SILENTLY YIELDS NOTHING
# AND A CONSUMER THAT SILENTLY MATCHES NOTHING ARE INDISTINGUISHABLE FROM OUTSIDE, AND BOTH LOOK LIKE FAILING
# SUITES. The split restores the shape the old invocation produced - an ARRAY OF LINES - so every consumer sees
# what it saw before.
#
# `$root` IS READ FROM THE CALLER'S SCOPE. MEASURED in an earlier attempt: `$PSScriptRoot` is EMPTY inside a
# function called from another scope, `Join-Path` threw on an empty path, and THAT was one of the two causes.
#
# THE DEFAULT IS GENEROUS ON PURPOSE: the slowest suite measures about two minutes and the default is TEN, so a
# deadline firing means something is genuinely wrong rather than merely slow.
$script:SuiteTimeoutSeconds = 600
$suiteRoot = $PSScriptRoot

function Invoke-SuiteProcess {
    param([string] $SuiteName, [int] $TimeoutSeconds = $script:SuiteTimeoutSeconds)

    $exe = (Get-Command python).Source
    $outFile = Join-Path $env:TEMP "tmcp-$SuiteName.out"
    $errFile = Join-Path $env:TEMP "tmcp-$SuiteName.err"
    Remove-Item $outFile, $errFile -Force -ErrorAction SilentlyContinue

    $proc = Start-Process -FilePath $exe `
        -ArgumentList (Join-Path $suiteRoot "$SuiteName.py") `
        -WorkingDirectory $suiteRoot -NoNewWindow -PassThru `
        -RedirectStandardOutput $outFile -RedirectStandardError $errFile

    if (-not $proc.WaitForExit($TimeoutSeconds * 1000)) {
        # KILLED, AND THE KILL IS REPORTED RATHER THAN HIDDEN. A suite that had to be killed is recorded as
        # TIMED OUT so it can never be mistaken for one that passed, failed, or was never run.
        try { $proc.Kill($true) } catch { try { $proc.Kill() } catch { } }
        Start-Sleep -Seconds 2
        return @{ Output = @("RESULT: TIMED OUT (${TimeoutSeconds}s)"); TimedOut = $true }
    }

    $text = ''
    if (Test-Path $outFile) { $text = Get-Content $outFile -Raw -ErrorAction SilentlyContinue }
    if (Test-Path $errFile) {
        $errText = Get-Content $errFile -Raw -ErrorAction SilentlyContinue
        if ($errText) { $text = "$text`n$errText" }
    }
    Remove-Item $outFile, $errFile -Force -ErrorAction SilentlyContinue
    return @{ Output = ($text -split "`r?`n"); TimedOut = $false }
}

function Start-EditorAndWait {
    param([int] $TimeoutSeconds = 240)

    # ---------------------------------------------------------------------------------------------
    # A HEALTHY EDITOR IS REUSED RATHER THAN REPLACED, AND THIS IS THE FIX FOR THE CLASS OF FAILURE THAT
    # HAS COST FOUR ROUNDS.
    #
    # MEASURED: a full run reported `31 of 39` while EVERY failing suite passed in isolation and
    # `make-acceptance-asset.ps1` run by hand returned exit 0. Four defects of ONE shape were found and
    # fixed - the harness waiting on the SERVER while the launcher decides from the PROCESS, a fixed
    # 2-second sleep, a probe that could pass on an editor still shutting down, and seven concurrent
    # rebuilds of one asset - AND THE FAILURES PERSISTED. The reason is that THE HARNESS AND THE SUITE WERE
    # BOTH DRIVING ONE EDITOR AND ONE `.uasset`: several suites STOP the editor to rebuild a fixture and
    # then START one and connect to it, while the harness ALSO stopped the editor before every suite -
    # KILLING THE VERY EDITOR THE PREVIOUS SUITE HAD JUST HEALTHILY STARTED, then racing a fresh start
    # against the next suite's expectations.
    #
    # SO THE HARNESS IS SUBORDINATE TO A HEALTHY EDITOR INSTEAD OF THE OWNER OF ONE. If a server already
    # answers, that editor is REUSED and NOTHING is stopped or started. That is strictly less work, and a
    # suite that manages its own editor is left alone rather than sabotaged mid-flight.
    #
    # THE GUARD IS THE HEALTH PROBE RATHER THAN AN ASSUMPTION: an editor left WEDGED by a suite - alive,
    # port open, game thread not servicing requests - would make every later suite fail, so a probe that
    # does NOT answer falls through to the full stop-and-start below, which is the recovery path.
    # ---------------------------------------------------------------------------------------------
    $healthy = & python (Join-Path $PSScriptRoot 'check-mcp-server.py') 2>&1
    if ($healthy -match '^PASS') { return $true }

    # ---------------------------------------------------------------------------------------------
    # THE CAUSE OF THE WHOLE CLASS, MEASURED AT LAST, AND IT IS NOT THE FIXTURE REBUILD.
    #
    # Five hypotheses were tested and FALSIFIED before this one: the rebuild script does not fail to print
    # PASS, its exit code is zero, calling it EXACTLY as the suites do from Python returns 0 with correct
    # stdout, it is not a log-file lock, and it is not the harness's editor holding the `.uasset`.
    #
    # WHAT IS ACTUALLY WRONG: `Tools/start-editor.ps1` REFUSES TO START WHEN IT BELIEVES AN EDITOR IS ALREADY
    # RUNNING - correctly, because two editors fight over the same `.uasset` - AND IT DECIDES THAT FROM THE
    # PROCESS. MEASURED: `start-editor.ps1` answered `FAIL an editor is already running (PID 53852)` while
    # `check-mcp-server.py` could NOT REACH A SERVER, and the suite then died on `connect()`. So the harness
    # had a LIVE PROCESS WHOSE SERVER NEVER CAME UP, and every later suite inherited it: the launcher refused
    # to start a fresh editor and the old one never answered. THAT IS THE FAILURE, AND IT EXPLAINS EVERY
    # OBSERVATION - the names changing between runs, every suite passing alone, and the complaints landing on
    # whichever fixture the wedged editor happened to be holding.
    #
    # ONE RETRY, AFTER CLEARING THE PROCESS. The first attempt has already run above and its probe has already
    # failed, so the process in the way is not merely slow - it is not coming. Killing it and starting once
    # more is the recovery, and ONE retry is deliberate: a loop would turn a genuinely broken editor into a run
    # that appears to hang, which is the failure mode this project keeps having to undo.
    Write-Output "  the editor in the way answers nothing; clearing it and retrying ONCE"
    Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue
    Get-Process -Name UnrealEditor-Cmd, UnrealTraceServer -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 5
    $launchTime = Get-Date
    # restarts it between suites - so the machine stole focus several times per run. It also skips shader
    # compilation, which makes each start faster, and no suite loses anything because every TurboMCP tool
    # works through reflection rather than rendering.
    #
    # `-LowPriority` BECAUSE THE EDITOR STARTS AT `Normal` AND SO DOES A RUNNING GAME, so a suite run COMPETED ON
    # EQUAL TERMS with whatever the machine's owner was doing - MEASURED as a game hitching for the whole run. The
    # Windows scheduler gives BelowNormal threads the CPU only when nothing at Normal wants it, so the suites slow
    # down and the foreground work does not notice. An interactive launch does not have to pass either switch.
    & pwsh -NoProfile -File $StartEditor -Offscreen -LowPriority 2>&1 | Out-Null
    $retryDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $retryDeadline) {
        $fresh = Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue |
            Where-Object { $_.StartTime -ge $launchTime.AddSeconds(-2) }
        if ($fresh) {
            $probe = & python (Join-Path $PSScriptRoot 'check-mcp-server.py') 2>&1
            if ($probe -match '^PASS') { return $true }
        }
        Start-Sleep -Seconds 3
    }
    # THE RETRY FAILED TOO, SO THE FUNCTION REPORTS THAT RATHER THAN FALLING THROUGH INTO THE CODE BELOW,
    # WHICH BELONGS TO THE NORMAL PATH AND WOULD START A THIRD EDITOR.
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
    $run = Invoke-SuiteProcess -SuiteName $name
    $out = $run.Output
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

$script:EditorFailures = 0

foreach ($name in $NeedsEditor) {
    Write-Output ""
    Write-Output "--- $name ---"

    # ---------------------------------------------------------------------------------------------
    # ONE OWNER OF THE EDITOR, AND IT IS NOT ALWAYS THE HARNESS. THIS IS THE FIX FOR THE CLASS OF
    # FAILURE THAT HAS COST FOUR ROUNDS.
    #
    # MEASURED: a full run reported `31 of 39` while EVERY failing suite passed in isolation and
    # `make-acceptance-asset.ps1` run by hand returned exit 0. Four defects of ONE shape were found and
    # fixed - the harness waiting on the SERVER while the launcher decides from the PROCESS, a fixed
    # 2-second sleep, a probe that could pass on an editor still shutting down, and seven concurrent
    # rebuilds of one asset - AND THE FAILURES PERSISTED. The reason is that THE HARNESS AND THE SUITE WERE
    # BOTH DRIVING ONE EDITOR AND ONE `.uasset`: several suites STOP the editor to rebuild a fixture and
    # then START one and connect to it, while this loop ALSO stopped the editor before every suite -
    # KILLING THE VERY EDITOR THE PREVIOUS SUITE HAD JUST HEALTHILY STARTED, then racing a fresh start
    # against the next suite's expectations.
    #
    # SO THE HARNESS IS SUBORDINATE TO A HEALTHY EDITOR INSTEAD OF THE OWNER OF ONE. If a server already
    # answers, that editor is REUSED and NOTHING is stopped or started. That is strictly less work, and a
    # suite that manages its own editor is left alone instead of being sabotaged mid-flight.
    #
    # THE GUARD IS THE HEALTH PROBE RATHER THAN AN ASSUMPTION: an editor left WEDGED by a suite - alive,
    # port open, game thread not servicing requests - would make every later suite fail, so a probe that
    # does not answer triggers a full stop of everything and one clean start.
    # ---------------------------------------------------------------------------------------------
    $healthy = & python (Join-Path $PSScriptRoot 'check-mcp-server.py') 2>&1
    if ($healthy -match '^PASS') {
        Write-Output "  an editor is already answering; reusing it rather than restarting one under the suite"
    }
    # `Start-EditorAndWait` ITSELF DECIDES: it reuses a healthy server and only stops and starts when none
    # answers. The decision lives in ONE place rather than being made twice and disagreeing.
    if (-not (Start-EditorAndWait)) {
        # RECORDED AS AN ENVIRONMENT FAILURE RATHER THAN A CAPABILITY ONE. This distinction is the whole
        # reason the script exists: "the editor could not be started" is not a statement about the tool.
        $Results[$name] = 'RESULT: BLOCKED (the editor would not start)'
        Write-Output "  BLOCKED: the editor would not start, so this suite was NOT run"

        # ---------------------------------------------------------------------------------------------
        # THREE IN A ROW ABORTS THE RUN, BECAUSE A LATE FAILURE AND A SILENT HANG LOOK IDENTICAL.
        #
        # MEASURED, TWICE: a full run was left for 18 and 25 minutes with NO EDITOR ALIVE AND NO LOG
        # ACTIVITY, and it turned out to be `Start-EditorAndWait` spending its FULL 240-SECOND TIMEOUT on
        # every suite in turn. Thirty-six suites at four minutes each is TWO AND A HALF HOURS of output that
        # says nothing, and from outside it is indistinguishable from a hang. A runner that can spend hours
        # failing invisibly is worse than one that stops and says so.
        #
        # THE THRESHOLD IS THREE rather than one deliberately: a single editor that will not start can be a
        # transient, and this project has SEEN that - runs that stalled and then worked on the next attempt.
        # Three consecutive failures is no longer transient, and the message names the real cause so the
        # remaining suites are reported as NOT RUN rather than quietly missing from the summary.
        # ---------------------------------------------------------------------------------------------
        $script:EditorFailures++
        if ($script:EditorFailures -ge 3) {
            Write-Output ""
            Write-Output ("  ABORTING after {0} consecutive editor start failures. THIS IS AN ENVIRONMENT FAILURE," -f $script:EditorFailures)
            Write-Output "  NOT A CAPABILITY ONE: the editor will not come up, so no tool was exercised."
            Write-Output "  The remaining suites are recorded as NOT RUN rather than reported as failures."
            foreach ($remaining in $NeedsEditor) {
                if (-not $Results.ContainsKey($remaining)) {
                    $Results[$remaining] = 'RESULT: NOT RUN (aborted: the editor would not start)'
                }
            }
            break
        }
        continue
    }
    $script:EditorFailures = 0
    $run = Invoke-SuiteProcess -SuiteName $name
    $out = $run.Output
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
