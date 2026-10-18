<#
.SYNOPSIS
    Read one Windows process record for the persistence collector.

.DESCRIPTION
    Fixed, checked-in helper. It accepts only a process id, opens a limited-query
    process handle, reads creation FILETIME and exit state before and after a
    single-PID CIM read, and emits exactly one compressed JSON object. Provider
    errors and command lines are never written outside the result channel.

    Native handles are closed on every path. This script contains no child
    launch. Windows PowerShell, Add-Type or the CIM/WMI provider may still use
    internal helper processes; offline source inspection cannot rule that out.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [uint32] $ProcessId
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$schema = 1
$result = $null
$handle = [IntPtr]::Zero
$closeOk = $true

function Write-Json([string] $Text) {
    $stream = [Console]::OpenStandardOutput()
    $encoding = New-Object -TypeName System.Text.UTF8Encoding -ArgumentList $false
    $writer = New-Object -TypeName System.IO.StreamWriter -ArgumentList @($stream, $encoding)
    try {
        $writer.Write($Text)
        $writer.Flush()
    }
    finally {
        $writer.Dispose()
    }
}

function New-Failure([string] $Code) {
    return [pscustomobject]@{ schema = $schema; ok = $false; error = $Code }
}

function New-NativeFailure([int] $Code) {
    if ($Code -eq 5) { return New-Failure 'permission' }
    if ($Code -eq 87 -or $Code -eq 1168) { return New-Failure 'not_found' }
    return New-Failure 'unknown'
}

function Get-Liveness($Handle) {
    # Microsoft documents GetProcessTimes lpExitTime as undefined while the
    # process is active, so liveness uses a zero-time wait on the same retained
    # handle. WAIT_TIMEOUT=0x102; WAIT_OBJECT_0=0 means exited, even for exit
    # code 259; any other result is a refusal.
    # https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getprocesstimes
    # https://learn.microsoft.com/en-us/windows/win32/api/synchapi/nf-synchapi-waitforsingleobject
    $wait = [TurboMCPProcessNative]::WaitForSingleObject($Handle, 0)
    if ($wait -eq 0x102) { return 'running' }
    if ($wait -eq 0) { return 'exited' }
    return 'failed'
}

