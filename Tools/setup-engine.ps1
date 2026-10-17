<#
.SYNOPSIS
    Set up Unreal Engine 5.8.3 from source and build the TurboMCP editor plugin.

.DESCRIPTION
    This script exists because the setup is not obvious and every step below was
    arrived at by hitting a real failure. It is safe to re-run: each step checks
    whether it is already done.

    The steps, and why each one is needed:

    1. Clone the engine. The UnrealEngine repository is private and requires a
       GitHub account linked to an Epic account.
    2. Setup.bat. Downloads about 31 GB of binary dependencies. Without it the
       build fails on missing third-party headers.
    3. GenerateProjectFiles.bat. Also compiles UnrealBuildTool.
    4. Mount the plugin. UBT cannot discover a plugin outside Engine/Plugins, and
       reports "Unable to find plugin". A directory junction puts the repository
       where UBT looks without copying it.
    5. Build the project editor target. The plugin is compiled as part of a
       project, so it cannot be built on its own.

.PARAMETER EngineDir
    Where the engine source lives. **Defaults to `C:\UnrealEngine`, which is what this script's own
    default of creating the engine there means** - it is the one place in `Tools/` that legitimately
    names a path, because installing an engine is the whole of its job. Every other script in this
    directory resolves the engine instead, so pass `-EngineDir` and nothing else has to change.

.PARAMETER RepoDir
    The TurboMCP repository. Default is the parent of this script.

.PARAMETER Branch
    Engine branch to clone. Default "release", which is 5.8.3.

.PARAMETER MaxParallelActions
    Compiler processes to allow. See the memory note below. Default 8.

.PARAMETER SkipClone
    Assume the engine is already cloned.

.PARAMETER SkipSetup
    Assume Setup.bat has already run.

.EXAMPLE
    .\setup-engine.ps1
    Full setup into C:\UnrealEngine.

.EXAMPLE
    .\setup-engine.ps1 -SkipClone -SkipSetup
    Just re-mount the plugin and rebuild.

.NOTES
    MEMORY IS THE LIMITING FACTOR, NOT CPU, AND THIS IS THE ONE PLACE WHERE A MEASUREMENT IS ABOUT THE
    MACHINE RATHER THAN THE SOFTWARE - so it is stated as a rule rather than as one computer's numbers.

    A UE 5.8 editor build wants roughly **1.4 GB of commit charge per parallel action** at default
    settings, and Unreal Build Accelerator kills compilers rather than waiting when it runs out:

        UbaSessionServer - Killed process SharedPCH.Engine.Cpp20.cpp -
        Low on memory. Kill threshold is ...

    *** IF THE KILL THRESHOLD IS BELOW THE COMMIT CHARGE THE BUILD NEEDS, LOWER `-MaxParallelActions`
    UNTIL IT FITS. *** The build then runs close to serially and takes hours, but it finishes. Raising
    the pagefile or adding RAM is the real fix and both need an elevated prompt.

    The default of **8** is conservative and was chosen against a machine with 32 GB of RAM and a fixed
    ~15 GB pagefile, where the ceiling was about 42 GB and 8 actions stayed under it. **A machine with
    more memory can raise it; `Tools/build.ps1` passes the same figure through.**

    Unreal resolves config through the settings class's own config file, not the
    one that looks natural. See TestProject/Config/DefaultEditorPerProjectUserSettings.ini.
#>

[CmdletBinding()]
param(
    [string] $EngineDir = 'C:\UnrealEngine',
    [string] $RepoDir = (Split-Path -Parent $PSScriptRoot),
    [string] $Branch = 'release',
    [int]    $MaxParallelActions = 8,
    [switch] $SkipClone,
    [switch] $SkipSetup,
    # Refuse-by-default on an engine version this project does not target (release finding R09's second half).
    # The flag exists so an operator who knowingly wants another version can proceed; it is never implied.
    [switch] $AllowOtherEngineVersion
)

$ErrorActionPreference = 'Stop'

