"""Validate complete task-review coverage without claiming runtime acceptance."""
import json
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
DECISIONS = {'confirmed', 'partial', 'investigate', 'deferred', 'duplicate',
             'withdrawn', 'reference', 'superseded', 'plan', 'completed', 'requires_spec'}

def check(root=ROOT):
    register = json.loads((root / 'roadmap/task-review.json').read_text(encoding='utf8'))
    if register.get('schema') != 1 or not re.fullmatch(r'[0-9a-f]{40}', register.get('reviewed_commit', '')):
        raise ValueError('Task review needs a schema and exact source revision')
    actual = {}
    for path in (root / 'todo').glob('*.md'):
        text = path.read_text(encoding='utf8')
        match = re.search(r'^ID: (.+)$', text, re.M)
        if not match or match[1] in actual:
            raise ValueError('Missing or duplicate task ID')
        actual[match[1]] = (path.relative_to(root).as_posix(), text)
    rows = register['tasks']
    ids = [row['id'] for row in rows]
    if len(ids) != len(set(ids)) or set(ids) != set(actual):
        raise ValueError('Review register must cover every task exactly once')
    catalog = {s['path'] for s in json.loads((root / 'Tests/catalog.json').read_text(encoding='utf8'))['suites']}
    known_suites = {s['id'] for s in json.loads((root / 'Tests/catalog.json').read_text(encoding='utf8'))['suites']}
    for row in rows:
        path, text = actual[row['id']]
        if row['path'] != path or row['decision'] not in DECISIONS:
            raise ValueError('Review path or disposition mismatch: ' + row['id'])
        if 'Review: source-reviewed' not in text or '## Current review' not in text:
            raise ValueError('Task has no visible current review: ' + row['id'])
        if row['finding'] not in text or row['action'] not in text or not text.startswith('# ' + row['title'] + '\n'):
            raise ValueError('Task text and review register disagree: ' + row['id'])
        if not row['finding'].strip() or not row['action'].strip() or not row['owners']:
            raise ValueError('Task review lacks a finding, action or owner: ' + row['id'])
        if any(owner not in actual for owner in row['owners']):
            raise ValueError('Unknown follow-up owner: ' + row['id'])
        if row['decision'] == 'requires_spec' and 'Readiness: ready' in text:
            raise ValueError('Unspecified implementation cannot be ready: ' + row['id'])
        if not row['source_refs'] or not row['verification_commands']:
            raise ValueError('Missing source or verification references: ' + row['id'])
        for source in row['source_refs']:
            candidate = pathlib.PurePosixPath(source['path'])
            if candidate.is_absolute() or '..' in candidate.parts or not (root / candidate).is_file():
                raise ValueError('Invalid source reference: ' + source['path'])
            if not re.fullmatch(r'[0-9a-f]{64}', source['sha256']):
                raise ValueError('Missing reviewed-source hash')
            if source['line'] is not None and (not isinstance(source['line'], int) or source['line'] < 1):
                raise ValueError('Invalid source line')
        for command in row['verification_commands']:
            direct = re.fullmatch(r'python (Tools/test-[a-z0-9-]+[.]py)', command)
            if direct and direct[1] not in catalog:
                raise ValueError('Unknown test command: ' + command)
            selected = re.search(r'--suites ([a-z0-9,-]+)', command)
            if selected and not set(selected[1].split(',')) <= known_suites:
                raise ValueError('Unknown selected suite: ' + command)
    return len(rows)

if __name__ == '__main__':
    try:
        print(f'PASS: {check()} task reviews, source references, owners and test commands')
    except (ValueError, KeyError, OSError, TypeError) as error:
        sys.exit('FAIL: ' + str(error))
