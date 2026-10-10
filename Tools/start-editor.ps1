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
    [string] $Engine   = 'C:\UnrealEngine',
    [int]    $TimeoutSeconds = 420,
    [string] $Culture = 'en',
    [switch] $NullRHI,
    [switch] $LowPriority,
    [switch] $Offscreen,
    [switch] $StopFirst,
    [switch] $Stop,
    [switch] $KeepOpen
)

$ErrorActionPreference = 'Stop'

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

# ---------------------------------------------------------------------------
# -Stop: stop the editor and exit. No file checks, because stopping must work
# even when something is missing or half built.
# ---------------------------------------------------------------------------
if ($Stop) {
    Step 'Stopping the editor'
    $Editors = Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue

    # LiveCodingConsole is stopped too, and this is not tidiness.
    #
    # It is a separate process that the editor starts, and it can outlive the
    # editor. While it runs, UnrealBuildTool refuses every build with
    # "Unable to build while Live Coding is active", which is a confusing failure
    # an hour later and has nothing to do with the change being built. A leftover
    # console from a Live Coding experiment once blocked a build exactly that way.
    $LiveCoding = Get-Process -Name LiveCodingConsole -ErrorAction SilentlyContinue
    if ($LiveCoding) {
        Write-Host "  stopping $($LiveCoding.Count) leftover LiveCodingConsole process(es)"
        $LiveCoding | Stop-Process -Force -ErrorAction SilentlyContinue
        Start-Sleep -Seconds 2
        Ok 'LiveCodingConsole stopped, so builds will not be blocked'
    }

    if (-not $Editors) {
        Ok 'no editor was running'
        exit 0
    }
    Write-Host "  stopping $($Editors.Count) process(es)"
    $Editors | Stop-Process -Force -ErrorAction SilentlyContinue

    # Wait for the DLL lock to actually clear before claiming success, because a
    # rebuild started too early fails with a locked binary.
    $Deadline = (Get-Date).AddSeconds(30)
    while ((Get-Date) -lt $Deadline) {
        if (-not (Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue)) { break }
        Start-Sleep -Seconds 1
    }

    if (Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue) {
        Fail 'an editor process is still alive after 30s'
        exit 6
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
$ProjectDir = Split-Path $Project -Parent
$RepoDir = Split-Path $ProjectDir -Parent
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
Step 'Existing editor'
# ---------------------------------------------------------------------------
# Refuse rather than warn. A warning here once let a second editor start while a
# first was still running, which means two servers racing for the MCP port and a
# confusing picture for whoever is watching. Starting a second editor is never
# what the caller meant.
$Running = Get-Process -Name UnrealEditor -ErrorAction SilentlyContinue
$PortBusy = Get-NetTCPConnection -LocalPort $McpPort -State Listen -ErrorAction SilentlyContinue

if ($Running -or $PortBusy) {
    if ($StopFirst) {
        if ($Running) {
            Write-Host "  stopping $($Running.Count) running editor process(es)"
            $Running | Stop-Process -Force -ErrorAction SilentlyContinue
            Start-Sleep -Seconds 6
            Ok 'stopped'
        } else {
            # Something else owns the port. Stopping UnrealEditor processes is not
            # guaranteed to free it, so say what is holding it rather than guess.
            Warn "port $McpPort is held by a process that is not UnrealEditor; the new server may fail to bind"
        }
    } else {
        if ($Running) {
            Fail "an editor is already running (PID $($Running[0].Id))"
            Write-Host "        Re-run with -StopFirst to replace it, or use -Stop to stop it." -ForegroundColor Yellow
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
# `-Offscreen` IS WHAT THE SUITES USE, AND IT IS `-nullrhi -unattended` WITH A HIDDEN WINDOW. MEASURED REASON: the
# windowed editor TAKES THE FOREGROUND ON EVERY START, and a full suite run restarts it between suites, so the
# machine stole focus several times per run. It also skips shader compilation, which makes each start faster. Every
# TurboMCP tool works on graphs, assets and Blueprints through reflection, so NO SUITE LOSES ANYTHING - and the
# interactive path is untouched, because a human who launches the editor wants the window and the renderer.
if ($Offscreen) { $NullRHI = $true }
if ($NullRHI) { $Args += @('-nullrhi','-unattended') }
if (-not [string]::IsNullOrWhiteSpace($Culture)) { $Args += @("-culture=$Culture") }
$Args += @('-nosplash','-nosound','-nopause')

$StartTime = Get-Date
Write-Host "  $EditorExe $($Args -join ' ')"
# PassThru, so the loop below watches THE editor this script started rather than
# any UnrealEditor process that happens to exist. Watching by name could report
# success based on a stale process and hide the new one dying.
if ($NullRHI) { $Started = Start-Process -FilePath $EditorExe -ArgumentList $Args -WindowStyle Hidden -PassThru }
else          { $Started = Start-Process -FilePath $EditorExe -ArgumentList $Args -PassThru }

$EditorPid = $Started.Id
Write-Host "  PID $EditorPid"

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

    if (Get-NetTCPConnection -LocalPort $McpPort -State Listen -ErrorAction SilentlyContinue) {
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
    Start-Sleep -Seconds 3
    $ProbeOut = & python $ProbeScript 2>&1
    $ProbeCode = $LASTEXITCODE
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