function Write-Step { param([string] $Text) Write-Host "`n=== $Text ===" -ForegroundColor Cyan }
function Write-Ok   { param([string] $Text) Write-Host "  ok    $Text" -ForegroundColor Green }
function Write-Warn { param([string] $Text) Write-Host "  warn  $Text" -ForegroundColor Yellow }
function Write-Fail { param([string] $Text) Write-Host "  FAIL  $Text" -ForegroundColor Red }

# ---------------------------------------------------------------------------
# Preconditions
# ---------------------------------------------------------------------------
Write-Step 'Checking prerequisites'

if (-not (Test-Path $RepoDir\TurboMCP.uplugin)) {
    throw "TurboMCP.uplugin not found in $RepoDir. Pass -RepoDir explicitly."
}
Write-Ok "plugin repository at $RepoDir"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw 'Visual Studio not found. Install VS 2022 with the C++ game development workload.'
}
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    throw 'Visual Studio is installed but has no C++ toolset. Add the "Desktop development with C++" workload.'
}
Write-Ok "C++ toolset at $vsPath"

$freeGB = [math]::Round((Get-PSDrive C).Free / 1GB, 1)
Write-Host "  note  $freeGB GB free on C: (a source build needs roughly 200 GB)"
if ($freeGB -lt 150) {
    Write-Warn 'less than 150 GB free; the build may run out of space'
}

# ---------------------------------------------------------------------------
# 1. Clone
# ---------------------------------------------------------------------------
Write-Step "Engine source at $EngineDir"

if ($SkipClone -or (Test-Path "$EngineDir\Engine\Build\Build.version")) {
    Write-Ok 'engine already cloned'
} else {
    # *** A TOKEN IN A URL IS NOT AN INSTRUCTION THIS SCRIPT GIVES ANY MORE, AND IT DOES NOT PRINT THE SHAPE. ***
    #
    # Release finding R09: this advised a clone URL of the form scheme://host/path. A URL is written
    # to shell history, to `.git/config`, to clone logs and to the process table, so the secret leaks to all
    # four. The note below now names a credential manager instead, and it does not reproduce the shape it is
    # warning about either - `Tools/test-ownership.py` scans this file for that pattern and a regression must
    # not be able to hide behind a quotation of itself.
    Write-Host '  note  the UnrealEngine repository is private, so git must authenticate.'
    Write-Host '        Use a credential manager rather than embedding a secret in the clone URL:'
    Write-Host '          git config --global credential.helper manager'
    Write-Host '        or, for a single clone, set a helper that prompts:'
    Write-Host '          git -c credential.helper=manager clone ...'
    Write-Host '        A secret inside a URL is written to shell history, to .git/config, to logs and to the'
    Write-Host '        process table, so this script will not suggest one.'
    & git clone --filter=blob:none --depth 1 --single-branch --branch $Branch `
        "https://github.com/EpicGames/UnrealEngine.git" $EngineDir
    if ($LASTEXITCODE -ne 0) { throw 'git clone failed' }
    Write-Ok 'cloned'
}

$versionFile = "$EngineDir\Engine\Build\Build.version"
if (-not (Test-Path $versionFile)) { throw "Build.version missing at $versionFile" }
$version = Get-Content $versionFile -Raw | ConvertFrom-Json
$found = "{0}.{1}.{2}" -f $version.MajorVersion, $version.MinorVersion, $version.PatchVersion
Write-Ok "engine is $found"

# *** A VERSION MISMATCH IS REFUSED HERE, WHICH IS RELEASE FINDING R09's SECOND HALF. ***
#
# The old code PRINTED the version and then continued with whatever it found. That is the same shape as the
# build that printed "plugin DLL: NOT FOUND" and returned success (R11): a fact stated and then not acted on.
# This project targets one engine version, and building against another fails inside the engine's own headers,
# where the failure reads like a defect in this plugin - which is exactly how a machine with both 5.8.3 and a
# launcher 5.6 resolved the WRONG engine by discovery once already.
$expected = '5.8.3'
if ($found -ne $expected) {
    Write-Fail "engine version is $found but this project targets $expected"
    Write-Host "        The plugin is built and tested against $expected only. Another version fails in the" -ForegroundColor Yellow
    Write-Host "        engine's own headers and reads as a defect here." -ForegroundColor Yellow
    Write-Host "        Pass -AllowOtherEngineVersion to proceed anyway, and expect to diagnose engine-side errors." -ForegroundColor Yellow
    if (-not $AllowOtherEngineVersion) { throw "engine version mismatch: found $found, expected $expected" }
    Write-Warn "continuing with $found because -AllowOtherEngineVersion was given"
}

# ---------------------------------------------------------------------------
# 2. Dependencies
# ---------------------------------------------------------------------------
Write-Step 'Engine dependencies (about 31 GB)'

if ($SkipSetup -or (Test-Path "$EngineDir\Engine\Binaries\ThirdParty")) {
    Write-Ok 'dependencies present'
} else {
    & cmd.exe /c "`"$EngineDir\Setup.bat`""
    if ($LASTEXITCODE -ne 0) { throw 'Setup.bat failed' }
    Write-Ok 'dependencies installed'
}

