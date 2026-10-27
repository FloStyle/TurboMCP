"""Check notice coverage for tracked plugin sources or an installable directory.

Coverage is not proof of ownership. Release mode also refuses explicitly
unverified non-source provenance. Unknown package files always fail closed.
"""
import argparse
import json
from pathlib import Path, PurePosixPath
import subprocess

ROOT = Path(__file__).resolve().parents[1]
PREFIX = 'Plugins/TurboMCP/'


def validate(files, canonical_license, release=False):
    """Validate plugin-relative byte entries, including the exact export bytes."""
    if not canonical_license or files.get('LICENSE', b'').replace(b'\r\n', b'\n') != canonical_license.replace(b'\r\n', b'\n'):
        raise ValueError('Plugin LICENSE is missing or differs from the canonical license')
    if 'NOTICES.json' not in files:
        raise ValueError('Plugin NOTICES.json is missing')
    manifest = json.loads(files['NOTICES.json'])
    if manifest.get('schema') != 1:
        raise ValueError('Unsupported plugin notice schema')
    entries = manifest.get('files', {})
    if not isinstance(entries, dict):
        raise ValueError('Notice files must be an exact-path mapping')
    pending = []
    for name, data in files.items():
        path = PurePosixPath(name)
        if path.is_absolute() or '..' in path.parts or '\\' in name:
            raise ValueError('Unsafe package path: ' + name)
        if path.suffix in ('.h', '.cpp', '.cs'):
            header = data[:2048].decode('utf-8-sig')
            if 'SPDX-License-Identifier: MPL-2.0' not in header or 'Copyright' not in header:
                raise ValueError('Missing source notice: ' + name)
        else:
            entry = entries.get(name)
            if not isinstance(entry, dict) or not all(entry.get(k) for k in ('license', 'copyright', 'provenance', 'status')):
                raise ValueError('Missing complete notice: ' + name)
            if entry['status'] not in ('repository_notice', 'unverified'):
                raise ValueError('Invalid notice status: ' + name)
            if entry['status'] == 'unverified' or entry['license'] == 'NOASSERTION' or entry['copyright'] == 'NOASSERTION':
                pending.append(name)
    stale = set(entries) - set(files)
    if stale:
        raise ValueError('Notice names absent from package: ' + ', '.join(sorted(stale)))
    if release and pending:
        raise ValueError('Release blocked: provenance/permission not established for ' + ', '.join(sorted(pending)))
    return sorted(pending)


def tracked_files(root=ROOT):
    names = subprocess.check_output(['git', '-C', str(root), 'ls-files', '-z', PREFIX], text=True).split('\0')
    return {name[len(PREFIX):]: (root / name).read_bytes() for name in names if name}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path, help='Inspect every file in an installable plugin directory')
    parser.add_argument('--release', action='store_true', help='Also refuse unresolved provenance')
    args = parser.parse_args()
    if args.package:
        if not args.package.is_dir():
            raise ValueError('Package must be an existing directory')
        paths = list(args.package.rglob('*'))
        if any(p.is_symlink() for p in paths):
            raise ValueError('Package symlinks are not supported')
        files = {p.relative_to(args.package).as_posix(): p.read_bytes() for p in paths if p.is_file()}
    else:
        files = tracked_files()
    pending = validate(files, (ROOT / 'LICENSE').read_bytes(), args.release)
    print('PASS: notice coverage for ' + str(len(files)) + ' plugin files')
    if pending:
        print('RELEASE BLOCKED: unresolved provenance: ' + ', '.join(pending))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.SubprocessError) as exc:
        raise SystemExit('FAIL: ' + str(exc))
