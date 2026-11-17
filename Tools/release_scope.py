"""Derive the release scope label that the package manifest records.

The manifest must not describe a stable release as a development preview, and it
must not describe a preview as stable. The label therefore follows the owner
approval recorded in release/status.json instead of a constant in the packaging
script, so the two files cannot disagree.

Stable requires both fields: readiness "ready" and public_export_approved true.
Any other value, including a missing key or a non-boolean "true", is a preview.
"""

import argparse
import json
import pathlib
import sys

STABLE = 'stable'
PREVIEW = 'development-preview'


def scope_for(status):
    """Return the release scope label for a parsed release/status.json."""
    if not isinstance(status, dict):
        return PREVIEW
    ready = status.get('readiness') == 'ready'
    approved = status.get('public_export_approved') is True
    return STABLE if ready and approved else PREVIEW


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--status', required=True,
                        help='path to release/status.json')
    args = parser.parse_args(argv)
    try:
        status = json.loads(pathlib.Path(args.status).read_text(encoding='utf8'))
    except (OSError, ValueError) as error:
        print('FAIL: cannot read %s: %s' % (args.status, error), file=sys.stderr)
        return 2
    print(scope_for(status))
    return 0


if __name__ == '__main__':
    sys.exit(main())
