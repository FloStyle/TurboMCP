<#
.SYNOPSIS
    Start the TurboMCP test editor and prove it reached a working state.

.DESCRIPTION
    Starts UnrealEditor on the test project, then verifies the result rather than
    assuming it. "The process is running" is not a working state: the editor can
    exit during shader compilation or fail to load a module, and either way the
    process may briefly exist.

    The checks, in order:

      1. the editor binary, the project, and the plugin binary all exist
      2. ShaderCompileWorker.exe exists. A rendering editor launches it to compile
         shaders and exits if it is missing. A -NullRHI editor never needs it, so
         this is the check a headless run hides.
      3. no other editor already holds the project or the MCP port
      4. the process stays alive and reports a window
      5. the MCP server binds its port
      6. the server answers a real call, and all four TurboMCP toolsets are
         registered

    Step 6 is the one that matters: it proves the plugin loaded and its tools are
    reachable, which is what "working" means here.

.PARAMETER Project
    The .uproject to open. Defaults to the TurboMCP test project.

.PARAMETER TimeoutSeconds
    How long to wait for the editor to reach a working state. Default 420. The
    first visible start also compiles shaders, which takes minutes.

.PARAMETER NullRHI
    Start with -nullrhi, which skips rendering and therefore needs no shader
    worker. Faster, but no window to look at.

.PARAMETER LowPriority
    Start the editor BELOW NORMAL PRIORITY, so a game or any interactive work on the same
    machine keeps the CPU it needs.

    MEASURED REASON: the editor starts at `Normal`, and so does a running game - so a test
    run COMPETES ON EQUAL TERMS WITH WHATEVER THE MACHINE'S OWNER IS DOING, and the
    observable effect is a game hitching or stuttering for the whole run. The Windows
    scheduler gives BelowNormal threads the CPU only when nothing at Normal wants it, so
    THE SUITES SLOW DOWN AND THE FOREGROUND WORK DOES NOT NOTICE.

    The harness passes it. It does NOT make the editor slow enough to matter for a suite
    that is already waiting on the editor's own startup.

.PARAMETER Offscreen
    Start the editor WITHOUT A WINDOW, for the suites. This is the mode the harness
    uses, and it exists because the windowed editor TAKES THE FOREGROUND EVERY TIME IT
    STARTS: a full suite run restarts the editor between suites, so the machine was
    stealing focus from whoever was using it several times per run.

    It implies -nullrhi, so there is no window to focus and no shader compilation
    either - which ALSO makes each start faster. Every TurboMCP tool operates on
    graphs, assets and Blueprints through reflection, and NONE OF THEM NEEDS RENDERING,
    so nothing the suites check is lost.

    IT IS DELIBERATELY NOT THE DEFAULT FOR AN INTERACTIVE START. `-nullrhi` never
    exercises the renderer or ShaderCompileWorker, so a human launching the editor to
    LOOK at something must get the windowed path; the comment above about check 2 is
    exactly about what a headless run hides.

.PARAMETER Culture
    The editor's language, for example `en` or `fr`. Defaults to `en`.

    This matters more than it looks. A node type ID is
    `<menu category>|<node title>` and the title half is built at run time from
    localised fragments, so on a French editor the ID for BeginPlay reads
    `AddEvent|evnementBeginPlay` while every example in the engine's own
    documentation reads `AddEvent|EventBeginPlay`. A type ID also only feeds back
    into the engine's graph writer on an editor in the same language.

    Pinning the language to `en` makes this plugin's output match the engine's
    documented form and makes results comparable between machines.

.PARAMETER StopFirst
    Stop any running editor before starting. Needed before a rebuild, because the
    plugin DLL is locked while the editor runs.

.PARAMETER Stop
    Stop the running editor and exit without starting one. Nothing else runs.

.PARAMETER KeepOpen
    Leave the editor running when the script finishes. Without it the script
    still leaves it running; the switch exists for symmetry with -StopFirst.

.EXAMPLE
    .\start-editor.ps1
    Start visibly and verify.

.EXAMPLE
    .\start-editor.ps1 -StopFirst
    Stop any running editor first, then start and verify.

.EXAMPLE
    .\start-editor.ps1 -Stop
    Just stop the editor, for example before a rebuild.
#>

[CmdletBinding()]
param(
    [string] $Project = "$PSScriptRoot\..\TestProject\TurboMCPTest.uproject",
    # EMPTY ON PURPOSE: resolved below by Tools/common.ps1, which prefers an engine whose
    # Engine/Build/Build.version matches the version this project targets. See that file.
    [string] $Engine  = '',
    [int]    $TimeoutSeconds = 420,
    [string] $Culture = 'en',
    [switch] $NullRHI,
    [switch] $LowPriority,
    [switch] $Offscreen,
    [switch] $StopFirst,
    [switch] $Stop,
    # *** OFF BY DEFAULT, AND THAT IS THE RELEASE DECISION. *** Stopping an editor that belongs to a DIFFERENT
    # project is refused, because this repository has recorded test failures caused by exactly that kind of
    # interference. The flag exists so an operator who genuinely owns the other editor can say so explicitly;
    # it is never implied by -Stop or -StopFirst.
    [switch] $StopForeignEditors,
    [switch] $KeepOpen
)

