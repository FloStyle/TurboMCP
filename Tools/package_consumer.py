"""Exercise the exact Win64 archive in two supervisor-owned consumer projects."""
import argparse
import http.client
import json
import os
from pathlib import Path, PureWindowsPath
import re
import stat
import subprocess
import sys
import time
import urllib.request
import zipfile

from execution_recovery import digest
from execution_runtime import canonical, require_context, run_owned
from plugin_identity import (check_installed_discovery, consumer_observations, ordinary,
                             verify_consumer_listener, verify_consumer_process)
from tmcp import EDIT_TOOLSET, LOOK_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session, StructuredAnswer


ROOT = Path(__file__).resolve().parents[1]
PHASES = ('bp-only', 'cpp')
PROJECTS = {'bp-only': 'ConsumerBlueprint', 'cpp': 'ConsumerCpp'}
DLL = 'Binaries/Win64/UnrealEditor-TurboMCP.dll'
RECEIPT = 'Binaries/Win64/UnrealEditor.modules'
DESCRIPTOR = 'TurboMCP.uplugin'
REQUIRED_FILES = (DLL, RECEIPT, DESCRIPTOR, 'LICENSE', 'NOTICES.json')
MAX_PACKAGE_BYTES = 1024 * 1024 * 1024
MAX_PACKAGE_FILES = 10000
ASSET = '/Game/TurboMCPConsumer/BFL_InstallSmoke'
OBJECT = ASSET + '.BFL_InstallSmoke'
PARENT = '/Script/Engine.BlueprintFunctionLibrary'
READ_TOOL = 'ReadTurboMCPAssetDescribe'
CREATE_TOOL = 'EditTurboMCPFunctionLibraryCreate'


def hex_id(value, length):
    return isinstance(value, str) and re.fullmatch(r'[0-9a-f]{%d}' % length, value)


DIRTY_HEADER = re.compile(r'^dirty_packages: before=(\d+) after=(\d+) newly_dirty=(\d+) no_longer_dirty=(\d+)$')
DIRTY_REVIEW = ('Review newly dirty packages against the requested targets; this report does not save '
                'or discard them. Dependency/editor side effects may be included.')


def split_edit_reply(reply):
    """Split an edit reply into its message lines and its dirty-package block.

    Every edit reply carries a dirty-package report after the message
    (TurboMCPCallRecorder). The message is compared exactly and the report is
    validated on its own, so a documented report cannot hide a change to the
    operation's own answer.
    """
    lines = reply.splitlines()
    for index, line in enumerate(lines):
        if line.startswith(('dirty_packages:', '  newly_dirty:', '  no_longer_dirty:')):
            return lines[:index], lines[index:]
    return lines, []


def check_dirty_block(block, expect_name=None, require=False):
    """Validate the dirty-package report and return the newly dirty names."""
    if not block:
        if require:
            raise ValueError('Edit reply carried no dirty-package report')
        return []
    header = DIRTY_HEADER.match(block[0])
    if not header:
        raise ValueError('Dirty-package report header is not in the documented form: %r' % block[0])
    newly = int(header.group(3))
    names = [line[len('  newly_dirty: '):] for line in block if line.startswith('  newly_dirty: ')]
    if len(names) != newly:
        raise ValueError('Dirty-package report has %d newly_dirty row(s) for newly_dirty=%d'
                         % (len(names), newly))
    if expect_name is not None and expect_name not in names:
        raise ValueError('Dirty-package report does not name %s' % expect_name)
    if require and block[-1] != DIRTY_REVIEW:
        raise ValueError('Dirty-package report is missing its review sentence')
    return names


def windows_component(name):
    base = name.partition('.')[0].rstrip(' ').upper()
    return (name not in ('', '.', '..') and name[-1] not in '. '
            and not re.search(r'[<>:"/\\|?*\x00-\x1f\x7f]', name)
            and base not in ('CON', 'PRN', 'AUX', 'NUL', 'CONIN$', 'CONOUT$')
            and not re.fullmatch(r'(?:COM|LPT)[0-9\u00b9\u00b2\u00b3]', base))


def validate_artifact_ref(ref):
    if not isinstance(ref, dict) or set(ref) != {'directory', 'source_commit', 'archive_sha256', 'manifest_sha256'}:
        raise ValueError('Consumer requires exactly one artifact identity')
    name = ref['directory']
    if (not isinstance(name, str) or not re.fullmatch(r'TurboMCP-[A-Za-z0-9._+-]{1,180}', name)
            or not windows_component(name)):
        raise ValueError('Consumer artifact must name one package directory')
    if not (hex_id(ref['source_commit'], 40) and hex_id(ref['archive_sha256'], 64)
            and hex_id(ref['manifest_sha256'], 64)):
        raise ValueError('Consumer requires full source, archive and manifest digests')


def read_json(path):
    ordinary(path)
    if path.stat().st_size > 8 * 1024 * 1024:
        raise ValueError(f'JSON evidence exceeds its size limit: {path}')
    def unique(pairs):
        fields = {}
        for key, value in pairs:
            if key in fields:
                raise ValueError(f'Duplicate JSON key in {path}: {key}')
            fields[key] = value
        return fields

    return json.loads(path.read_text(encoding='utf-8-sig'), object_pairs_hook=unique)


def check_run(repo, run):
    if run.parent != repo / '.local/executions' or not hex_id(run.name, 32):
        raise ValueError('Invalid consumer execution directory')
    for directory in (repo, run.parent.parent, run.parent, run):
        ordinary(directory, directory=True)


def create_run(repo, run):
    if run.parent != repo / '.local/executions' or not hex_id(run.name, 32):
        raise ValueError('Invalid consumer execution directory')
    ordinary(repo, directory=True)
    for directory in (run.parent.parent, run.parent):
        if not ordinary(directory, directory=True, optional=True):
            directory.mkdir()
    run.mkdir()
    check_run(repo, run)


