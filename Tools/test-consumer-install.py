"""Check that the installed plugin still matches the archive.

A C++ consumer build writes UnrealBuildTool scratch under the plugin's
Intermediate folder because the package ships source. That scratch is not part of
the package. These cases prove the check still catches a changed file, a missing
file and a new file elsewhere, and tolerates scratch only where it is allowed.
"""

import hashlib
import pathlib
import shutil
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import package_consumer  # noqa: E402

DLL = 'TurboMCP/Binaries/Win64/UnrealEditor-TurboMCP.dll'
DESCRIPTOR = 'TurboMCP/TurboMCP.uplugin'


def write(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)


def manifest_for(root, names):
    files = {}
    for name in names:
        path = root / name
        files[name] = {'bytes': path.stat().st_size,
                       'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    return {'files': files}


def expect_error(callable_, label):
    try:
        callable_()
    except ValueError:
        return 0
    print('FAIL: %s was accepted' % label)
    return 1


def main():
    failures = 0
    with tempfile.TemporaryDirectory() as temp:
        base = pathlib.Path(temp)
        plugins = base / 'Plugins'
        write(plugins / DLL, b'binary')
        write(plugins / DESCRIPTOR, b'{"FileVersion":3}')
        names = [DLL, DESCRIPTOR]
        data = manifest_for(plugins, names)

        if package_consumer.verify_installed(plugins, data) != 2:
            failures += 1
            print('FAIL: a clean installation did not report two files')

        write(plugins / 'TurboMCP/Intermediate/Build/Module.TurboMCP.gen.cpp', b'generated')
        failures += expect_error(lambda: package_consumer.verify_installed(plugins, data),
                                 'UBT scratch without permission')

        allowed = package_consumer.verify_installed(
            plugins, data, generated_prefixes=('TurboMCP/Intermediate/',))
        if allowed != 2:
            failures += 1
            print('FAIL: the generated prefix was not excluded from the comparison')

        generated = 'TurboMCP/Intermediate/UHT/TurboMCP.init.gen.cpp'
        write(plugins / generated, b'shipped header')
        data_with_generated = manifest_for(plugins, names + [generated])
        write(plugins / generated, b'regenerated header')
        if package_consumer.verify_installed(
                plugins, data_with_generated,
                generated_prefixes=('TurboMCP/Intermediate/',)) != 2:
            failures += 1
            print('FAIL: a regenerated generated file failed the comparison')
        failures += expect_error(
            lambda: package_consumer.verify_installed(plugins, data_with_generated),
            'a regenerated file without the generated prefix')
        shutil.rmtree(plugins / 'TurboMCP/Intermediate')
        data = manifest_for(plugins, names)

        write(plugins / 'TurboMCP/Extra.dll', b'not in archive')
        failures += expect_error(
            lambda: package_consumer.verify_installed(
                plugins, data, generated_prefixes=('TurboMCP/Intermediate/',)),
            'a new file outside the generated prefix')
        (plugins / 'TurboMCP/Extra.dll').unlink()

        write(plugins / DLL, b'binary changed')
        failures += expect_error(
            lambda: package_consumer.verify_installed(
                plugins, data, generated_prefixes=('TurboMCP/Intermediate/',)),
            'a changed packaged file')
        write(plugins / DLL, b'binary')

        (plugins / DESCRIPTOR).unlink()
        failures += expect_error(
            lambda: package_consumer.verify_installed(
                plugins, data, generated_prefixes=('TurboMCP/Intermediate/',)),
            'a missing packaged file')

    if failures:
        print('RESULT: FAIL (%d case(s))' % failures)
        return 1
    print('RESULT: PASS (clean install, allowed scratch, new file elsewhere, changed file, missing file)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
