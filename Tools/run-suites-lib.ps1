# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
<#
.SYNOPSIS
    The suite runner's verdict and selection decisions, extracted so they can be tested without running a suite.

.DESCRIPTION
    WHY THIS FILE EXISTS. Release finding R01 records that the release gate could report success without a
    successful acceptance, and the audit confirmed it against the real classifier: a child that printed
    `RESULT: PASS` and then died was accepted as passing. Four separate causes were named:

      * the child's exit code was discarded, so `PASS` followed by exit 7 counted as a pass;
      * a timeout was recorded as its own verdict text and then classified as a failure, but nothing
        distinguished "the child failed" from "the child never finished";
      * a selection that matched nothing only printed a warning, which made an empty run look green;
      * a partly misspelled selection silently dropped the names it could not match.

    Each of those is a decision, not a process. Keeping them inside `run-suites.ps1` meant the only way to
    test them was to run the whole harness, so the decisions were never tested. They live here instead, and
    `Tools/test-runner-verdicts.py` drives this file directly through PowerShell with fake children.

    THE RULE THIS ENCODES: a suite passes only when its printed verdict is structurally accepted, its exit
    code is zero, it did not time out, and its output was actually captured. Anything else is a failure the
    release gate can see.

    STRICT MODE IS NOT SET HERE. This file is dot-sourced into `run-suites.ps1`, whose long-standing style
    reads variables that are declared later in the script body; enabling strict mode at file scope would
    reject that code for reasons unrelated to a verdict. The functions below keep their own types explicit.
#>

#: Exit code the runner uses for a refused selection. Distinct from 1 so a caller can tell
#: "your request could not be honoured" from "a suite failed".
$script:SelectionExitCode = 2

function New-TurboMCPVerdict {
    <#
    .SYNOPSIS
        One suite's measured result, carrying the evidence the classifier needs.
    #>
    [CmdletBinding()]
    param(
        [string] $Suite = '',
        [string] $Verdict = '',
        [object] $ExitCode = $null,
        [bool] $TimedOut = $false,
        [bool] $OutputCaptured = $true,
        [bool] $OutputTruncated = $false,
        [string] $LogPath = '',
        [string] $Detail = ''
    )

    [pscustomobject]@{
        Suite           = $Suite
        Verdict         = $Verdict
        ExitCode        = $ExitCode
        TimedOut        = $TimedOut
        OutputCaptured  = $OutputCaptured
        OutputTruncated = $OutputTruncated
        LogPath         = $LogPath
        Detail          = $Detail
    }
}

function Get-TurboMCPVerdict {
    <#
    .SYNOPSIS
        The verdict line a suite printed, or an explicit statement that it printed none.

    .DESCRIPTION
        Last match wins, because a suite that prints an isolated sub-result after its primary result must have
        the primary result judged - and the primary is the one printed first in every suite in this repository.
        A suite that printed nothing usable gets a verdict that names the absence; it never gets a pass.
    #>
    [CmdletBinding()]
    param(
        [string[]] $Output,
        [string] $Diagnostic = 'no result line'
    )

    $verdictLines = @($Output | Select-String -Pattern '^RESULT|^PASS|^FAIL' -ErrorAction SilentlyContinue)
    if ($verdictLines.Count -gt 0) {
        return $verdictLines[-1].Line.Trim()
    }

    $tail = @($Output | Select-Object -Last 4) -join ' | '
    if ($tail) { return "RESULT: $Diagnostic ($tail)" }
    return "RESULT: $Diagnostic and no output"
}

function Get-TurboMCPVerdictKind {
    <#
    .SYNOPSIS
        Classify one verdict line as pass, blocked, timeout or fail.
    #>
    [CmdletBinding()]
    param([string] $Verdict)

    if (-not $Verdict) { return 'fail' }
    $primary = ($Verdict -split '\s+\*\*\* ALONE:')[0].Trim()
    if ($primary -match 'RESULT:\s*TIMED OUT|TIMED OUT \(') { return 'timeout' }
    if ($primary -match 'BLOCKED') { return 'blocked' }
    if ($primary -match 'RESULT:\s*PASS|^PASS\b') { return 'pass' }
    return 'fail'
}

