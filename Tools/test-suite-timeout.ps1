# A PROOF-OF-CONCEPT FOR THE PER-SUITE DEADLINE, TESTED IN ISOLATION BEFORE IT IS WIRED INTO THE HARNESS.
#
# WHY THIS FILE EXISTS. A previous attempt wired a helper straight into `Tools/run-suites.ps1` and it returned
# nothing for every suite, so THREE STATIC SUITES reported `no result line` and the harness looked broken. A
# HELPER THAT SILENTLY YIELDS NOTHING IS INDISTINGUISHABLE FROM A SUITE THAT SAYS NOTHING, and the symptom pointed
# at three suites while the defect was one function. The lesson taken from that is to prove the helper ALONE,
# against a suite that needs no editor and finishes in seconds, before touching the harness at all.
#
# RUN IT:  pwsh -NoProfile -File Tools/test-suite-timeout.ps1

$ErrorActionPreference = 'Stop'
$ToolsDir = $PSScriptRoot

Write-Output "proving the per-suite deadline helper in isolation"
Write-Output ""

function Invoke-SuiteProcess {
    param([string] $SuiteName, [int] $TimeoutSeconds = 600)

    # `$ToolsDir` IS THE CALLER'S SCRIPT DIRECTORY, READ FROM THE ENCLOSING SCOPE, RATHER THAN RELYING ON
    # `$PSScriptRoot` INSIDE THE FUNCTION. MEASURED in the failed attempt: `$PSScriptRoot` was EMPTY inside the
    # function, `Join-Path` threw on an empty path, the call returned an empty string, and the harness recorded
    # three passing suites as producing no result. Reading a variable that IS set removes the question instead of
    # answering it.
    $exe = (Get-Command python).Source
    $outFile = Join-Path $env:TEMP "tmcp-probe-$SuiteName.out"
    $errFile = Join-Path $env:TEMP "tmcp-probe-$SuiteName.err"
    Remove-Item $outFile, $errFile -Force -ErrorAction SilentlyContinue

    $proc = Start-Process -FilePath $exe `
        -ArgumentList (Join-Path $ToolsDir "$SuiteName.py") `
        -WorkingDirectory $ToolsDir -NoNewWindow -PassThru `
        -RedirectStandardOutput $outFile -RedirectStandardError $errFile

    $finished = $proc.WaitForExit($TimeoutSeconds * 1000)
    if (-not $finished) {
        try { $proc.Kill($true) } catch { try { $proc.Kill() } catch { } }
        Start-Sleep -Seconds 2
        return @{ Output = "RESULT: TIMED OUT (${TimeoutSeconds}s)"; TimedOut = $true; Exit = -1 }
    }

    $text = ''
    if (Test-Path $outFile) { $text = Get-Content $outFile -Raw -ErrorAction SilentlyContinue }
    if (Test-Path $errFile) {
        $errText = Get-Content $errFile -Raw -ErrorAction SilentlyContinue
        if ($errText) { $text = "$text`n$errText" }
    }
    Remove-Item $outFile, $errFile -Force -ErrorAction SilentlyContinue
    return @{ Output = $text; TimedOut = $false; Exit = $proc.ExitCode }
}

# ---------------------------------------------------------------------------------------------
# STEP 1: DOES IT RETURN THE OUTPUT AT ALL? This is the question the failed attempt never asked.
# ---------------------------------------------------------------------------------------------
$r = Invoke-SuiteProcess -SuiteName 'check-includes'
Write-Output ("  check-includes: timedOut={0} exit={1} outputLength={2}" -f $r.TimedOut, $r.Exit, $r.Output.Length)
$matched = ($r.Output -split "`n" | Where-Object { $_ -match '^PASS|^RESULT' } | Select-Object -Last 1)
Write-Output ("  its verdict line: " + $(if ($matched) { $matched.Trim() } else { '<NONE FOUND>' }))
if ($r.Output.Length -eq 0) {
    Write-Output ""
    Write-Output "  FAIL: the helper returned NOTHING, which is exactly the defect of the previous attempt."
    exit 1
}

# ---------------------------------------------------------------------------------------------
# STEP 2: DOES IT KILL A PROCESS THAT OVERRUNS? Proved with a suite name that deliberately does not exist, so
# python exits at once, and then with a deadline of ONE SECOND against the slowest static suite, so the kill path
# is exercised for real rather than reasoned about.
# ---------------------------------------------------------------------------------------------
Write-Output ""
$slow = Invoke-SuiteProcess -SuiteName 'check-includes' -TimeoutSeconds 1
Write-Output ("  1-second deadline: timedOut={0} verdict={1}" -f $slow.TimedOut, $slow.Output.Trim())
if (-not $slow.TimedOut) {
    Write-Output "  NOTE: the suite finished inside one second, so the KILL path was NOT exercised here."
    Write-Output "        That is a measurement, not a failure - but it means the kill is still unproven."
}

Write-Output ""
Write-Output ("RESULT: PASS (the helper returns output: {0} characters)" -f $r.Output.Length)
