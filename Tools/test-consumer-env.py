"""Check the consumer picks the engine's bundled .NET runtime for UnrealBuildTool.

UBT targets a .NET version that is not installed machine-wide, so a direct call
must use the runtime the engine ships. These cases cover selection, the PATH and
DOTNET_ROOT it sets, and the loud failure when the engine has no runtime.
"""

import os
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import package_consumer  # noqa: E402


def make_engine(root, versions):
    for version in versions:
        (root / 'Engine' / 'Binaries' / 'ThirdParty' / 'DotNet' / version / 'win-x64').mkdir(parents=True)


def main():
    failures = 0
    with tempfile.TemporaryDirectory() as temp:
        base = pathlib.Path(temp)

        empty = base / 'empty-engine'
        (empty / 'Engine' / 'Binaries' / 'ThirdParty' / 'DotNet').mkdir(parents=True)
        try:
            package_consumer.ubt_environment(empty, {})
        except ValueError:
            pass
        else:
            failures += 1
            print('FAIL: an engine without a bundled runtime was accepted')

        engine = base / 'engine'
        make_engine(engine, ['9.0', '10.0', '10.1'])
        env = {'PATH': r'C:\Windows', 'KEEP': 'yes'}
        updated = package_consumer.ubt_environment(engine, env)
        chosen = pathlib.Path(updated['DOTNET_ROOT'])
        if chosen.name != 'win-x64' or chosen.parent.name != '10.1':
            failures += 1
            print('FAIL: did not select the highest bundled version: %s' % chosen)
        if updated['PATH'] != str(chosen) + os.pathsep + r'C:\Windows':
            failures += 1
            print('FAIL: PATH was not prefixed once: %r' % updated['PATH'])
        if updated.get('KEEP') != 'yes':
            failures += 1
            print('FAIL: the existing environment was not preserved')
        if env.get('DOTNET_ROOT') is not None:
            failures += 1
            print('FAIL: the caller environment was modified')

    if failures:
        print('RESULT: FAIL (%d case(s))' % failures)
        return 1
    print('RESULT: PASS (highest bundled runtime, PATH prefix, preserved environment, missing runtime refused)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
