<#
.SYNOPSIS
    A git credential helper that never prompts.

.DESCRIPTION
    Git calls a credential helper as `git-credential-<name> get`, `store` and
    `erase`, passing the request on stdin as `key=value` lines and reading the
    answer from stdout.

    This reads an existing credential straight out of the Windows Credential
    Manager with [archived item] and prints it.

    WHY THIS REPLACES GIT CREDENTIAL MANAGER FOR THIS REPOSITORY
    -----------------------------------------------------------
    GCM is configured on this machine and works, but Windows holds several
    github.com credentials: a generic one, one per account name, and entries
    written by GitHub CLI and SourceTree. GCM finds more than one match and opens
    an interactive "Select an account" dialog on every push, which makes an
    unattended push impossible.

    This reads one named entry, so there is nothing to choose and nothing to ask.
    It adds no secret: it reads a credential that is already stored, and the
    repository's own config points at it, so the machine-wide setup is untouched
    and every other repository keeps using GCM.

.NOTES
    Set as `credential.helper` in this repository's `.git/config`, with an empty
    first value so that no other helper runs first:

        git config --local --replace-all credential.helper ""
        git config --local --add credential.helper "!powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File <path>"
#>

[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [string] $Operation = 'get'
)

$ErrorActionPreference = 'Stop'

# The credential to use. The generic name is the one git itself writes, and it
# holds the FloStyle account.
$TargetName = 'git:https://github.com'
$HelperExe = Join-Path $PSScriptRoot '[archived item]'

# Git sends the request on stdin and reads the reply from stdout. `store` and
# `erase` are accepted and deliberately do nothing: nothing should overwrite a
# credential that was verified by hand, and dropping the credential would put the
# prompt back.
if ($Operation -ne 'get') {
    exit 0
}

$request = @{}
$line = $null
while ($null -ne ($line = [Console]::In.ReadLine())) {
    if ($line -match '^(?<key>[^=]+)=(?<value>.*)$') {
        $request[$Matches['key']] = $Matches['value']
    }
}

# Only answer for this host. Answering for another host would send a GitHub token
# somewhere it does not belong.
if ($request['host'] -ne 'github.com') {
    exit 0
}

if (-not (Test-Path $HelperExe)) {
    [Console]::Error.WriteLine(
        "TurboMCP: $HelperExe is missing. Rebuild it with " +
        "csc.exe /target:exe /out:[archived item] GitCredentialHelper.cs")
    exit 1
}

$output = & $HelperExe get $TargetName 2>&1
if ($LASTEXITCODE -ne 0) {
    [Console]::Error.WriteLine("TurboMCP: could not read $TargetName : $output")
    exit 1
}

$username = $null
$password = $null
foreach ($entry in $output) {
    if ($entry -match '^username=(.*)$') { $username = $Matches[1] }
    elseif ($entry -match '^password=(.*)$') { $password = $Matches[1] }
}

if ([string]::IsNullOrEmpty($password)) {
    [Console]::Error.WriteLine(
        "TurboMCP: $TargetName holds no password. Re-add it with 'cmdkey' or let " +
        "GCM store one, then push once.")
    exit 1
}

# The reply git expects. Values are printed raw: git reads the whole line after
# the equals sign, so a token needs no escaping.
Write-Output "protocol=https"
Write-Output "host=github.com"
Write-Output "username=$username"
Write-Output "password=$password"
exit 0
