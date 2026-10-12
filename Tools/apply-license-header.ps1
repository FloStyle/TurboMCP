<#
.SYNOPSIS
    Puts the MPL-2.0 notice at the top of every .h, .cpp and .cs in the plugin.

.DESCRIPTION
    The notice is Exhibit A of the license, which is the form the license itself asks a file to carry. It
    lives in `Tools/license-header.txt` rather than in this script, so the text has ONE home and a change to
    it is visible as a change to a text file rather than as a change to a script.

    LINE ENDINGS ARE DETECTED, NOT CHOSEN, AND THAT IS NOT A DETAIL. This repository pins LF in
    `.gitattributes` because the engine uses LF throughout, and the first version of this script hard-coded
    a `\r\n` join. PowerShell's `-split "\`n"` also silently failed to split an LF-only file in that
    context, so the whole file arrived as ONE line, no pattern matched the top of it, and the script
    reported that it would PREPEND a notice to files that already had one. Detecting the ending per file
    removes both problems at once.

    IT IS IDEMPOTENT. Running it twice leaves the file identical the second time, which matters because the
    natural failure of a header script is to STACK notices, and a file with three copies of the same notice
    is a file nobody reads. It recognises three starting states:

      - a file with NO notice                      -> the notice is prepended;
      - a file with an OLD copyright line only     -> that line is REPLACED, so the count stays one;
      - a file that already carries this notice    -> left untouched, byte for byte.

    It keeps a leading `#pragma once` on the FIRST line, because a notice above it is legal but reads as if
    it were part of the guard.

    It does not touch generated files: `Intermediate/` holds what UHT emitted and rewriting it would be
    undone by the next build.

.PARAMETER Check
    Report what WOULD change and exit non-zero if anything would, without writing. Use in a build check.

.PARAMETER WhatIf
    Print the plan without writing anything and without failing.

.PARAMETER Root
    The tree to operate on. Defaults to this repository, so the normal use needs no argument. It exists so
    the script can be TRIED ON A COPY before it is trusted with the real tree, which is how it was
    developed after its first version reported that it would overwrite notices it had not recognised.

.EXAMPLE
    pwsh -File Tools\apply-license-header.ps1
    pwsh -File Tools\apply-license-header.ps1 -Check
    pwsh -File Tools\apply-license-header.ps1 -Root $env:TEMP\scratch
#>
[CmdletBinding()]
param(
    [switch] $Check,
    [switch] $WhatIf,
    [string] $Root = ''
)

$ErrorActionPreference = 'Stop'

$Repo = if ($Root) { $Root } else { Split-Path -Parent $PSScriptRoot }
$HeaderFile = Join-Path $PSScriptRoot 'license-header.txt'
# EVERY first-party C++ tree, not only the plugin module. `TestProject/Source` holds the test module, which
# is this project's own code and carries the same notice; the first version of this script missed it because
# it only walked `Source/`.
$SourceRoots = @(
    (Join-Path $Repo 'Source'),
    (Join-Path $Repo 'TestProject\Source')
) | Where-Object { Test-Path $_ }

if (-not (Test-Path $HeaderFile)) { throw "the notice text is not at $HeaderFile" }
if ($SourceRoots.Count -eq 0) { throw "no source tree found under $Repo" }

$LF = [string][char]10
$CR = [string][char]13
$CRLF = $CR + $LF

# The canonical notice. Its OWN line endings are normalised here so that the text file can be saved in
# whatever an editor chose, and the ending that reaches a source file is the source file's own.
$noticeRaw = Get-Content $HeaderFile -Raw
$noticeRaw = $noticeRaw.Replace($CRLF, $LF).Replace($CR, $LF)
$NoticeLines = $noticeRaw.TrimEnd([char]10) -split $LF

# *** THE MARKER IS THE COPYRIGHT LINE EXACTLY AS `license-header.txt` SPELLS IT, NOT THE SPDX LINE. ***
#
# It used to be `SPDX-License-Identifier: MPL-2.0`, on the reasoning that the SPDX line is unique to this
# notice and cannot appear in an old one. THAT IS TRUE, AND IT WAS THE WRONG TEST: THE SPDX LINE IS IDENTICAL
# IN EVERY GENERATION OF THIS NOTICE. So when the holder line changed, the script classified all 193 files as
# `already correct`, reported `0 replaced`, and the tree kept the old spelling while `-Check` went on passing.
#
# A CHECK THAT MATCHES A PART OF WHAT IT VERIFIES CANNOT SEE A CHANGE IN THE REST OF IT - the defect this
# project has recorded eleven times, here inside the licence gate itself. The expected line is read from
# `license-header.txt` so there is one spelling, and this script follows it rather than keeping a second copy.
$ExpectedCopyright = (Get-Content $HeaderFile | Where-Object { $_ -match 'Copyright' } | Select-Object -First 1)
$Marker = if ($ExpectedCopyright) { $ExpectedCopyright.Trim() } else { 'Copyright (c) 2026 FloStyle' }

