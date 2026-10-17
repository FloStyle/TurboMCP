# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
<#
.SYNOPSIS
    Ownership, artifact provenance, and mount validation for the release tooling.

.DESCRIPTION
    WHY THIS FILE EXISTS. Release finding R10 records that the lifecycle scripts can stop UNRELATED editors:
    `start-editor.ps1 -Stop` stops every process named UnrealEditor, the runner's recovery path does the same,
    and an earlier ownership check does not make a later global stop safe against a process that started in
    between. Release finding R11 records the mirror problem on the other side: `build.ps1` prints that the
    plugin DLL is missing and then still returns PASS with exit zero.

    Three decisions were buried inside process-launching scripts, where the only way to test them was to have
    a real editor running - so they were never tested. They live here instead, and `Tools/test-ownership.py`
    drives them.

      * A LEASE names one owner for the project, the engine, the MCP port and the fixture set. Build and live
        tests take it. A second owner is refused rather than allowed to collide.
      * OWNERSHIP of a process is decided from its COMMAND LINE - this editor, this project - never from its
        name alone, and never from "nobody else claimed it".
      * An ARTIFACT is checked for existence, size and hash before a build reports success.

    NOTHING HERE STARTS OR STOPS A PROCESS BY ITSELF. Every function answers a question; the caller acts.
#>

<#
.SYNOPSIS
    The MCP port every TurboMCP editor on this machine binds.
#>
$script:TurboMCPMcpPort = 8000

function Get-TurboMCPLeasePath {
    <#
    .SYNOPSIS
        Where the lease for one repository lives.
    #>
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $RepoRoot)
    return (Join-Path $RepoRoot '.agent\turbo-mcp-lease.json')
}

function Read-TurboMCPLease {
    <#
    .SYNOPSIS
        The current lease for a repository, or $null.

    .DESCRIPTION
        A lease is read back from disk rather than kept in memory because the tools that need it are separate
        processes: the runner, the editor launcher and the fixture commandlets.
    #>
    [CmdletBinding()]
    param([Parameter(Mandatory)][string] $RepoRoot)

    $path = Get-TurboMCPLeasePath -RepoRoot $RepoRoot
    if (-not (Test-Path $path)) { return $null }
    try {
        $lease = Get-Content $path -Raw -ErrorAction Stop | ConvertFrom-Json
    } catch {
        # An unreadable lease is not an absent lease. Saying "no owner" about a file that exists but cannot
        # be parsed would be the same lie as treating an unknown reference count as zero.
        return [pscustomobject]@{
            Unreadable = $true
            Path = $path
            Reason = "the lease file exists but could not be parsed: $($_.Exception.Message)"
        }
    }
    return $lease
}

