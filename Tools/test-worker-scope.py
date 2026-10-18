"""Real temporary Git history tests for the trusted worker diff gate."""
import importlib.util
import json
import pathlib
import subprocess
import tempfile

spec = importlib.util.spec_from_file_location('scope', pathlib.Path(__file__).with_name('check-worker-scope.py'))
scope = importlib.util.module_from_spec(spec)
spec.loader.exec_module(scope)

with tempfile.TemporaryDirectory() as temp:
    root = pathlib.Path(temp)
    def git(*args):
        return subprocess.check_output(['git', '-C', str(root), *args], stderr=subprocess.STDOUT).decode().strip()
    def write(path, value):
        p = root / path
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(value, encoding='utf8', newline='\n')
    def commit():
        git('add', '-A')
        git('-c', 'user.name=Scope Test', '-c', 'user.email=scope@example.invalid',
            '-c', 'commit.gpgsign=false', 'commit', '-qm', 'scope fixture')
        return git('rev-parse', 'HEAD')
    git('init', '-q')
    task = '# Task\nStatus: todo\n## Completion evidence\nPending.\n\n## Scope\nRead only.\n'
    policy = {'schema': 1, 'tasks': {'R13': {'task_file': 'todo/r13.md',
        'files': ['docs/report.md', 'docs/routes.json', 'todo/r13.md'],
        'preserve_rows': [{'path': 'docs/routes.json', 'key': 'tool'}]}}}
    write('roadmap/worker-scopes.json', json.dumps(policy))
    write('todo/r13.md', task)
    write('docs/report.md', 'Before\n')
    routes = {'tools': [{'tool': 'A', 'preference': 'unverified'}, {'tool': 'B', 'preference': 'unverified'}]}
    write('docs/routes.json', json.dumps(routes))
    base = commit()
    write('docs/report.md', 'Evidence\n')
    write('todo/r13.md', task.replace('Pending.', 'Inspected pinned declarations. Runtime unverified.'))
    accepted = commit()
    assert scope.check(root, base, accepted, 'R13')['result'] == 'PASS'
    cases = [
        ('plugin edit', lambda: write('Plugins/code.cpp', 'change')),
        ('self-expanded policy', lambda: write('roadmap/worker-scopes.json', '{}')),
        ('self-completion', lambda: write('todo/r13.md', task.replace('todo', 'done'))),
        ('deletion', lambda: (root / 'docs/report.md').unlink()),
        ('rename escape', lambda: (root / 'docs/report.md').rename(root / 'outside.md')),
        ('missing row', lambda: write('docs/routes.json', json.dumps({'tools': routes['tools'][:1]}))),
        ('duplicate row', lambda: write('docs/routes.json', json.dumps({'tools': routes['tools'] * 2}))),
        ('unsupported preference', lambda: write('docs/routes.json', json.dumps(routes).replace('unverified', 'prefer-turbomcp'))),
        ('executable mode', lambda: git('update-index', '--chmod=+x', 'docs/report.md')),
    ]
    for label, change in cases:
        git('reset', '--hard', accepted)
        change()
        candidate = commit()
        try:
            scope.check(root, base, candidate, 'R13')
        except ValueError:
            pass
        else:
            raise AssertionError('Accepted ' + label)
    for label, b, c, task_id in [('unknown task', base, accepted, 'R99'),
                                 ('empty', base, base, 'R13'),
                                 ('symbolic base', 'main', accepted, 'R13'),
                                 ('non-ancestor', accepted, base, 'R13')]:
        try:
            scope.check(root, b, c, task_id)
        except (ValueError, subprocess.CalledProcessError):
            pass
        else:
            raise AssertionError('Accepted ' + label)

with tempfile.TemporaryDirectory() as temp:
    root = pathlib.Path(temp)
    def git(*args):
        return subprocess.check_output(['git', '-C', str(root), *args], stderr=subprocess.STDOUT).decode().strip()
    def write(path, value):
        p = root / path
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(value, encoding='utf8', newline='\n')
    def commit():
        git('add', '-A')
        git('-c', 'user.name=Scope Test', '-c', 'user.email=scope@example.invalid',
            '-c', 'commit.gpgsign=false', 'commit', '-qm', 'scope fixture')
        return git('rev-parse', 'HEAD')
    git('init', '-q')
    task = '# Task\nStatus: todo\n## Completion evidence\nPending.\n\n## Scope\nRead only.\n'
    policy = {'schema': 2, 'tasks': {'R04': {'task_file': 'todo/r04.md',
        'protected': ['Tests/catalog.json'],
        'assignments': {
            'a': {'files': ['src/a.py']},
            'b': {'files': ['src/b.py']},
        }}}}
    write('roadmap/worker-scopes.json', json.dumps(policy))
    write('todo/r04.md', task)
    write('src/a.py', 'a\n')
    write('src/b.py', 'b\n')
    write('Tests/catalog.json', '{}\n')
    base = commit()
    write('src/a.py', 'a changed\n')
    accepted = commit()
    assert scope.check(root, base, accepted, 'R04', 'a')['result'] == 'PASS'
    for label, change, assignment in [
        ('other assignment file', lambda: write('src/b.py', 'b changed\n'), 'a'),
        ('self-expanded policy', lambda: write('roadmap/worker-scopes.json', '{}'), 'a'),
        ('unknown assignment', lambda: write('src/a.py', 'a again\n'), 'missing'),
        ('schema2 without assignment', lambda: write('src/a.py', 'a again\n'), None),
    ]:
        git('reset', '--hard', accepted)
        change()
        candidate = commit()
        try:
            scope.check(root, base, candidate, 'R04', assignment)
        except ValueError:
            pass
        else:
            raise AssertionError('Accepted ' + label)
    git('reset', '--hard', accepted)
    write('src/b.py', 'b changed\n')
    b_accepted = commit()
    assert scope.check(root, accepted, b_accepted, 'R04', 'b')['result'] == 'PASS'
print('RESULT: PASS (schema-1 and schema-2 scope/identity negative controls)')