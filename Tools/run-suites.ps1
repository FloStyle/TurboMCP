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

    # THE SERVER FROM THE PREVIOUS SUITE MUST BE PROVEN DEAD BEFORE THE NEW ONE IS STARTED, AND THIS IS A
    # FIX RATHER THAN A PRECAUTION.
    #
    # MEASURED: `Start-EditorAndWait` used to call `start-editor.ps1` and then poll until the MCP server
    # answered. But a suite that stops the editor to rebuild a fixture - `test-graph-dead` does exactly that,
    # twice - can leave the PREVIOUS editor's server still answering for a few seconds while it shuts down.
    # The probe then returned `PASS` immediately, the suite was launched against an editor that was still
    # going away, and it died with NO RESULT LINE AT ALL. `test-comments,test-graph-dead` in sequence failed
    # that way, and `test-graph-dead` alone passed - which is the signature of a race, not of a broken tool.
    #
    # A PROBE THAT CAN PASS ON A DYING PROCESS IS A PROBE THAT MEASURES THE WRONG THING. So the wait now
    # runs in both directions: DOWN first, then up.
    $downDeadline = (Get-Date).AddSeconds(60)
    while ((Get-Date) -lt $downDeadline) {
        $probe = & python (Join-Path $PSScriptRoot 'check-mcp-server.py') 2>&1
        if ($probe -notmatch '^PASS') { break }
        Start-Sleep -Seconds 2
    }

    & pwsh -NoProfile -File $StartEditor 2>&1 | Out-Null
    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    while ((Get-Date) -lt $deadline) {
        # THE SERVER IS POLLED, NOT ASSUMED READY. A fixed sleep is either too short on a cold start or
        # wasted time on a warm one, and this project has been bitten by both.
        $probe = & python (Join-Path $PSScriptRoot 'check-mcp-server.py') 2>&1
        if ($probe -match '^PASS') { return $true }
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
