"""Reject duplicate plugin discovery and prove the owned process loaded our built DLL."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
from execution_runtime import canonical


def check_discovery(repo,engine):
    repo=Path(repo);engine=Path(engine)
    expected=(repo/'Plugins/TurboMCP/TurboMCP.uplugin').resolve()
    project=repo/'TestProject/TurboMCPTest.uproject'
    config=json.loads(project.read_text(encoding='utf8'))
    roots=[engine/'Engine/Plugins',project.parent/'Plugins']
    roots.extend((project.parent/p).resolve() for p in config.get('AdditionalPluginDirectories',[]))
    seen=set();found=[]
    def error(exc):raise exc
    for root in roots:
        if not root.exists():continue
        for base,dirs,names in os.walk(root,followlinks=True,onerror=error):
            real=canonical(base)
            if real in seen:dirs[:]=[];continue
            seen.add(real)
            for name in names:
                if not name.endswith('.uplugin'):continue
                path=Path(base)/name
                raw=path.read_text(encoding='utf-8-sig')
                if path.name!='TurboMCP.uplugin' and '"TurboMCP"' not in raw:continue
                data=json.loads(raw)
                if path.name=='TurboMCP.uplugin' or any(m.get('Name')=='TurboMCP' for m in data.get('Modules',[])):
                    if path.resolve()!=expected:
                        raise ValueError('Duplicate TurboMCP installation would shadow the checkout: '+str(path))
                    found.append(path)
    if not found:raise ValueError('The active plugin is not discoverable from the test project')


def verify_binary(provenance,loaded):
    if canonical(loaded)!=canonical(provenance['ArtifactPath']):
        raise ValueError('Loaded TurboMCP binary differs from the build artifact: '+str(loaded))
    with Path(loaded).open('rb') as handle:actual=hashlib.file_digest(handle,'sha256').hexdigest()
    if actual.lower()!=provenance['ArtifactSha256'].lower():raise ValueError('Loaded binary hash differs from build provenance')


def verify_loaded(repo):
    repo=Path(repo)
    lease=json.loads((repo/'.agent/turbo-mcp-lease.json').read_text(encoding='utf-8-sig'))
    if lease['OwnerId']!=os.environ.get('TURBOMCP_OWNER_ID'):raise ValueError('Editor lease belongs to another owner')
    env=dict(os.environ,TMCP_IDENTITY_PID=str(int(lease['EditorPid'])))
    command="$ErrorActionPreference='Stop'; @((Get-Process -Id ([int]$env:TMCP_IDENTITY_PID)).Modules | Where-Object { $_.ModuleName -eq 'UnrealEditor-TurboMCP.dll' } | ForEach-Object { $_.FileName }) | ConvertTo-Json -Compress"
    result=subprocess.run(['pwsh','-NoProfile','-Command',command],env=env,capture_output=True,text=True,timeout=15,check=True)
    paths=json.loads(result.stdout) if result.stdout.strip() else []
    if isinstance(paths,str):paths=[paths]
    if len(paths)!=1:raise ValueError('Cannot establish the owned editor loaded exactly one TurboMCP DLL')
    provenance=json.loads((repo/'.agent/build-provenance.json').read_text(encoding='utf-8-sig'))
    verify_binary(provenance,paths[0])
    return paths[0]
