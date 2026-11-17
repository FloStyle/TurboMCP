"""Check the manifest release scope follows the owner approval.

A stable package must not call itself a preview, and a preview package must not
call itself stable. The scope is derived only from release/status.json, so this
test fixes both directions plus the cases that must stay a preview.
"""

import json
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import release_scope  # noqa: E402

CASES = [
    ('ready and approved is stable',
     {'readiness': 'ready', 'public_export_approved': True}, 'stable'),
    ('ready without approval stays preview',
     {'readiness': 'ready', 'public_export_approved': False}, 'development-preview'),
    ('approval without readiness stays preview',
     {'readiness': 'not_ready', 'public_export_approved': True}, 'development-preview'),
    ('a string approval is not approval',
     {'readiness': 'ready', 'public_export_approved': 'true'}, 'development-preview'),
    ('missing keys stay preview', {}, 'development-preview'),
    ('no object stays preview', None, 'development-preview'),
]


def main():
    failures = 0
    for label, status, expected in CASES:
        actual = release_scope.scope_for(status)
        if actual != expected:
            failures += 1
            print('FAIL: %s: expected %s, got %s' % (label, expected, actual))

    status_path = HERE.parent / 'release' / 'status.json'
    try:
        on_disk = json.loads(status_path.read_text(encoding='utf8'))
    except (OSError, ValueError) as error:
        print('FAIL: cannot read %s: %s' % (status_path, error))
        return 1
    actual = release_scope.scope_for(on_disk)
    if actual not in (release_scope.STABLE, release_scope.PREVIEW):
        failures += 1
        print('FAIL: release/status.json produced an unknown scope: %r' % actual)
    print('release/status.json scope: %s' % actual)

    if failures:
        print('RESULT: FAIL (%d case(s))' % failures)
        return 1
    print('RESULT: PASS (%d rule cases plus the on-disk status)' % len(CASES))
    return 0


if __name__ == '__main__':
    sys.exit(main())