def phase_command(phase):
    if phase not in PHASES:
        raise ValueError('Unsupported consumer phase')
    return [sys.executable, 'Tools/package_consumer.py', phase]


def admitted_run(repo, engine, phase):
    command = phase_command(phase)
    context = Path(os.environ.get('TURBOMCP_EXECUTION_CONTEXT', ''))
    run = context.parent
    if context.name != 'context.json':
        raise RuntimeError('Consumer requires the live supervisor context')
    check_run(repo, run)
    ordinary(context)
    require_context(repo, engine)
    report = read_json(run / 'report.json')
    request = report['request']
    if (report['status'] != 'running' or report['active_command'] != command
            or report['engine'] != canonical(engine) or request['profile'] != 'consumer' or request['suites']
            or len(report['results']) != PHASES.index(phase)
            or any(prior['exit_code'] != 0 or prior.get('verdict') != 'passed' for prior in report['results'])):
        raise RuntimeError('Consumer phase is absent from the active, ordered supervisor request')
    validate_artifact_ref(request['consumer_artifact'])
    return run, request


def artifact_paths(repo, ref):
    validate_artifact_ref(ref)
    root = repo / 'release/artifacts' / ref['directory']
    for directory in (repo, root.parent.parent, root.parent, root):
        ordinary(directory, directory=True)
    return root / (root.name + '.zip'), root / 'manifest.json'


def verify_artifact_file(path, sha256, limit):
    ordinary(path)
    if path.stat().st_size > limit or digest(path) != sha256:
        raise ValueError(f'Consumer artifact size or digest differs from the approved identity: {path.name}')


def artifact_files(repo, ref):
    archive, manifest = artifact_paths(repo, ref)
    verify_artifact_file(archive, ref['archive_sha256'], MAX_PACKAGE_BYTES)
    verify_artifact_file(manifest, ref['manifest_sha256'], 8 * 1024 * 1024)
    return archive, manifest


def git(root, *args):
    return subprocess.check_output(
        ['git', '-C', str(root), *args], encoding='utf-8', timeout=60,
        env=dict(os.environ, GIT_OPTIONAL_LOCKS='0'),
    ).strip()


def check_engine(repo, engine):
    ordinary(engine, directory=True)
    pin = read_json(repo / 'engine.lock.json')
    head = git(engine, 'rev-parse', 'HEAD')
    if head != pin['commit'] or git(engine, 'status', '--porcelain', '--untracked-files=no'):
        raise ValueError('Consumer requires the matching clean stock source-engine checkout')
    version = read_json(engine / 'Engine/Build/Build.version')
    actual = '.'.join(str(version[key]) for key in ('MajorVersion', 'MinorVersion', 'PatchVersion'))
    if actual != pin['version']:
        raise ValueError('Consumer engine version differs from its execution pin')
    receipt = read_json(engine / 'Engine/Binaries/Win64/UnrealEditor.modules')
    if not isinstance(receipt.get('BuildId'), str) or not receipt['BuildId']:
        raise ValueError('Missing consumer engine BuildId')
    return {'commit': head, 'version': actual, 'build_id': receipt['BuildId']}


def validate_file_index(files):
    if not isinstance(files, dict) or not 1 <= len(files) <= MAX_PACKAGE_FILES:
        raise ValueError('Invalid consumer manifest file count')
    folded = {}
    total = 0
    for name, entry in files.items():
        parts = name.split('/')
        if len(parts) < 2 or parts[0] != 'TurboMCP' or not all(map(windows_component, parts)):
            raise ValueError(f'Unsafe consumer archive path: {name}')
        if (not isinstance(entry, dict) or set(entry) != {'bytes', 'sha256'} or type(entry['bytes']) is not int
                or entry['bytes'] < 0 or not hex_id(entry['sha256'], 64)):
            raise ValueError(f'Invalid archive file identity: {name}')
        total += entry['bytes']
        for length in range(1, len(parts) + 1):
            path = '/'.join(parts[:length])
            value = (path, length == len(parts))
            if folded.setdefault(path.casefold(), value) != value:
                raise ValueError(f'Case or file-directory collision in consumer archive: {name}')
    if total > MAX_PACKAGE_BYTES:
        raise ValueError('Consumer archive expands beyond its size limit')
    if any(files.get(f'TurboMCP/{name}', {}).get('bytes', 0) < 1 for name in REQUIRED_FILES):
        raise ValueError('Archive is missing a required packaged file')


def validate_zip(zipped, files):
    entries = {}
    for entry in zipped.infolist():
        name = entry.orig_filename
        expected = files.get(name)
        mode = stat.S_IFMT(entry.external_attr >> 16)
        dos_flags = entry.external_attr & 0xffff
        if (not expected or name in entries or entry.filename != name
                or mode not in (0, stat.S_IFREG) or dos_flags & (stat.FILE_ATTRIBUTE_REPARSE_POINT | 0x10)
                or entry.flag_bits & 1 or entry.file_size != expected['bytes']):
            raise ValueError(f'Unexpected, duplicate or special archive entry: {name}')
        entries[name] = entry
    if set(entries) != files.keys():
        raise ValueError('ZIP and manifest file sets differ')
    return entries


