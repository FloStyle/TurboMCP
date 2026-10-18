"""Review a committed worker submission using policy from an approved base.

Run this script from the trusted integration checkout, never the worker branch.
This is a diff gate, not a sandbox or semantic acceptance test.
"""
import argparse
import json
import pathlib
import re
import subprocess
import sys


def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args])


def revision(root, value):
    if not re.fullmatch(r'[0-9a-f]{40}', value):
        raise ValueError('Use the full approved base and candidate commit SHA')
    if git(root, 'rev-parse', '--verify', value + '^{commit}').decode().strip() != value:
        raise ValueError('Not a commit: ' + value)
    return value


def blob(root, commit, path):
    return git(root, 'show', commit + ':' + path)


def evidence_only(text):
    parts = text.split('## Completion evidence\n')
    if len(parts) != 2:
        raise ValueError('Expected one completion evidence section')
    tail = parts[1].split('\n## ', 1)
    return parts[0], tail[1] if len(tail) == 2 else ''


def check(root, base, candidate, task, assignment=None):
    base, candidate = revision(root, base), revision(root, candidate)
    subprocess.run(['git', '-C', str(root), 'merge-base', '--is-ancestor', base, candidate], check=True)
    policy = json.loads(blob(root, base, 'roadmap/worker-scopes.json'))
    if task not in policy.get('tasks', {}):
        raise ValueError('No approved worker scope for ' + task)
    scope = policy['tasks'][task]
    if policy.get('schema') == 2:
        if assignment is None:
            raise ValueError('Schema 2 requires an explicit --assignment')
        entry = scope.get('assignments', {}).get(assignment)
        if not isinstance(entry, dict) or 'files' not in entry:
            raise ValueError('Unknown assignment ' + assignment + ' for ' + task)
        allowed = set(entry['files'])
    elif policy.get('schema') == 1:
        if assignment is not None:
            raise ValueError('Schema 1 has no named assignments; omit --assignment')
        allowed = set(scope['files'])
    else:
        raise ValueError('Unsupported worker scope schema')
    task_path = scope['task_file']
    # No rename detection: both the removed and added paths must be examined.
    raw = git(root, 'diff', '--no-ext-diff', '--name-status', '--no-renames', '-z', base, candidate).split(b'\0')
    changes = []
    for i in range(0, len(raw) - 1, 2):
        status, path = raw[i].decode(), raw[i + 1].decode('utf8')
        if path not in allowed or status not in ('A', 'M'):
            raise ValueError('Out-of-scope change: ' + status + ' ' + path)
        entry = git(root, 'ls-tree', candidate, '--', path).decode().split()
        if not entry or entry[0] != '100644' or entry[1] != 'blob':
            raise ValueError('Only ordinary non-executable files are allowed: ' + path)
        changes.append(path)
    if not changes:
        raise ValueError('Empty submission')
    if task_path in changes:
        before = blob(root, base, task_path).decode('utf8')
        after = blob(root, candidate, task_path).decode('utf8')
        if evidence_only(before) != evidence_only(after):
            raise ValueError('Worker may change only task Completion evidence; reviewer owns status and scope')
    for invariant in scope.get('preserve_rows', []):
        path, key = invariant['path'], invariant['key']
        before = json.loads(blob(root, base, path))['tools']
        after = json.loads(blob(root, candidate, path))['tools']
        old_ids, new_ids = [r[key] for r in before], [r[key] for r in after]
        if len(new_ids) != len(set(new_ids)) or set(old_ids) != set(new_ids):
            raise ValueError('Lost, duplicated or invented tool row: ' + path)
        if any(r.get('preference') != 'unverified' for r in after):
            raise ValueError('Investigation cannot approve runtime routing preferences')
    return {'task': task, 'assignment': assignment, 'base': base, 'candidate': candidate,
            'files': sorted(changes),
            'result': 'PASS', 'limit': 'Scope only; independent evidence and semantic review still required.'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo', type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    parser.add_argument('--base', required=True)
    parser.add_argument('--candidate', required=True)
    parser.add_argument('--task', required=True)
    parser.add_argument('--assignment')
    args = parser.parse_args()
    print(json.dumps(check(args.repo, args.base, args.candidate, args.task, args.assignment), indent=2))


if __name__ == '__main__':
    try:
        main()
    except (ValueError, KeyError, OSError, subprocess.SubprocessError) as exc:
        sys.exit('FAIL: ' + str(exc))