function Get-TurboMCPLeaseConflict {
    <#
    .SYNOPSIS
        Whether an existing lease blocks a new owner, and why.

    .DESCRIPTION
        A live lease belongs to this owner only when the owner id AND the project path match. A lease whose
        process is gone is STALE: it is reported as stale and does not block, but it is never silently
        deleted, because the next owner is entitled to know that a previous run did not clean up.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $RepoRoot,
        [Parameter(Mandatory)][string] $OwnerId,
        [string] $Project = ''
    )

    $lease = Read-TurboMCPLease -RepoRoot $RepoRoot
    if ($null -eq $lease) {
        return [pscustomobject]@{ State = 'free'; Blocking = $false; Reason = 'no lease is held'; Lease = $null }
    }
    if ($lease.PSObject.Properties.Name -contains 'Unreadable' -and $lease.Unreadable) {
        return [pscustomobject]@{
            State = 'unreadable'; Blocking = $true; Reason = $lease.Reason; Lease = $lease
        }
    }
    if ($lease.OwnerId -eq $OwnerId) {
        $projectMatches = (-not $Project) -or (-not $lease.Project) -or ($lease.Project -eq $Project)
        if ($projectMatches) {
            return [pscustomobject]@{ State = 'held-by-caller'; Blocking = $false; Reason = 'this owner holds it'; Lease = $lease }
        }
        return [pscustomobject]@{
            State = 'project-mismatch'; Blocking = $true
            Reason = ("this owner id holds a lease for a DIFFERENT project: {0} (asked for {1})" -f $lease.Project, $Project)
            Lease = $lease
        }
    }

    $ownerAlive = $false
    if ($lease.Pid) {
        $ownerAlive = [bool](Get-Process -Id ([int]$lease.Pid) -ErrorAction SilentlyContinue)
    }
    $projectMatches = (-not $Project) -or (-not $lease.Project) -or ($lease.Project -eq $Project)

    if (-not $projectMatches) {
        # A different project on the same MCP port is a conflict whether or not the process is alive, because
        # the two editors would race for the port the moment both are up.
        return [pscustomobject]@{
            State = 'foreign-project'; Blocking = $true
            Reason = ("the lease belongs to project '{0}' owned by '{1}' (pid {2}), not to '{3}'" -f `
                      $lease.Project, $lease.OwnerId, $lease.Pid, $Project)
            Lease = $lease
        }
    }

    # *** A LEASE WHOSE EDITOR IS GONE IS FREE, AND THAT IS THE MEASURED FIX. ***
    #
    # MEASURED: the lease recorded the LAUNCHER's pid, the launcher exited as soon as the editor was up, and every
    # later start was refused with "the owning process is gone" plus advice to release it by hand. The
    # fixture-rebuilding suites call the launcher from Python, so EVERY one of them failed on a lease no live
    # process held.
    #
    # `EditorPid` is written when the editor starts. If that process is gone, the thing the lease protects is not
    # running, so a new start is legitimate. A lease with NO `EditorPid` is an OLD lease from before this field
    # existed: it keeps the strict behaviour, because the conservative direction for an unreadable claim is to
    # refuse rather than to assume.
    if ($lease.PSObject.Properties.Name -contains 'EditorPid' -and $lease.EditorPid) {
        $editorAlive = [bool](Get-Process -Id ([int]$lease.EditorPid) -ErrorAction SilentlyContinue)
        if (-not $editorAlive) {
            return [pscustomobject]@{
                State = 'editor-gone'; Blocking = $false
                Reason = ("the lease names editor pid {0}, which is not running, so the lease is free" -f $lease.EditorPid)
                Lease = $lease
            }
        }

        # *** THE RECLAIM RULE: THIS PROJECT MAY RESTART ITS OWN EDITOR. ***
        #
        # MEASURED, and it is why this row exists: the lease correctly blocked a start while its editor was alive,
        # and that broke the FIXTURE-REBUILDING SUITES, which stop and start the editor as part of their own work.
        # Twice, `test-graph-repoint.py` reported its rebuild SUCCEEDING ("lease held by start-editor:2408 ...
        # RESULT: PASS") and then its NEXT start being refused by the lease that rebuild had just created:
        #
        #   FAIL the lease could not be taken: an editor of this project is already running (pid 22720)
        #
        # The lease was right and the design did not fit the workflow. The property that matters is not "some editor
        # of this project is up" - a suite restarting its own editor is legitimate - it is "a FOREIGN editor, or a
        # foreign checkout, must never be pre-empted". So a lease whose editor belongs to THIS project is taken
        # over, and the takeover is recorded in the new lease's Note rather than done silently.
        #
        # The editor's ownership is decided from ITS OWN COMMAND LINE by `Test-TurboMCPOwnsProcess`, the same
        # function the stop path uses - not from the lease's own claim about itself, which would make this rule a
        # tautology.
        $ownerVerdict = Test-TurboMCPOwnsProcess -ProcessId ([int]$lease.EditorPid) -Project $Project -Engine $Engine
        if ($ownerVerdict.Owned)
        {
            $takeoverReason = "the lease names editor pid " + [string]$lease.EditorPid + ", which is THIS project's editor, so a start of the same project takes the lease over"
            return [pscustomobject]@{ State = 'reclaimable'; Blocking = $false; Reason = $takeoverReason; Lease = $lease }
        }
        $foreignReason = "an editor is running (pid " + [string]$lease.EditorPid + ") that this project does NOT own, held by '" + [string]$lease.OwnerId + "'. A foreign editor is never pre-empted."
        return [pscustomobject]@{ State = 'held-by-editor'; Blocking = $true; Reason = $foreignReason; Lease = $lease }
    }

    $age = ''
    try {
        if ($lease.AcquiredAt) {
            $acquired = [datetime]::Parse($lease.AcquiredAt, $null, [System.Globalization.DateTimeStyles]::RoundtripKind)
            $age = " (acquired {0:N0} minute(s) ago)" -f ((Get-Date).ToUniversalTime() - $acquired.ToUniversalTime()).TotalMinutes
        }
    } catch { $age = '' }
    $liveness = if ($ownerAlive) { 'the owning process is still alive' } else { 'the owning process is gone' }
    $leasePath = Get-TurboMCPLeasePath -RepoRoot $RepoRoot
    $reason = ("another owner holds it: '{0}' (pid {1}){2}; {3}. Release it with -Stop, or delete {4} if the " +
               "run is over.") -f $lease.OwnerId, $lease.Pid, $age, $liveness, $leasePath
    return [pscustomobject]@{
        State = 'held-by-other'; Blocking = $true
        Reason = $reason
        Lease = $lease
    }
}

function New-TurboMCPLease {
    <#
    .SYNOPSIS
        Take the lease, or refuse.

    .DESCRIPTION
        Refusal is the point. Two editors racing for one `.uasset` and one MCP port is the failure this
        repository has recorded more than once, and a warning is not a refusal.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $RepoRoot,
        [Parameter(Mandatory)][string] $OwnerId,
        [string] $Project = '',
        [string] $Engine = '',
        [string] $FixtureDir = '',
        [int] $McpPort = 0
    )

    $conflict = Get-TurboMCPLeaseConflict -RepoRoot $RepoRoot -OwnerId $OwnerId -Project $Project
    if ($conflict.Blocking) {
        return [pscustomobject]@{ Ok = $false; Reason = $conflict.Reason; State = $conflict.State }
    }

    if ($McpPort -eq 0) { $McpPort = $script:TurboMCPMcpPort }
    $takeoverNote = $conflict.Reason
    if ($conflict.State -eq 'reclaimable') {
        # *** THE TAKEOVER IS RECORDED, NOT SILENT. *** A lease that changed hands without saying so would be the
        # same class as an invisible refusal: the next reader could not tell whether it was looking at a fresh
        # claim or one that had been displaced. So the note names what was taken over.
        $takeoverNote = "took over a lease from '" + [string]$conflict.Lease.OwnerId + "' whose editor (pid " + [string]$conflict.Lease.EditorPid + ") is this project's own; a same-project restart is legitimate"
    }
    $lease = [pscustomobject]@{
        SchemaVersion = 1
        OwnerId       = $OwnerId
        Pid           = $PID
        AcquiredAt    = (Get-Date).ToUniversalTime().ToString('o')
        RepoRoot      = $RepoRoot
        Project       = $Project
        Engine        = $Engine
        FixtureDir    = $FixtureDir
        McpPort       = $McpPort
        Note          = $takeoverNote
    }
    $path = Get-TurboMCPLeasePath -RepoRoot $RepoRoot
    $dir = Split-Path $path -Parent
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    $lease | ConvertTo-Json -Depth 4 | Set-Content -Path $path -Encoding utf8
    return [pscustomobject]@{ Ok = $true; Reason = 'lease acquired'; State = 'acquired'; Path = $path; Lease = $lease }
}

function Remove-TurboMCPLease {
    <#
    .SYNOPSIS
        Release the lease, but only the caller's own.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $RepoRoot,
        [Parameter(Mandatory)][string] $OwnerId
    )
    $lease = Read-TurboMCPLease -RepoRoot $RepoRoot
    if ($null -eq $lease) { return $false }
    if ($lease.PSObject.Properties.Name -contains 'Unreadable' -and $lease.Unreadable) { return $false }
    if ($lease.OwnerId -ne $OwnerId) { return $false }
    Remove-Item (Get-TurboMCPLeasePath -RepoRoot $RepoRoot) -Force -ErrorAction SilentlyContinue
    return $true
}

function Get-TurboMCPCommandLineProject {
    <#
    .SYNOPSIS
        The project path named on a process command line, or an empty string when it cannot be established.

    .DESCRIPTION
        *** THE FIRST VERSION OF THIS MEMBER-OWNERSHIP TEST LOOKED ONLY FOR `-Project=`, AND THE EDITOR THIS
        REPOSITORY RUNS DOES NOT USE IT. ***

        MEASURED: the live editor's command line is

            "C:\\ExampleUnrealengine\\Engine\\Binaries\\Win64\\UnrealEditor.exe"
              C:\\Users\\Example\\Desktop\\TurboMCP\\TestProject\\TurboMCPTest.uproject -nullrhi -unattended ...

        The project is a POSITIONAL argument. A recogniser that only understood `-Project=<path>` therefore
        reported this repository's own editor as foreign, which is a false negative in the dangerous direction:
        `-Stop` would have refused to stop the editor it was asked to stop, and the runner would have concluded
        that no editor of its own was running.

        So three forms are accepted, most explicit first:

          * `-Project=<path>` or `-Project <path>`, quoted or not;
          * a QUOTED token ending in `.uproject`;
          * an UNQUOTED token ending in `.uproject`, which cannot contain a space.

        An ambiguous command line returns an empty string, and the caller treats that as NOT OWNED. Being
        unable to establish ownership must never read as ownership.
    #>
    [CmdletBinding()]
    param([Parameter(Mandatory)][AllowEmptyString()][string] $CommandLine)

    if (-not $CommandLine) { return '' }

    $project = ''
    foreach ($pattern in @(
        '-Project\s*=\s*"([^"]+\.uproject)"',
        "-Project\s*=\s*'([^']+\.uproject)'",
        '-Project\s*=\s*([^\s"]+\.uproject)',
        '-Project\s+"([^"]+\.uproject)"',
        '-Project\s+([^\s"]+\.uproject)'
    )) {
        $match = [regex]::Match($CommandLine, $pattern, 'IgnoreCase')
        if ($match.Success) { $project = $match.Groups[1].Value; break }
    }
    if (-not $project) {
        $match = [regex]::Match($CommandLine, '"([^"]+\.uproject)"', 'IgnoreCase')
        if ($match.Success) { $project = $match.Groups[1].Value }
    }
    if (-not $project) {
        $match = [regex]::Match($CommandLine, '(?<![\w\.\\/])([A-Za-z]:[^\s"]*\.uproject)', 'IgnoreCase')
        if ($match.Success) { $project = $match.Groups[1].Value }
    }
    return $project
}

function Test-TurboMCPOwnsProcess {
    <#
    .SYNOPSIS
        Whether one process is THIS repository's editor, decided from its command line.

    .DESCRIPTION
        *** A NAME IS NOT OWNERSHIP, AND THIS IS THE WHOLE OF FINDING R10. *** `Get-Process -Name
        UnrealEditor` answers "some editor is running". The release rule is "kill only owned descendants",
        and the only honest way to know is what the process says it is running: its own executable path plus
        its `-Project=` argument.

        Rules, in order:

          * the process id must exist;
          * an editor binary must be running from the ENGINE this caller expects (when one is given);
          * the `-Project=` argument must resolve to the PROJECT this caller expects (when one is given).

        A process whose command line cannot be read is NOT owned. Being unable to prove ownership is not
        evidence of ownership, so the caller refuses rather than kills. A process with NO command line at all
        is likewise not owned, which is the safe reading.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][int] $ProcessId,
        [string] $Project = '',
        [string] $Engine = '',
        [string] $CommandLine = '',
        [string] $ExecutablePath = ''
    )

    $proc = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
    if (-not $proc) {
        return [pscustomobject]@{ Owned = $false; Reason = "process $ProcessId does not exist" }
    }

    # *** AN EMPTY COMMAND LINE IS A DECISION, NOT A PROMPT TO GO LOOKING, AND IT IS TESTABLE THAT WAY. ***
    # When the caller supplies a command line - including an empty one - that IS the evidence, and the function
    # answers from it. Reading the real process instead would make the caller's input meaningless, and it would
    # make the unreadable case untestable, which is how this branch went unexercised until the E-suite drove it.
    $callerSuppliedCommandLine = $PSBoundParameters.ContainsKey('CommandLine')
    if ($callerSuppliedCommandLine -and -not $CommandLine) {
        return [pscustomobject]@{
            Owned = $false
            Reason = "the command line of process $ProcessId could not be read, and an unprovable owner is not an owner"
        }
    }

    if (-not $CommandLine) {
        try {
            $cim = Get-CimInstance Win32_Process -Filter "ProcessId=$ProcessId" -ErrorAction Stop
            $CommandLine = [string]$cim.CommandLine
        } catch {
            return [pscustomobject]@{
                Owned = $false
                Reason = "the command line of process $ProcessId could not be read, and an unprovable owner is not an owner"
            }
        }
    }
    if (-not $ExecutablePath) {
        try {
            $cim2 = Get-CimInstance Win32_Process -Filter "ProcessId=$ProcessId" -ErrorAction Stop
            $ExecutablePath = [string]$cim2.ExecutablePath
        } catch { $ExecutablePath = [string]$proc.Path }
    }

    if (-not $CommandLine) {
        return [pscustomobject]@{ Owned = $false; Reason = "process $ProcessId has no readable command line" }
    }

    if ($Engine) {
        $expectedEditor = [System.IO.Path]::GetFullPath((Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor.exe'))
        if ($ExecutablePath) {
            # *** SEPARATORS ARE NORMALISED BEFORE THE COMPARISON, BECAUSE THEY DIFFER BY SOURCE. *** MEASURED:
            # `Get-CimInstance` reports `C:/ExampleUnrealengine/Engine/Binaries/Win64/UnrealEditor.exe` with forward
            # slashes while `Join-Path` builds a backslash path, so a raw string comparison reported this
            # engine's own editor as a foreign one. Normalising both sides removes the whole class.
            $actual = ([System.IO.Path]::GetFullPath($ExecutablePath)).Replace('/', '\').TrimEnd('\')
            $expected = $expectedEditor.Replace('/', '\').TrimEnd('\')
            if ($actual -ne $expected) {
                return [pscustomobject]@{
                    Owned = $false
                    Reason = "process $ProcessId runs $actual, not this engine's editor ($expected)"
                }
            }
        }
    }

    if ($Project) {
        $expected = [System.IO.Path]::GetFullPath($Project)
        $actual = Get-TurboMCPCommandLineProject -CommandLine $CommandLine
        if (-not $actual) {
            return [pscustomobject]@{
                Owned = $false
                Reason = ("no .uproject was named on the command line of process {0}, so ownership could not be " +
                          "established - and an unprovable owner is not an owner" -f $ProcessId)
            }
        }
        try { $actualFull = [System.IO.Path]::GetFullPath($actual) } catch { $actualFull = $actual }
        if ($actualFull -ne $expected) {
            return [pscustomobject]@{
                Owned = $false
                Reason = "process $ProcessId owns project '$actualFull', not '$expected'"
            }
        }
        return [pscustomobject]@{ Owned = $true; Reason = "process $ProcessId is this project's editor" }
    }

    # No project given: ownership then rests on the engine binary alone, which the caller asked for by
    # supplying -Engine. Without either, nothing is owned.
    if ($Engine) {
        return [pscustomobject]@{ Owned = $true; Reason = "process $ProcessId runs this engine's editor" }
    }
    return [pscustomobject]@{
        Owned = $false
        Reason = "ownership was not established: neither -Project nor -Engine was given, and a process name is not ownership"
    }
}

function Select-TurboMCPOwnedProcesses {
    <#
    .SYNOPSIS
        Which of a candidate set THIS repository owns.

    .DESCRIPTION
        The caller passes the candidates; this decides. A foreign process is returned in `Foreign` with the
        reason it was not claimed, so the caller can report it instead of silently leaving it out.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][AllowEmptyCollection()][object[]] $Candidates,
        [string] $Project = '',
        [string] $Engine = ''
    )

    $owned = @()
    $foreign = @()
    foreach ($candidate in $Candidates) {
        if ($null -eq $candidate) { continue }
        $id = if ($candidate -is [int]) { $candidate } else { [int]$candidate.Id }
        $verdict = Test-TurboMCPOwnsProcess -ProcessId $id -Project $Project -Engine $Engine
        if ($verdict.Owned) { $owned += $id } else { $foreign += [pscustomobject]@{ Id = $id; Reason = $verdict.Reason } }
    }
    return [pscustomobject]@{ Owned = $owned; Foreign = $foreign }
}

function Get-TurboMCPOwnedDescendants {
    <#
    .SYNOPSIS
        The PIDs that descend from one owned editor, walked while its children still have a parent.

    .DESCRIPTION
        *** THE WALK MUST HAPPEN BEFORE THE PARENT DIES, BECAUSE WINDOWS FORGETS THE LINK. *** A commandlet or
        a trace server can outlive the editor and still hold a plugin DLL or a package handle, so a stop has to
        clear them - but the only safe list is the editor's own descendants, and after the editor exits there is
        nothing left to walk. This function is therefore called BEFORE the stop.

        It walks by parent PID, so it never consults a process NAME. A process with an unreadable record is
        skipped rather than guessed at.
    #>
    [CmdletBinding()]
    param([Parameter(Mandatory)][int] $EditorPid)

    if ($EditorPid -le 0) { return @() }
    try {
        $all = @(Get-CimInstance Win32_Process -ErrorAction Stop)
    } catch {
        return @()
    }
    $byParent = @{}
    foreach ($record in $all) {
        $parent = [int]$record.ParentProcessId
        if (-not $byParent.ContainsKey($parent)) { $byParent[$parent] = @() }
        $byParent[$parent] += [int]$record.ProcessId
    }
    $descendants = New-Object System.Collections.Generic.List[int]
    $queue = New-Object System.Collections.Generic.Queue[int]
    $queue.Enqueue($EditorPid)
    $guard = 0
    while ($queue.Count -gt 0 -and $guard -lt 4096) {
        $guard++
        $current = $queue.Dequeue()
        if (-not $byParent.ContainsKey($current)) { continue }
        foreach ($child in $byParent[$current]) {
            if ($child -eq $EditorPid -or $descendants.Contains($child)) { continue }
            $descendants.Add($child)
            $queue.Enqueue($child)
        }
    }
    return $descendants.ToArray()
}

function Test-TurboMCPAnyOwnedEditorRunning {
    <#
    .SYNOPSIS
        Whether an editor belonging to THIS repository is still alive.

    .DESCRIPTION
        The caller supplies the project and engine, so the answer is about this repository rather than about a
        process name. A script that must not end a foreign editor also must not WAIT for one - which is the
        second way the old name-based check went wrong: it could spin for thirty seconds on someone else's
        editor and then throw.
    #>
    [CmdletBinding()]
    param(
        [string] $Project = '',
        [string] $Engine = ''
    )

    $records = @()
    try { $records = @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" -ErrorAction Stop) } catch { return $false }
    foreach ($record in $records) {
        $verdict = Test-TurboMCPOwnsProcess -ProcessId ([int]$record.ProcessId) -Project $Project -Engine $Engine `
            -CommandLine ([string]$record.CommandLine) -ExecutablePath ([string]$record.ExecutablePath)
        if ($verdict.Owned) { return $true }
    }
    return $false
}

function Get-TurboMCPArtifactVerdict {
    <#
    .SYNOPSIS
        Whether one expected build artifact is really there.

    .DESCRIPTION
        *** FINDING R11: `build.ps1` PRINTED THAT THE DLL WAS MISSING AND STILL RETURNED PASS. *** A build that
        produced nothing is a failed build, and the exit code has to say so or no automated gate can tell the
        difference.

        Existence is not the whole check. A zero-byte file, or a file older than the newest source it is
        supposed to have been built from, is not an artifact - the second case is measured in this
        repository's own history (`-SingleFile` reported success with an object file two hours older than its
        source). Age is reported separately from existence so a legitimate no-op UP-TO-DATE build can still be
        recognised: `NoOpCandidate` means "exists and is older than its inputs", not "failed".
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $Path,
        [string] $NewestInput = '',
        [long] $MinimumBytes = 1
    )

    if (-not (Test-Path $Path)) {
        return [pscustomobject]@{
            Ok = $false; Reason = "the expected artifact does not exist: $Path"; Exists = $false; NoOpCandidate = $false
        }
    }
    $item = Get-Item $Path
    if ($item.Length -lt $MinimumBytes) {
        return [pscustomobject]@{
            Ok = $false
            Reason = ("the expected artifact is {0} bytes, below the required {1}: {2}" -f $item.Length, $MinimumBytes, $Path)
            Exists = $true
            NoOpCandidate = $false
        }
    }
    $hash = (Get-FileHash $Path -Algorithm SHA256).Hash
    $noOp = $false
    if ($NewestInput -and (Test-Path $NewestInput)) {
        $newest = (Get-Item $NewestInput).LastWriteTimeUtc
        if ($item.LastWriteTimeUtc -lt $newest) {
            $noOp = $true
        }
    }
    return [pscustomobject]@{
        Ok = $true
        Reason = "artifact present: $Path ($($item.Length) bytes, sha256 $($hash.Substring(0,16))...)"
        Exists = $true
        NoOpCandidate = $noOp
        Length = $item.Length
        Sha256 = $hash
        LastWriteTimeUtc = $item.LastWriteTimeUtc
    }
}