def package_metadata(repo, archive, manifest, ref, engine):
    data = read_json(manifest)
    src = ref['source_commit']
    tree = git(repo, 'rev-parse', '--verify', f'{src}:Plugins/TurboMCP')
    source_descriptor = json.loads(git(repo, 'show', f'{src}:Plugins/TurboMCP/{DESCRIPTOR}'))
    expected = {'schema': 1, 'archive': archive.name, 'platform': 'Win64',
                'engine_distribution': 'source-build', 'source_commit': src,
                'plugin_tree': tree, 'engine_commit': engine['commit'], 'engine_version': engine['version'],
                'module_build_id': engine['build_id'], 'archive_sha256': ref['archive_sha256']}
    for key, value in expected.items():
        if data.get(key) != value:
            raise ValueError(f'Package manifest {key} differs from the approved identity')
    prefix = f"TurboMCP-{source_descriptor['VersionName']}-Win64-UE{engine['version']}-{src[:12]}-"
    if not archive.stem.startswith(prefix) or not hex_id(archive.stem[len(prefix):], 32):
        raise ValueError('Packaged ZIP basename differs from its source and engine identities')
    files = data['files']
    validate_file_index(files)
    if files['TurboMCP/' + DLL]['sha256'] != data.get('dll_sha256'):
        raise ValueError('Package DLL hash does not agree with the manifest file list')
    return data, source_descriptor


def extract(zipped, entries, files, plugins):
    for name, entry in entries.items():
        target = plugins.joinpath(*name.split('/'))
        current = plugins
        for part in name.split('/')[:-1]:
            current /= part
            if not ordinary(current, directory=True, optional=True):
                current.mkdir()
        length = 0
        with zipped.open(entry) as source, target.open('xb') as destination:
            while chunk := source.read(1024 * 1024):
                length += len(chunk)
                if length > files[name]['bytes']:
                    raise ValueError(f'Archive entry expanded beyond its declared size: {name}')
                destination.write(chunk)
        if length != files[name]['bytes'] or digest(target) != files[name]['sha256']:
            raise ValueError(f'Extracted archive entry differs from its manifest: {name}')


def inventory(root):
    ordinary(root, directory=True)
    contents = {}
    for directory, children, filenames in os.walk(root, onerror=_raise_walk_error):
        path = Path(directory)
        for name in children:
            ordinary(path / name, directory=True)
        for name in filenames:
            file = path / name
            ordinary(file)
            contents[file.relative_to(root).as_posix()] = file
    return contents


def _raise_walk_error(error):
    raise error


def verify_installed(plugins, data):
    contents = inventory(plugins)
    files = data['files']
    if contents.keys() != files.keys():
        raise ValueError('Installed plugin file set differs from the archive')
    for name, path in contents.items():
        if path.stat().st_size != files[name]['bytes'] or digest(path) != files[name]['sha256']:
            raise ValueError(f'Installed plugin file differs from the archive: {name}')
    return len(contents)


def verify_packaged_descriptor(plugin, source, engine):
    descriptor = read_json(plugin / DESCRIPTOR)
    receipt = read_json(plugin / RECEIPT)
    if (descriptor.get('Installed') is not True or any(descriptor.get(key) != source.get(key)
            for key in ('VersionName', 'Modules', 'Plugins'))
            or receipt.get('BuildId') != engine['build_id']
            or receipt.get('Modules', {}).get('TurboMCP') != Path(DLL).name):
        raise ValueError('Packaged descriptor, dependencies or module receipt differ from the pinned installation')


def phase_root(run, phase):
    return run / ('Blueprint Consumer' if phase == 'bp-only' else 'C++ Consumer')


def write_new(path, content):
    ordinary(path.parent, directory=True)
    with path.open('x', encoding='utf-8', newline='\n') as handle:
        handle.write(content)


def generate_project(owned, phase):
    name = PROJECTS[phase]
    project = owned / (name + '.uproject')
    config = owned / 'Config'
    for directory in (config, owned / 'Content', owned / 'Temp', owned / 'DerivedDataCache'):
        directory.mkdir()
    descriptor = {'FileVersion': 3, 'Plugins': [{'Name': 'TurboMCP', 'Enabled': True}]}
    if phase == 'cpp':
        descriptor['Modules'] = [{'Name': name, 'Type': 'Runtime', 'LoadingPhase': 'Default'}]
        source = owned / 'Source'
        module = source / name
        source.mkdir()
        module.mkdir()
        write_new(source / f'{name}Editor.Target.cs', f"""using UnrealBuildTool;
public class {name}EditorTarget : TargetRules
{{
    public {name}EditorTarget(TargetInfo Target) : base(Target)
    {{
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
        ExtraModuleNames.Add("{name}");
    }}
}}
""")
        write_new(module / f'{name}.Build.cs', f"""using UnrealBuildTool;
public class {name} : ModuleRules
{{
    public {name}(ReadOnlyTargetRules Target) : base(Target)
    {{
        PCHUsage = PCHUsageMode.NoPCHs;
        bUseUnity = false;
        PrivateDependencyModuleNames.Add("Core");
    }}
}}
""")
        write_new(module / f'{name}.cpp', f"""#include "Modules/ModuleManager.h"
IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, {name}, "{name}");
""")
    write_new(project, json.dumps(descriptor, indent=2) + '\n')
    write_new(config / 'DefaultEditorPerProjectUserSettings.ini', """[/Script/TurboMCP.TurboMCPSettings]
bEnableTurboMCP=True
bEnableCallLog=False

[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]
bAutoStartServer=True
bEnableToolSearch=True
ServerPortNumber=8000
ServerUrlPath=/mcp

[/Script/UnrealEd.EditorLoadingSavingSettings]
bAutoSaveEnable=False
""")
    write_new(config / 'DefaultEngine.ini', """[HTTPServer.Listeners]
DefaultBindAddress=127.0.0.1
DefaultReuseAddressAndPort=False
!ListenerOverrides=ClearArray
""")
    return project


