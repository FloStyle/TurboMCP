# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Offline checks for the compile matrix and the compiler wrapper.

No compiler, engine or supervisor runs here: every case is a pure decision, a
parsed fixture log, or a fixture directory.
"""

import importlib.util
import json
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent


def load(name, filename):
    spec = importlib.util.spec_from_file_location(name, HERE / filename)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


matrix = load('compile_matrix', 'compile-matrix.py')
wrapper = load('compile_with_msvc', 'compile-with-msvc.py')

OURS = '''Using Visual Studio 14.50.35728 toolchain (C:\\Program Files (x86)\\Microsoft Visual Studio\\18\\BuildTools\\VC\\Tools\\MSVC\\14.50.35717) and Windows 10.0.22621.0 SDK
'''
COLLEAGUE = '''Using Visual Studio 14.44.35230 toolchain (C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.44.35207) and Windows 10.0.22621.0 SDK
'''

DIAGNOSTIC_LOG = '''Using Visual Studio 14.50.35728 toolchain (C:\\MSVC\\14.50.35717)
Plugins/TurboMCP/Source/TurboMCP/TurboMCPThing.cpp(12): error C4702: unreachable code
Plugins/TurboMCP/Source/TurboMCP/TurboMCPThing.cpp(40): warning C4100: unreferenced parameter
Runtime/Core/Public/CoreMinimal.h(9): warning C4005: engine noise
'''


def case_needs_compile(failures):
    must = ['Plugins/TurboMCP/Source/TurboMCP/A.cpp',
            'TestProject/Source/TestProject.Target.cs',
            'TestProject/Source/TestProject/A.Build.cs']
    must_not = ['docs/contributing.md', 'Tools/check-toolchain.py', 'TestProject/Content/X.uasset']
    for path in must:
        if not matrix.needs_compile([path]):
            failures.append('compile expected for %s' % path)
    for path in must_not:
        if matrix.needs_compile([path]):
            failures.append('no compile expected for %s' % path)


def case_short_path(failures):
    paths = ['docs/contributing.md']
    if matrix.short_path_applies('abc', 'abc', paths):
        failures.append('the short path must never apply on main itself')
    if not matrix.short_path_applies('abc', 'def', paths):
        failures.append('the short path must apply for a docs-only diff')
    if matrix.short_path_applies('abc', 'def', ['Plugins/TurboMCP/Source/TurboMCP/A.cpp']):
        failures.append('the short path must not apply when a plugin source changed')


def case_diagnostics(failures):
    found = matrix.parse_diagnostics(DIAGNOSTIC_LOG)
    if len(found) != 2:
        failures.append('expected 2 TurboMCP diagnostics, got %r' % found)
    else:
        if found[0]['code'] != 'C4702' or found[0]['severity'] != 'error':
            failures.append('first diagnostic parsed as %r' % found[0])
        if found[1]['code'] != 'C4100' or found[1]['severity'] != 'warning':
            failures.append('second diagnostic parsed as %r' % found[1])
    errors, warnings = matrix.count_diagnostics(found)
    if (errors, warnings) != (1, 1):
        failures.append('counts were %r' % ((errors, warnings),))


def case_report(failures):
    legs = [
        {'leg': 'A', 'status': 'passed', 'errors': 0, 'warnings': 0},
        {'leg': 'B', 'status': 'failed', 'errors': 2, 'warnings': 0},
    ]
    report = matrix.build_report('abc', False, legs)
    if report['result'] != 'failed':
        failures.append('a failing leg did not fail the report')
    report = matrix.build_report('abc', False, [
        {'leg': 'A', 'status': 'passed', 'errors': 0, 'warnings': 0},
        {'leg': 'B', 'status': 'passed', 'errors': 0, 'warnings': 0},
    ])
    if report['result'] != 'passed':
        failures.append('clean legs did not pass the report')
    if report['dirty'] is not False:
        failures.append('the report must record dirty=false')


def case_toolchains(failures):
    sys.path.insert(0, str(HERE))
    toolchain = load('check_toolchain', 'check-toolchain.py')
    with tempfile.TemporaryDirectory() as temp:
        base = pathlib.Path(temp)
        ours = base / 'ours.log'
        ours.write_text(OURS, encoding='utf-8')
        colleague = base / 'colleague.log'
        colleague.write_text(COLLEAGUE, encoding='utf-8')
        if toolchain.toolchain_from_file(ours) != ('14.50.35728', '14.50.35717'):
            failures.append('our 14.50 log line did not parse')
        if toolchain.toolchain_from_file(colleague) != ('14.44.35230', '14.44.35207'):
            failures.append('the colleague 14.44 log line did not parse')


def case_missing_toolset(failures):
    with tempfile.TemporaryDirectory() as temp:
        if matrix.find_msvc_144([pathlib.Path(temp)]) is not None:
            failures.append('a missing 14.44 toolset was reported as installed')
    status = matrix.leg_status(errors=0, warnings=0, skipped_reason='14.44 toolset missing')
    if status != 'failed':
        failures.append('a missing toolset must fail the leg, got %r' % status)


def case_exit_codes(failures):
    if wrapper.exit_code(0, 0, False) != 0:
        failures.append('a clean leg must exit 0')
    if wrapper.exit_code(3, 0, False) != 1:
        failures.append('errors must exit 1')
    if wrapper.exit_code(0, 2, True) != 3:
        failures.append('--allow-warnings with warnings must exit 3')
    if wrapper.exit_code(0, 2, False) != 1:
        failures.append('warnings without --allow-warnings must exit 1')


def case_clang_conversion(failures):
    lines = ['/I "Runtime/Core/Public"',
             '/external:I "C:/MSVC/14.50.35717/INCLUDE"',
             '/D WITH_EDITOR=1',
             '/std:c++20',
             '/Yu"../Intermediate/SharedPCH.h"',
             '/Fp"x.pch"',
             '/we4702']
    converted = wrapper.to_clang_shared(lines)
    joined = ' '.join(converted)
    if any(item.startswith('/Yu') or item.startswith('/Fp') for item in converted):
        failures.append('clang must not receive a PCH: %r' % converted)
    if '/I "Runtime/Core/Public"' not in converted:
        failures.append('clang lost an include path')
    if '/D WITH_EDITOR=1' not in converted:
        failures.append('clang lost a definition')
    if '-isystem "C:/MSVC/14.50.35717/INCLUDE"' not in converted:
        failures.append('external includes were not converted: %r' % converted)
    if '/std:c++20' not in converted:
        failures.append('clang lost the language standard')
    if '/we4702' in joined:
        failures.append('the MSVC-only warning flag reached clang')


def main():
    failures = []
    case_needs_compile(failures)
    case_short_path(failures)
    case_diagnostics(failures)
    case_report(failures)
    case_toolchains(failures)
    case_missing_toolset(failures)
    case_exit_codes(failures)
    case_clang_conversion(failures)
    for failure in failures:
        print('FAIL: %s' % failure)
    if failures:
        print('RESULT: FAIL (%d case(s))' % len(failures))
        return 1
    print('RESULT: PASS (short path, diagnostics, report, log lines, missing toolset, '
          'exit codes, clang conversion)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