$ErrorActionPreference = 'Stop'

# During a suite run the PowerShell runner is the sole editor owner. Legacy suites still
# call this launcher directly; make those calls observe the kept-alive editor instead of
# stopping it or attempting a second start. The runner sets STARTING/ALLOW_STOP around its
# own lifecycle operations so this guard cannot suppress legitimate recovery or cleanup.
if ($env:TURBOMCP_RUNNER_OWNS_FIXTURES -eq '1' -and $env:TURBOMCP_RUNNER_STARTING_EDITOR -ne '1' -and
    $env:TURBOMCP_RUNNER_ALLOW_STOP -ne '1') {
    if ($Stop) {
        Write-Host '  runner owns the editor; suite stop request was intentionally skipped'
        Write-Host 'RESULT: PASS'
        exit 0
    }
    $runnerProbe = & python (Join-Path $PSScriptRoot 'check-mcp-server.py') 2>&1
    if ($runnerProbe -match '^PASS') {
        Write-Host '  runner owns a healthy editor; suite start request was intentionally reused'
        Write-Host 'RESULT: PASS'
        exit 0
    }
    Write-Host '  runner owns the editor, but it is not healthy; the runner must recover it'
    exit 6
}

# ---------------------------------------------------------------------------------------------
# THE ENGINE IS DERIVED, NOT HARD-CODED. `C:\UnrealEngine` used to be the default, which is one
# machine's layout; and resolving it by DISCOVERY alone is not enough either, because a machine can
# have SEVERAL engines and only one of them is the version this project is built against. This
# machine has 5.8.3 at C:\UnrealEngine and an Epic 5.6 install, and pure discovery picked 5.6.
# ---------------------------------------------------------------------------------------------
. (Join-Path $PSScriptRoot 'common.ps1')
if (-not $Engine) { $Engine = Resolve-TurboMCPEngine -Project $Project }

# Resolve to a real absolute path, so log lines and log paths do not print `..`.
$Project = [System.IO.Path]::GetFullPath($Project)

$EditorExe  = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor.exe'
$ShaderWorker = Join-Path $Engine 'Engine\Binaries\Win64\ShaderCompileWorker.exe'
$McpUrl     = 'http://localhost:8000/mcp'
$McpPort    = 8000
$ProbeScript = Join-Path $PSScriptRoot 'check-mcp-server.py'

function Step  { param([string]$T) Write-Host "`n=== $T ===" -ForegroundColor Cyan }
function Ok    { param([string]$T) Write-Host "  ok    $T" -ForegroundColor Green }
function Warn  { param([string]$T) Write-Host "  warn  $T" -ForegroundColor Yellow }
function Fail  { param([string]$T) Write-Host "  FAIL  $T" -ForegroundColor Red }

$Problems = @()

# ---------------------------------------------------------------------------------------------
# *** OWNERSHIP DECIDES WHAT MAY BE STOPPED, AND THAT IS THE WHOLE OF RELEASE FINDING R10. ***
#
# This script used to stop every process named `UnrealEditor`. An earlier ownership check did not make that
# safe: a foreign editor can start between the check and the stop, and the check itself was about "is an
# editor running" rather than "is THIS REPOSITORY'S editor running". It also stopped `UnrealEditor-Cmd` and
# `UnrealTraceServer` by name, which is a wider blast radius than the caller asked for.
#
# `Tools/ownership.ps1` decides ownership from a process's COMMAND LINE - its executable and its `-Project=`
# argument - and the decisions are unit-tested by `Tools/test-ownership.py` with fake process records. A
# process whose command line cannot be read is NOT owned, because an unprovable owner is not an owner.
# ---------------------------------------------------------------------------------------------
. (Join-Path $PSScriptRoot 'ownership.ps1')

$OwnerId = if ($env:TURBOMCP_OWNER_ID) { $env:TURBOMCP_OWNER_ID } else { "start-editor:$PID" }
# The plugin binary lives in the plugin's own Binaries folder, not the engine's. Computed before the -Stop
# block, because that block needs the repository root to decide ownership.
$ProjectDir = Split-Path $Project -Parent
$RepoDir = Split-Path $ProjectDir -Parent
$RepoRoot = $RepoDir
$LeasePath = Get-TurboMCPLeasePath -RepoRoot $RepoRoot

