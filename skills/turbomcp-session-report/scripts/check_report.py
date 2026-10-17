"""Local lint for already-sanitized reports. Never certifies confidentiality."""
import pathlib
import re
import sys

FIELDS = ('Report ID', 'Date', 'Review state', 'Plugin version', 'Engine version',
          'Client', 'Test case', 'Context', 'Tools used', 'Outcome', 'Session limits', 'Correction of')
SECTIONS = ('1. Cross-cutting issues', '2. Tool observations', '3. Recurrence',
            '4. Features that would have helped', '5. Suggested priority', '6. Completeness and privacy review')
RULES = {
    'code-markup': r'`|~~~',
    'url-or-email': r'https?://|www\.|[\w.+-]+@[\w.-]+\.[A-Za-z]{2,}',
    'filesystem-path': r'[A-Za-z]:[\/]|\\|(?:^|[\s(])/(?:[A-Za-z0-9_.-]+/)+|/Game\b|~/|%USERPROFILE%',
    'guid-or-node-id': r'\b[0-9a-fA-F]{32}\b|\b[0-9a-fA-F]{8}(?:-[0-9a-fA-F]{4}){3}-[0-9a-fA-F]{12}\b|NodeGuid\s*=',
    'raw-structured-input': r'\{\s*["\x27]|Begin Object|Traceback \(most recent call last\)',
    'asset-identifier': r'\b(?:BP|WBP|DT|DA|SK|SM|MI|MPC)_[A-Za-z0-9_]+\b',
    'unfinished-placeholder': r'<[^>]+>|\[(?:generic|public|product|version|earlier|exact|task|play|context|short|proposed)[^\]]*\]',
}


def check(text):
    errors = []
    for n, line in enumerate(text.split('\n'), 1):
        if any(ord(c) < 32 or ord(c) > 126 for c in line.rstrip('\r')):
            errors.append(f'line {n}: printable-ascii')
        for rule, pattern in RULES.items():
            if re.search(pattern, line):
                errors.append(f'line {n}: {rule}')
    for field in FIELDS:
        if not re.search(r'^\*\*' + re.escape(field) + r':\*\* \S.+$', text, re.M):
            errors.append('missing-field: ' + field)
    if '**Review state:** Pending human review' not in text:
        errors.append('review-state-must-remain-pending')
    for section in SECTIONS:
        if '## ' + section + '\n' not in text:
            errors.append('missing-section: ' + section)
    ids = re.findall(r'^\*\*([A-Z]{2,3}-\d{2,}):', text, re.M)
    if len(ids) != len(set(ids)):
        errors.append('duplicate-item-id')
    return errors


def main():
    if len(sys.argv) != 2:
        return 'Usage: python check_report.py REPORT'
    try:
        text = pathlib.Path(sys.argv[1]).read_text(encoding='ascii')
    except (OSError, UnicodeError):
        return 'FAIL: report is unreadable or not ASCII; no content displayed'
    errors = check(text)
    if errors:
        print('\n'.join(errors))
        return 'FAIL: report requires correction; no content displayed'
    print('PASS: local lint only. Human confidentiality review is still required. Nothing was uploaded.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
