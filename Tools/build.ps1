# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
<#
.SYNOPSIS
    Builds the plugin's editor target, with NO PATH OF ANYONE'S MACHINE in it.

.DESCRIPTION
    The README used to tell a reader to run

        & cmd.exe /c "cd /d C:\UnrealEngine && Engine\Build\BatchFiles\Build.bat ..."

    WHICH IS ONE MACHINE'S LAYOUT WRITTEN AS AN INSTRUCTION. This script does the same thing and finds the
    engine the way the rest of `Tools/` does: the `-Engine` argument, then `TURBOMCP_ENGINE`, then
    `UE_ROOT`, then the Epic launcher registry, then the conventional source-build locations - AND IT
    PREFERS THE ONE WHOSE VERSION MATCHES `5.8.3`, because a machine can have several engines and a build
    against the wrong one fails inside the engine's own headers.

    IT ALSO STOPS THE EDITOR FIRST, because the running editor holds `UnrealEditor-TurboMCP.dll` open and
    the link step fails with `LNK1104` - a message about a file lock that reads like a build error.

.PARAMETER Target
    The build target. `TurboMCPTestEditor` is the one the test project uses.

.PARAMETER Configuration
    `Development` or `DebugGame`. Development is what every measurement in this repository was taken with.

.PARAMETER Engine
    The engine root. Omit to have it resolved, and prefer a matching version.

.PARAMETER NoStop
    Do not stop a running editor first.

.EXAMPLE
    pwsh -File Tools\build.ps1
    pwsh -File Tools\build.ps1 -Engine 'D:\UE5.8'
#>
[CmdletBinding()]
param(
    [string] $Target = 'TurboMCPTestEditor',
    [string] $Configuration = 'Development',
    [string] $Platform = 'Win64',
    # EMPTY ON PURPOSE: resolved below by Tools/common.ps1, which prefers a version-matching engine.
    [string] $Engine = '',
    [switch] $NoStop,
    # *** THE ARTIFACT THE BUILD MUST PRODUCE. ASSERTED, NOT DESCRIBED. ***
    #
    # Finding R11: build.ps1 printed "plugin DLL: NOT FOUND, so nothing was produced even though the build
    # succeeded" and then returned PASS with exit zero. A build that produced nothing is a failed build, and an
    # automated gate cannot tell the difference from prose. The path is a parameter so the assertion can be
    # shown FAILING against a fixture - a check whose failure path has never been exercised is not a check.
    [string] $ExpectArtifact = '',
    # Where the build's provenance record is written. R11's required correction names the fields.
    [string] $ProvenancePath = ''
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'common.ps1')
. (Join-Path $PSScriptRoot 'ownership.ps1')

$repo = Get-TurboMCPRepoRoot
$project = Get-TurboMCPProject -RepoRoot $repo
$Engine = Resolve-TurboMCPEngine -Project $project -Engine $Engine
if (-not $ExpectArtifact) { $ExpectArtifact = Join-Path $repo 'TestProject\Binaries\Win64\UnrealEditor-TurboMCP.dll' }
if (-not $ProvenancePath) { $ProvenancePath = Join-Path $repo '.agent\build-provenance.json' }

$buildBat = Join-Path $Engine 'Engine\Build\BatchFiles\Build.bat'
if (-not (Test-Path $buildBat)) { throw "no Build.bat under $Engine - that directory is not an engine root" }

$version = Get-TurboMCPEngineVersion -Engine $Engine
Write-Output "engine  : $Engine (version $(if ($version) { $version } else { 'unknown' }))"
Write-Output "project : $project"
Write-Output "target  : $Target $Platform $Configuration"

# THE EDITOR HOLDS THE DLL. `LNK1104: cannot open file 'UnrealEditor-TurboMCP.dll'` is a lock, not a
# build error, and every reader of this repository has hit it at least once.
if (-not $NoStop) {
    $startEditor = Join-Path $PSScriptRoot 'start-editor.ps1'
    if (Test-Path $startEditor) {
        & pwsh -NoProfile -File $startEditor -Stop 2>&1 | Out-Null
        Start-Sleep -Seconds 2
    }
}

