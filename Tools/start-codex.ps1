<# .SYNOPSIS
Start Codex in the private repository with explicit access to the engine tree.
#>
[CmdletBinding()]
param([string]$Engine='', [string[]]$CodexArgs=@())
$ErrorActionPreference='Stop'
. "$PSScriptRoot/common.ps1"
$repo=Get-TurboMCPRepoRoot
if (-not $Engine) { $Engine=Resolve-TurboMCPEngine }
# --add-dir grants write access. Engine changes still require an explicit task.
& codex --cd $repo --add-dir $Engine --sandbox workspace-write @CodexArgs
exit $LASTEXITCODE