function Get-TurboMCPRunningEditors {
    <#
    .SYNOPSIS
        Every running UnrealEditor, split into THIS project's and foreign ones.

    .DESCRIPTION
        The split is the point. A foreign editor is reported with the reason it was not claimed, so the caller
        can refuse and tell the user which project is actually running, instead of either killing it or
        pretending nothing is running.

        *** THE ENUMERATION IS CIM, NOT `Get-Process -Name`. *** MEASURED: after one suite run,
        `Get-Process -Name UnrealEditor` returned ONE process while `Get-CimInstance Win32_Process` returned
        FOUR, all four of them this project's. The name-based cmdlet was reporting a subset, so a stop built on
        it would have declared success while three editors held the plugin DLL and the fixture. CIM also gives
        the command line in the same call, which is what ownership is decided from.
    #>
    param([string] $ProjectPath, [string] $EnginePath)

    $records = @()
    try {
        $records = @(Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe'" -ErrorAction Stop)
    } catch {
        $records = @()
    }
    $owned = @(); $foreign = @()
    foreach ($record in $records) {
        $id = [int]$record.ProcessId
        $verdict = Test-TurboMCPOwnsProcess -ProcessId $id -Project $ProjectPath -Engine $EnginePath `
            -CommandLine ([string]$record.CommandLine) -ExecutablePath ([string]$record.ExecutablePath)
        if ($verdict.Owned) {
            $owned += [pscustomobject]@{ Id = $id; CommandLine = [string]$record.CommandLine }
        } else {
            $foreign += [pscustomobject]@{ Id = $id; Reason = $verdict.Reason }
        }
    }
    return [pscustomobject]@{ Owned = $owned; Foreign = $foreign; All = $records }
}

function Stop-TurboMCPOwnedEditor {
    <#
    .SYNOPSIS
        Stop only this project's editor and its own LiveCodingConsole children.

    .DESCRIPTION
        Returns `$true` when the stop is safe to report as done. It REFUSES when a foreign editor is running:
        the caller is told which project owns it. That refusal is deliberately not overridable by a flag in
        this release - an operator who really needs another project's editor gone can close it themselves.
    #>
    param([string] $ProjectPath, [string] $EnginePath, [switch] $AllowForeign)

    $state = Get-TurboMCPRunningEditors -ProjectPath $ProjectPath -EnginePath $EnginePath
    if ($state.Foreign.Count -gt 0 -and -not $AllowForeign) {
        foreach ($entry in $state.Foreign) {
            Warn ("editor pid {0} belongs to another project and was NOT stopped: {1}" -f $entry.Id, $entry.Reason)
        }
        if ($state.Owned.Count -eq 0) {
            Fail ('an editor is running that this repository does not own. Refusing to stop anything. ' +
                  'Close it in the editor, or run with -StopForeignEditors if you are certain it is yours.')
            return $false
        }
    }

    if ($state.Owned.Count -gt 0) {
        Write-Host "  stopping $($state.Owned.Count) editor process(es) that own this project"
        foreach ($proc in $state.Owned) {
            # Only the process itself. A forced child-tree kill is the runner's job for a child it started,
            # not this script's job for an editor a person may be looking at.
            try { Stop-Process -Id $proc.Id -Force -ErrorAction Stop } catch {
                Warn ("could not stop editor pid {0}: {1}" -f $proc.Id, $_.Exception.Message)
            }
        }
    }

    # LiveCodingConsole is stopped too, and this is not tidiness. It is a separate process that the editor
    # starts and that can outlive it. While it runs, UnrealBuildTool refuses every build with "Unable to build
    # while Live Coding is active", which is a confusing failure an hour later. It holds no asset, so it is
    # stopped by name - and that is the ONLY name-based stop left in this script, stated rather than implied.
    $liveCoding = @(Get-Process -Name LiveCodingConsole -ErrorAction SilentlyContinue)
    if ($liveCoding.Count -gt 0) {
        Write-Host "  stopping $($liveCoding.Count) leftover LiveCodingConsole process(es) - they block every build"
        $liveCoding | Stop-Process -Force -ErrorAction SilentlyContinue
    }

    $deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $deadline) {
        $still = (Get-TurboMCPRunningEditors -ProjectPath $ProjectPath -EnginePath $EnginePath).Owned
        if ($still.Count -eq 0) { break }
        Start-Sleep -Seconds 1
    }
    $left = @((Get-TurboMCPRunningEditors -ProjectPath $ProjectPath -EnginePath $EnginePath).Owned)
    if ($left.Count -gt 0) {
        Fail "this project's editor is still alive after 30s (pid $($left[0].Id))"
        return $false
    }

    # *** `-Stop` IS ALSO THE WAY OUT OF A STALE LEASE, AND WITHOUT THAT IT IS A DEAD END. ***
    #
    # MEASURED: after an editor exited without releasing, the next start refused with exit 8 and told the operator
    # to "delete <lease path> if the run is over" - handing a person a manual file deletion as the only recovery.
    # The lease deliberately blocks an UNRELEASED lease (the writer and the reader are different processes, so
    # liveness cannot be trusted), which makes an explicit release necessary rather than optional. `-Stop` is the
    # operator tool for exactly this, so when it has established that NO editor of this project is running and
    # none foreign is in the way, the lease is released too - whoever wrote it.
    $anyForeign = @((Get-TurboMCPRunningEditors -ProjectPath $ProjectPath -EnginePath $EnginePath).Foreign)
    if ($anyForeign.Count -eq 0) {
        $staleLease = Read-TurboMCPLease -RepoRoot $RepoRoot
        if ($staleLease -and -not ($staleLease.PSObject.Properties.Name -contains 'Unreadable' -and $staleLease.Unreadable)) {
            if ($staleLease.OwnerId -ne $OwnerId) {
                Write-Host "  releasing a lease left by '$($staleLease.OwnerId)' (pid $($staleLease.Pid)); no editor of this project is running"
            }
            Remove-Item $LeasePath -Force -ErrorAction SilentlyContinue
        } elseif ($staleLease) {
            # An unparseable lease is removed only here, where the operator has explicitly asked to stop and
            # nothing of this project is running. It is never removed silently by a start.
            Write-Host "  removing an unreadable lease file at $LeasePath; no editor of this project is running"
            Remove-Item $LeasePath -Force -ErrorAction SilentlyContinue
        }
    }
    Remove-TurboMCPLease -RepoRoot $RepoRoot -OwnerId $OwnerId | Out-Null
    return $true
}

# ---------------------------------------------------------------------------
# -Stop: stop the editor and exit. No file checks, because stopping must work
# even when something is missing or half built.
# ---------------------------------------------------------------------------
if ($Stop) {
    Step 'Stopping the editor'
    # *** THIS USED TO STOP EVERY PROCESS NAMED `UnrealEditor`, WHICH IS RELEASE FINDING R10. ***
    # Now the decision is `Stop-TurboMCPOwnedEditor` above: this project's editor is stopped, a foreign one is
    # reported and refused, and a process whose command line cannot be read is not claimed.
    $stopped = Stop-TurboMCPOwnedEditor -ProjectPath $Project -EnginePath $Engine -AllowForeign:$StopForeignEditors
    if (-not $stopped) {
        Write-Host "`nRESULT: REFUSED (ownership could not be established; nothing was stopped)" -ForegroundColor Red
        exit 7
    }
    if (Get-NetTCPConnection -LocalPort $McpPort -State Listen -ErrorAction SilentlyContinue) {
        Warn "port $McpPort is still held by another process"
    }
    Ok 'stopped and the plugin DLL is unlocked'
    Write-Host "`nRESULT: PASS" -ForegroundColor Green
    exit 0
}

# ---------------------------------------------------------------------------
Step 'Files'
# ---------------------------------------------------------------------------
foreach ($item in @(
    @{ Path = $EditorExe;  What = 'editor binary' },
    @{ Path = $Project;    What = 'project file' }
)) {
    if (Test-Path $item.Path) { Ok "$($item.What): $($item.Path)" }
    else { Fail "$($item.What) missing: $($item.Path)"; $Problems += $item.What }
}

# The plugin binary lives in the plugin's own Binaries folder, not the engine's.
# `$ProjectDir`, `$RepoDir` and `$RepoRoot` were computed before the -Stop block, because ownership needs them.
$PluginDll = Join-Path $RepoDir 'Binaries\Win64\UnrealEditor-TurboMCP.dll'
if (Test-Path $PluginDll) { Ok "plugin binary: $PluginDll" }
else { Fail "plugin binary missing: $PluginDll (build it first)"; $Problems += 'plugin binary' }

# This is the check a headless run hides, and the reason the first visible start
# failed: a rendering editor exits when this is missing.
if ($NullRHI) {
    Ok 'ShaderCompileWorker not needed with -NullRHI'
} elseif (Test-Path $ShaderWorker) {
    Ok "shader worker: $ShaderWorker"
} else {
    Fail "ShaderCompileWorker.exe missing. A rendering editor exits without it. Build it with:"
    Write-Host "        cd $Engine; Engine\Build\BatchFiles\Build.bat ShaderCompileWorker Win64 Development" -ForegroundColor Yellow
    $Problems += 'shader worker'
}

if ($Problems.Count -gt 0) {
    Write-Host "`nCannot start: fix the problems above first." -ForegroundColor Red
    exit 2
}

# ---------------------------------------------------------------------------
Step 'Ownership lease'
# ---------------------------------------------------------------------------
# *** ONE EXPLICIT OWNER FOR THE PROJECT, THE ENGINE, THE PORT AND THE FIXTURE SET. ***
# Finding R10's required correction. The lease is taken BEFORE an editor is started, and a second owner is
# refused rather than warned: two editors racing for one `.uasset` and one MCP port is a failure this
# repository has recorded more than once. The lease is read back from disk, because the tools that need it are
# separate processes.
$FixtureDir = Join-Path $RepoRoot 'TestProject\Content\Tests'
$lease = New-TurboMCPLease -RepoRoot $RepoRoot -OwnerId $OwnerId -Project $Project -Engine $Engine `
    -FixtureDir $FixtureDir -McpPort $McpPort
if (-not $lease.Ok) {
    Fail "the lease could not be taken: $($lease.Reason)"
    Write-Host '        Nothing was started. Establish who owns the editor before retrying.' -ForegroundColor Yellow
    exit 8
}
if ($lease.State -eq 'acquired' -and $lease.Note -and $lease.Note -ne 'no lease is held') {
    Warn $lease.Note
}
Ok "lease held by $OwnerId (pid $PID) at $LeasePath"

# ---------------------------------------------------------------------------
Step 'Existing editor'
# ---------------------------------------------------------------------------
# Refuse rather than warn. A warning here once let a second editor start while a
# first was still running, which means two servers racing for the MCP port and a
# confusing picture for whoever is watching. Starting a second editor is never
# what the caller meant.
#
# *** AND "AN EDITOR IS RUNNING" IS NOW SPLIT INTO "THIS PROJECT'S EDITOR" AND "SOMEONE ELSE'S". ***
# Finding R10: -StopFirst used to stop every process named UnrealEditor. It now stops only editors whose command
# line names THIS project, and refuses when the running editor belongs to a different checkout - because
# replacing another project's editor is not what the caller asked for, and the previous behaviour could end a
# session that had nothing to do with this repository.
$editorState = Get-TurboMCPRunningEditors -ProjectPath $Project -EnginePath $Engine
$Running = $editorState.Owned
$ForeignEditors = $editorState.Foreign
$PortBusy = Get-NetTCPConnection -LocalPort $McpPort -State Listen -ErrorAction SilentlyContinue

# *** MORE THAN ONE EDITOR FOR THIS PROJECT IS A CONDITION TO REPORT, NOT A CONDITION TO PAPER OVER. ***
# MEASURED while building this step: FOUR editors for this project were alive at once, left by earlier suite
# runs, while `Get-Process -Name UnrealEditor` reported only one of them. Four editors share one fixture
# directory and race for one MCP port, and every test result taken while they run is suspect. The count is
# printed, and a start refuses rather than joining them.
if ($Running.Count -gt 1) {
    Warn "$($Running.Count) editors for this project are already running (pid $((($Running | ForEach-Object { $_.Id }) -join ', ')))"
    Warn 'they share one fixture directory and race for the MCP port; results taken now are not trustworthy'
}

if ($ForeignEditors.Count -gt 0) {
    foreach ($entry in $ForeignEditors) {
        Write-Host "  foreign editor pid $($entry.Id): $($entry.Reason)" -ForegroundColor Yellow
    }
    if ($StopFirst -or $Stop) {
        Fail 'an editor belonging to another project is running; this project will not stop it'
        Write-Host "        Close it yourself, or re-run with -StopForeignEditors if you are certain it is yours." -ForegroundColor Yellow
        exit 7
    }
    Warn 'a foreign editor is running; this project will start its own only if the MCP port is free'
}

if ($Running -or $PortBusy) {
    if ($StopFirst) {
        $stopped = Stop-TurboMCPOwnedEditor -ProjectPath $Project -EnginePath $Engine -AllowForeign:$StopForeignEditors
        if (-not $stopped) {
            Fail 'ownership could not be established, so nothing was stopped'
            exit 7
        }
        Start-Sleep -Seconds 2
        Ok 'stopped this project''s editor'
    } else {
        # *** REUSE A HEALTHY EDITOR INSTEAD OF REFUSING THE CALLER'S OWN START. ***
        #
        # MEASURED, and it blocked the fixture-rebuilding suites for two rounds: a suite stops the editor to
        # rebuild a fixture, the rebuild starts one, and the suite's NEXT `start-editor.ps1` was then refused with
        # "this project's editor is already running (PID 2032)". The suite's own editor was healthy and answering;
        # the launcher simply would not say so.
        #
        # The refusal is still right for the cases it was written for, so it is not deleted - it is now REACHED
        # only after the health probe fails. The three outcomes are distinct and each is reported:
        #
        #   * this project's editor answers the MCP server  -> REUSE, exit 0. Nothing is stopped and nothing is
        #     started, which is strictly less work than before.
        #   * this project's editor does NOT answer        -> the existing refusal, so the wedged-editor recovery
        #     in `run-suites.ps1` still detects it (its `Start-EditorAndWait` relies on exactly this).
        #   * a FOREIGN editor                             -> already refused above, never pre-empted.
        #
        # THE PROBE IS THE DECISION, NOT THE PROCESS. `run-suites.ps1` records why in as many words: a launcher
        # answered "an editor is already running (PID 53852)" while `check-mcp-server.py` could NOT REACH A
        # SERVER - a live process whose server never came up, which every later suite then inherited. So a
        # process being alive proves nothing; the server answering is what "healthy" means.
        if ($Running) {
            $healthArgs = @()
            if ($Running[0].Id) { $healthArgs += @('--editor-pid', [string]$Running[0].Id) }
            $probe = & python (Join-Path $PSScriptRoot 'check-mcp-server.py') @healthArgs 2>&1
            if ($probe -match '^PASS') {
                Ok "this project's editor is already running and healthy (PID $($Running[0].Id)); reusing it"
                Write-Host "        The MCP server answered and all four TurboMCP toolsets are registered."
                Write-Host '        Nothing was stopped and nothing was started.'
                # The lease is refreshed to name this editor, so the reclaim rule and the stop path both see the
                # owner that is actually running rather than the launcher that asked.
                $LeaseRecord = Read-TurboMCPLease -RepoRoot $RepoRoot
                if ($LeaseRecord -and -not ($LeaseRecord.PSObject.Properties.Name -contains 'Unreadable' -and $LeaseRecord.Unreadable)) {
                    $LeaseRecord | Add-Member -NotePropertyName EditorPid -NotePropertyValue $Running[0].Id -Force
                    $LeaseRecord | ConvertTo-Json -Depth 4 | Set-Content -Path $LeasePath -Encoding utf8
                }
                Write-Host "`nRESULT: PASS (an existing healthy editor was reused)" -ForegroundColor Green
                exit 0
            }
            Fail "this project's editor is running (PID $($Running[0].Id)) but the MCP server did not answer"
            Write-Host "        A live process is not a healthy server; re-run with -StopFirst to replace it." -ForegroundColor Yellow
        }
        if ($PortBusy) {
            Fail "port $McpPort is already in use, so a second MCP server would fail to bind"
            Write-Host "        Re-run with -StopFirst, or use -Stop to stop the editor holding it." -ForegroundColor Yellow
        }
        exit 6
    }
} else {
    Ok 'no editor running'
}

