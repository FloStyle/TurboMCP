"""Identify the Unreal engine distribution a run targets.

The distribution and the engine path are local configuration, never a hard-coded
path: `TURBOMCP_ENGINE_DISTRIBUTION` wins over the ignored `workspace.local.json`,
which names `engineDistribution` and `engineRoot`.

Two distributions are supported:

* `source-build` - the clean pinned engine checkout in `engine.lock.json`, proved
  by git commit and a clean tree.
* `installed-launcher` - an Epic Launcher installation. It has no git history, so
  the identity is the promoted build number, the changelist and the engine
  BuildId. A Launcher build has one fixed BuildId that every installation of that
  build shares.

`source-build` is the default, so an unconfigured checkout behaves as before.
"""

import argparse
import json
import os
import pathlib
import subprocess
import sys

DISTRIBUTIONS = ('source-build', 'installed-launcher')
DEFAULT = 'source-build'
PRINTED = ['distribution', 'version', 'build_id', 'changelist', 'commit']


def read_json(path):
    return json.loads(pathlib.Path(path).read_text(encoding='utf-8-sig'))


def configured(repo):
    """Return the configured distribution name."""
    value = os.environ.get('TURBOMCP_ENGINE_DISTRIBUTION')
    if not value:
        config = pathlib.Path(repo) / 'workspace.local.json'
        if config.is_file():
            value = read_json(config).get('engineDistribution')
    value = value or DEFAULT
    if value not in DISTRIBUTIONS:
        raise ValueError('Unknown engine distribution: %r' % value)
    return value


def identify(engine, distribution, pin):
    """Return the identity of one engine install for the configured distribution."""
    if distribution not in DISTRIBUTIONS:
        raise ValueError('Unknown engine distribution: %r' % distribution)
    engine = pathlib.Path(engine)
    version = read_json(engine / 'Engine' / 'Build' / 'Build.version')
    actual = '%s.%s.%s' % (version.get('MajorVersion'), version.get('MinorVersion'),
                           version.get('PatchVersion'))
    if actual != pin['version']:
        raise ValueError('Engine version %s does not match the pin %s' % (actual, pin['version']))
    receipt = read_json(engine / 'Engine' / 'Binaries' / 'Win64' / 'UnrealEditor.modules')
    build_id = receipt.get('BuildId')
    if not isinstance(build_id, str) or not build_id:
        raise ValueError('Missing engine BuildId')

    if distribution == 'installed-launcher':
        if version.get('IsPromotedBuild') != 1:
            raise ValueError('An installed Launcher engine must be a promoted build')
        changelist = version.get('Changelist')
        if not isinstance(changelist, int) or changelist <= 0:
            raise ValueError('An installed Launcher engine must record its changelist')
        return {'distribution': distribution, 'version': actual,
                'build_id': build_id, 'changelist': changelist}

    def git(*args):
        try:
            return subprocess.check_output(['git', '-C', str(engine), *args],
                                           text=True, stderr=subprocess.STDOUT).strip()
        except subprocess.CalledProcessError as error:
            raise ValueError('Engine checkout is not a readable git working tree: %s'
                             % (error.output or '').strip().splitlines()[-1:]) from error

    head = git('rev-parse', 'HEAD')
    if head != pin['commit']:
        raise ValueError('Engine checkout HEAD %s does not match the pin %s' % (head, pin['commit']))
    if git('status', '--porcelain', '--untracked-files=no'):
        raise ValueError('Engine checkout is not clean')
    return {'distribution': distribution, 'version': actual,
            'build_id': build_id, 'commit': head}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', default=str(pathlib.Path(__file__).resolve().parents[1]))
    parser.add_argument('--engine', help='engine root; required with --json')
    parser.add_argument('--distribution', action='store_true', help='print only the distribution')
    parser.add_argument('--json', action='store_true', help='print the identity object')
    args = parser.parse_args(argv)

    if args.distribution:
        print(configured(args.repo))
        return 0
    if not args.engine:
        parser.error('--engine is required with --json')
    identity = identify(args.engine, configured(args.repo), read_json(pathlib.Path(args.repo) / 'engine.lock.json'))
    print(json.dumps({key: identity[key] for key in PRINTED if key in identity}, indent=2))
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        print('FAIL: %s' % error, file=sys.stderr)
        sys.exit(2)