# ---------------------------------------------------------------------------
# 3. Project files
# ---------------------------------------------------------------------------
Write-Step 'Project files and UnrealBuildTool'

if (Test-Path "$EngineDir\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.dll") {
    Write-Ok 'UnrealBuildTool already built'
} else {
    & cmd.exe /c "cd /d `"$EngineDir`" && GenerateProjectFiles.bat -2022 -progress"
    if ($LASTEXITCODE -ne 0) { throw 'GenerateProjectFiles.bat failed' }
    Write-Ok 'project files generated'
}

# ---------------------------------------------------------------------------
# 4. Mount the plugin
# ---------------------------------------------------------------------------
Write-Step 'Mounting the plugin where UBT looks'

$mount = "$EngineDir\Engine\Plugins\Marketplace\TurboMCP"
if (Test-Path $mount) {
    $item = Get-Item $mount -Force
    if ($item.LinkType -eq 'Junction') {
        Write-Ok "junction already present -> $($item.Target)"
    } else {
        Write-Warn "$mount exists and is not a junction. Leaving it alone."
    }
} else {
    New-Item -ItemType Junction -Path $mount -Target $RepoDir | Out-Null
    Write-Ok "junction created: $mount -> $RepoDir"
}

if (-not (Test-Path "$mount\TurboMCP.uplugin")) {
    throw 'the junction does not expose TurboMCP.uplugin'
}

# ---------------------------------------------------------------------------
# 5. Build
# ---------------------------------------------------------------------------
Write-Step 'Building TurboMCPTestEditor'

$project = "$RepoDir\TestProject\TurboMCPTest.uproject"
if (-not (Test-Path $project)) { throw "test project missing at $project" }

Write-Host "  note  limiting to $MaxParallelActions parallel actions to stay under the memory ceiling"
Write-Host '  note  this takes hours on a machine with 32 GB of RAM'

& cmd.exe /c "cd /d `"$EngineDir`" && Engine\Build\BatchFiles\Build.bat TurboMCPTestEditor Win64 Development -Project=`"$project`" -WaitMutex -MaxParallelActions=$MaxParallelActions"
if ($LASTEXITCODE -ne 0) { throw 'build failed; read the log for "error C" or "error LNK"' }
Write-Ok 'build succeeded'

# ---------------------------------------------------------------------------
# 6. Verify
# ---------------------------------------------------------------------------
Write-Step 'Verifying'

$dll = Get-ChildItem "$EngineDir\Engine\Binaries\Win64" -Filter '*TurboMCP*' -ErrorAction SilentlyContinue
if ($dll) {
    $dll | ForEach-Object { Write-Ok ("plugin binary: {0} ({1:N0} KB)" -f $_.Name, ($_.Length / 1KB)) }
} else {
    Write-Fail 'no TurboMCP binary was produced'
}

$exe = "$EngineDir\Engine\Binaries\Win64\UnrealEditor.exe"
if (Test-Path $exe) { Write-Ok 'UnrealEditor.exe built' } else { Write-Fail 'UnrealEditor.exe missing' }

Write-Host ''
Write-Host 'Next: run Tools/check-plugin.py, then open TestProject/TurboMCPTest.uproject'
Write-Host 'and run Tools/check-mcp-server.py to confirm the toolsets registered.'
