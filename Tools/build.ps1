# Copyright (c) 2026 Florian ESCHALIER (FloStyle)
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
    [switch] $NoStop
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'common.ps1')

$repo = Get-TurboMCPRepoRoot
$project = Get-TurboMCPProject -RepoRoot $repo
if (-not $Engine) { $Engine = Resolve-TurboMCPEngine -Project $project }

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

# `-WaitMutex` because a stale UBT mutex turns a build into a silent wait that looks like a hang.
& cmd.exe /c "`"$buildBat`" $Target $Platform $Configuration -Project=`"$project`" -WaitMutex -MaxParallelActions=6"
$exit = $LASTEXITCODE

Write-Output ""
if ($exit -eq 0) {
    # *** `Result: Succeeded` ALONE IS NOT EVIDENCE THE BINARY CHANGED, WHICH THIS PROJECT MEASURED: a
    # `-SingleFile` build once reported success while the object file was TWO HOURS OLDER than its source.
    # So the DLL's timestamp is printed beside the verdict, and a reader can see whether it moved. ***
    $dll = Join-Path $repo 'Binaries\Win64\UnrealEditor-TurboMCP.dll'
    if (Test-Path $dll) {
        Write-Output ("  plugin DLL: {0}" -f (Get-Item $dll).LastWriteTime.ToString('yyyy-MM-dd HH:mm:ss'))
    } else {
        Write-Output "  plugin DLL: NOT FOUND, so nothing was produced even though the build succeeded"
    }
    Write-Output "  RESULT: PASS (the build succeeded)"
    exit 0
}

Write-Output "  RESULT: FAIL (Build.bat exited $exit)"
exit $exit
