<#
.SYNOPSIS
    Runs acceptance suites with one kept-alive editor and isolated fixtures.

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

    Source-only, pure-Python and commandlet suites run first without an interactive editor. The runner then starts
    one editor and reuses it. Between normal suites, the editor releases package handles, the runner restores exact
    run-start bytes, and the editor reloads those packages. This proves both disk and in-memory fixture isolation
    without a per-suite stop/start. A suite that kills the editor is detected and recovered before the next suite.

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
    [switch] $SkipSlow,
    # -Fast defers the editor-side fixture reload until the end. It is for focused timing only; the default path
    # restores disk bytes and loaded packages after every suite, and is the fixture-isolation proof.
    [switch] $Fast
)

$ErrorActionPreference = 'Stop'
$Repo = Split-Path -Parent $PSScriptRoot
$StartEditor = Join-Path $PSScriptRoot 'start-editor.ps1'

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
    # *** INSTRUMENTED: THE REUSE/START/FAIL DECISION IS NOW PRINTED, BECAUSE THE FAILING SUITE NAMES MOVE
    # BETWEEN RUNS AND THIS IS THE POINT THAT DECIDES WHY. A rebuilding suite STOPS the editor (a `-run=` commandlet
    # needs the project to itself), so the harness has to bring one back; if it cannot, EVERY SUITE AFTER IT inherits
    # the absence and the failure set moves with the race rather than with the code. ONE LINE OF OUTPUT DECIDES
    # BETWEEN "the restart failed" AND "the reuse path is at fault".
    if ($healthy -match '^PASS') {
        $script:EditorPid = @(Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue |
            Sort-Object StartTime -Descending | Select-Object -First 1 | ForEach-Object { $_.Id })[0]
        Write-Host "  [editor] REUSED a healthy editor (PID $script:EditorPid)"
        return $true
    }

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
    Write-Host "  the editor in the way answers nothing; clearing it and retrying ONCE"
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
    $oldStartingEditor = $env:TURBOMCP_RUNNER_STARTING_EDITOR
    $env:TURBOMCP_RUNNER_STARTING_EDITOR = '1'
    try { & pwsh -NoProfile -File $StartEditor -LowPriority 2>&1 | Out-Null }
    finally { $env:TURBOMCP_RUNNER_STARTING_EDITOR = $oldStartingEditor }
    $retryDeadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $retryDeadline) {
        $fresh = Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue |
            Where-Object { $_.StartTime -ge $launchTime.AddSeconds(-2) }
        if ($fresh) {
            $probe = & python (Join-Path $PSScriptRoot 'check-mcp-server.py') 2>&1
    if ($probe -match '^PASS') {
        $script:EditorPid = $fresh.Id
        Write-Host "  [editor] STARTED a fresh editor (PID $script:EditorPid)"
        return $true
    }
        }
        Start-Sleep -Seconds 3
    }
    # THE RETRY FAILED TOO, SO THE FUNCTION REPORTS THAT RATHER THAN FALLING THROUGH INTO THE CODE BELOW,
    # WHICH BELONGS TO THE NORMAL PATH AND WOULD START A THIRD EDITOR.
    Write-Host "  [editor] *** COULD NOT BE STARTED after the full timeout ***"
    return $false
}