def check_build_actions(output, name):
    # Pinned ActionLogger emits one indexed line for every completed local action.
    expected_compile = f'Compile [x64] {name}.cpp'
    expected_link = f'Link [x64] UnrealEditor-{name}.dll'
    allowed = {expected_compile, expected_link, f'Link [x64] UnrealEditor-{name}.lib',
               f'WriteMetadata {name}Editor.target'}
    actions = []
    totals = set()
    indices = []
    for match in re.finditer(r'^\[(\d+)/(\d+)\]\s+([^\r\n]+)', output, re.M):
        index, total, action = match.groups()
        action = action.removesuffix(' [NoUba]')
        if action not in allowed:
            if action.startswith(('Compile ', 'Link ')):
                raise ValueError(f'Consumer build ran an unexpected compile/link action: {action}')
            raise RuntimeError(f'Consumer build has an unclassified prerequisite action: {action}')
        actions.append(action)
        indices.append(int(index))
        totals.add(int(total))
    if (not {expected_compile, expected_link}.issubset(actions)
            or totals != {len(actions)} or sorted(indices) != list(range(1, len(actions) + 1))):
        raise RuntimeError('Cannot prove all compile and link actions for the consumer module')
    return actions


def engine_output_metadata(engine):
    # Inspect the prebuilt engine's UBT output and rules areas for generated changes.
    files = {}
    for relative in ('Engine/Intermediate/Build', 'Engine/Binaries/Win64', 'Engine/Programs/UnrealBuildTool'):
        root = engine / relative
        if ordinary(root, directory=True, optional=True):
            for path in inventory(root).values():
                info = path.stat()
                files[path.relative_to(engine).as_posix()] = (info.st_size, info.st_mtime_ns)
    return files


def record_prerequisites(engine, before):
    after = engine_output_metadata(engine)
    changes = []
    for path in sorted(before.keys() | after.keys()):
        if before.get(path) == after.get(path):
            continue
        entry = {'path': path, 'action': 'updated' if path in before else 'created',
                 'disposition': 'unverified'}
        if path not in after:
            entry['action'] = 'removed'
        elif '/BuildRules/' in path:
            entry['disposition'] = 'generated rules metadata'
        elif PureWindowsPath(path).name in ('ActionHistory.bin', 'SourceFileCache.bin', 'DependencyCache.bin'):
            entry['disposition'] = 'generated build cache'
        elif path.startswith('Engine/Programs/UnrealBuildTool/') and Path(path).suffix == '.uba':
            entry['disposition'] = 'generated UnrealBuildTool trace'
        if entry['disposition'] != 'unverified':
            try:
                git(engine, 'check-ignore', '--', path)
            except subprocess.CalledProcessError as error:
                if error.returncode != 1:
                    raise
                entry['disposition'] = 'unverified'
        changes.append(entry)
    return changes


def project_metadata_outputs(data, project, engine, build_id):
    """Select only the owned project outputs from stock UBT's target metadata."""
    binaries = project.parent / 'Binaries/Win64'
    target = project.stem + 'Editor'
    version = engine / 'Engine/Binaries/Win64/UnrealEditor.version'
    receipt_path = binaries / (target + '.target')
    module_path = binaries / 'UnrealEditor.modules'
    plugin_path = project.parent / 'Plugins/TurboMCP' / RECEIPT
    receipt = data['Receipt']
    if (Path(data['ProjectFile']) != project or Path(receipt['ProjectFile']) != project
            or Path(data['VersionFile']) != version or data['Version'] is not None
            or Path(data['ReceiptFile']) != receipt_path or receipt['TargetName'] != target
            or receipt['Platform'] != 'Win64' or receipt['Configuration'] != 'Development'
            or receipt['Version'].get('BuildId') not in (None, '', build_id)
            or data['FileToLoadOrderManifest'] or data['MergeModules'] is not False):
        raise ValueError('UBT target metadata does not identify the owned Win64 editor receipt')
    mappings = {
        module_path: {project.stem: f'UnrealEditor-{project.stem}.dll'},
        plugin_path: {'TurboMCP': Path(DLL).name},
    }
    selected = {}
    seen = set()
    for destination, item in data['FileToManifest'].items():
        path = Path(destination)
        if path not in mappings or path in seen or item['ModuleNameToFileName'] != mappings[path]:
            raise ValueError(f'Unexpected UBT module manifest output: {destination}')
        seen.add(path)
        if path == module_path:
            selected[destination] = item
    if not selected:
        raise ValueError('UBT did not generate the owned consumer module manifest input')
    return {**data, 'FileToManifest': selected}, receipt_path, module_path


