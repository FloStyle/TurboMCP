"""Check the engine-distribution resolver and identity rules.

The packaging path must work for a clean source checkout and for an installed
Launcher engine, and the distribution must come from local configuration. These
cases cover the selection order, the Launcher identity fields, and the refusals.
"""

import json
import os
import pathlib
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import engine_distribution  # noqa: E402

PIN = {'version': '5.8.4', 'commit': 'f45729c0b6f097038e72b52d366a62baeca3275b'}


def make_engine(root, version='5.8.4', promoted=1, changelist=58902052, build_id='55116800'):
    parts = version.split('.')
    build = root / 'Engine' / 'Build'
    build.mkdir(parents=True, exist_ok=True)
    (build / 'Build.version').write_text(json.dumps({
        'MajorVersion': int(parts[0]), 'MinorVersion': int(parts[1]), 'PatchVersion': int(parts[2]),
        'Changelist': changelist, 'CompatibleChangelist': 55116800,
        'IsLicenseeVersion': 0, 'IsPromotedBuild': promoted,
        'BranchName': '++UE5+Release-5.8'}), encoding='utf-8')
    binaries = root / 'Engine' / 'Binaries' / 'Win64'
    binaries.mkdir(parents=True, exist_ok=True)
    (binaries / 'UnrealEditor.modules').write_text(json.dumps({'BuildId': build_id, 'Modules': {}}),
                                                   encoding='utf-8')


def expect_error(callable_, label):
    try:
        callable_()
    except (ValueError, OSError, KeyError):
        return 0
    print('FAIL: %s was accepted' % label)
    return 1


def check_package_profile():
    """The package profile must gate on the matching distribution, not on doctor."""
    import importlib.util

    repo = HERE.parent
    spec = importlib.util.spec_from_file_location('execute_tests', repo / 'Tools' / 'execute-tests.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    failures = 0
    engine = str(repo / 'EngineThatDoesNotNeedToExist')
    request = {'profile': 'package'}

    os.environ['TURBOMCP_ENGINE_DISTRIBUTION'] = 'installed-launcher'
    first = module.commands(request, engine)[0]
    if not any('engine_distribution.py' in str(part) for part in first):
        failures += 1
        print('FAIL: the Launcher package profile does not start with the distribution check')
    if any('doctor.py' in str(part) for part in first):
        failures += 1
        print('FAIL: the Launcher package profile still starts with doctor.py')

    os.environ['TURBOMCP_ENGINE_DISTRIBUTION'] = 'source-build'
    first = module.commands(request, engine)[0]
    if not any('doctor.py' in str(part) for part in first):
        failures += 1
        print('FAIL: the source package profile no longer starts with doctor.py')
    os.environ.pop('TURBOMCP_ENGINE_DISTRIBUTION', None)
    return failures


def main():
    failures = 0
    with tempfile.TemporaryDirectory() as temp:
        base = pathlib.Path(temp)
        repo = base / 'repo'
        repo.mkdir()
        (repo / 'engine.lock.json').write_text(json.dumps(PIN), encoding='utf-8')

        os.environ.pop('TURBOMCP_ENGINE_DISTRIBUTION', None)
        if engine_distribution.configured(repo) != 'source-build':
            failures += 1
            print('FAIL: default distribution is not source-build')

        (repo / 'workspace.local.json').write_text(json.dumps({'engineDistribution': 'installed-launcher'}),
                                                   encoding='utf-8')
        if engine_distribution.configured(repo) != 'installed-launcher':
            failures += 1
            print('FAIL: workspace.local.json was not honoured')

        os.environ['TURBOMCP_ENGINE_DISTRIBUTION'] = 'source-build'
        if engine_distribution.configured(repo) != 'source-build':
            failures += 1
            print('FAIL: the environment did not win over the config file')
        os.environ.pop('TURBOMCP_ENGINE_DISTRIBUTION', None)

        os.environ['TURBOMCP_ENGINE_DISTRIBUTION'] = 'installed-lego'
        failures += expect_error(lambda: engine_distribution.configured(repo), 'an unknown distribution')
        os.environ.pop('TURBOMCP_ENGINE_DISTRIBUTION', None)

        engine = base / 'launcher'
        make_engine(engine)
        identity = engine_distribution.identify(engine, 'installed-launcher', PIN)
        if identity != {'distribution': 'installed-launcher', 'version': '5.8.4',
                        'build_id': '55116800', 'changelist': 58902052}:
            failures += 1
            print('FAIL: Launcher identity was %r' % (identity,))

        bad = base / 'not-promoted'
        make_engine(bad, promoted=0)
        failures += expect_error(
            lambda: engine_distribution.identify(bad, 'installed-launcher', PIN),
            'a non-promoted Launcher engine')

        no_cl = base / 'no-changelist'
        make_engine(no_cl, changelist=0)
        failures += expect_error(
            lambda: engine_distribution.identify(no_cl, 'installed-launcher', PIN),
            'a Launcher engine without a changelist')

        wrong = base / 'wrong-version'
        make_engine(wrong, version='5.8.3')
        failures += expect_error(
            lambda: engine_distribution.identify(wrong, 'installed-launcher', PIN),
            'an engine whose version differs from the pin')

        not_git = base / 'source'
        make_engine(not_git)
        failures += expect_error(
            lambda: engine_distribution.identify(not_git, 'source-build', PIN),
            'a source build that is not a git checkout')

    failures += check_package_profile()

    if failures:
        print('RESULT: FAIL (%d case(s))' % failures)
        return 1
    print('RESULT: PASS (default, config, environment precedence, unknown value, Launcher identity, '
          'not-promoted, missing changelist, version mismatch, non-git source build, package profile gate)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