try {
    if ($ProcessId -eq 0) {
        $result = New-Failure 'unknown'
    }
    else {
        Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;

public static class TurboMCPProcessNative
{
    [StructLayout(LayoutKind.Sequential)]
    public struct FILETIME
    {
        public uint dwLowDateTime;
        public uint dwHighDateTime;
    }

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr OpenProcess(uint access, bool inherit, uint pid);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool GetProcessTimes(
        IntPtr process,
        out FILETIME creation,
        out FILETIME exit,
        out FILETIME kernel,
        out FILETIME user);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool CloseHandle(IntPtr handle);
}
'@ -ErrorAction Stop

        # SYNCHRONIZE (0x00100000) is required for WaitForSingleObject;
        # PROCESS_QUERY_LIMITED_INFORMATION (0x1000) is required for GetProcessTimes.
        $handle = [TurboMCPProcessNative]::OpenProcess(0x101000, $false, [uint32]$ProcessId)
        if ($handle -eq [IntPtr]::Zero) {
            $result = New-NativeFailure ([Runtime.InteropServices.Marshal]::GetLastWin32Error())
        }
        else {
            $beforeCreation = New-Object 'TurboMCPProcessNative+FILETIME'
            $beforeExitTime = New-Object 'TurboMCPProcessNative+FILETIME'
            $beforeKernel = New-Object 'TurboMCPProcessNative+FILETIME'
            $beforeUser = New-Object 'TurboMCPProcessNative+FILETIME'
            $beforeLiveness = Get-Liveness $handle

            if (-not [TurboMCPProcessNative]::GetProcessTimes(
                    $handle,
                    [ref]$beforeCreation,
                    [ref]$beforeExitTime,
                    [ref]$beforeKernel,
                    [ref]$beforeUser)) {
                $result = New-NativeFailure ([Runtime.InteropServices.Marshal]::GetLastWin32Error())
            }
            elseif ($beforeLiveness -eq 'failed') {
                $result = New-Failure 'unknown'
            }
            elseif ($beforeLiveness -eq 'exited') {
                $result = New-Failure 'not_found'
            }
            else {
                $rows = @(Get-CimInstance Win32_Process -Filter "ProcessId=$ProcessId" -ErrorAction Stop)
                if ($rows.Count -eq 0) {
                    $result = New-Failure 'not_found'
                }
                elseif ($rows.Count -ne 1) {
                    $result = New-Failure 'ambiguous'
                }
                else {
                    $row = $rows[0]
                    if (
                        (($row.ProcessId -isnot [uint32]) -and ($row.ProcessId -isnot [int])) -or
                        [uint32]$row.ProcessId -ne [uint32]$ProcessId -or
                        $row.ExecutablePath -isnot [string] -or
                        [string]::IsNullOrWhiteSpace([string]$row.ExecutablePath) -or
                        $row.CommandLine -isnot [string] -or
                        [string]::IsNullOrWhiteSpace([string]$row.CommandLine)
                    ) {
                        $result = New-Failure 'incomplete'
                    }
                    else {
                        $afterCreation = New-Object 'TurboMCPProcessNative+FILETIME'
                        $afterExitTime = New-Object 'TurboMCPProcessNative+FILETIME'
                        $afterKernel = New-Object 'TurboMCPProcessNative+FILETIME'
                        $afterUser = New-Object 'TurboMCPProcessNative+FILETIME'
                        $afterLiveness = Get-Liveness $handle

                        if (-not [TurboMCPProcessNative]::GetProcessTimes(
                                $handle,
                                [ref]$afterCreation,
                                [ref]$afterExitTime,
                                [ref]$afterKernel,
                                [ref]$afterUser)) {
                            $result = New-NativeFailure ([Runtime.InteropServices.Marshal]::GetLastWin32Error())
                        }
                        elseif ($afterLiveness -eq 'failed') {
                            $result = New-Failure 'unknown'
                        }
                        elseif ($afterLiveness -eq 'exited') {
                            $result = New-Failure 'not_found'
                        }
                        elseif (
                            $afterCreation.dwLowDateTime -ne $beforeCreation.dwLowDateTime -or
                            $afterCreation.dwHighDateTime -ne $beforeCreation.dwHighDateTime
                        ) {
                            $result = New-Failure 'provider_error'
                        }
                        else {
                            [long] $ticks = ([long]$beforeCreation.dwHighDateTime -shl 32) -bor [long]$beforeCreation.dwLowDateTime
                            [string] $executablePath = ([string]$row.ExecutablePath).Replace('/', '\')
                            $result = [pscustomobject]@{
                                schema = $schema
                                ok = $true
                                pid = [uint32]$ProcessId
                                start_ticks = [long]$ticks
                                executable_path = [string]$executablePath
                                command_line = [string]$row.CommandLine
                            }
                        }
                    }
                }
            }
        }
    }
}
catch {
    if ($_.Exception.HResult -eq -2147024891) {
        $result = New-Failure 'permission'
    }
    else {
        $result = New-Failure 'provider_error'
    }
}
finally {
    if ($handle -ne [IntPtr]::Zero) {
        try {
            if (-not [TurboMCPProcessNative]::CloseHandle($handle)) {
                $closeOk = $false
            }
        }
        catch {
            $closeOk = $false
        }
    }
}

if (-not $closeOk) {
    $result = New-Failure 'unknown'
}

try {
    $json = $result | ConvertTo-Json -Compress
    Write-Json $json
}
catch {
    $json = [pscustomobject]@{ schema = $schema; ok = $false; error = 'unknown' } | ConvertTo-Json -Compress
    Write-Json $json
}