def write_stock_project_receipt(ubt, project, engine, build_id, env, evidence):
    owned = project.parent
    metadata = owned / 'Intermediate/Build/Win64/x64' / (project.stem + 'Editor') / 'Development/TargetMetadata.json'
    for parent in reversed(tuple(metadata.parent.relative_to(owned).parents)):
        ordinary(owned / parent, directory=True)
    ordinary(metadata.parent, directory=True)
    ordinary(metadata)
    raw_hash = digest(metadata)
    data = read_json(metadata)
    selected, receipt, manifest = project_metadata_outputs(data, project, engine, build_id)
    report = evidence['project_metadata'] = {
        'original': str(metadata), 'original_sha256': raw_hash,
        'original_outputs': [data['ReceiptFile'], *data['FileToManifest']],
        'selected_outputs': [selected['ReceiptFile'], *selected['FileToManifest']],
    }
    if not (ordinary(receipt, optional=True) and ordinary(manifest, optional=True)):
        version = Path(data['VersionFile'])
        if read_json(version).get('BuildId') != build_id:
            raise ValueError('UBT metadata input engine version does not match the editor BuildId')
        for output in (receipt, manifest):
            if ordinary(output, optional=True):
                raise ValueError(f'UBT produced only part of the project metadata: {output}')
        input_path = owned / 'ConsumerTargetMetadata.json'
        write_new(input_path, json.dumps(selected, indent=2) + '\n')
        report['selected_input_sha256'] = digest(input_path)
        # Match an ordinary recursive UBT action. This skips the default engine trace.
        command = [str(ubt), f'-Session={owned.parent.name}', '-Mode=WriteMetadata',
                   f'-Input={input_path}', '-Version=2']
        report['command'] = command
        with (owned / 'ubt-metadata.log').open('xb') as output:
            result = subprocess.run(command, cwd=engine / 'Engine/Source', env=env, stdout=output,
                                    stderr=subprocess.STDOUT, timeout=60, creationflags=subprocess.CREATE_NO_WINDOW)
        report['exit_code'] = result.returncode
        if result.returncode:
            raise ValueError(f'Stock UBT metadata writer failed with exit {result.returncode}')
        if digest(input_path) != report['selected_input_sha256']:
            raise ValueError('Stock UBT metadata input changed while writing the project receipt')
    if digest(metadata) != raw_hash:
        raise ValueError('Original UBT target metadata changed while writing the project receipt')
    written = read_json(manifest)
    written_receipt = read_json(receipt)
    if (written.get('BuildId') != build_id or written.get('Modules') != {
            project.stem: f'UnrealEditor-{project.stem}.dll'}
            or written_receipt.get('TargetName') != project.stem + 'Editor'
            or written_receipt.get('Version', {}).get('BuildId') != build_id):
        raise ValueError('Stock-generated consumer module receipt does not match the editor and consumer DLL')
    report.update(build_id=build_id, receipt_sha256=digest(receipt), module_manifest_sha256=digest(manifest))


def ubt_environment(engine, env):
    """Point a direct UnrealBuildTool call at the engine's bundled .NET runtime.

    UnrealBuildTool targets a .NET version that is not installed machine-wide on
    this host. RunUAT sets the runtime up for itself; a direct UBT call does not,
    and the host then fails with a framework-not-found exit before UBT writes
    any log. Use the runtime the engine already ships.
    """
    root = engine / 'Engine/Binaries/ThirdParty/DotNet'
    candidates = []
    for path in root.glob('*/win-x64') if root.is_dir() else []:
        parts = path.parent.name.split('.')
        if all(part.isdigit() for part in parts):
            candidates.append((tuple(int(part) for part in parts), path))
    if not candidates:
        raise ValueError('The engine ships no bundled win-x64 dotnet runtime for UnrealBuildTool')
    bundled = max(candidates)[1]
    updated = dict(env)
    updated['DOTNET_ROOT'] = str(bundled)
    updated['PATH'] = str(bundled) + os.pathsep + updated.get('PATH', '')
    return updated


def build_consumer(project, engine, build_id, env, evidence):
    name = project.stem
    owned = project.parent
    ubt = engine / 'Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.exe'
    ordinary(ubt)
    log = owned / 'ubt-stdout.log'
    command = [str(ubt), name + 'Editor', 'Win64', 'Development', f'-Project={project}', f'-Module={name}',
               '-NoHotReload', '-NoEngineChanges', '-NoUBA', '-NoXGE', '-NoFASTBuild', '-NoSNDBS',
               f'-Log={owned / "ubt.log"}', f'-XmlConfigCache={owned / "XmlConfigCache.bin"}']
    before = engine_output_metadata(engine)
    evidence['build_command'] = command
    try:
        with log.open('xb') as output:
            result = subprocess.run(command, cwd=engine / 'Engine/Source', env=env, stdout=output,
                                    stderr=subprocess.STDOUT, timeout=900, creationflags=subprocess.CREATE_NO_WINDOW)
        evidence['build_exit'] = result.returncode
        if result.returncode:
            raise ValueError(f'Consumer module build failed with exit {result.returncode}')
        evidence['build_actions'] = check_build_actions(log.read_text(encoding='utf-8-sig'), name)
        binary = owned / 'Binaries/Win64' / f'UnrealEditor-{name}.dll'
        ordinary(owned / 'Binaries', directory=True)
        ordinary(binary.parent, directory=True)
        ordinary(binary)
        write_stock_project_receipt(ubt, project, engine, build_id, env, evidence)
    finally:
        evidence['engine_prerequisite_outputs'] = record_prerequisites(engine, before)
    if any(entry['disposition'] == 'unverified' for entry in evidence['engine_prerequisite_outputs']):
        raise RuntimeError('Engine output changes include unclassified prerequisites')
    return binary, digest(binary)


class UnknownMutation(RuntimeError):
    pass


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


class ConsumerSession(Session):
    """Use the shared MCP handshake with owned, single-attempt loopback requests."""
    def __init__(self, guard):
        super().__init__()
        self.guard = guard
        self.opener = urllib.request.build_opener(urllib.request.ProxyHandler({}), NoRedirect())

    def rpc(self, method, params=None, request_id=1, mutates=None, before_send=None):
        self.guard()
        payload = {'jsonrpc': '2.0', 'id': request_id, 'method': method}
        if params is not None:
            payload['params'] = params
        headers = {'Content-Type': 'application/json', 'Accept': 'application/json, text/event-stream'}
        if self.id:
            headers['Mcp-Session-Id'] = self.id
        request = urllib.request.Request('http://127.0.0.1:8000/mcp', json.dumps(payload).encode(), headers)
        if before_send:
            before_send()
        try:
            with self.opener.open(request, timeout=25) as response:
                raw = response.read(4 * 1024 * 1024 + 1)
                session = response.headers.get('Mcp-Session-Id')
            if len(raw) > 4 * 1024 * 1024:
                raise ValueError('Consumer RPC response exceeds its bound')
            body = raw.decode('utf-8')
            if body.lstrip().startswith(('event:', 'data:')):
                lines = [line[5:].strip() for line in body.splitlines() if line.startswith('data:')]
                if len(lines) != 1:
                    raise ValueError('Unexpected consumer RPC event stream')
                body = lines[0]
            reply = json.loads(body)
            if reply.get('jsonrpc') != '2.0' or reply.get('id') != request_id or 'result' not in reply:
                raise ValueError('Consumer RPC response did not confirm the requested operation')
            self.id = session or self.id
            return reply['result']
        except (OSError, http.client.HTTPException, AttributeError, ValueError) as error:
            if mutates:
                raise UnknownMutation('Consumer edit outcome is unknown. Inspect the owned project before retrying.') from error
            raise


