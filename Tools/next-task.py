"""Print one approved task. Never execute it, skip a blocker or invent work."""
import argparse,json,pathlib,re,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
def field(text,name):
    found=re.search(r'^'+re.escape(name)+r': (.+)$',text,re.M)
    if not found:raise ValueError('Missing task field: '+name)
    return found.group(1)
def load_queue(root,track='v1'):
    manifest=json.loads((root/'roadmap/queue.json').read_text(encoding='utf8'))
    rows=manifest['tasks'];seen=set();loaded=[]
    for row in rows:
        tid=row['id'];p=(root/row['path']).resolve()
        if tid in seen or not p.is_relative_to(root.resolve()) or not p.is_file():raise ValueError('Duplicate/missing/unsafe queue task: '+tid)
        if any(dep not in seen for dep in row['depends_on']):raise ValueError('Dependency must precede task: '+tid)
        seen.add(tid);text=p.read_text(encoding='utf8')
        if field(text,'ID')!=tid:raise ValueError('Queue/task identity mismatch')
        if field(text,'Status') not in ('todo','in_progress','blocked','done','backlog'):raise ValueError('Invalid status')
        if field(text,'Readiness') not in ('ready','needs_reconciliation'):raise ValueError('Invalid readiness')
        for section in ['Problem','Scope','Dependencies','Acceptance','Verification','Stop conditions','Deliverable','Completion evidence']:
            if '## '+section+'\n' not in text:raise ValueError('Missing '+section+' in '+tid)
        if field(text,'Status')=='done':
            evidence=text.split('## Completion evidence\n',1)[1].split('\n## ',1)[0].strip()
            if not evidence or evidence.startswith('Pending'):raise ValueError('Done without completion evidence: '+tid)
        loaded.append((row,text))
    return [(row,text) for row,text in loaded if row.get('track','v1')==track]
def select(root,track='v1'):
    items=load_queue(root,track)
    statuses={row['id']:field(text,'Status') for row,text in load_queue(root,'v1')+load_queue(root,'compatibility')}
    for row,text in items:
        status=field(text,'Status')
        if status=='done':continue
        missing=[d for d in row['depends_on'] if statuses.get(d)!='done']
        if missing:return None,'STOP: incomplete prerequisites for '+row['id']+': '+', '.join(missing)
        if status in ('blocked','backlog') or field(text,'Readiness')!='ready':return None,'STOP: '+row['id']+' is '+status+' / '+field(text,'Readiness')
        if status=='in_progress':return None,'STOP: '+row['id']+' is already in progress; coordinate with its owner instead of starting duplicate work'
        return row,text
    return None,'No unfinished approved task on track '+track+'. Do not invent another task.'
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--check',action='store_true');parser.add_argument('--track',choices=['v1','compatibility'],default='v1');parser.add_argument('--claim',metavar='OWNER');args=parser.parse_args()
    load_queue(ROOT,args.track)
    if args.check:print('PASS: roadmap IDs, ordered dependencies, task contracts and completion evidence');return 0
    from task_claim import current,claim,location
    active=current(ROOT)
    if active:
        print('STOP: task '+active['task']+' is claimed by '+active['owner']+'; claim '+active['id']);return 2
    row,text=select(ROOT,args.track)
    if not row:print(text);return 2
    if args.claim:
        assigned=claim(ROOT,row['id'],args.claim)
        print('CLAIM: '+assigned['id']+'; owner '+assigned['owner'])
        print('CLAIM STORE: '+str(location(ROOT)))
    print('NEXT: '+row['id']+' | '+row['path'])
    print('Read AGENTS.md and this task. Claim its ownership before work. A task claim does not grant engine ownership or execute commands.\n')
    print(text);return 0
if __name__=='__main__':
    try:sys.exit(main())
    except (ValueError,KeyError,OSError,RuntimeError) as exc:sys.exit('FAIL: '+str(exc))
