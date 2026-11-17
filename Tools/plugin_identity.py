"""Reject duplicate plugin discovery and prove the owned process loaded our built DLL."""
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import stat
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


def ordinary(path, *, directory=False, optional=False):
    """Check an item without following links, junctions or other reparse points."""
    try:
        info = path.lstat()
    except FileNotFoundError:
        if optional:
            return False
        raise
    expected = stat.S_ISDIR if directory else stat.S_ISREG
    if not expected(info.st_mode) or getattr(info, 'st_file_attributes', 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT:
        raise ValueError(f'Expected an ordinary {"directory" if directory else "file"}: {path}')
    return True


def _extension_children(container):
    if not ordinary(container, directory=True, optional=True):
        return
    for path in container.iterdir():
        info = path.lstat()
        if getattr(info, 'st_file_attributes', 0) & stat.FILE_ATTRIBUTE_REPARSE_POINT or stat.S_ISLNK(info.st_mode):
            raise ValueError(f'Redirected plugin extension directory: {path}')
        if stat.S_ISDIR(info.st_mode):
            yield path


def _plugin_extension_roots(base):
    # FPaths::GetExtensionDirs also checks platform directories inside restricted roots.
    if not ordinary(base, directory=True, optional=True):
        return
    yield base / 'Plugins'
    for platform in _extension_children(base / 'Platforms'):
        yield platform / 'Plugins'
    for restricted in _extension_children(base / 'Restricted'):
        yield from _plugin_extension_roots(restricted)


def check_installed_discovery(project, engine):
    """Verify the installed descriptor without relying on TestProject mounts."""
    expected = project.parent / 'Plugins/TurboMCP/TurboMCP.uplugin'
    expected_dll = expected.parent / 'Binaries/Win64/UnrealEditor-TurboMCP.dll'
    found = []
    roots = [*_plugin_extension_roots(project.parent), *_plugin_extension_roots(engine / 'Engine'),
             project.parent / 'Mods']
    if ordinary(engine / 'Enterprise', directory=True, optional=True):
        roots.append(engine / 'Enterprise/Plugins')
    for root in roots:
        if not ordinary(root, directory=True, optional=True):
            continue
        for directory, children, names in os.walk(root, onerror=_walk_error):
            base = Path(directory)
            for name in children:
                ordinary(base / name, directory=True)
            for name in names:
                file = base / name
                if name.casefold() == expected_dll.name.casefold() and file != expected_dll:
                    raise ValueError(f'Unexpected TurboMCP module in plugin discovery: {file}')
                if file.suffix.casefold() != '.uplugin':
                    continue
                ordinary(file)
                raw = file.read_text(encoding='utf-8-sig')
                # Stock engine descriptors can have trailing commas. Keep quoted strings intact.
                raw = re.sub(r'("(?:[^"\\]|\\.)*")|,\s*(?=[}\]])', lambda match: match[1] or '', raw)
                descriptor = json.loads(raw)
                modules = descriptor.get('Modules', [])
                extends_turbomcp = (descriptor.get('bIsPluginExtension') is True
                                    and file.stem.partition('_')[0].casefold() == 'turbomcp')
                if name.casefold() == expected.name.casefold() or extends_turbomcp or any(
                    module['Name'].casefold() == 'turbomcp' for module in modules
                ):
                    found.append(file)
    if found != [expected]:
        raise ValueError(f'Expected exactly one installed TurboMCP descriptor, found {found}')


def _walk_error(error):
    raise error


def consumer_observations(pid=None):
    """Collect OS evidence for the fixed listener and, when given, its launch PID."""
    query = r"""
    $ErrorActionPreference = 'Stop'
    [Console]::OutputEncoding = [System.Text.Encoding]::UTF8
    $listeners = @(Get-NetTCPConnection -State Listen | Where-Object LocalPort -eq 8000 |
        Select-Object LocalAddress,OwningProcess)
    $process = $null
    $modules = @()
    if ($env:TMCP_CONSUMER_PID) {
        $id = [int]$env:TMCP_CONSUMER_PID
        $process = Get-CimInstance Win32_Process -Filter "ProcessId=$id" |
            Select-Object ProcessId,ExecutablePath,CommandLine
        $modules = @((Get-Process -Id $id).Modules | ForEach-Object { $_.FileName })
    }
    @{listeners=$listeners; process=$process; modules=$modules} | ConvertTo-Json -Compress -Depth 4
    """
    env = dict(os.environ, TMCP_CONSUMER_PID='' if pid is None else str(pid))
    try:
        result = subprocess.run(
            ['pwsh', '-NoProfile', '-NonInteractive', '-Command', query],
            env=env, capture_output=True, encoding='utf-8-sig', timeout=20, check=True,
            creationflags=subprocess.CREATE_NO_WINDOW,
        )
        return json.loads(result.stdout)
    except (OSError, subprocess.SubprocessError, ValueError) as error:
        raise RuntimeError('Cannot enumerate the consumer listener and process identity') from error


def verify_consumer_listener(rows, pid):
    if not isinstance(rows, list) or any(
        not isinstance(row, dict) or type(row.get('OwningProcess')) is not int
        or not isinstance(row.get('LocalAddress'), str) for row in rows
    ):
        raise RuntimeError('Consumer listener inventory is unavailable')
    if pid is None:
        if rows:
            raise RuntimeError(f'Port 8000 already has a listener: {rows}')
        return False
    if any(row['OwningProcess'] != pid or row['LocalAddress'] not in ('127.0.0.1', '::1') for row in rows):
        raise ValueError(f'Unexpected consumer listener ownership or bind address: {rows}')
    return any(row['LocalAddress'] == '127.0.0.1' for row in rows)


def verify_consumer_process(observations, pid, executable, project, expected_modules):
    process = observations.get('process')
    paths = observations.get('modules')
    if (not isinstance(process, dict) or not process.get('ExecutablePath') or not process.get('CommandLine')
            or not isinstance(paths, list) or not paths or any(not isinstance(path, str) for path in paths)):
        raise RuntimeError('Consumer process, launch arguments or loaded modules could not be enumerated')
    args = shlex.split(process['CommandLine'], posix=False)
    if (process.get('ProcessId') != pid or canonical(process['ExecutablePath']) != canonical(executable)
            or len(args) < 2 or canonical(args[1].strip('"')) != canonical(project)):
        raise ValueError('Observed editor does not match the launched consumer executable and project')
    loaded = {}
    for file, sha256 in expected_modules.items():
        matches = [path for path in paths if Path(path).name.casefold() == file.name.casefold()]
        if len(matches) != 1:
            raise ValueError(f'Expected exactly one loaded {file.name}')
        verify_binary({'ArtifactPath': str(file), 'ArtifactSha256': sha256}, matches[0])
        loaded[file.name] = {'path': matches[0], 'sha256': sha256}
    return loaded
