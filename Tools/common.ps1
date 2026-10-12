# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
<#
.SYNOPSIS
    Finds the repository root, the test project and the engine install, WITHOUT A PATH OF ANYONE'S MACHINE.

.DESCRIPTION
    *** THIS FILE EXISTS BECAUSE THE SCRIPTS IN `Tools/` USED TO HARD-CODE AN ABSOLUTE PATH. ***

    `make-kit-assets.ps1`, `make-material-asset.ps1`, `make-acceptance-asset.ps1` and `make-test-map.ps1`
    each defaulted `$Project` to one developer's absolute `.uproject`, and `start-editor.ps1` and
    `run-suites.ps1` each defaulted the engine to `C:\UnrealEngine`. THAT WORKED ON EXACTLY ONE MACHINE:
    a clone anywhere else failed on the first invocation with "the project is not at ...", which is a
    message about the author's disk rather than about the repository.

    SO EVERY DEFAULT IS NOW DERIVED, IN THE ORDER A USER WOULD EXPECT:

        the repository root   the parent of this file's directory - no configuration needed
        the test project      <repo>/TestProject/TurboMCPTest.uproject
        the engine            every candidate below is COLLECTED, then the one whose
                              Engine/Build/Build.version MATCHES the version this project targets is
                              preferred, and only then is the first working one taken

    *** AND THE VERSION MATTER IS NOT A NICETY: THIS MACHINE HAS UnrealEngine 5.8.3 AT C:\\UnrealEngine
    AND AN EPIC LAUNCHER INSTALL OF 5.6, AND THE FIRST VERSION OF THIS FILE RETURNED 5.6. *** It resolved
    honestly - 5.6 has an editor and 5.8.3 was only in the conventional-location fallback - AND IT WAS
    STILL THE WRONG ENGINE, which would have produced a build failure inside the engine's own headers and
    read as a defect in this plugin. SO THE RESOLVER ASKS THE ENGINE ITS OWN VERSION rather than trusting
    where it was found, and a candidate that does not match is now reported as a near miss rather than
    silently used.

    AND WHEN NOTHING RESOLVES, `Resolve-TurboMCPEngine` THROWS WITH THE LIST OF WHAT IT TRIED rather
    than guessing. A path that silently falls back to the wrong tree is the same class of defect this
    project spends its time finding: an absent measurement that reads as a present one.

.EXAMPLE
    . "$PSScriptRoot\common.ps1"
    $repo    = Get-TurboMCPRepoRoot
    $project = Get-TurboMCPProject
    $engine  = Resolve-TurboMCPEngine -Project $project

.NOTES
    Dot-source this file; it defines functions and no state.
#>

# ---------------------------------------------------------------------------------------------
# *** NO `Set-StrictMode` HERE, AND THAT IS DELIBERATE - IT COST TWELVE FAILING SUITES. ***
#
# The first version of this file set `Set-StrictMode -Version Latest`. Dot-sourcing does not contain
# that: IT APPLIES TO THE CALLER'S SCOPE AND EVERY SCOPE THE CALLER CREATES. So a script that
# legitimately read `$script:Something` set in one place and read in another began throwing
# "Impossible de récupérer la variable « $script:... » parce qu'elle n'a pas été définie", WHICH IS A
# MESSAGE ABOUT STRICT MODE AND NOT ABOUT THE VARIABLE.
#
# MEASURED: a full run went from `passed 58, failed 0` to `passed 46, failed 12`, and the twelve were
# fixture rebuilds - `make-acceptance-asset.ps1` was the one that threw, and every suite that needed a
# fixture it builds then failed for a reason that pointed at the SUITE rather than at this file.
#
# A helper that changes the caller's global behaviour is not a helper. Each script keeps whatever strict
# mode it chooses for itself.
# ---------------------------------------------------------------------------------------------

<#
.SYNOPSIS
    The repository root, derived from this file's own location.