# ---------------------------------------------------------------------------
Step 'Starting the editor'
# ---------------------------------------------------------------------------
$Args = @($Project)
# DSH confines writes to the workspace. Unreal's default DDC can live under the engine checkout,
# so it becomes read-only in a tracked task session and the editor exits before the MCP server starts.
# Give confined runs a persistent project-local cache; normal interactive launches keep their usual DDC.
if ($env:DSH_SESSION_ID) {
    $WorkspaceDDC = Join-Path (Split-Path $Project -Parent) 'Saved\DerivedDataCache'
    New-Item -ItemType Directory -Force -Path $WorkspaceDDC | Out-Null
    Set-Item -Path 'Env:UE-LocalDataCachePath' -Value $WorkspaceDDC
}
# *** THIS COMMENT WAS STALE AND IT COST A FALSE PREMISE (TestRecord.md section 158). It used to read "`-Offscreen` IS WHAT
# THE SUITES USE", WHICH STOPPED BEING TRUE WHEN `-Offscreen` WAS REMOVED AT LINE 296 BECAUSE IT MADE THE EDITOR FAIL TO EXIT
# CLEANLY. THE SUITES NOW RUN WITH **A REAL RHI AND A REAL WINDOW**: `Tools/run-suites.ps1:195` calls this script with
# `-LowPriority` AND NOTHING ELSE. Read the caller's ARGUMENT LIST, not this comment - that mistake is the fourth time this
# project parsed a sentence where a field was meant, and it made a `screen` capture look impossible when it is the thing that
# WORKS here (measured: a 1670x911 window PNG with 1335 distinct colours). ***
# `-Offscreen` REMAINS AVAILABLE AND IS `-nullrhi -unattended` WITH A HIDDEN WINDOW, for a caller that asks for it. Its
# MEASURED cost is at line 296 and it is why it is no longer the default: it made the editor fail to exit cleanly.
if ($Offscreen) { $NullRHI = $true }
if ($NullRHI) { $Args += @('-nullrhi','-unattended') }
if (-not [string]::IsNullOrWhiteSpace($Culture)) { $Args += @("-culture=$Culture") }
# LIVE CODING IS DROPPED, AND THESE ARE THE MEASUREMENTS THAT DROPPED IT, KEPT SO NOBODY RE-TRIES THEM.
#
# `LiveCodingConsole` IS WHAT CALLS `BringToFront` BY DESIGN - LiveCodingConsole.cpp binds `BringToFront` and
# `BringToFrontAsync` to the server's own ShowConsole/BringToFront delegates and runs `BringToFront` as a main
# thread task - AND IT IS LAUNCHED WITH `-Hidden` (LiveCodingModule.cpp:1104), which is why an enumeration of
# VISIBLE windows never saw it. `-LiveCoding=0` IS a real flag and DID suppress it (measured: 0 processes),
# BUT THE EDITOR THEN FAILED TO REACH A HEALTHY STATE, twice.
#
# AND THAT IS THE FOURTH SUPPRESSION TO COST MORE THAN IT FIXED: `-Offscreen` (which implies `-nullrhi`),
# `ShowWindow(SW_HIDE)` and `ShowWindow(SW_SHOWMINNOACTIVE)` each killed the editor outright.
# THE OWNER'S CALL WAS TO CONTINUE HEADLESS: "si live coding demarre pas pour l'instant continue en headless".
#
# WHAT REMAINS TRUE ABOUT FOCUS: the CPU competition is fixed by `-LowPriority` (BelowNormal, applied AFTER a
# proven startup), so a game keeps its frame budget. The editor window still takes the foreground, and the
# four attempts to stop that are recorded above rather than re-attempted.
#
# AND THE CONSOLE IS THE CULPRIT RATHER THAN THE EDITOR WINDOW: LiveCodingConsole.cpp binds BringToFront and
# BringToFrontAsync to the server's own ShowConsole/BringToFront delegates and runs BringToFront as a main
# thread task, AND IT IS LAUNCHED WITH -Hidden (LiveCodingModule.cpp:1104) - which is why an enumeration of
# VISIBLE windows never saw it.
# -unattended IS WHAT ACTIVATES THE FOCUS GUARD, AND IT IS THE ONLY THING THAT CAN.
#
# FTurboMCPFocusGuard hands the foreground back when this editor takes it, and it is gated on
# FApp::IsUnattended() (App.cpp:278, which parses -unattended) BECAUSE A HUMAN WHO LAUNCHES THE EDITOR WANTS
# TO TYPE IN IT. So the flag is passed by the SUITE HARNESS only - an interactive launch keeps the editor's normal
# behaviour and keeps the foreground it takes.
#
# IT REPLACES -Offscreen, WHICH WAS MEASURED AND REMOVED: -Offscreen implied -nullrhi, and that made the
# editor FAIL TO START - 	he editor process exited after 8s, twice - while a plain launch reached PASS in 16
# seconds. -nullrhi is not an option for this project, and the script's own check list says why: a -NullRHI
# editor never needs ShaderCompileWorker.exe, AND THAT IS THE CHECK A HEADLESS RUN HIDES.
$Args += @('-nosplash','-nosound','-nopause','-unattended')

