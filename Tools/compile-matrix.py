# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Compile the plugin under the three toolchains the release gate needs.

A single compiler hid the 1.0.0 C4702 break. This gate runs three legs on the
current HEAD and writes one report:

* A: MSVC 14.50 through the supervised build profile, the normal UnrealBuildTool
  build;
* B: MSVC 14.44 through compile-with-msvc.py, the preferred second family;
* C: clang-cl 19 or newer, syntax only, with unreachable code as an error.

It refuses a dirty tree, records the HEAD commit, and passes only when every leg
has zero errors and zero warnings in TurboMCP sources. A missing 14.44 toolset or
a missing or old clang-cl fails its leg; none is skipped.

A change that touches no compiled source takes the short path: no leg runs and
the report says no-compiled-sources-changed. On main itself the short path never
applies.
"""

import argparse
import datetime
import importlib.util
import json
import pathlib
import re
import subprocess
import sys
import uuid

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE_PREFIXES = ('Plugins/', 'TestProject/Source/')
SOURCE_SUFFIXES = ('.Build.cs', '.Target.cs')
DIAGNOSTIC = re.compile(r'^(Plugins/TurboMCP/[^(]+)\((\d+)\):\s*(error|warning)\s+(C\d{4})')
MSVC_144_ROOT = re.compile(r'^14\.44\.\d+$')
CLANG = re.compile(r'clang version (\d+)\.(\d+)\.(\d+)')
VERSIONS = pathlib.Path(r'C:\Program Files\Microsoft Visual Studio')
LLVM_PATHS = [
    pathlib.Path(r'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\Llvm\x64\bin\clang-cl.exe'),
    pathlib.Path(r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe'),
]
MSVC_ROOTS = [
    pathlib.Path(r'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC'),
    pathlib.Path(r'C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Tools\MSVC'),
]


def load_toolchain():
    spec = importlib.util.spec_from_file_location('check_toolchain', ROOT / 'Tools/check-toolchain.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def git(repo, *args):
    result = subprocess.run(['git'] + list(args), cwd=str(repo), capture_output=True, text=True)
    if result.returncode:
        raise RuntimeError('git %s failed: %s' % (' '.join(args), result.stderr.strip()))
    return result.stdout.strip()


def tree_clean(repo):
    return git(repo, 'status', '--porcelain') == ''


def head_commit(repo):
    return git(repo, 'rev-parse', 'HEAD')


def needs_compile(paths):
    for path in paths:
        normal = path.replace('\\', '/')
        if normal.startswith(SOURCE_PREFIXES) or normal.endswith(SOURCE_SUFFIXES):
            return True
    return False


def short_path_applies(head, main_head, paths):
    return head != main_head and not needs_compile(paths)


def parse_diagnostics(text):
    found = []
    for line in text.splitlines():
        match = DIAGNOSTIC.match(line.strip())
        if match:
            found.append({'file': match.group(1), 'line': int(match.group(2)),
                          'severity': match.group(3), 'code': match.group(4)})
    return found


def count_diagnostics(diagnostics):
    errors = sum(1 for item in diagnostics if item['severity'] == 'error')
    return errors, len(diagnostics) - errors


def leg_status(errors, warnings, skipped_reason=None):
    if skipped_reason:
        return 'failed'
    return 'passed' if not errors and not warnings else 'failed'


def build_report(commit, dirty, legs):
    passed = bool(legs) and all(leg.get('status') == 'passed' for leg in legs)
    return {'schema': 1, 'commit': commit, 'dirty': bool(dirty),
            'result': 'passed' if passed else 'failed', 'legs': legs,
            'generated': datetime.datetime.now().isoformat(timespec='seconds')}


def find_msvc_144(roots):
    newest = None
    for root in roots:
        if not pathlib.Path(root).is_dir():
            continue
        for child in pathlib.Path(root).iterdir():
            if not child.is_dir() or not MSVC_144_ROOT.match(child.name):
                continue
            version = tuple(int(part) for part in child.name.split('.'))
            if newest is None or version > newest[0]:
                newest = (version, child)
    return newest[1] if newest else None


def compiler_version(text, pattern):
    match = pattern.search(text)
    if not match:
        return None
    major, minor, patch = match.groups()
    if pattern is MSVC_VERSION:
        return '%s.%s.%s' % ('14', minor, patch)
    return '%s.%s.%s' % (major, minor, patch)


MSVC_VERSION = re.compile(r'19\.(\d{2})\.(\d{5})')


def run_leg(identifier, compiler, path, version, family, log, scratch, diagnostics, reason=None):
    errors, warnings = count_diagnostics(diagnostics)
    return {'leg': identifier, 'compiler': compiler,
            'path': str(path) if path else None,
            'version': version, 'family': family,
            'status': leg_status(errors, warnings, reason),
            'reason': reason, 'errors': errors, 'warnings': warnings,
            'diagnostics': ['%s:%d:%s' % (d['file'], d['line'], d['code']) for d in diagnostics],
            'log': str(log), 'scratch': str(scratch)}


def leg_a(repo, out):
    log = out / 'leg-a.log'
    with log.open('w', encoding='utf-8') as handle:
        process = subprocess.run([sys.executable, 'Tools/execute-tests.py', '--profile', 'build',
                                  '--execute'], cwd=str(repo), stdout=handle,
                                 stderr=subprocess.STDOUT)
    text = log.read_text(encoding='utf-8', errors='replace')
    version = family = None
    try:
        version, family = load_toolchain().toolchain_from_file(log)
    except (OSError, ValueError):
        pass
    diagnostics = parse_diagnostics(text)
    reason = None if process.returncode == 0 else 'build exited %d' % process.returncode
    return run_leg('A', 'MSVC 14.50', None, version, family, log, out, diagnostics, reason)


def leg_b(repo, out):
    log = out / 'leg-b.log'
    toolset = find_msvc_144(MSVC_ROOTS)
    if toolset is None:
        return run_leg('B', 'MSVC 14.44', None, None, None, log, out, [],
                       '14.44 toolset missing')
    cl = toolset / 'bin/Hostx64/x64/cl.exe'
    version = None
    if cl.is_file():
        probe = subprocess.run([str(cl)], capture_output=True, text=True)
        version = compiler_version(probe.stdout + probe.stderr, MSVC_VERSION)
    if not version or version < '14.44.35211':
        return run_leg('B', 'MSVC 14.44', cl, version, toolset.name, log, out, [],
                       '14.44 toolset older than 14.44.35211')
    scratch = out / 'leg-b-scratch'
    with log.open('w', encoding='utf-8') as handle:
        process = subprocess.run([sys.executable, 'Tools/compile-with-msvc.py', '--msvc', str(toolset),
                                  '--scratch', str(scratch)], cwd=str(repo), stdout=handle,
                                 stderr=subprocess.STDOUT)
    diagnostics = parse_diagnostics(log.read_text(encoding='utf-8', errors='replace'))
    reason = None if process.returncode == 0 else 'compile exited %d' % process.returncode
    return run_leg('B', 'MSVC 14.44', cl, version, toolset.name, log, out, diagnostics, reason)


def leg_c(repo, out):
    log = out / 'leg-c.log'
    clang = next((path for path in LLVM_PATHS if path.is_file()), None)
    if clang is None:
        return run_leg('C', 'clang-cl', None, None, None, log, out, [],
                       'clang-cl missing')
    probe = subprocess.run([str(clang), '--version'], capture_output=True, text=True)
    version = compiler_version(probe.stdout + probe.stderr, CLANG)
    if not version or int(version.split('.')[0]) < 19:
        return run_leg('C', 'clang-cl', clang, version, None, log, out, [],
                       'clang-cl older than 19')
    scratch = out / 'leg-c-scratch'
    with log.open('w', encoding='utf-8') as handle:
        process = subprocess.run([sys.executable, 'Tools/compile-with-msvc.py', '--clang',
                                  '--msvc', str(clang), '--scratch', str(scratch)],
                                 cwd=str(repo), stdout=handle, stderr=subprocess.STDOUT)
    diagnostics = parse_diagnostics(log.read_text(encoding='utf-8', errors='replace'))
    reason = None if process.returncode == 0 else 'clang exited %d' % process.returncode
    return run_leg('C', 'clang-cl', clang, version, None, log, out, diagnostics, reason)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', default='.')
    parser.add_argument('--full', action='store_true', help='run every leg, even on a docs-only diff')
    args = parser.parse_args(argv)

    repo = pathlib.Path(args.repo).resolve()
    if not tree_clean(repo):
        print('FAIL: working tree is dirty; commit or stash before running the matrix')
        return 2
    commit = head_commit(repo)

    run = repo / '.local/compile-matrix' / uuid.uuid4().hex
    run.mkdir(parents=True)

    main_head = git(repo, 'rev-parse', 'origin/main')
    base = git(repo, 'merge-base', 'HEAD', 'origin/main')
    changed = [line for line in git(repo, 'diff', '--name-only', base, 'HEAD').splitlines() if line]
    if not args.full and short_path_applies(commit, main_head, changed):
        report = {'schema': 1, 'commit': commit, 'dirty': False,
                  'result': 'no-compiled-sources-changed', 'legs': [], 'changed': changed,
                  'generated': datetime.datetime.now().isoformat(timespec='seconds')}
        (run / 'report.json').write_text(json.dumps(report, indent=1) + '\n', encoding='utf-8')
        print('PASS: no-compiled-sources-changed (%d file(s)); report %s' % (len(changed), run / 'report.json'))
        return 0

    legs = [leg_a(repo, run), leg_b(repo, run), leg_c(repo, run)]
    report = build_report(commit, False, legs)
    report['changed'] = changed
    target = run / 'report.json'
    target.write_text(json.dumps(report, indent=1) + '\n', encoding='utf-8')

    for leg in legs:
        print('leg %s %-10s errors=%d warnings=%d status=%s%s'
              % (leg['leg'], leg['compiler'], leg['errors'], leg['warnings'], leg['status'],
                 '' if not leg['reason'] else ' (%s)' % leg['reason']))
    if report['result'] != 'passed':
        print('FAIL: compile matrix is not clean; report %s' % target)
        return 1
    print('PASS: three legs clean; report %s' % target)
    return 0


if __name__ == '__main__':
    sys.exit(main())