def call_text(session, name, arguments, *, mutates=False, before_send=None):
    result = session.rpc('tools/call', {'name': name, 'arguments': arguments},
                         mutates=mutates, before_send=before_send)
    blocks = result.get('content') if isinstance(result, dict) else None
    if (not isinstance(blocks, list) or not blocks
            or any(not isinstance(block, dict) or not isinstance(block.get('text'), str) for block in blocks)):
        if mutates:
            raise UnknownMutation('Consumer edit returned no usable result')
        raise ValueError(f'Consumer tool returned no text: {name}')
    text = StructuredAnswer([block['text'] for block in blocks], None).unwrapped_text()
    if result.get('isError'):
        raise ValueError(f'Consumer tool reported an MCP error: {text[:320]}')
    return text


def verify_schema(session, toolset, tool, field):
    schema = json.loads(call_text(session, 'describe_toolset', {'toolset_name': toolset}))
    matches = [row['inputSchema'] for row in schema['tools'] if row['name'].rsplit('.', 1)[-1] == tool]
    if len(matches) != 1:
        raise ValueError(f'Installed schema did not expose exactly one {tool}')
    shape = matches[0]
    keys = [name for name in shape.get('required', []) if name.casefold() == field.casefold()]
    if len(keys) != 1 or shape.get('properties', {}).get(keys[0], {}).get('type') != 'string':
        raise ValueError(f'Installed schema did not expose {tool} with required string {field}')
    return keys[0]


def record_mutation(owned, label, stage, outcome=None):
    path = owned / f'{label}-{stage}.json'
    data = {'tool': CREATE_TOOL, 'operation': label, 'stage': stage}
    if outcome is not None:
        data['outcome'] = outcome
    ordinary(owned, directory=True)
    with path.open('x', encoding='utf-8', newline='\n') as output:
        json.dump(data, output)
        output.write('\n')
        output.flush()
        os.fsync(output.fileno())


def mutation_progress(owned):
    """An edit with a durable dispatch record and no usable completion is ambiguous."""
    edits = {}
    if not ordinary(owned, directory=True, optional=True):
        return edits
    for operation in ('create', 'duplicate'):
        pending = owned / f'{operation}-pending.json'
        if not ordinary(pending, optional=True):
            continue
        try:
            record = read_json(pending)
        except (OSError, ValueError):
            # An incomplete pending write never authorizes sending the request.
            edits[operation] = 'dispatch_record_incomplete'
            continue
        if record != {'tool': CREATE_TOOL, 'operation': operation, 'stage': 'pending'}:
            edits[operation] = 'dispatch_record_incomplete'
            continue
        edits[operation] = 'outcome_unknown'
        complete = owned / f'{operation}-completed.json'
        try:
            report = read_json(complete)
        except (OSError, ValueError):
            continue
        allowed = {'created', 'reported_failure'} if operation == 'create' else {'already_exists', 'reported_failure'}
        if (isinstance(report, dict) and report.get('tool') == CREATE_TOOL and report.get('operation') == operation
                and report.get('stage') == 'completed' and report.get('outcome') in allowed):
            edits[operation] = report['outcome']
    return edits


