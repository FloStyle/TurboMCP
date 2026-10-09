<#
.SYNOPSIS
    Rebuilds the acceptance asset from its commandlet.

.DESCRIPTION
    `TurboMCPAcceptance` is a BUILD ARTIFACT, not a hand-made file: it is produced by
    `UTurboMCPAcceptanceAssetCommandlet` from `Source/TurboMCP/TurboMCPAcceptanceAssetCommandlet.cpp`.
    This script runs that commandlet so the asset can be regenerated at any time, which is what makes
    it safe for a TEST to mutate it.

    WHY THAT MATTERS
    ----------------
    `P/E-GRAPH-DEAD`'s Edit REMOVES nodes and `P/E-LOCAL-VARS`'s Edit ADDS a local variable and a
    getter. Both are destructive, so a suite that runs them against the committed asset would corrupt
    the shared fixture for every other suite. Two options were considered:

      duplicate the asset, run the test on the copy, delete the copy;
      regenerate the asset.

    REGENERATION IS THE ONE CHOSEN, because it is the only one that is EXACT. A duplicate can drift
    from the original in ways nothing checks, and the value of this asset is that its structure is
    known precisely. Regenerating also means the asset on disk always matches the C++ that describes
    it, so a reader can trust either one.

    The cost is that the suites that mutate it cannot run concurrently with each other. Given they
    share one editor, that was never possible anyway.

    THE `-run=` TOKEN NEEDS A MODULE PREFIX. The engine calls `LoadModule` only when the token contains
    a period (`LaunchEngineLoop.cpp:4100`), so `-run=TurboMCP.TurboMCPAcceptanceAssetCommandlet` is
    what loads this plugin. Without the prefix the run answers "looked like a commandlet, but we could
    not find the class" having loaded nothing.

.PARAMETER EditorExe
    Path to `UnrealEditor-Cmd.exe`. Defaults to the source-engine build this project uses.

.PARAMETER Project
    Path to the test project's `.uproject`.

.EXAMPLE
    pwsh -File Tools\make-acceptance-asset.ps1
#>
[CmdletBinding()]
param(
    [string]$EditorExe = 'C:\ExampleUnrealengine\Engine\Binaries\Win64\UnrealEditor-Cmd.exe',
    [string]$Project = '<REPO>\TestProject\TurboMCPTest.uproject'
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path $EditorExe)) { throw "the editor is not at $EditorExe" }
if (-not (Test-Path $Project))   { throw "the project is not at $Project" }

$repo = Split-Path -Parent $PSScriptRoot
$log = Join-Path $repo '.agent\logs\make-acceptance-asset.log'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $log) | Out-Null

# The editor holds the asset open, so it is stopped first. A stale handle would make the save fail
# for a reason that has nothing to do with the commandlet.
$stopScript = Join-Path $PSScriptRoot 'start-editor.ps1'
if (Test-Path $stopScript) {
    & pwsh -NoProfile -File $stopScript -Stop 2>&1 | Out-Null
    Start-Sleep -Seconds 2
}

& cmd.exe /c "`"$EditorExe`" `"$Project`" -run=TurboMCP.TurboMCPAcceptanceAssetCommandlet -unattended -nullrhi -culture=en -MaxParallelActions=8 > `"$log`" 2>&1"
$exit = $LASTEXITCODE

# THE EXIT CODE IS NOT THE EVIDENCE. The commandlet has reported success while writing nothing, so the
# line it prints about the file is read back rather than trusted.
$wrote = Select-String -Path $log -Pattern 'TurboMCP: wrote' -EA SilentlyContinue | Select-Object -Last 1
$counts = Select-String -Path $log -Pattern 'TurboMCP: Display:\s+\w+: \d+ node' -EA SilentlyContinue

if ($exit -ne 0 -or -not $wrote) {
    Write-Output "FAILED (exit $exit)"
    Select-String -Path $log -Pattern 'TurboMCP:|Error' -EA SilentlyContinue |
        Select-Object -Last 12 | ForEach-Object { "  " + $_.Line.Trim() }
    exit 1
}

Write-Output "PASS  the acceptance asset was rebuilt"
Write-Output ("  " + ($wrote.Line -replace '.*wrote\s+', ''))
foreach ($line in $counts) { Write-Output ("  " + ($line.Line -replace '.*LogTurboMCP: Display:\s+', '')) }
