# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Check the toolchain classifier against fixture rules.

The fixture mirrors the shape of Unreal 5.8.4's Windows_SDK.json. The cases
separate the two numbers UnrealBuildTool tracks: the banned and minimum ranges
apply to the compiler version, and the preferred ranges apply to the MSVC family.
"""

import importlib.util
import json
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('check_toolchain', HERE / 'check-toolchain.py')
toolchain = importlib.util.module_from_spec(spec)
spec.loader.exec_module(toolchain)

FIXTURE = {
    'MainVersion': '10.0.22621.0',
    'MinimumVisualCppVersion': '14.38.33130',
    'PreferredVisualCppVersions': ['14.50.35717-14.50.99999', '14.44.35207-14.44.99999'],
    'BannedVisualCppVersions': ['14.50.0-14.50.35722', '14.44.0-14.44.35210',
                                '14.40.0-14.43.99999', '14.39.0-14.39.99999'],
}

CASES = [
    ('14.50.35728', '14.50.35717', 'preferred'),
    ('14.44.35230', '14.44.35207', 'preferred'),
    ('14.44.35209', '14.44.35207', 'banned'),
    ('14.43.34808', '14.43.34808', 'banned'),
    ('14.50.35720', '14.50.35717', 'banned'),
    ('14.38.33130', '14.38.33130', 'allowed'),
]

LOG_OURS = ('Using Visual Studio 14.50.35728 toolchain '
            '(C:\\Program Files (x86)\\Microsoft Visual Studio\\18\\BuildTools\\VC\\Tools\\MSVC\\14.50.35717) '
            'and Windows 10.0.22621.0 SDK\n')
LOG_COLLEAGUE = ('Using Visual Studio 14.44.35230 toolchain '
                 '(C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.44.35207) '
                 'and Windows 10.0.22621.0 SDK\n')


def main():
    failures = 0
    for version, family, expected in CASES:
        actual = toolchain.classify(version, family, FIXTURE)
        if actual != expected:
            failures += 1
            print('FAIL: %s in family %s classified %s, expected %s'
                  % (version, family, actual, expected))

    if toolchain.classify('14.37.10000', '14.37.10000', FIXTURE) != 'below-minimum':
        failures += 1
        print('FAIL: a version below the minimum was not reported')

    with tempfile.TemporaryDirectory() as temp:
        base = pathlib.Path(temp)
        sdk = base / 'Windows_SDK.json'
        sdk.write_text(json.dumps(FIXTURE), encoding='utf-8')

        ours = base / 'ours.log'
        ours.write_text(LOG_OURS, encoding='utf-8')
        version, family = toolchain.toolchain_from_file(ours)
        if (version, family) != ('14.50.35728', '14.50.35717'):
            failures += 1
            print('FAIL: our log parsed as %r' % ((version, family),))
        if toolchain.main(['--log', str(ours), '--sdk', str(sdk)]) != 0:
            failures += 1
            print('FAIL: our own build evidence was refused')

        colleague = base / 'colleague.log'
        colleague.write_text(LOG_COLLEAGUE, encoding='utf-8')
        if toolchain.main(['--log', str(colleague), '--sdk', str(sdk)]) != 0:
            failures += 1
            print('FAIL: the colleague toolchain was refused')

        if toolchain.main(['--version', '14.43.34808', '--sdk', str(sdk)]) != 1:
            failures += 1
            print('FAIL: a banned version did not fail')
        if toolchain.main(['--version', '14.44.35211', '--family', '14.44.35207',
                           '--sdk', str(sdk)]) != 0:
            failures += 1
            print('FAIL: a preferred version failed')

    if failures:
        print('RESULT: FAIL (%d case(s))' % failures)
        return 1
    print('RESULT: PASS (preferred versions 14.50.35728 and 14.44.35230, banned versions '
          '14.44.35209, 14.43.34808 and 14.50.35720, allowed 14.38.33130, below minimum, '
          'log parsing, CLI exits)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
