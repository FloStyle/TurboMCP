"""Copy one complete tracked Git tree without history or path rewriting."""

import argparse
import hashlib
import json
import ntpath
import os
import pathlib
import re
import stat
import subprocess
import sys

import plugin_notices


ROOT = pathlib.Path(__file__).resolve().parents[1]
POLICY = ROOT / 'release/export-policy.json'
SECRET = re.compile(
    rb'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}'
    rb'|-----BEGIN (?:RSA |EC |DSA |OPENSSH |ENCRYPTED )?PRIVATE KEY-----'
    rb'|(?i:https?)://[^\s/<>]+:[^\s/<>]+@)'
)
PERSONAL = re.compile(rb'[A-Za-z]:[\\/]+Users[\\/]+(?!<)[^\\/\s]+', re.I)


def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])


def git_text(*args):
    return git(*args).decode('utf8').rstrip('\r\n')


def tree_blob(tree, path):
    return git('cat-file', 'blob', tree + ':' + path)


def staged_tree():
    # Freeze the entire index as a tree before reading any of its blobs.
    if subprocess.run(['git', '-C', str(ROOT), 'diff', '--quiet'], check=False).returncode:
        raise ValueError('Unstaged tracked changes: stage the candidate before previewing')
    return git_text('write-tree')


def source_entries(tree, release):
    policy = json.loads(tree_blob(tree, POLICY.relative_to(ROOT).as_posix()))
    if policy.get('schema') != 2 or policy.get('mode') != 'complete-tracked-tree':
        raise ValueError('Unsupported full-tree export policy')
    forbidden_components, forbidden_names, forbidden_suffixes = (
        {value.casefold() for value in policy[field]}
        for field in ('forbidden_components', 'forbidden_names', 'forbidden_suffixes')
    )
    result = []
    used_paths = {}
    records = git('ls-tree', '-r', '-z', '--full-tree', tree).split(b'\0')
    for record in records:
        if not record:
            continue
        meta, raw_rel = record.split(b'\t', 1)
        mode, kind, oid = meta.decode('ascii').split()
        if SECRET.search(raw_rel) or PERSONAL.search(raw_rel):
            raise ValueError('Sensitive-data pattern in a tracked path; matching content suppressed')
        rel = raw_rel.decode('utf8')
        path = pathlib.PurePosixPath(rel)
        if kind != 'blob' or mode not in ('100644', '100755'):
            raise ValueError('Non-regular source: ' + rel)
        if (path.is_absolute() or path.as_posix() != rel or '\\' in rel
                or any(part in ('.', '..') or ntpath.isreserved(part) for part in path.parts)):
            raise ValueError('Unsafe source path: ' + rel)
        if (any(part.casefold() in forbidden_components for part in path.parts)
                or path.name.casefold() in forbidden_names
                or path.suffix.casefold() in forbidden_suffixes):
            raise ValueError('Forbidden tracked content: ' + rel)
        for depth in range(1, len(path.parts) + 1):
            prefix = '/'.join(path.parts[:depth])
            entry = (prefix, depth < len(path.parts))
            if used_paths.setdefault(prefix.casefold(), entry) != entry:
                raise ValueError('Case-colliding export paths: ' + rel)
        data = git('cat-file', 'blob', oid)
        if SECRET.search(data) or PERSONAL.search(data):
            raise ValueError('Sensitive-data pattern in ' + rel + '; matching content suppressed')
        result.append((path, mode, data))
    if not result:
        raise ValueError('Empty export')
    blobs = {path.as_posix(): data for path, _, data in result}
    if not blobs.get('LICENSE') or blobs.get('Plugins/TurboMCP/LICENSE') != blobs['LICENSE']:
        raise ValueError('Export requires matching root and installable-plugin LICENSE files')
    plugin = {name[len(plugin_notices.PREFIX):]: data for name, data in blobs.items()
              if name.startswith(plugin_notices.PREFIX)}
    plugin_notices.validate(plugin, blobs['LICENSE'], release)
    return result


def source_files(release=False):
    """Return staged preview paths and bytes for the offline boundary check."""
    return [(ROOT / path, path, data) for path, _, data in source_entries(staged_tree(), release)]


def output_paths(raw_output):
    requested = pathlib.Path(raw_output)
    out = requested.resolve()
    if out != pathlib.Path(os.path.abspath(requested)):
        raise ValueError('Output path must not traverse a filesystem link or alias')
    manifest = out.with_name(out.name + '.manifest.json')
    if os.path.lexists(manifest):
        raise ValueError('Manifest output already exists')
    if os.path.lexists(out):
        raise ValueError('Output must be a new directory; existing output is never overwritten')
    if ROOT.is_relative_to(out):
        raise ValueError('Output cannot contain the source repository')
    git_directories = (git_text('rev-parse', '--absolute-git-dir'),
                       git_text('rev-parse', '--path-format=absolute', '--git-common-dir'))
    if any(out.is_relative_to(pathlib.Path(directory).resolve()) for directory in git_directories):
        raise ValueError('Output cannot be inside Git metadata')
    return out, manifest


def release_tree(head, evidence):
    if git('status', '--porcelain'):
        raise ValueError('Release export requires a clean committed source tree')
    tree = git_text('rev-parse', head + '^{tree}')
    status = json.loads(tree_blob(tree, 'release/status.json'))
    plugin_tree = git_text('rev-parse', tree + ':Plugins/TurboMCP')
    if (status.get('readiness') != 'ready' or status.get('public_export_approved') is not True
            or status.get('validated_tree') != plugin_tree):
        raise ValueError('Release gates require ready, owner approval, and the validated plugin tree ID')
    if not evidence:
        raise ValueError('Release export requires --evidence from acceptance execution')
    subprocess.run([sys.executable, str(ROOT / 'Tools/release-check.py'), '--evidence', evidence], check=True)
    if git_text('rev-parse', 'HEAD') != head or git('status', '--porcelain'):
        raise ValueError('Source changed during release verification; retry from a clean committed tree')
    return tree


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True)
    parser.add_argument('--preview', action='store_true')
    parser.add_argument('--evidence')
    args = parser.parse_args()
    out, manifest = output_paths(args.output)
    head = git_text('rev-parse', 'HEAD')
    tree = staged_tree() if args.preview else release_tree(head, args.evidence)
    files = source_entries(tree, release=not args.preview)

    out.mkdir(parents=True)
    entries = []
    for target, mode, data in files:
        dest = out / target
        dest.parent.mkdir(parents=True, exist_ok=True)
        with dest.open('xb') as stream:
            stream.write(data)
        if mode == '100755' and os.name != 'nt':
            dest.chmod(stat.S_IMODE(dest.stat().st_mode) | stat.S_IXUSR)
        entries.append({'path': target.as_posix(), 'git_mode': mode,
                        'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data)})
    # A sidecar keeps the source directory exactly equal to the exported tree.
    with manifest.open('x', encoding='utf8', newline='\n') as stream:
        json.dump({'schema': 2, 'preview': args.preview, 'source_commit': head,
                   'source_tree': tree, 'files': entries}, stream, indent=2)
        stream.write('\n')
    print(f'PASS: copied all {len(files)} tracked files unchanged; preview={args.preview}; manifest={manifest}')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, OSError, subprocess.SubprocessError) as exc:
        sys.exit(f'FAIL: {exc}')