def smoke(session, content, evidence):
    results = []
    evidence['calls'] = results

    def tool(toolset, name, arguments, label, *, mutates=False):
        before_send = (lambda: record_mutation(content.parent, label, 'pending')) if mutates else None
        raw = call_text(session, 'call_tool', {
            'toolset_name': toolset, 'tool_name': name, 'arguments': arguments,
        }, mutates=mutates, before_send=before_send)
        results.append({'call': label, 'tool': name, 'reply': raw[:4000]})
        return raw

    if inventory(content):
        raise ValueError('Consumer Content must begin empty')
    initialized = session.connect()
    if not isinstance(initialized, dict) or 'protocolVersion' not in initialized or not session.id:
        raise ValueError('Installed MCP server did not establish a session')
    listed = session.rpc('tools/list')
    names = {row['name'] for row in listed['tools']}
    if not {'list_toolsets', 'describe_toolset', 'call_tool'}.issubset(names):
        raise ValueError('Installed MCP server did not advertise expected discovery tools')
    listed = call_text(session, 'list_toolsets', {})
    registered = re.findall(r'^- ([^:\r\n]+)', listed, re.M)
    expected = {READ_TOOLSET, LOOK_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET}
    if any(registered.count(name) != 1 for name in expected):
        raise ValueError('Installed TurboMCP toolsets are missing or duplicated')
    evidence['registered_toolsets'] = sorted(expected)
    assets_key = verify_schema(session, READ_TOOLSET, READ_TOOL, 'Assets')
    package_key = verify_schema(session, EDIT_TOOLSET, CREATE_TOOL, 'PackagePath')
    evidence['schema_fields'] = {READ_TOOL: assets_key, CREATE_TOOL: package_key}

    def describe(label):
        return tool(READ_TOOLSET, READ_TOOL, {assets_key: OBJECT}, label)

    def create(label):
        return tool(EDIT_TOOLSET, CREATE_TOOL, {package_key: ASSET}, label, mutates=True)

    before = describe('absent')
    if not before.startswith('read: describing 1 asset(s), 0 described, 1 refused') or (
            f"error: no asset named '{OBJECT}'." not in before):
        raise ValueError('Initial read did not prove the exact consumer asset was absent')
    if inventory(content):
        raise ValueError('Consumer Content changed before the edit')
    created = create('create')
    lines, dirty = split_edit_reply(created)
    if lines != ['changed', f'Created Blueprint function library: {OBJECT}', f'Parent: {PARENT}',
                 f'Generated class: {OBJECT}_C',
                 'Unsaved changes: yes. No disk save was requested. Inspect the library before saving; do not automatically retry creation.']:
        if any(line.startswith('error:') for line in lines) or lines[:1] == ['unchanged']:
            record_mutation(content.parent, 'create', 'completed', 'reported_failure')
            raise ValueError('Consumer function library creation returned a refusal or partial failure')
        raise UnknownMutation('Consumer creation response did not confirm all expected outcomes')
    check_dirty_block(dirty, ASSET, require=True)
    record_mutation(content.parent, 'create', 'completed', 'created')
    described = describe('after_create')
    expected_lines = {f'package: {ASSET}', f'object: {OBJECT}', 'class: Blueprint',
                      f'parent: {PARENT}', f'generated class: {OBJECT}_C', 'loaded: yes',
                      'unsaved changes: yes', 'on disk: no (no saved package found)'}
    if not described.startswith('read: describing 1 asset(s), all 1 described') or (
            not expected_lines.issubset(line.strip() for line in described.splitlines())):
        raise ValueError('Independent read did not verify the created unsaved function library')
    duplicate = create('duplicate')
    dup_lines, dup_dirty = split_edit_reply(duplicate)
    if dup_lines != ['unchanged', 'refused: The destination package already exists in memory, the registry or on disk: ' + ASSET]:
        if dup_lines[:1] in (['changed'], ['unchanged']):
            record_mutation(content.parent, 'duplicate', 'completed', 'reported_failure')
            raise ValueError('Duplicate consumer create did not refuse as expected')
        raise UnknownMutation('Duplicate consumer create returned an ambiguous result')
    check_dirty_block(dup_dirty)
    record_mutation(content.parent, 'duplicate', 'completed', 'already_exists')
    if describe('after_refusal') != described or inventory(content):
        raise ValueError('Duplicate create changed the consumer asset or wrote Content')


def launch_and_probe(project, engine, modules, env, evidence):
    owned = project.parent
    exe = engine / 'Engine/Binaries/Win64/UnrealEditor.exe'
    ordinary(exe)
    log = owned / 'editor.log'
    command = [str(exe), str(project), '-unattended', '-NoSplash', '-NoLiveCoding',
               '-stdout', f'-abslog={log}', '-DDC=(Local)']
    evidence['editor_command'] = command
    verify_consumer_listener(consumer_observations()['listeners'], None)
    with (owned / 'editor-stdout.log').open('xb') as output:
        editor = subprocess.Popen(command, cwd=owned, env=env, stdout=output, stderr=subprocess.STDOUT)
        evidence['editor_pid'] = editor.pid

        def guard():
            if editor.poll() is not None:
                raise RuntimeError(f'Consumer editor exited: {editor.returncode}')
            actual = consumer_observations(editor.pid)
            if not verify_consumer_listener(actual['listeners'], editor.pid):
                raise RuntimeError('Owned editor has no IPv4 loopback MCP listener')
            evidence['listeners'] = actual['listeners']
            evidence['loaded_modules'] = verify_consumer_process(actual, editor.pid, exe, project, modules)
            if editor.poll() is not None:
                raise RuntimeError('Consumer editor exited during identity verification')

        try:
            deadline = time.monotonic() + 240
            while time.monotonic() < deadline:
                if editor.poll() is not None:
                    raise RuntimeError(f'Consumer editor did not start: exit {editor.returncode}')
                rows = consumer_observations()['listeners']
                ready = verify_consumer_listener(rows, editor.pid)
                if ready and ordinary(log, optional=True):
                    # The endpoint can listen before TurboMCP's PostEngineInit registration.
                    with log.open(encoding='utf-8', errors='replace') as stream:
                        if any('LogTurboMCP: TurboMCP toolsets registered' in line for line in stream):
                            break
                time.sleep(1)
            else:
                raise RuntimeError('Owned editor did not reach registration and loopback readiness')
            smoke(ConsumerSession(guard), owned / 'Content', evidence)
        finally:
            if editor.poll() is None:
                evidence['editor_shutdown'] = 'terminated owned editor handle after runtime observation'
                editor.terminate()
                editor.wait(timeout=15)
            evidence['editor_exit'] = editor.returncode


