"""Check how engine-side UnrealBuildTool changes are classified.

UnrealBuildTool rewrites its trace files under the engine folder on every run,
including rotating the previous trace to a timestamped backup. Those changes are
generated scratch, not unexplained engine modification, and a removed backup
must be classified the same way as a created one.
"""

import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import package_consumer  # noqa: E402

CASES = [
    ('created trace', 'Engine/Programs/UnrealBuildTool/Trace.uba', 'created',
     'generated UnrealBuildTool trace'),
    ('rotated trace backup', 'Engine/Programs/UnrealBuildTool/Trace-backup-2026.11.17-11.42.50.uba',
     'removed', 'generated UnrealBuildTool trace'),
    ('updated trace backup', 'Engine/Programs/UnrealBuildTool/Trace-backup-2026.11.17-11.42.50.uba',
     'updated', 'generated UnrealBuildTool trace'),
    ('rules metadata', 'Engine/Intermediate/Build/Win64/x64/UnrealEditor/BuildRules/ModuleRules.dll',
     'created', 'generated rules metadata'),
    ('removed build cache', 'Engine/Intermediate/Build/Win64/x64/ActionHistory.bin', 'removed',
     'generated build cache'),
    ('unexplained binary', 'Engine/Binaries/Win64/UnrealEditor-TurboMCP.dll', 'removed', 'unverified'),
    ('unexplained trace elsewhere', 'Engine/Intermediate/Build/Win64/Other.uba', 'removed', 'unverified'),
    ('generated UHT header',
     'Engine/Intermediate/Build/Win64/UnrealEditor/Inc/NetCore/UHT/NetCore.init.gen.cpp',
     'updated', 'generated UHT header'),
    ('Intermediate file that is not UHT metadata',
     'Engine/Intermediate/Build/Win64/UnrealEditor/Inc/NetCore/NetCore.h',
     'updated', 'unverified'),
]


def main():
    failures = 0
    for label, path, action, expected in CASES:
        actual = package_consumer.classify_engine_change(path, action)
        if actual != expected:
            failures += 1
            print('FAIL: %s: expected %s, got %s' % (label, expected, actual))
    if failures:
        print('RESULT: FAIL (%d case(s))' % failures)
        return 1
    print('RESULT: PASS (%d classification cases)' % len(CASES))
    return 0


if __name__ == '__main__':
    sys.exit(main())