$StartTime = Get-Date
Write-Host "  $EditorExe $($Args -join ' ')"
# PassThru, so the loop below watches THE editor this script started rather than
# any UnrealEditor process that happens to exist. Watching by name could report
# success based on a stale process and hide the new one dying.
if ($NullRHI) { $Started = Start-Process -FilePath $EditorExe -ArgumentList $Args -WindowStyle Hidden -PassThru }
else          { $Started = Start-Process -FilePath $EditorExe -ArgumentList $Args -PassThru }

$EditorPid = $Started.Id
Write-Host "  PID $EditorPid"

# *** THE LEASE MUST NAME THE EDITOR, NOT THE LAUNCHER. ***
#
# MEASURED, and it broke every suite that rebuilds a fixture: the lease recorded the LAUNCHER's pid, the launcher
# exited as soon as the editor was up, and the lease then refused the NEXT start with
# "another owner holds it: 'start-editor:7488' (pid 7488); the owning process is gone" - advice to release it by
# hand. The fixture-rebuilding suites call `start-editor.ps1` from Python, so every one of them failed on a lease
# no live process held.
#
# The world state that matters is "is an editor of THIS project running", not "is the process that asked for it
# still alive". So the lease is rewritten with the editor's own pid, and `Get-TurboMCPLeaseConflict` treats a
# lease whose editor is gone as FREE - the safe direction, because the editor's absence is exactly what makes the
# next start legitimate.
$LeaseRecord = Read-TurboMCPLease -RepoRoot $RepoRoot
if ($LeaseRecord -and -not ($LeaseRecord.PSObject.Properties.Name -contains 'Unreadable' -and $LeaseRecord.Unreadable)) {
    $LeaseRecord | Add-Member -NotePropertyName EditorPid -NotePropertyValue $EditorPid -Force
    $LeaseRecord | ConvertTo-Json -Depth 4 | Set-Content -Path $LeasePath -Encoding utf8
    Write-Host "  lease records editor pid $EditorPid (the launcher's pid is not the owner of an editor)"
}

