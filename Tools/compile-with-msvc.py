# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Compile the plugin module with a chosen MSVC, writing only to scratch.

The 1.0.0 break was invisible because every gate used one compiler. This tool
reuses the response files UnrealBuildTool already produced for the module and
replays them with a different MSVC, so a second compiler can be checked without
a second engine build.

It rebuilds the shared PCH with the chosen compiler, then compiles the module's
translation units. It never writes into an engine or repository Intermediate
folder: the object, PCH and log paths are redirected to a scratch directory, and
the UBT `/experimental:log` and `/sourceDependencies` outputs are dropped.

Requires a prior build in the repository so the response files exist.
"""

import argparse
import json
import os
import pathlib
import re
import shutil
import subprocess
import sys

MODULE = 'TurboMCP'
SUBDIR = pathlib.Path('TestProject/Intermediate/External/Build/Win64/x64/UnrealEditor/Development') / MODULE
KNOWN_ROOTS = [
    pathlib.Path(r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC'),
    pathlib.Path(r'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC'),
]


def find_compiler(value):
    candidate = pathlib.Path(value)
    if candidate.is_file():
        return candidate
    if candidate.is_dir() and (candidate / 'bin/Hostx64/x64/cl.exe').is_file():
        return candidate / 'bin/Hostx64/x64/cl.exe'
    for root in KNOWN_ROOTS:
        direct = root / value / 'bin/Hostx64/x64/cl.exe'
        if direct.is_file():
            return direct
    raise SystemExit('BLOCKED: no cl.exe for %r' % value)


def read_rsp(path):
    lines = []
    for raw in pathlib.Path(path).read_text(encoding='utf-8', errors='replace').splitlines():
        line = raw.strip()
        if not line or line.startswith('/experimental:log') or line.startswith('/sourceDependencies'):
            continue
        lines.append(line)
    return lines


def split_flags(lines):
    includes, yuu, shared, trailing = [], None, None, []
    seen_object = False
    for line in lines:
        if line.startswith('/FI'):
            includes.append(line)
        elif line.startswith('/Yu'):
            yuu = line[3:].strip('"')
        elif line.startswith('@"'):
            shared = line[1:].strip('"')
        elif line.startswith('/Fo'):
            seen_object = True
        elif seen_object:
            trailing.append(line)
    return includes, yuu, shared, trailing


def retarget_shared(shared_path, cl, scratch, trailing):
    """Replay the module's shared flags with the chosen compiler.

    UBT pins the compiler it used in two ways this must undo: an
    `/external:I <old MSVC>/INCLUDE` entry, and no `/std:` flag, because the
    per-file response carries it. Both are rewritten here, and the trailing
    per-file flags are appended.
    """
    msvc_include = (cl.parents[3] / 'include').as_posix()
    lines = []
    for raw in pathlib.Path(shared_path).read_text(encoding='utf-8', errors='replace').splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.lower().startswith('/external:i') and 'MSVC' in line:
            line = '/external:I "%s"' % msvc_include
        lines.append(line)
    for flag in trailing:
        if flag.lower().startswith('/std:') and any(l.lower().startswith('/std:') for l in lines):
            continue
        if flag not in lines:
            lines.append(flag)
    target = scratch / 'shared.rsp'
    target.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    return target


def run(command, cwd, log):
    with log.open('w', encoding='utf-8') as handle:
        result = subprocess.run(command, cwd=str(cwd), stdout=handle,
                                stderr=subprocess.STDOUT)
    return result.returncode


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--msvc', required=True, help='MSVC version directory or cl.exe path')
    parser.add_argument('--repo', default='.', help='repository with a prior build')
    parser.add_argument('--sources', help='plugin source directory; defaults to the repository copy')
    parser.add_argument('--engine', help='engine root; defaults to the configured engine')
    parser.add_argument('--file', help='compile one translation unit instead of the module')
    parser.add_argument('--scratch', help='scratch directory; defaults to <repo>/.local/x1-compiler')
    parser.add_argument('--allow-warnings', action='store_true',
                        help='diagnosis only: do not fail on warnings')
    args = parser.parse_args(argv)

    repo = pathlib.Path(args.repo).resolve()
    module_dir = repo / SUBDIR
    references = sorted(module_dir.glob('*.obj.rsp'))
    if not references:
        print('BLOCKED: no UBT response files under %s; build the repository first' % module_dir)
        return 2

    engine = pathlib.Path(args.engine) if args.engine else None
    if engine is None:
        from tmcp_paths import find_engine
        engine = pathlib.Path(find_engine())
    engine_source = engine / 'Engine' / 'Source'
    if not engine_source.is_dir():
        print('BLOCKED: no engine source at %s' % engine_source)
        return 2

    cl = find_compiler(args.msvc)
    sources = pathlib.Path(args.sources) if args.sources else repo / 'Plugins/TurboMCP/Source/TurboMCP'
    scratch = pathlib.Path(args.scratch) if args.scratch else repo / '.local/x1-compiler'
    logs = scratch / 'logs'
    logs.mkdir(parents=True, exist_ok=True)

    reference = references[0]
    if args.file:
        wanted = pathlib.Path(args.file).stem + '.obj.rsp'
        matches = [p for p in references if p.name == wanted]
        if not matches:
            print('BLOCKED: no response file for %s' % args.file)
            return 2
        reference = matches[0]
    includes, yuu, shared, trailing = split_flags(read_rsp(reference))
    if not yuu or not shared:
        print('BLOCKED: %s does not name a shared PCH and its response file' % reference.name)
        return 2
    # /Yu is relative to Engine/Source, which is the working directory of every
    # command this tool runs.
    pch_cpp = pathlib.Path(os.path.normpath(engine_source / pathlib.PureWindowsPath(yuu).as_posix()))
    pch_cpp = pch_cpp.with_suffix('.cpp')
    if not pch_cpp.is_file():
        print('BLOCKED: no shared PCH source at %s' % pch_cpp)
        return 2

    pch = scratch / 'pch.pch'
    pch_log = logs / 'pch.log'
    shared = retarget_shared(shared, cl, scratch, trailing)
    pch_rsp = [f'"{pch_cpp}"', f'@"{shared}"', f'/FI"{yuu}"', f'/Yc"{yuu}"', f'/Fp"{pch}"', '/nologo', '/TP']
    rsp_path = scratch / 'pch.rsp'
    rsp_path.write_text('\n'.join(pch_rsp) + '\n', encoding='utf-8')
    code = run([str(cl), '@' + str(rsp_path)], engine_source, pch_log)
    print('pch: exit %d (log %s)' % (code, pch_log))
    if code:
        return 1

    files = sorted(sources.glob('*.cpp'))
    if args.file:
        wanted = pathlib.Path(args.file).name
        files = [p for p in files if p.name == wanted]
        if not files:
            print('BLOCKED: %s is not under %s' % (args.file, sources))
            return 2

    total_errors = 0
    total_warnings = 0
    total_c4702 = 0
    failed = []
    for source in files:
        obj = scratch / (source.stem + '.obj')
        log = logs / (source.stem + '.log')
        lines = [f'"{source}"']
        if args.sources:
            lines.append(f'/I"{sources.as_posix()}"')
        lines += [f'@"{shared}"'] + includes + [
            f'/Yu"{yuu}"', f'/Fp"{pch}"', f'/Fo"{obj}"', '/nologo', '/TP']
        per_file = scratch / (source.stem + '.rsp')
        per_file.write_text('\n'.join(lines) + '\n', encoding='utf-8')
        code = run([str(cl), '@' + str(per_file)], engine_source, log)
        text = log.read_text(encoding='utf-8', errors='replace')
        errors = len(re.findall(r' error ', text))
        warnings = len(re.findall(r' warning C', text))
        c4702 = len(re.findall(r'C4702', text))
        total_errors += errors
        total_warnings += warnings
        total_c4702 += c4702
        if code or errors or (warnings and not args.allow_warnings):
            failed.append(source.name)
        print('%-52s exit=%-3d errors=%-4d warnings=%-3d C4702=%d'
              % (source.name, code, errors, warnings, c4702))

    print()
    print('files=%d errors=%d warnings=%d C4702=%d failing=%d'
          % (len(files), total_errors, total_warnings, total_c4702, len(failed)))
    if total_warnings and args.allow_warnings:
        print('ALLOWING %d WARNING(S): --allow-warnings is for diagnosis only, not a pass'
              % total_warnings)
    if failed:
        print('failing files: %s' % ', '.join(failed))
        return 1
    print('PASS: %d translation units compiled with %s' % (len(files), cl))
    return 0


if __name__ == '__main__':
    sys.exit(main())
