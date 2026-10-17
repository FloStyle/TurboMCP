"""Reject missing, stale and unsafe task-review handoffs."""
import copy
import importlib.util
import json
import pathlib
import tempfile

spec = importlib.util.spec_from_file_location('task_review', pathlib.Path(__file__).with_name('check-task-review.py'))
review = importlib.util.module_from_spec(spec)
spec.loader.exec_module(review)

with tempfile.TemporaryDirectory() as directory:
    root = pathlib.Path(directory)
    for name in ('roadmap', 'todo', 'Tools', 'Tests'):
        (root / name).mkdir()
    task = '# Example\nID: R1\nReadiness: needs_reconciliation\nReview: source-reviewed\n\n## Current review\nObserved behavior.\nProbe before changing.\n'
    (root / 'todo/example.md').write_text(task, encoding='utf8')
    (root / 'Tools/test-example.py').write_text('pass\n', encoding='utf8')
    (root / 'Tests/catalog.json').write_text(json.dumps({'suites': [{'id': 'test-example', 'path': 'Tools/test-example.py'}]}), encoding='utf8')
    base = {'schema': 1, 'reviewed_commit': 'a' * 40, 'tasks': [{
        'id': 'R1', 'path': 'todo/example.md', 'title': 'Example', 'decision': 'requires_spec',
        'owners': ['R1'], 'finding': 'Observed behavior.', 'action': 'Probe before changing.',
        'source_refs': [{'path': 'Tools/test-example.py', 'line': 1, 'sha256': 'b' * 64}],
        'verification_commands': ['python Tools/test-example.py']}]}
    def save(data):
        (root / 'roadmap/task-review.json').write_text(json.dumps(data), encoding='utf8')
    save(base)
    assert review.check(root) == 1
    cases = []
    data = copy.deepcopy(base); data['tasks'] = []; cases.append(('missing record', data))
    data = copy.deepcopy(base); data['tasks'].append(copy.deepcopy(data['tasks'][0])); cases.append(('duplicate record', data))
    data = copy.deepcopy(base); data['tasks'][0]['owners'] = ['unknown']; cases.append(('unknown owner', data))
    data = copy.deepcopy(base); data['tasks'][0]['action'] = 'A stale, different instruction.'; cases.append(('stale action', data))
    data = copy.deepcopy(base); data['tasks'][0]['source_refs'][0]['path'] = '../outside.py'; cases.append(('outside reference', data))
    data = copy.deepcopy(base); data['tasks'][0]['verification_commands'] = ['python Tools/test-missing.py']; cases.append(('missing suite', data))
    data = copy.deepcopy(base); data['tasks'][0]['verification_commands'] = ['python Tools/execute-tests.py --profile engine --suites test-missing --execute']; cases.append(('missing selected suite', data))
    for label, data in cases:
        save(data)
        try:
            review.check(root)
        except ValueError:
            pass
        else:
            raise AssertionError('Accepted ' + label)
    save(base)
    (root / 'todo/example.md').write_text(task.replace('needs_reconciliation', 'ready'), encoding='utf8')
    try:
        review.check(root)
    except ValueError:
        pass
    else:
        raise AssertionError('Accepted ready implementation without a specification')
print('RESULT: PASS (valid review and eight missing/stale/unsafe handoff controls)')