# *** MSBUILD'S WORKER NODES ARE TURNED OFF, AND THAT IS A MEASURED FIX RATHER THAN A PREFERENCE. ***
#
# WITH NODE REUSE ON, A BUILD FAILED TWICE IN A ROW, BOTH TIMES IN `RunUBT` AND BEFORE ANY OF THIS PLUGIN'S
# CODE WAS COMPILED:
#
#     UnrealEngine.CSharp.targets(65,5): error MSB3491: ... fichier
#     " %TEMP%\DotnetDepends-<N>\EpicGames.Build.dll.dep.csv ". Access to the path
#     '...\DotnetDepends-<N>\<random>.tmp' is denied.
#
# IT IS NOT A PERMISSION ON THAT DIRECTORY, AND THAT WAS MEASURED RATHER THAN ASSUMED: a probe file AND a fresh
# subdirectory were both created in `%TEMP%` in that session, a `cmd.exe` child wrote there too, and no `DENY`
# entry exists anywhere on the chain. WHAT WAS DIFFERENT WAS TWENTY `dotnet` WORKER NODES LEFT ALIVE BY THE
# PREVIOUS BUILD, WHICH MSBUILD REUSED: the node that CREATES `DotnetDepends-<N>` is not always the node that
# WRITES its `.dep.csv`, and the write then lands in a directory the writing node cannot use.
#
# WITH REUSE OFF, A FRESH NODE SET MADE THE SAME BUILD PASS: `Result: Succeeded`, 43.51 seconds, and a new
# `UnrealEditor-TurboMCP.dll` timestamp. The cost is a few seconds of node start-up per build, which is nothing
# beside a build that cannot run at all.
$env:MSBUILDDISABLENODEREUSE = '1'

# `-WaitMutex` because a stale UBT mutex turns a build into a silent wait that looks like a hang.
#
# `Tee-Object` KEEPS THE BUILD OUTPUT VISIBLE AND COLLECTS IT. The console sees every line as it arrives, and
# the same lines are captured so the toolchain can be READ FROM THE BUILD rather than assumed. A silent
# capture would hide a long build's progress, which is the failure mode the comment above records.
$script:BuildOutput = New-Object System.Collections.Generic.List[string]
& cmd.exe /c "`"$buildBat`" $Target $Platform $Configuration -Project=`"$project`" -WaitMutex -MaxParallelActions=6" 2>&1 |
    ForEach-Object { $script:BuildOutput.Add([string]$_) ; Write-Output $_ }
$exit = $LASTEXITCODE
$buildOutput = @($script:BuildOutput)

Write-Output ""
if ($exit -eq 0) {
    # *** `Result: Succeeded` ALONE IS NOT EVIDENCE THE BINARY CHANGED, WHICH THIS PROJECT MEASURED: a
    # `-SingleFile` build once reported success while the object file was TWO HOURS OLDER than its source.
    # So the artifact is checked for existence and size, hashed, and compared with its newest input - and a
    # MISSING artifact now FAILS the build instead of being mentioned and then passed over. ***
    $newestInput = ''
    $sourceDir = Join-Path $repo 'Plugins\TurboMCP\Source'
    if (Test-Path $sourceDir) {
        $newest = Get-ChildItem $sourceDir -Recurse -File -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
        if ($newest) { $newestInput = $newest.FullName }
    }
    $artifact = Get-TurboMCPArtifactVerdict -Path $ExpectArtifact -NewestInput $newestInput

    # The toolchain, recorded from the build's own output rather than assumed.
    $toolchain = ''
    $toolchainMatch = [regex]::Match(($buildOutput -join "`n"), 'Using Visual Studio ([^\r\n]+)')
    if ($toolchainMatch.Success) { $toolchain = $toolchainMatch.Groups[1].Value.Trim() }

    $provenance = New-TurboMCPBuildProvenance -RepoRoot $repo -Engine $Engine -Target $Target `
        -Configuration $Configuration -Platform $Platform -Artifact $artifact -Project $project `
        -ExitCode $exit -Toolchain $toolchain -OutPath $ProvenancePath

    if ($artifact.Exists) {
        Write-Output ("  plugin DLL: {0} ({1} bytes, {2})" -f `
            $ExpectArtifact, $artifact.Length, $artifact.LastWriteTimeUtc.ToString('yyyy-MM-dd HH:mm:ss'))
        if ($artifact.NoOpCandidate) {
            # UP-TO-DATE IS NOT A FAILURE. R11's correction is explicit that a legitimate no-op build must not
            # be rejected; what must not happen is reporting a moved timestamp it did not have.
            Write-Output "  NOTE: the artifact is older than its newest input, so this was an UP-TO-DATE build. That is not a defect."
        }
    }
    Write-Output ("  provenance: {0}" -f $ProvenancePath)

    if (-not $artifact.Ok) {
        Write-Output ("  RESULT: FAIL (the build succeeded but {0})" -f $artifact.Reason)
        exit 1
    }
    Write-Output ("  RESULT: PASS ({0})" -f $artifact.Reason)
    exit 0
}

Write-Output "  RESULT: FAIL (Build.bat exited $exit)"
exit $exit