def execute_phase(repo, engine, run, request, phase, evidence):
    ref = request['consumer_artifact']
    archive, manifest = artifact_files(repo, ref)
    identity = check_engine(repo, engine)
    data, descriptor = package_metadata(repo, archive, manifest, ref, identity)
    evidence.update(harness_source=request['commit'], package_source=ref['source_commit'],
                    archive_sha256=ref['archive_sha256'], manifest_sha256=ref['manifest_sha256'],
                    packaged_dll_sha256=data['dll_sha256'], engine=identity)
    owned = phase_root(run, phase)
    plugins = owned / 'Plugins'
    with zipfile.ZipFile(archive) as zipped:
        entries = validate_zip(zipped, data['files'])
        owned.mkdir()
        plugins.mkdir()
        extract(zipped, entries, data['files'], plugins)
    plugin = plugins / 'TurboMCP'
    evidence['installed_path'] = str(plugin)
    evidence['installed_files'] = verify_installed(plugins, data)
    verify_packaged_descriptor(plugin, descriptor, identity)
    project = generate_project(owned, phase)
    check_installed_discovery(project, engine)
    ignored = {'UBT_EXTRA_ARGS', 'UNREALBUILDTOOL_TMP', 'UE_ADDITIONAL_PLUGIN_PATHS'}
    env = {key: value for key, value in os.environ.items() if key.upper() not in ignored}
    env.update(TEMP=str(owned / 'Temp'), TMP=str(owned / 'Temp'))
    env['UE-LOCALDATACACHEPATH'] = str(owned / 'DerivedDataCache')
    modules = {plugin / DLL: data['dll_sha256']}
    if phase == 'cpp':
        env = ubt_environment(engine, env)
        binary, sha256 = build_consumer(project, engine, identity['build_id'], env, evidence)
        if check_engine(repo, engine) != identity:
            raise ValueError('Engine identity changed during the consumer build')
        verify_installed(plugins, data)
        modules[binary] = sha256
    launch_and_probe(project, engine, modules, env, evidence)
    if phase == 'bp-only' and any((owned / path).exists() for path in ('Source', 'Binaries')):
        raise ValueError('Blueprint-only consumer produced project source or binaries')


def run_phase(repo, engine, run, request, command, env, log, timeout):
    """The supervisor owns post-exit checks, including for a dead child."""
    phase = command[-1]
    errors = []
    after = {}
    started = time.monotonic()
    ref = request['consumer_artifact']
    try:
        _, manifest = artifact_paths(repo, ref)
        verify_artifact_file(manifest, ref['manifest_sha256'], 8 * 1024 * 1024)
        original_manifest = read_json(manifest)
        validate_file_index(original_manifest['files'])
    except (OSError, ValueError, KeyError, TypeError) as error:
        return {'exit_code': 1, 'timed_out': False, 'verdict': 'failed',
                'post_check_errors': [f'Package manifest admission: {error}']}
    try:
        result = run_owned(command, str(repo), env, str(log), timeout)
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        result = {'exit_code': 2, 'timed_out': False, 'duration_seconds': time.monotonic() - started}
        errors.append(f'Owned execution: {error}')

    child_code = result['exit_code']
    verdict = {0: 'passed', 1: 'failed', 2: 'blocked', 3: 'outcome_unknown'}.get(child_code, 'failed')
    if result['timed_out']:
        verdict = 'timed_out'
    try:
        check_run(repo, run)
        edits = mutation_progress(phase_root(run, phase))
        after['edits'] = edits
        if 'outcome_unknown' in edits.values():
            verdict = 'outcome_unknown'
            if child_code == 0:
                result['exit_code'] = 3
        elif 'dispatch_record_incomplete' in edits.values() or (
                child_code == 0 and edits != {'create': 'created', 'duplicate': 'already_exists'}):
            errors.append('Consumer edit dispatch or completion evidence is incomplete')
    except (OSError, ValueError) as error:
        errors.append(f'Consumer edit evidence after child exit: {error}')
    for key, index, limit in (('archive_sha256', 0, MAX_PACKAGE_BYTES),
                              ('manifest_sha256', 1, 8 * 1024 * 1024)):
        try:
            path = artifact_paths(repo, ref)[index]
            verify_artifact_file(path, ref[key], limit)
            after[key] = ref[key]
        except (OSError, ValueError) as error:
            errors.append(f'Original package after child exit: {error}')
    try:
        check_run(repo, run)
        owned = phase_root(run, phase)
        if ordinary(owned, directory=True, optional=True):
            after['installed_files'] = verify_installed(owned / 'Plugins', original_manifest)
        elif child_code == 0:
            raise ValueError('Successful consumer phase did not leave an installation')
        details = run / f'{phase}-consumer.json'
        if ordinary(details, optional=True):
            after['phase_evidence'] = details.name
        elif child_code == 0:
            raise ValueError('Successful consumer phase did not leave its evidence record')
    except (OSError, ValueError, KeyError) as error:
        errors.append(f'Package integrity after child exit: {error}')
    try:
        verify_consumer_listener(consumer_observations()['listeners'], None)
        after['listener_after_teardown'] = 'port 8000 free'
        after['engine'] = check_engine(repo, engine)
    except (OSError, ValueError, KeyError, RuntimeError, subprocess.SubprocessError) as error:
        errors.append(f'Engine or listener after child exit: {error}')

    if errors and result['exit_code'] == 0:
        result['exit_code'] = 1
        verdict = 'failed'
    result.update(verdict=verdict, child_exit_code=child_code, after_teardown=after, post_check_errors=errors)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('phase', choices=PHASES)
    phase = parser.parse_args().phase
    engine = Path(os.environ.get('TURBOMCP_ENGINE', ''))
    try:
        run, request = admitted_run(ROOT, engine, phase)
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        print(f'REFUSED: {error}', file=sys.stderr)
        return 2

    evidence = {'phase': phase}
    code = 0
    try:
        execute_phase(ROOT, engine, run, request, phase, evidence)
        evidence['verdict'] = 'passed'
    except UnknownMutation as error:
        evidence.update(verdict='outcome_unknown', reason=str(error))
        code = 3
    except (OSError, http.client.HTTPException, RuntimeError, subprocess.SubprocessError) as error:
        evidence.update(verdict='blocked', reason=str(error))
        code = 2
    except (ValueError, KeyError, TypeError, zipfile.BadZipFile) as error:
        evidence.update(verdict='failed', reason=str(error))
        code = 1
    check_run(ROOT, run)
    write_new(run / f'{phase}-consumer.json', json.dumps(evidence, indent=2) + '\n')
    print(f"RESULT: {evidence['verdict']} ({phase})")
    if code:
        print(evidence['reason'])
    return code


if __name__ == '__main__':
    sys.exit(main())