function Stop-Editor {
    $oldAllowStop = $env:TURBOMCP_RUNNER_ALLOW_STOP
    $env:TURBOMCP_RUNNER_ALLOW_STOP = '1'
    try { & pwsh -NoProfile -File $StartEditor -Stop 2>&1 | Out-Null }
    finally { $env:TURBOMCP_RUNNER_ALLOW_STOP = $oldAllowStop }
    # A commandlet or trace server can outlive UnrealEditor and still hold package
    # handles. Clear those descendants before any disk-only restore.
    Get-Process -Name UnrealEditor-Cmd, UnrealTraceServer -ErrorAction SilentlyContinue |
        Stop-Process -Force -ErrorAction SilentlyContinue
    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        $left = Get-Process -Name UnrealEditor, UnrealEditor-Cmd, UnrealTraceServer -ErrorAction SilentlyContinue
        if (-not $left) { return }
        Start-Sleep -Seconds 1
    }
    throw 'editor-related process(es) still alive after 30s'
}
# ---------------------------------------------------------------------------------------------
# Fixture isolation without editor churn
# ---------------------------------------------------------------------------------------------
# The old implementation restored with `git checkout`. Windows cannot unlink a package while the
# editor owns its handle, so that route forced a stop/start around every mutating suite. The restore
# routes are deliberately ordered and each one is verified:
#   1. ask the live editor tools to release and reload the package;
#   2. if that is unavailable, restore bytes before the next editor opens the fixture;
#   3. use a direct file copy as the final disk-only fallback (only with no editor open).
# A green run is not accepted from hashes alone: the live route must also resolve the packages again.
function Get-RelativeFixtureFiles {
    param([string] $Root)
    if (-not (Test-Path $Root)) { return @() }
    return @(Get-ChildItem $Root -File -Recurse | ForEach-Object {
        $_.FullName.Substring($Root.Length).TrimStart('\', '/')
    })
}

function New-FixtureSnapshot {
    param([string] $FixtureDir, [string] $TempRoot, [string] $Name = 'fixture-start')
    $files = @(Get-RelativeFixtureFiles $FixtureDir)
    if ($files.Count -eq 0) { throw "fixture snapshot refused: $FixtureDir is empty" }
    $snapshot = Join-Path $TempRoot $Name
    # A reused temp root must not contribute files from an older snapshot. The manifest
    # is the isolation contract, so recreate the destination before copying it.
    Remove-Item $snapshot -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Path $snapshot -Force | Out-Null
    Copy-Item (Join-Path $FixtureDir '*') $snapshot -Recurse -Force
    $copied = @(Get-RelativeFixtureFiles $snapshot)
    if ($copied.Count -ne $files.Count) {
        throw "fixture snapshot incomplete: expected $($files.Count) file(s), copied $($copied.Count)"
    }
    return $snapshot
}

function Restore-FixtureSnapshot {
    param(
        [string] $FixtureDir,
        [string] $SnapshotDir,
        [string] $TempRoot,
        [switch] $EditorLive
    )

    $baseline = @(Get-RelativeFixtureFiles $SnapshotDir)
    $current = @(Get-RelativeFixtureFiles $FixtureDir)
    $baselineSet = @{}; foreach ($relative in $baseline) { $baselineSet[$relative] = $true }
    $changed = @()
    foreach ($relative in $baseline) {
        $source = Join-Path $SnapshotDir $relative
        $target = Join-Path $FixtureDir $relative
        if (-not (Test-Path $target) -or
            (Get-FileHash $source -Algorithm SHA256).Hash -ne (Get-FileHash $target -Algorithm SHA256).Hash) {
            $changed += $relative
        }
    }
    $extras = @($current | Where-Object { -not $baselineSet.ContainsKey($_) })
    if (-not $EditorLive -and $changed.Count -eq 0 -and $extras.Count -eq 0) { return 0 }
    $restoredCount = $changed.Count + $extras.Count

    if ($EditorLive) {
        # Route 1: project fixture tools release package handles before the copy, then reload the
        # restored package. This is the only live-editor route that proves in-memory isolation.
        $plan = Join-Path $TempRoot 'fixture-plan.json'
        $prepareOutput = @(& python (Join-Path $PSScriptRoot 'fixture-isolation.py') prepare `
            --fixture $FixtureDir --snapshot $SnapshotDir --plan $plan 2>&1)
        if ($LASTEXITCODE -ne 0) {
            throw "the editor could not prepare the fixture restore: $($prepareOutput -join ' | ')"
        }

        foreach ($relative in $changed) {
            $target = Join-Path $FixtureDir $relative
            New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
            Copy-Item (Join-Path $SnapshotDir $relative) $target -Force
        }
        # The editor tool removes package extras. Sidecars are not packages, so remove them only
        # after prepare released the live handles and before reload verifies the fixture.
        foreach ($relative in $extras | Where-Object { $_ -notmatch '\.(uasset|umap)$' }) {
            Remove-Item (Join-Path $FixtureDir $relative) -Force
        }
        $reloadOutput = @(& python (Join-Path $PSScriptRoot 'fixture-isolation.py') reload --plan $plan 2>&1)
        if ($LASTEXITCODE -ne 0) {
            throw "the editor could not reload the restored fixture packages: $($reloadOutput -join ' | ')"
        }
        $restorePlan = Get-Content $plan -Raw | ConvertFrom-Json
        $restoredCount = @($restorePlan.asset_paths).Count + $extras.Count
    } else {
        # Route 2: before an editor opens any package, ordinary replacement is safe.
        foreach ($relative in $changed) {
            $target = Join-Path $FixtureDir $relative
            New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
            Copy-Item (Join-Path $SnapshotDir $relative) $target -Force
        }
        foreach ($relative in $extras) { Remove-Item (Join-Path $FixtureDir $relative) -Force }
    }

    # The editor-side helper proves the in-memory half. This proves the disk half byte-for-byte.
    $after = @(Get-RelativeFixtureFiles $FixtureDir)
    if ($after.Count -ne $baseline.Count) { throw 'fixture isolation failed: file count differs from run start' }
    foreach ($relative in $baseline) {
        $source = Join-Path $SnapshotDir $relative
        $target = Join-Path $FixtureDir $relative
        if (-not (Test-Path $target) -or
            (Get-FileHash $source -Algorithm SHA256).Hash -ne (Get-FileHash $target -Algorithm SHA256).Hash) {
            throw "fixture isolation failed: $relative differs from run start"
        }
    }
    return $restoredCount
}

function Get-Verdict {
    param([string[]] $Output)
    $line = ($Output | Select-String -Pattern '^RESULT|^PASS|^FAIL' | Select-Object -Last 1).Line
    if ($line) { return $line.Trim() }
    $tail = @($Output | Select-Object -Last 4) -join ' | '
    if ($tail) { return "RESULT: no result line ($tail)" }
    return 'RESULT: no result line and no output'
}

function Test-VerdictPassed {
    param([string] $Verdict)
    # A -Fast failure may carry an isolated PASS after the primary verdict. Count the
    # primary result only; otherwise a non-hermetic suite is reported as green.
    $primary = ($Verdict -split '\s+\*\*\* ALONE:')[0].Trim()
    return $primary -match 'RESULT:\s*PASS|^PASS '
}

$script:RunnerCleanupDone = $false
function Invoke-RunnerCleanup {
    if ($script:RunnerCleanupDone) { return }
    $script:RunnerCleanupDone = $true
    # An interrupted suite must not leave a mutated fixture or a runner-owned editor behind.
    # Cleanup is best-effort here; the original exception remains the verdict.
    try {
        if ($runStartSnapshot -and (Test-Path $runStartSnapshot) -and (Test-Path $fixtureDir)) {
            Restore-FixtureSnapshot $fixtureDir $runStartSnapshot $tempRoot -EditorLive:$editorReady
        }
    } catch { }
    try {
        if ($replacementOwned -and $NeedsEditor.Count -gt 0) { Stop-Editor }
    } catch { }
    try {
        if ($tempRoot -and (Test-Path $tempRoot)) { Remove-Item $tempRoot -Recurse -Force -ErrorAction SilentlyContinue }
    } catch { }
}
trap {
    Invoke-RunnerCleanup
    throw
}

# ---------------------------------------------------------------------------------------------
# Select suites. Source-only, pure-Python and true commandlet suites run before any interactive editor.
# ---------------------------------------------------------------------------------------------
$NoEditorSuites = @(
    'audit-tool-coverage', 'audit-suite-quality', 'check-includes', 'check-plugin',
    'check-probe-graphs', 'test-kit-ta17-ta19', 'test-lookup-guards', 'test-panel'
)
$All = @(Get-ChildItem $PSScriptRoot -Filter '*.py' |
    Where-Object { $_.BaseName -match '^(test|check|audit)-' } |
    ForEach-Object { $_.BaseName } | Sort-Object)
$Selected = @($All)
if ($Suites -notcontains 'all' -and $Suites.Count -gt 0) {
    $wanted = @()
    foreach ($entry in $Suites) { $wanted += ($entry -split ',') }
    $wanted = @($wanted | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    $Selected = @($All | Where-Object { $wanted -contains $_ })
    if ($Selected.Count -eq 0) { Write-Output "WARNING: no suite matched: $($wanted -join ', ')" }
}
if ($SkipSlow) { $Selected = @($Selected | Where-Object { $SlowSuites -notcontains $_ }) }
$Static = @($Selected | Where-Object { $NoEditorSuites -contains $_ })
$NeedsEditor = @($Selected | Where-Object { $NoEditorSuites -notcontains $_ })

$fullClock = [Diagnostics.Stopwatch]::StartNew()
$tempRoot = Join-Path $Repo ('.agent\tmp\run-suites-' + $PID)
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null
# Do not redirect TEMP into the workspace. Python's tempfile module gives each child directory an
# explicit owner-only ACL; under DSH confinement that strips the sandbox grant from grandchildren.
# The platform temporary directory is writable and avoids that false test failure.
$workspaceDdc = Join-Path $Repo '.agent\ddc'
New-Item -ItemType Directory -Path $workspaceDdc -Force | Out-Null
Set-Item 'Env:UE-LocalDataCachePath' $workspaceDdc
$fixtureDir = Join-Path $Repo 'TestProject\Content\Tests'
$runStartSnapshot = New-FixtureSnapshot $fixtureDir $tempRoot 'fixture-run-start'
$fixtureSnapshot = $runStartSnapshot
$Results = [ordered]@{}
$startupSeconds = 0.0; $testSeconds = 0.0; $restoreSeconds = 0.0; $healthSeconds = 0.0
$shutdownSeconds = 0.0; $restoreCount = 0
$script:EditorPid = 0

# Record ownership BEFORE a commandlet is allowed to stop an existing editor.
$ownershipClock = [Diagnostics.Stopwatch]::StartNew()
$initialHealth = & python (Join-Path $PSScriptRoot 'check-mcp-server.py') 2>&1
$ownershipClock.Stop()
$startupSeconds += $ownershipClock.Elapsed.TotalSeconds
$initialEditorHealthy = [bool]($initialHealth | Select-String -Pattern '^PASS')
$replacementOwned = $false

Write-Output "=== TurboMCP suite runner: $($Static.Count) editor-free, $($NeedsEditor.Count) needing an editor ==="
Write-Output '--- pure, static, and commandlet suites; no interactive editor ---'
if ($Static.Count -gt 0 -and $initialEditorHealthy) {
    $clock = [Diagnostics.Stopwatch]::StartNew(); Stop-Editor; $clock.Stop()
    $shutdownSeconds += $clock.Elapsed.TotalSeconds
}
$env:TURBOMCP_NO_EDITOR = '1'
foreach ($name in $Static) {
    $clock = [Diagnostics.Stopwatch]::StartNew(); $run = Invoke-SuiteProcess $name; $clock.Stop()
    $testSeconds += $clock.Elapsed.TotalSeconds
    $Results[$name] = Get-Verdict $run.Output
    Write-Output ("  {0,-28} {1}" -f $name, $Results[$name])
}
$env:TURBOMCP_NO_EDITOR = $null

# Commandlet suites deliberately regenerate canonical fixtures. Snapshot that result for every live suite,
# while keeping the separate run-start snapshot so the caller receives its exact original bytes at the end.
$fixtureSnapshot = New-FixtureSnapshot $fixtureDir $tempRoot 'fixture-editor-baseline'

Write-Output '--- editor suites; one healthy editor is reused ---'
# Destructive suites historically called make-*.ps1 before and after their test. Those commandlets stop
# the interactive editor. The runner's exact snapshot now owns that responsibility, so inherited child
# processes skip redundant rebuilds while standalone suite runs keep their original hygiene.
$env:TURBOMCP_RUNNER_OWNS_FIXTURES = '1'
$env:TURBOMCP_FIXTURE_DIR = $fixtureDir
$env:TURBOMCP_FIXTURE_SNAPSHOT = $fixtureSnapshot
$env:TURBOMCP_FIXTURE_TEMP = $tempRoot
$editorReady = $false
if ($NeedsEditor.Count -gt 0) {
    if ($initialEditorHealthy -and $Static.Count -eq 0) {
        $script:EditorPid = @(Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue |
            Sort-Object StartTime -Descending | Select-Object -First 1 | ForEach-Object { $_.Id })[0]
        Write-Host "  [editor] REUSED a healthy editor (PID $script:EditorPid)"
        $editorReady = $true
    } else {
        $clock = [Diagnostics.Stopwatch]::StartNew(); $editorReady = Start-EditorAndWait; $clock.Stop()
        $startupSeconds += $clock.Elapsed.TotalSeconds
        $replacementOwned = $true
    }
}
if ($NeedsEditor.Count -gt 0 -and $editorReady) {
    # A runner may attach to an editor left by an earlier invocation. Restore once before
    # the first suite so a focused -Fast run starts from the exact fixture baseline,
    # including dirty in-memory packages whose disk bytes still match.
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $n = Restore-FixtureSnapshot $fixtureDir $fixtureSnapshot $tempRoot -EditorLive
    $clock.Stop(); $restoreSeconds += $clock.Elapsed.TotalSeconds; $restoreCount += $n
    $initialProbeArgs = @()
    if ($script:EditorPid -gt 0) { $initialProbeArgs += @('--editor-pid', [string]$script:EditorPid) }
    $initialProbe = & python (Join-Path $PSScriptRoot 'check-mcp-server.py') @initialProbeArgs 2>&1
    if ($initialProbe -notmatch '^PASS') {
        Write-Output '  initial live restore left no healthy editor; recovering before the first suite'
        $clock = [Diagnostics.Stopwatch]::StartNew(); $editorReady = Start-EditorAndWait; $clock.Stop()
        $startupSeconds += $clock.Elapsed.TotalSeconds
        $replacementOwned = $true
    }
    Write-Output "  initial fixture isolation: kept-alive editor restored ($n file(s))"
}
foreach ($name in $NeedsEditor) {
    Write-Output "--- $name ---"
    if (-not $editorReady) {
        $Results[$name] = 'RESULT: BLOCKED (the editor would not start)'
        continue
    }
    $clock = [Diagnostics.Stopwatch]::StartNew(); $run = Invoke-SuiteProcess $name; $clock.Stop()
    $testSeconds += $clock.Elapsed.TotalSeconds
    $Results[$name] = Get-Verdict $run.Output
    Write-Output ("  " + $Results[$name])

    # One post-suite probe detects a suite that stopped or wedged the editor. A
    # successful editor-side restore is itself the second health proof, so probing
    # again after it only adds another network round trip to every suite.
    $probeClock = [Diagnostics.Stopwatch]::StartNew()
    # Keep the owner check here. A foreign editor on port 8000 must never be treated as
    # permission to unload or reload this project's fixture. When we own a PID, verify that
    # exact process still exists even on hosts where CIM command-line queries are unavailable.
    $probeArgs = @()
    if ($script:EditorPid -gt 0) { $probeArgs += @('--editor-pid', [string]$script:EditorPid) }
    $probe = & python (Join-Path $PSScriptRoot 'check-mcp-server.py') @probeArgs 2>&1
    $probeClock.Stop(); $healthSeconds += $probeClock.Elapsed.TotalSeconds
    $editorReady = [bool]($probe | Select-String -Pattern '^PASS')

    if (-not $Fast) {
        # A dead or wedged editor must not remain holding package handles while the disk route runs.
        if (-not $editorReady -and (Get-Process -Name UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue)) {
            $clock = [Diagnostics.Stopwatch]::StartNew(); Stop-Editor; $clock.Stop()
            $shutdownSeconds += $clock.Elapsed.TotalSeconds
        }
        $clock = [Diagnostics.Stopwatch]::StartNew()
        try {
            # Route 1: release and reload through the live editor tools.
            $n = Restore-FixtureSnapshot $fixtureDir $fixtureSnapshot $tempRoot -EditorLive:$editorReady
            $mode = if ($editorReady) { 'disk and kept-alive editor' } else { 'disk after editor stop' }
        } catch {
            if (-not $editorReady -or -not $replacementOwned) { throw }
            # Route 2: the runner owns this editor, so stop it before the next editor can open the
            # fixture. Route 3 is the direct copy inside Restore-FixtureSnapshot's disk branch.
            Write-Output "  live fixture tools unavailable; restoring before editor reopen: $($_.Exception.Message)"
            $stopClock = [Diagnostics.Stopwatch]::StartNew(); Stop-Editor; $stopClock.Stop()
            $shutdownSeconds += $stopClock.Elapsed.TotalSeconds
            $editorReady = $false
            $n = Restore-FixtureSnapshot $fixtureDir $fixtureSnapshot $tempRoot
            $mode = 'disk before editor reopen'
        }
        $clock.Stop(); $restoreSeconds += $clock.Elapsed.TotalSeconds; $restoreCount += $n
        Write-Output "  fixture isolation: $mode restored ($n file(s))"
    }

    if (-not $editorReady) {
        $clock = [Diagnostics.Stopwatch]::StartNew(); $editorReady = Start-EditorAndWait; $clock.Stop()
        $healthSeconds += $clock.Elapsed.TotalSeconds
        $replacementOwned = $true
    }
}

if ($Fast -and $NeedsEditor.Count -gt 0) {
    if (-not $editorReady -and (Get-Process -Name UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue)) {
        $clock = [Diagnostics.Stopwatch]::StartNew(); Stop-Editor; $clock.Stop()
        $shutdownSeconds += $clock.Elapsed.TotalSeconds
    }
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $n = Restore-FixtureSnapshot $fixtureDir $fixtureSnapshot $tempRoot -EditorLive:$editorReady
    $clock.Stop(); $restoreSeconds += $clock.Elapsed.TotalSeconds; $restoreCount += $n
    Write-Output "  -Fast deferred restore: $n file(s)"

    $failedInPass = @($NeedsEditor | Where-Object {
        $Results.Contains($_) -and $Results[$_] -notmatch 'RESULT: PASS|^PASS ' -and $Results[$_] -notmatch 'BLOCKED'
    })
    foreach ($name in $failedInPass) {
        $clock = [Diagnostics.Stopwatch]::StartNew(); $editorReady = Start-EditorAndWait; $clock.Stop()
        $healthSeconds += $clock.Elapsed.TotalSeconds
        if (-not $editorReady) { continue }
        $clock = [Diagnostics.Stopwatch]::StartNew(); $alone = Invoke-SuiteProcess $name; $clock.Stop()
        $testSeconds += $clock.Elapsed.TotalSeconds
        $aloneLine = Get-Verdict $alone.Output
        Write-Output "  isolated $name -> $aloneLine"
        $Results[$name] = "$($Results[$name]) *** ALONE: $aloneLine ***"
        $clock = [Diagnostics.Stopwatch]::StartNew()
        $n = Restore-FixtureSnapshot $fixtureDir $fixtureSnapshot $tempRoot -EditorLive
        $clock.Stop(); $restoreSeconds += $clock.Elapsed.TotalSeconds; $restoreCount += $n
    }
}

# Restore the caller's exact tree. Prefer the live editor route while it is healthy; only
# fall back to a disk restore after stopping an editor owned by this runner.
try {
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $n = Restore-FixtureSnapshot $fixtureDir $runStartSnapshot $tempRoot -EditorLive:$editorReady
    $clock.Stop(); $restoreSeconds += $clock.Elapsed.TotalSeconds; $restoreCount += $n
} catch {
    Write-Output ("  final live fixture restore failed: " + $_.Exception.Message)
    if ($replacementOwned -and $NeedsEditor.Count -gt 0) {
        $clock = [Diagnostics.Stopwatch]::StartNew(); Stop-Editor; $clock.Stop()
        $shutdownSeconds += $clock.Elapsed.TotalSeconds
        $editorReady = $false
        $clock = [Diagnostics.Stopwatch]::StartNew()
        $n = Restore-FixtureSnapshot $fixtureDir $runStartSnapshot $tempRoot
        $clock.Stop(); $restoreSeconds += $clock.Elapsed.TotalSeconds; $restoreCount += $n
    } else {
        throw
    }
}
if ($replacementOwned -and $NeedsEditor.Count -gt 0) {
    $clock = [Diagnostics.Stopwatch]::StartNew(); Stop-Editor; $clock.Stop()
    $shutdownSeconds += $clock.Elapsed.TotalSeconds
}
Remove-Item $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
$fullClock.Stop()

Write-Output '=== SUMMARY ==='
$passed = 0; $failed = 0; $blocked = 0
foreach ($name in $Results.Keys) {
    $r = $Results[$name]
    if (Test-VerdictPassed $r) { $passed++ }
    elseif ($r -match 'BLOCKED') { $blocked++ }
    else { $failed++ }
    $mark = if (Test-VerdictPassed $r) { 'PASS' } elseif ($r -match 'BLOCKED') { 'BLOCKED' } else { 'FAIL' }
    Write-Output ("  {0,-7} {1,-28} {2}" -f $mark, $name, $r)
}
Write-Output ("  passed {0}, failed {1}, blocked {2}, of {3}" -f $passed, $failed, $blocked, $Results.Count)
Write-Output '=== WALL TIME ==='
Write-Output ("  editor startup/reuse gate : {0,8:N3} s" -f $startupSeconds)
Write-Output ("  editor health/recovery    : {0,8:N3} s" -f $healthSeconds)
Write-Output ("  editor final stop         : {0,8:N3} s" -f $shutdownSeconds)
Write-Output ("  suite processes           : {0,8:N3} s" -f $testSeconds)
Write-Output ("  fixture restore           : {0,8:N3} s ({1} file(s))" -f $restoreSeconds, $restoreCount)
Write-Output ("  full runner wall          : {0,8:N3} s" -f $fullClock.Elapsed.TotalSeconds)
if ($failed -gt 0) { exit 1 }
exit 0