function New-TurboMCPBuildProvenance {
    <#
    .SYNOPSIS
        The identity of one build, as data.

    .DESCRIPTION
        R11's required correction: "Record source commit, dirty diff hash, engine commit, target, toolchain,
        artifact hash, and loaded module identity. Do not require a timestamp change for a legitimate no-op
        build. Require artifact provenance instead."

        Every field is either measured here or passed in as a measured value. Nothing is inferred.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $RepoRoot,
        [Parameter(Mandatory)][string] $Engine,
        [Parameter(Mandatory)][string] $Target,
        [Parameter(Mandatory)][string] $Configuration,
        [Parameter(Mandatory)][string] $Platform,
        [Parameter(Mandatory)][object] $Artifact,
        [string] $Project = '',
        [int] $ExitCode = 0,
        [string] $Toolchain = '',
        [string] $OutPath = ''
    )

    function GitOutput([string[]]$Arguments) {
        try {
            $out = & git -C $RepoRoot @Arguments 2>&1
            if ($LASTEXITCODE -ne 0) { return '' }
            return ($out -join "`n").Trim()
        } catch { return '' }
    }

    $head = GitOutput @('rev-parse', 'HEAD')
    $diff = GitOutput @('diff')
    $diffHash = ''
    if ($diff) {
        $sha = [System.Security.Cryptography.SHA256]::Create()
        try {
            $bytes = [System.Text.Encoding]::UTF8.GetBytes($diff)
            $diffHash = (($sha.ComputeHash($bytes) | ForEach-Object { $_.ToString('x2') }) -join '')
        } finally { $sha.Dispose() }
    }
    $engineHead = ''
    if (Test-Path (Join-Path $Engine '.git')) {
        $engineHead = (GitOutput @('rev-parse', 'HEAD'))
        if (-not $engineHead) {
            try {
                $engineHead = (& git -C $Engine rev-parse HEAD 2>&1 | Select-Object -First 1)
            } catch { $engineHead = '' }
        }
    }
    if (-not $engineHead) {
        $versionFile = Join-Path $Engine 'Engine\Build\Build.version'
        if (Test-Path $versionFile) {
            $version = Get-Content $versionFile -Raw | ConvertFrom-Json
            $engineHead = "$($version.MajorVersion).$($version.MinorVersion).$($version.PatchVersion)"
        }
    }

    $provenance = [pscustomobject]@{
        SchemaVersion     = 1
        RecordedAt        = (Get-Date).ToUniversalTime().ToString('o')
        RepoRoot          = $RepoRoot
        SourceCommit      = $head
        DirtyDiffSha256   = $diffHash
        DirtyDiffPresent  = [bool]$diff
        Engine            = $Engine
        EngineIdentity    = $engineHead
        Project           = $Project
        Target            = $Target
        Configuration     = $Configuration
        Platform          = $Platform
        Toolchain         = $Toolchain
        BuildExitCode     = $ExitCode
        ArtifactPath      = $Artifact.Reason
        ArtifactExists    = [bool]$Artifact.Exists
        ArtifactBytes     = $Artifact.Length
        ArtifactSha256    = $Artifact.Sha256
        ArtifactTimestamp = if ($Artifact.LastWriteTimeUtc) { $Artifact.LastWriteTimeUtc.ToString('o') } else { '' }
        NoOpCandidate     = [bool]$Artifact.NoOpCandidate
    }
    if ($OutPath) {
        $dir = Split-Path $OutPath -Parent
        if ($dir -and -not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
        $provenance | ConvertTo-Json -Depth 5 | Set-Content -Path $OutPath -Encoding utf8
    }
    return $provenance
}