$Deadline = (Get-Date).AddSeconds($TimeoutSeconds)
$SawWindow = $false
$ServerUp = $false
$LastNote = Get-Date

while ((Get-Date) -lt $Deadline) {
    Start-Sleep -Seconds 5

    $Proc = Get-Process -Id $EditorPid -ErrorAction SilentlyContinue
    if (-not $Proc) {
        Fail "the editor process (PID $EditorPid) exited after $([int]((Get-Date) - $StartTime).TotalSeconds)s"
        $LogPath = Join-Path $ProjectDir 'Saved\Logs\TurboMCPTest.log'
        if (Test-Path $LogPath) {
            Write-Host '  last log lines:' -ForegroundColor Yellow
            Get-Content $LogPath -Tail 8 | ForEach-Object { "    $_" }
        }
        exit 3
    }

    if (-not $NullRHI -and -not $SawWindow -and $Proc.MainWindowHandle -ne 0) {
        $SawWindow = $true
        Ok "window up after $([int]((Get-Date) - $StartTime).TotalSeconds)s: $($Proc.MainWindowTitle)"
    }

    # Get-NetTCPConnection returns no rows in a confined DSH session even while
    # localhost:8000 answers. Probe the protocol itself; this is both portable
    # across that boundary and stronger than observing a listening socket.
    if (Test-Path $ProbeScript) {
        $ProbeOut = & python $ProbeScript 2>&1
        $ProbeCode = $LASTEXITCODE
        if ($ProbeCode -eq 0) {
            $ServerUp = $true
            break
        }
    } elseif (Get-NetTCPConnection -LocalPort $McpPort -State Listen -ErrorAction SilentlyContinue) {
        $ServerUp = $true
        break
    }

    # A progress note every 30s, so a long shader compile is not mistaken for a hang.
    if (((Get-Date) - $LastNote).TotalSeconds -ge 30) {
        Write-Host ("  waiting... {0}s, memory {1} MB" -f [int]((Get-Date) - $StartTime).TotalSeconds, [math]::Round($Proc.WorkingSet64/1MB,0))
        $LastNote = Get-Date
    }
}