function Test-TurboMCPVerdictAcceptance {
    <#
    .SYNOPSIS
        The release gate's decision for one suite.

    .DESCRIPTION
        A pass requires ALL of: an accepted verdict line, exit code exactly zero, no timeout, and captured
        output. Each condition is reported separately so a failure names its own cause instead of collapsing
        into "not green".
    #>
    [CmdletBinding()]
    param([Parameter(Mandatory)] $Verdict)

    $kind = Get-TurboMCPVerdictKind $Verdict.Verdict
    $reasons = @()

    if ($Verdict.TimedOut) { $reasons += 'the suite was killed at its deadline' }
    if (-not $Verdict.OutputCaptured) { $reasons += 'the suite output was not captured, so its verdict is not evidence' }
    if ($kind -ne 'pass') {
        if ($kind -eq 'timeout') { $reasons += 'the verdict line reports a timeout' }
        elseif ($kind -eq 'blocked') { $reasons += 'a required prerequisite was blocked' }
        else { $reasons += 'the verdict line is not an accepted pass' }
    }
    # `$null` is a real state here: a suite that never ran has no exit code, and an absent exit code is not
    # evidence of success. Only an explicit 0 is.
    if ($null -eq $Verdict.ExitCode) { $reasons += 'the child exit code was not recorded' }
    elseif ([int]$Verdict.ExitCode -ne 0) { $reasons += "the child exited $($Verdict.ExitCode)" }

    if ($reasons.Count -gt 0) {
        return [pscustomobject]@{ Accepted = $false; Kind = $kind; Reasons = $reasons }
    }
    return [pscustomobject]@{ Accepted = $true; Kind = 'pass'; Reasons = @() }
}

function Resolve-TurboMCPSuiteSelection {
    <#
    .SYNOPSIS
        Turn a requested suite selection into an exact list, or refuse the whole request.

    .DESCRIPTION
        Release finding R01: "a partly misspelled selection silently omits unknown names" and "a selection that
        matched nothing only warned". Both make an empty or reduced run look like a green one.

        A request is honoured exactly or refused. Unknown names are refused rather than skipped, duplicates are
        refused rather than silently collapsed, and a selection that resolves to nothing is refused rather than
        ran. `Ok` is the only field a caller may act on.
    #>
    [CmdletBinding()]
    param(
        [string[]] $Requested,
        [string[]] $Available
    )

    $availableSet = @{}
    foreach ($name in $Available) { $availableSet[$name] = $true }

    if ($null -eq $Requested -or $Requested.Count -eq 0) {
        return [pscustomobject]@{
            Ok = $false
            Selected = @()
            Reason = 'no suite was requested, and an empty selection is a refused request rather than a green run'
        }
    }

    $expanded = @()
    foreach ($entry in $Requested) {
        foreach ($piece in @($entry -split ',')) {
            $trimmed = $piece.Trim()
            if ($trimmed) { $expanded += $trimmed }
        }
    }
    if ($expanded.Count -eq 0) {
        return [pscustomobject]@{
            Ok = $false
            Selected = @()
            Reason = 'the selection contained only separators and whitespace'
        }
    }

    $seen = @{}
    $duplicates = @()
    foreach ($name in $expanded) {
        if ($seen.ContainsKey($name)) { $duplicates += $name } else { $seen[$name] = $true }
    }
    if ($duplicates.Count -gt 0) {
        return [pscustomobject]@{
            Ok = $false
            Selected = @()
            Reason = "duplicate suite name(s) in the selection: $((@($duplicates | Sort-Object -Unique)) -join ', ')"
        }
    }

    if ($expanded.Count -eq 1 -and $expanded[0] -eq 'all') {
        return [pscustomobject]@{ Ok = $true; Selected = @($Available); Reason = '' }
    }

    $unknown = @($expanded | Where-Object { -not $availableSet.ContainsKey($_) })
    if ($unknown.Count -gt 0) {
        $known = (@($Available | Sort-Object) -join ', ')
        $reason = ("unknown suite name(s): {0}. A partly misspelled selection is refused rather than " +
                   "silently reduced. Known suites: {1}") -f ($unknown -join ', '), $known
        return [pscustomobject]@{ Ok = $false; Selected = @(); Reason = $reason }
    }

    # Declaration order: the caller's order is stable, and a caller that wants the canonical pass order
    # asks for `all`.
    $ordered = @()
    foreach ($name in $expanded) {
        if ($availableSet.ContainsKey($name) -and ($ordered -notcontains $name)) { $ordered += $name }
    }
    return [pscustomobject]@{ Ok = $true; Selected = $ordered; Reason = '' }
}