function Get-TurboMCPExpectedMountPath {
    <#
    .SYNOPSIS
        Where an engine's Marketplace plugin mount should point for a given repository.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $Engine,
        [Parameter(Mandatory)][string] $RepoRoot
    )
    $pluginName = Split-Path $RepoRoot -Leaf
    return (Join-Path $Engine ("Engine\Plugins\Marketplace\$pluginName"))
}

function Get-TurboMCPMountVerdict {
    <#
    .SYNOPSIS
        Whether an existing plugin mount really points at THIS repository.

    .DESCRIPTION
        *** FINDING R09: "Setup accepts an existing junction without checking whether it points to this
        repository. A user can build a different checkout while believing the new one is mounted." ***

        The verdict is decided from the mount's resolved target, and the three outcomes are distinct:

          * `ok` - the target resolves to this repository;
          * `foreign` - it resolves somewhere else. REFUSED, never replaced: R09's own correction says an
            existing mount target must not be replaced automatically, because the other checkout may be
            someone's working tree;
          * `missing` - no mount exists, so creating one is safe.
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)][string] $MountPath,
        [Parameter(Mandatory)][string] $RepoRoot
    )

    if (-not (Test-Path $MountPath)) {
        return [pscustomobject]@{ State = 'missing'; Ok = $false; CanCreate = $true; Target = '' }
    }
    $item = Get-Item $MountPath -Force
    $target = ''
    if ($item.LinkType) {
        $target = @($item.Target)[0]
    } elseif ($item.PSObject.Properties.Name -contains 'Target') {
        $target = [string]$item.Target
    }
    if (-not $target) {
        # A real directory where a link is expected is not a mount this tooling created. Do not touch it.
        return [pscustomobject]@{
            State = 'foreign'; Ok = $false; CanCreate = $false; Target = $MountPath
            Reason = "$MountPath exists as a real directory, not a link, so it was not created by this setup and will not be replaced"
        }
    }
    try { $targetFull = [System.IO.Path]::GetFullPath($target) } catch { $targetFull = $target }
    try { $repoFull = [System.IO.Path]::GetFullPath($RepoRoot) } catch { $repoFull = $RepoRoot }
    if ($targetFull.TrimEnd('\', '/') -eq $repoFull.TrimEnd('\', '/')) {
        return [pscustomobject]@{ State = 'ok'; Ok = $true; CanCreate = $false; Target = $targetFull }
    }
    return [pscustomobject]@{
        State = 'foreign'; Ok = $false; CanCreate = $false; Target = $targetFull
        Reason = "the mount points at '$targetFull', not at this repository ('$repoFull'). It is NOT replaced automatically."
    }
}