#>
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
    param(
        [string] $Project,
        [string] $Engine,
        # THE VERSION THIS PROJECT IS BUILT AGAINST. It is a parameter rather than a constant so a fork on
        # another engine changes one default instead of editing the resolver.
        [string] $ExpectedVersion = '5.8.3',
        [switch] $AnyVersion
    )

    $tried    = New-Object System.Collections.Generic.List[string]
    $nearMiss = New-Object System.Collections.Generic.List[string]
    $candidates = New-Object System.Collections.Generic.List[string]

    # *** AN EXPLICIT ARGUMENT WINS OUTRIGHT, VERSION OR NOT. *** A caller who names a tree means it, and
    # second-guessing them would make the override useless for testing against another engine.
    if ($Engine) {
        $tried.Add("the -Engine argument: $Engine")
        if (Test-Path (Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor.exe')) {
            $found = Get-TurboMCPEngineVersion -Engine $Engine
            if (-not $AnyVersion -and $found -and $found -ne $ExpectedVersion) {
                Write-Warning ("-Engine $Engine is version $found, not $ExpectedVersion. Using it anyway, " +
                               "because an explicit argument is never overridden.")
            }
            return $Engine
        }
    }

    foreach ($var in @('TURBOMCP_ENGINE', 'UE_ROOT')) {
        $value = [Environment]::GetEnvironmentVariable($var)
        if ($value) {
            $tried.Add("the $var environment variable: $value")
            if (Test-Path (Join-Path $value 'Engine\Binaries\Win64\UnrealEditor.exe')) { $candidates.Add($value) }
        }
    }

    # `EngineAssociation` in the .uproject. It is USUALLY EMPTY in this repository, which was measured, so
    # this route is kept for the forks that set it and not relied on.
    if ($Project -and (Test-Path $Project)) {
        try {
            $assoc = (Get-Content $Project -Raw | ConvertFrom-Json).EngineAssociation
            if ($assoc) {
                foreach ($candidate in @(
                    (Join-Path 'C:\Program Files\Epic Games' "UE_$assoc"),
                    $assoc)) {
                    if ($candidate -and (Test-Path (Join-Path $candidate 'Engine\Binaries\Win64\UnrealEditor.exe'))) {
                        $candidates.Add($candidate)
                    }
                }
                if ($candidates.Count -eq 0) { $tried.Add("EngineAssociation '$assoc' names no directory with an editor") }
            } else {
                $tried.Add("EngineAssociation in $Project is EMPTY, which is the case in this repository")
            }
        } catch {
            $tried.Add("EngineAssociation in $Project could not be read as JSON")
        }
    }

    # Every Epic launcher install the registry knows about, ALL of them rather than the first.
    try {
        $reg = Get-ItemProperty 'HKLM:\SOFTWARE\EpicGames\Unreal Engine\*' -ErrorAction Stop
        foreach ($entry in $reg) {
            $installed = $entry.InstalledDirectory
            if ($installed -and (Test-Path (Join-Path $installed 'Engine\Binaries\Win64\UnrealEditor.exe'))) {
                $candidates.Add($installed)
            }
        }
    } catch {
        $tried.Add('the Epic launcher registry key HKLM:\SOFTWARE\EpicGames\Unreal Engine - not present')
    }

    foreach ($conventional in @('C:\UnrealEngine', 'D:\UnrealEngine')) {
        # *** THE DRIVE IS CHECKED BEFORE THE PATH, BECAUSE `Join-Path D:\...` ON A MACHINE WITH NO D:
        # THROWS "Cannot find drive" AND THAT IS NOISE A USER SHOULD NOT HAVE TO READ. *** A missing drive
        # is not a finding; it is a machine that does not have one.
        $drive = [System.IO.Path]::GetPathRoot($conventional)
        if (-not $drive -or -not (Test-Path $drive)) { continue }
        if (Test-Path (Join-Path $conventional 'Engine\Binaries\Win64\UnrealEditor.exe')) { $candidates.Add($conventional) }
    }

    if ($candidates.Count -eq 0) {
        $tried.Add('no candidate directory contained Engine\Binaries\Win64\UnrealEditor.exe')
        throw ("no Unreal Engine install could be found. Tried, in order:`n  - " + ($tried -join "`n  - ") +
               "`n`nPass -Engine <path>, or set TURBOMCP_ENGINE, or see Tools/setup-engine.ps1.")
    }

    # *** THE VERSION DECIDES, NOT THE ORDER OF DISCOVERY. *** The first version of this file returned the
    # Epic 5.6 install on a machine where C:\UnrealEngine is 5.8.3, because 5.6 came out of the registry
    # first and 5.8.3 was only in the fallback list. It resolved honestly and was still wrong.
    if (-not $AnyVersion) {
        foreach ($candidate in $candidates) {
            $found = Get-TurboMCPEngineVersion -Engine $candidate
            if ($found -eq $ExpectedVersion) { return $candidate }
        }
        foreach ($candidate in $candidates) {
            $found = Get-TurboMCPEngineVersion -Engine $candidate
            $nearMiss.Add("$candidate (version $(if ($found) { $found } else { 'unknown' }))")
        }
        throw ("no installed engine is version $ExpectedVersion, which is what this project targets. " +
               "Found:`n  - " + ($nearMiss -join "`n  - ") +
               "`n`nPass -Engine <path> to use one of them anyway, or -AnyVersion to accept the first that works.")
    }

    return $candidates[0]
}
