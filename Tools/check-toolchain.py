# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""Check an MSVC toolchain against the pinned engine's Windows SDK rules.

UnrealBuildTool keeps two numbers apart (MicrosoftPlatformSDK.cs):

* the **version** the compiler reports, which the banned and minimum ranges are
  checked against, and
* the **family**, the MSVC folder name, which the preferred ranges rank.

A UE toolchain line carries both: "Using Visual Studio 14.50.35728 toolchain
(...\\MSVC\\14.50.35717)". That is version 14.50.35728 in family 14.50.35717.
Checking only the folder name would call that build banned, and checking only the
version would rank it against family ranges.

Read both numbers, classify, then fail on a banned version, warn below the
minimum or outside the preferred families, and pass otherwise. This is a release
gate: run it against release evidence, not on every build.
"""

import argparse
import json
import pathlib
import re
import sys

ANY_VERSION = re.compile(r'(?<![\d.])(\d{2}\.\d{2}\.\d{1,5})(?![\d.])')
LOG_VERSION = re.compile(r'Using Visual Studio (\d{2}\.\d{2}\.\d{1,5}) toolchain')
LOG_FAMILY = re.compile(r'MSVC[\\/]+(\d{2}\.\d{2}\.\d{1,5})')


def read_json(path):
    return json.loads(pathlib.Path(path).read_text(encoding='utf-8-sig'))


def parse_version(text):
    match = re.fullmatch(r'(\d{2})\.(\d{2})\.(\d{1,5})', str(text).strip())
    if not match:
        raise ValueError('MSVC version must look like 14.50.35717: %r' % text)
    return tuple(int(part) for part in match.groups())


def parse_range(text):
    bounds = str(text).split('-')
    if len(bounds) == 1:
        value = parse_version(bounds[0])
        return value, value
    if len(bounds) != 2:
        raise ValueError('Unreadable MSVC range: %r' % text)
    low = parse_version(bounds[0])
    high = parse_version(bounds[1])
    if high[2] < 1000:
        high = (high[0], high[1], 99999)
    return low, high


def in_ranges(value, ranges):
    for entry in ranges or []:
        low, high = parse_range(entry)
        if low <= value <= high:
            return True
    return False


def classify(version, family, sdk):
    """Return 'banned', 'below-minimum', 'preferred' or 'allowed'."""
    value = parse_version(version)
    family_value = parse_version(family or version)
    if in_ranges(value, sdk.get('BannedVisualCppVersions')):
        return 'banned'
    minimum = sdk.get('MinimumVisualCppVersion')
    if minimum and value < parse_version(str(minimum).split('-')[0]):
        return 'below-minimum'
    if in_ranges(family_value, sdk.get('PreferredVisualCppVersions')):
        return 'preferred'
    return 'allowed'


def toolchain_from_file(path):
    """Return (version, family) from the toolchain line a build log carries.

    Only "Using Visual Studio <version> toolchain (<dir>)" is accepted. The
    version is the number on that line; the family is the MSVC folder on the same
    line. A missing line, a missing folder or two different versions all fail,
    because a guessed version is worse than no answer.
    """
    text = pathlib.Path(path).read_text(encoding='utf-8', errors='replace')
    lines = [line for line in text.splitlines() if LOG_VERSION.search(line)]
    if not lines:
        raise ValueError('cannot determine the real compiler version in %s' % path)
    found = {}
    for line in lines:
        version = LOG_VERSION.search(line).group(1)
        family = LOG_FAMILY.search(line.replace('/', '\\'))
        found.setdefault(version, family.group(1) if family else None)
    if len(found) > 1:
        raise ValueError('conflicting compiler versions in %s: %s'
                         % (path, ', '.join(sorted(found))))
    version, family = next(iter(found.items()))
    if not family:
        raise ValueError('no MSVC folder on the toolchain line in %s' % path)
    return version, family

def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--version', help='compiler version, for example 14.50.35728')
    parser.add_argument('--family', help='MSVC folder name, for example 14.50.35717')
    parser.add_argument('--log', help='build log or evidence file that names the toolchain')
    parser.add_argument('--sdk', help='Windows_SDK.json; defaults to the pinned engine path')
    parser.add_argument('--engine', help='engine root that contains Engine/Config/Windows')
    args = parser.parse_args(argv)

    if bool(args.version) == bool(args.log):
        parser.error('give exactly one of --version or --log')

    if args.sdk:
        sdk_path = pathlib.Path(args.sdk)
    else:
        if args.engine:
            engine = pathlib.Path(args.engine)
        else:
            from tmcp_paths import find_engine
            engine = pathlib.Path(find_engine())
        sdk_path = engine / 'Engine' / 'Config' / 'Windows' / 'Windows_SDK.json'
    if not sdk_path.is_file():
        print('FAIL: no Windows SDK rules at %s' % sdk_path)
        return 2

    try:
        if args.log:
            version, family = toolchain_from_file(args.log)
        else:
            version, family = args.version, args.family
    except (OSError, ValueError) as error:
        print('FAIL: %s' % error, file=sys.stderr)
        return 2
    family = family or version

    sdk = read_json(sdk_path)
    verdict = classify(version, family, sdk)
    label = 'MSVC %s (family %s)' % (version, family)
    if verdict == 'banned':
        print('FAIL: %s is banned for this engine (%s)' % (label, sdk_path))
        return 1
    if verdict == 'below-minimum':
        print('FAIL: %s is below the engine minimum' % label)
        return 1
    if verdict == 'preferred':
        print('PASS: %s is preferred for this engine' % label)
        return 0
    print('WARN: %s is allowed but not preferred for this engine' % label)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError) as error:
        print('FAIL: %s' % error, file=sys.stderr)
        sys.exit(2)