# Lines this script may REPLACE rather than duplicate, matched only at the TOP of a file.
#
# THE COPYRIGHT PATTERN MUST MATCH ANY COPYRIGHT LINE, AND IT USED TO REQUIRE "All Rights Reserved".
# Measured consequence of that requirement: when the holder line changed to a form that does NOT end with
# that phrase, the script no longer recognised it as replaceable and PREPENDED the notice ABOVE it - so two
# files ended up carrying the copyright line TWICE, once inside the notice and once below it. A pattern that
# matches one spelling of the line it is supposed to consume will duplicate the others, and a duplicated
# notice is exactly the failure this script exists to prevent.
$ReplaceablePatterns = @(
    '^// Copyright\b.*$',
    '^// SPDX-License-Identifier:.*$',
    '^// This Source Code Form is subject to the terms of the Mozilla Public.*$',
    '^// License, v\. 2\.0\..*$',
    '^// file, You can obtain one at.*$',
    '^//\s*$'
)

function Get-FilePlan {
    param([string] $Path)

    $raw = Get-Content $Path -Raw
    if ($null -eq $raw) { $raw = '' }

    # THE ENDING IS READ OFF THE FILE. A file with a CR anywhere is treated as CRLF; otherwise LF, which is
    # what every file in this tree uses.
    $ending = if ($raw.Contains($CR)) { $CRLF } else { $LF }

    $normalised = $raw.Replace($CRLF, $LF).Replace($CR, $LF)
    $lines = $normalised.Split([char]10)

    if ($normalised.Contains($Marker)) {
        return [pscustomobject]@{ Path = $Path; State = 'already'; Text = $raw; Ending = $ending }
    }

    $index = 0

    # `#pragma once` KEEPS THE FIRST LINE.
    $pragma = ''
    if ($lines.Count -gt 0 -and $lines[0].Trim() -match '^#pragma\s+once\s*$') {
        $pragma = $lines[0]
        $index = 1
    }

    # Consume the old notice so it is REPLACED rather than duplicated. Only a contiguous run of matching
    # lines at the top is consumed, so a stray match deeper in the file stops the scan.
    $hadOld = $false
    while ($index -lt $lines.Count) {
        $candidate = $lines[$index]
        $matched = $false
        foreach ($pattern in $ReplaceablePatterns) {
            if ($candidate -match $pattern) { $matched = $true; break }
        }
        if (-not $matched) { break }
        if ($candidate -match '^// Copyright') { $hadOld = $true }
        $index++
    }

    # Skip the blank line that usually separated the old notice from the code.
    while ($index -lt $lines.Count -and $lines[$index].Trim() -eq '') { $index++ }

    $body = ($lines[$index..($lines.Count - 1)] -join $LF)

    $prefix = @()
    if ($pragma) { $prefix += $pragma; $prefix += '' }
    $prefix += $NoticeLines
    $prefix += ''

    $text = (($prefix -join $LF) + $LF + $body)

    $state = if ($hadOld) { 'replaced' } else { 'prepended' }
    return [pscustomobject]@{ Path = $Path; State = $state; Text = $text; Ending = $ending }
}

$files = Get-ChildItem $SourceRoots -Recurse -Include '*.h', '*.cpp', '*.cs' -File |
    Where-Object { $_.FullName -notmatch '[\\/]Intermediate[\\/]' } |
    Sort-Object FullName

$counts = @{ prepended = 0; replaced = 0; already = 0 }
$pending = @()

foreach ($file in $files) {
    $plan = Get-FilePlan -Path $file.FullName
    $counts[$plan.State]++

    if ($plan.State -eq 'already') { continue }
    $pending += $plan

    if ($WhatIf) { Write-Output ("  would {0,-9} {1}" -f $plan.State, $file.Name); continue }
    if ($Check) { Write-Output ("  needs {0,-9} {1}" -f $plan.State, $file.Name); continue }

    # THE FILE'S OWN ENDING IS RESTORED, so this script cannot introduce the CRLF that `.gitattributes`
    # exists to prevent.
    $out = if ($plan.Ending -eq $CRLF) { $plan.Text.Replace($LF, $CRLF) } else { $plan.Text }

    # UTF8 WITHOUT a BOM: a BOM above `#pragma once` is something some tools strip and some compilers warn
    # about, and there is no reason to introduce one.
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($file.FullName, $out, $utf8NoBom)
    Write-Output ("  {0,-9} {1}" -f $plan.State, $file.Name)
}

Write-Output ""
# The `-f` binds INSIDE the parentheses. Written on the following line it parses as a separate statement
# and PowerShell reports a missing `)`, which is what happened on the first run of this script.
Write-Output ("  {0} file(s): {1} prepended, {2} replaced, {3} already correct" -f `
    $files.Count, $counts.prepended, $counts.replaced, $counts.already)

if ($Check -and $pending.Count -gt 0) {
    Write-Output ""
    Write-Output ("RESULT: FAIL ({0} file(s) lack the notice)" -f $pending.Count)
    exit 1
}
if ($Check) {
    Write-Output ""
    Write-Output "RESULT: PASS (every source file carries the notice)"
    exit 0
}
Write-Output ""
Write-Output "RESULT: DONE"
exit 0