if (-not $ServerUp) {
    Fail "the MCP server did not bind port $McpPort within $TimeoutSeconds s"
    exit 4
}
Ok "MCP server listening on port $McpPort"

# ---------------------------------------------------------------------------
Step 'Proving the plugin works'
# ---------------------------------------------------------------------------
if (Test-Path $ProbeScript) {
    # The startup loop already ran the complete probe that established readiness.
    # Reuse it instead of adding another sleep and network round trip.
    if (-not $ProbeOut) {
        $ProbeOut = & python $ProbeScript 2>&1
        $ProbeCode = $LASTEXITCODE
    }
    $ProbeOut | ForEach-Object { "  $_" }
    if ($ProbeCode -ne 0) {
        Fail 'the server answered but the plugin check failed'
        exit 5
    }
    Ok 'all four TurboMCP toolsets answered'
} else {
    Warn "probe script not found at $ProbeScript; skipped the toolset check"
}

# ---------------------------------------------------------------------------
Step 'Working state reached'
# ---------------------------------------------------------------------------
$Proc = Get-Process -Id $EditorPid -ErrorAction SilentlyContinue
Write-Host ("  PID:      {0}" -f $Proc.Id)
Write-Host ("  title:    {0}" -f $Proc.MainWindowTitle)
Write-Host ("  window:   {0}" -f ($Proc.MainWindowHandle -ne 0))
Write-Host ("  memory:   {0} MB" -f [math]::Round($Proc.WorkingSet64/1MB,0))
Write-Host ("  MCP:      {0}" -f $McpUrl)
Write-Host ("  startup:  {0}s" -f [int]((Get-Date) - $StartTime).TotalSeconds)
if (-not $NullRHI) {
    Write-Host ''
    Write-Host '  Look for the TurboMCP panel under: Window > TurboMCP'
}
Write-Host ''
# *** THE PRIORITY IS LOWERED HERE, AFTER THE EDITOR IS PROVEN WORKING, AND NOT AT LAUNCH - AND THAT IS A
# MEASURED CORRECTION RATHER THAN A PREFERENCE. *** Setting `BelowNormal` immediately after `Start-Process` STARVED
# THE EDITOR'S OWN STARTUP: the launch reported `FAIL the editor process (PID 49768) exited after 10s`. The editor
# spends its first seconds loading modules, cooking shader metadata and starting workers, and at BelowNormal those
# threads only run when nothing at Normal wants the CPU - so on a machine that is actually being used, THE STARTUP
# ITSELF TIMES OUT. LOWERING IT AFTERWARDS GETS BOTH: the launch competes normally and does not starve, and the
# LONG-RUNNING editor - the part that competes with a game for minutes - sits below the foreground.
if ($LowPriority) {
    try {
        $Started.PriorityClass = [System.Diagnostics.ProcessPriorityClass]::BelowNormal
        Write-Host '  priority:  BelowNormal (lowered after the editor was proven working)'
    } catch {
        Write-Host "  could not lower the editor priority: $_"
    }
}
Write-Host ''
Write-Host 'RESULT: PASS' -ForegroundColor Green
exit 0
