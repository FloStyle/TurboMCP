"""Generate the task index from the repository-owned task files."""
import pathlib, re, sys
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
        items.append((status,priority,tid,text.splitlines()[0].removeprefix('# '),path))
    return sorted(items,key=lambda x:(STATUSES.index(x[0]),x[1],x[2]))
def render():
    items=tasks()
    lines=['# TODO','','Repository task files are authoritative. No external Kanban board is used.',
           'Edit the selected task, then run `python Tools/update-todo.py`. See [AGENTS.md](AGENTS.md).',
           'Imported evidence is historical; revalidate claims before implementation or completion.','']
    for status in STATUSES:
        group=[x for x in items if x[0]==status]
        if not group:continue
        lines += [f'## {status} ({len(group)})','']
        for _,priority,tid,title,path in group:
            title=title.replace('[','(').replace(']',')')
            lines.append(f'- [{"x" if status=="done" else " "}] **{priority}** [{title}](todo/{path.name})')
        lines.append('')
    return '\n'.join(lines).rstrip()+'\n'
if __name__=='__main__':
    expected=render(); path=ROOT/'TODO.md'
    if '--check' in sys.argv:
        if not path.exists() or path.read_text(encoding='utf8')!=expected:
            sys.exit('FAIL: TODO.md is stale; run python Tools/update-todo.py')
        print('PASS: TODO index and unique task IDs')
    else:
        path.write_text(expected,encoding='utf8');print(f'Updated TODO.md: {len(tasks())} tasks')
