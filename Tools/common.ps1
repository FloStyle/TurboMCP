# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
# Shared repository, project, and explicit engine-path resolution.

function Get-TurboMCPRepoRoot {
    [CmdletBinding()]
    param()
    # `Tools/` sits directly under the root, so the root is this directory's parent. Nothing is
    # configured, so nothing can be misconfigured.
    return (Split-Path -Parent $PSScriptRoot)
}

<#
.SYNOPSIS
    The test project's `.uproject`, derived from the repository root.
#>
function Get-TurboMCPProject {
    [CmdletBinding()]
    param(
        [string] $RepoRoot = (Get-TurboMCPRepoRoot),
        [string] $Name = 'TurboMCPTest'
    )
    $project = Join-Path $RepoRoot "TestProject\$Name.uproject"
    if (-not (Test-Path $project)) {
        throw ("the test project is not at {0} - this repository's own layout is wrong, which is a " -f $project) +
              "different problem from a missing engine"
    }
    return $project
}

<#
.SYNOPSIS
    The engine's own version, read from `Engine/Build/Build.version`, or $null.
#>
function Get-TurboMCPEngineVersion {
    [CmdletBinding()]
    param([string] $Engine)
    $file = Join-Path $Engine 'Engine\Build\Build.version'
    if (-not (Test-Path $file)) { return $null }
    try {
        $v = Get-Content $file -Raw | ConvertFrom-Json
        return ("{0}.{1}.{2}" -f $v.MajorVersion, $v.MinorVersion, $v.PatchVersion)
    } catch { return $null }
}

<#
.SYNOPSIS
    The engine install, preferring the one whose VERSION matches what this project targets.
    THROWS rather than guessing when nothing resolves.
#>
function Resolve-TurboMCPEngine {
    [CmdletBinding()]
    param([string]$Project, [string]$Engine, [string]$ExpectedVersion='', [switch]$AnyVersion)
    $repo = Get-TurboMCPRepoRoot
    if (-not $ExpectedVersion) { $ExpectedVersion = (Get-Content (Join-Path $repo 'engine.lock.json') -Raw | ConvertFrom-Json).version }
    if (-not $Engine) { $Engine = $env:TURBOMCP_ENGINE }
    if (-not $Engine) { $Engine = $env:UE_ROOT }
    if (-not $Engine) {
        $config = Join-Path $repo 'workspace.local.json'
        if (Test-Path $config) { $Engine = (Get-Content $config -Raw | ConvertFrom-Json).engineRoot }
    }
    if (-not $Engine) { throw 'Set TURBOMCP_ENGINE or engineRoot in workspace.local.json. See docs/workspace.md.' }
    if (-not [IO.Path]::IsPathRooted($Engine)) { $Engine = Join-Path $repo $Engine }
    $Engine = [IO.Path]::GetFullPath($Engine)
    $found = Get-TurboMCPEngineVersion -Engine $Engine
    if (-not $found) { throw "No Engine/Build/Build.version at $Engine" }
    if (-not $AnyVersion -and $found -ne $ExpectedVersion) { throw "Engine $found differs from pinned version $ExpectedVersion" }
    return $Engine
}