function Test-TurboMCPTokenInUrl {
    <#
    .SYNOPSIS
        Whether a line of text instructs a reader to put a credential in a URL.

    .DESCRIPTION
        FINDING R09: setup recommended putting an access token in a Git URL. That exposes the token through
        shell history, configuration, logs and process inspection. This is the detector, so the documentation
        can be checked rather than trusted - and so a regression cannot reintroduce the instruction silently.
    #>
    [CmdletBinding()]
    param([Parameter(Mandatory)][AllowEmptyString()][string] $Text)

    if (-not $Text) { return @() }
    # The URL literals below need no escaping in single-quoted strings, and `$`-free content means no
    # interpolation surprise. The patterns detect a credential IN a URL, which is what must never be
    # recommended back to a user.
    $patterns = @(
        @{ Name = 'userinfo-in-url'; Regex = 'https?://[^/\s"''`]*:[^/\s"''`@]*@' },
        @{ Name = 'token-in-url';   Regex = 'https?://[^/\s"''`]*(?:token|access_token|pat|password|apikey|api_key)=[^\s"''`&]+' },
        @{ Name = 'token-query';    Regex = '\?[^\s"''`]*(?:token|access_token|pat|password|api_key)=' }
    )
    $hits = @()
    $index = 0
    foreach ($line in ($Text -split "`r?`n")) {
        $index++
        foreach ($pattern in $patterns) {
            if ([regex]::IsMatch($line, $pattern.Regex, 'IgnoreCase')) {
                $hits += [pscustomobject]@{ Line = $index; Kind = $pattern.Name; Text = $line.Trim() }
                break
            }
        }
    }
    return $hits
}
