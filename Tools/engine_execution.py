"""Engine preflight and recovery boundaries for the local supervisor."""
import json
import os
from pathlib import Path
import socket
import subprocess
from execution_runtime import canonical


def preflight(engine):
    query=r"""$ErrorActionPreference='Stop'; Get-CimInstance Win32_Process | Where-Object {
        $_.Name -match '^(UnrealEditor|UnrealEditor-Cmd|UnrealBuildTool)\.exe$' -or
        ($_.Name -eq 'dotnet.exe' -and $_.CommandLine -match 'UnrealBuildTool')
    } | Select-Object ProcessId,ExecutablePath,CommandLine | ConvertTo-Json -Compress"""
    result=subprocess.run(['pwsh','-NoProfile','-Command',query],capture_output=True,text=True,timeout=20,check=True)
    processes=json.loads(result.stdout) if result.stdout.strip() else []
    if isinstance(processes,dict):processes=[processes]
    for process in processes:
        command=process.get('CommandLine')
        text=(str(process.get('ExecutablePath') or '')+' '+str(command or '')).replace('/','\\').lower()
        if not command or canonical(engine) in text:
            raise RuntimeError('An existing editor/build may use this engine; no process was stopped')
    with socket.socket() as sock:
        sock.setsockopt(socket.SOL_SOCKET,socket.SO_EXCLUSIVEADDRUSE,1)
        sock.bind(('127.0.0.1',8000))


def finish_lease(repo,owner):
    path=Path(repo)/'.agent/turbo-mcp-lease.json'
    if path.exists():
        lease=json.loads(path.read_text(encoding='utf-8-sig'))
        if lease.get('OwnerId')!=owner:
            raise RuntimeError('Foreign lease was preserved; explicit inspection required')
        path.unlink()
