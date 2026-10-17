"""Generate the task index from the repository-owned task files."""
import json, pathlib, re, sys
ROOT = pathlib.Path(__file__).resolve().parents[1]
STATUSES = ('in_progress', 'blocked', 'todo', 'backlog', 'done')
def tasks():
    items=[]; seen=set()
    for path in sorted((ROOT/'todo').glob('*.md')):
        text=path.read_text(encoding='utf8')
        def field(name):
            match=re.search(r'^'+name+r': (.+)$',text,re.M)
            if not match: raise ValueError(f'{path.name}: missing {name}')
            return match.group(1)
        tid,status,priority=field('ID'),field('Status'),field('Priority')
        if tid in seen or status not in STATUSES or not re.fullmatch(r'P[0-6]',priority):
            raise ValueError(f'{path.name}: duplicate ID or invalid status/priority')
        seen.add(tid)
        readiness=field('Readiness')
        if readiness not in ('ready','needs_reconciliation'): raise ValueError(f'{path.name}: invalid readiness')
        items.append((status,priority,tid,'['+readiness+'] '+text.splitlines()[0].removeprefix('# '),path))
    return sorted(items,key=lambda x:(STATUSES.index(x[0]),x[1],x[2]))
def render():
    items=tasks()
    queue=json.loads((ROOT/'roadmap/queue.json').read_text(encoding='utf8'))['tasks']
    by_id={item[2]:item for item in items}
    queued={entry['id'] for entry in queue}
    lines=['# Task index','','[Repository home](README.md) · [Roadmap](ROADMAP.md) · [Contributing](CONTRIBUTING.md)','',
           'Run `python Tools/next-task.py` to select work. The approved queue below defines execution order; historical findings need reconciliation before implementation.',
           '', 'Generated from task files and the approved queue. After editing task status, run `python Tools/update-todo.py`.','',
           '## Approved queue','','| Task | Track | Kind | Status |','| --- | --- | --- | --- |']
    for entry in queue:
        status,priority,tid,title,path=by_id[entry['id']]
        title=title.split('] ',1)[-1]
        lines.append(f"| [{title}](todo/{path.name}) | {entry['track']} | {entry['kind']} | {status} |")
    lines += ['', '## Historical findings', '', 'These records preserve original requirements and evidence. Their status does not grant implementation permission.', '']
    for status in STATUSES:
        group=[x for x in items if x[0]==status and x[2] not in queued]
        if not group:continue
        lines += ['<details>',f'<summary>{status.replace("_"," ")} ({len(group)})</summary>','']
        for _,priority,tid,title,path in group:
            title=title.split('] ',1)[-1].replace('[','(').replace(']',')')
            lines.append(f'- **{priority}** [{title}](todo/{path.name})')
        lines += ['', '</details>', '']
    return '\n'.join(lines).rstrip()+'\n'
if __name__=='__main__':
    expected=render(); path=ROOT/'TODO.md'
    if '--check' in sys.argv:
        if not path.exists() or path.read_text(encoding='utf8')!=expected:
            sys.exit('FAIL: TODO.md is stale; run python Tools/update-todo.py')
        print('PASS: TODO index and unique task IDs')
    else:
        path.write_text(expected,encoding='utf8');print(f'Updated TODO.md: {len(tasks())} tasks')
