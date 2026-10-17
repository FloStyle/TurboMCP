"""Negative controls for deterministic task selection and no silent blocker bypass."""
import importlib.util,json,pathlib,tempfile
spec=importlib.util.spec_from_file_location('next_task',pathlib.Path(__file__).with_name('next-task.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
with tempfile.TemporaryDirectory() as temp:
    root=pathlib.Path(temp);(root/'roadmap').mkdir();(root/'todo').mkdir()
    rows=[{'id':'A','path':'todo/a.md','depends_on':[]},{'id':'B','path':'todo/b.md','depends_on':['A']}]
    (root/'roadmap/queue.json').write_text(json.dumps({'tasks':rows}))
    def write(name,status='todo',readiness='ready',evidence='Pending.'):
        text=f'# Task\nID: {name}\nStatus: {status}\nReadiness: {readiness}\n'
        text+='\n'.join('## '+s+'\nConcrete contract.\n' for s in ['Problem','Scope','Dependencies','Acceptance','Verification','Stop conditions','Deliverable'])
        text+='## Completion evidence\n'+evidence+'\n';(root/f'todo/{name.lower()}.md').write_text(text)
    write('A');write('B');assert m.select(root)[0]['id']=='A'
    for status in ['blocked','in_progress','backlog']:
        write('A',status);assert m.select(root)[0] is None
    write('A',readiness='needs_reconciliation');assert m.select(root)[0] is None
    write('A','done')
    try:m.select(root)
    except ValueError:pass
    else:raise AssertionError('Done task without evidence was accepted')
    write('A','done',evidence='Recorded command, exit and reviewed log.');assert m.select(root)[0]['id']=='B'
    rows[0]['depends_on']=['B'];(root/'roadmap/queue.json').write_text(json.dumps({'tasks':rows}))
    try:m.select(root)
    except ValueError:pass
    else:raise AssertionError('Cycle/forward dependency accepted')
print('RESULT: PASS (ordered selection, blocker refusal, evidence and dependency controls)')
