"""Disposable Git worktree tests for serial and per-assignment claims."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from task_claim import _overlaps,active_claims,claim,claim_assignment,current,location,migrate,release,rollback


LEGACY_CLIENT = r'''
import json,sys
from pathlib import Path
p=Path(sys.argv[1])
def current():
    data=json.loads(p.read_text(encoding='utf8'))
    if data.get('schema')!=1 or 'active' not in data:
        raise ValueError('Invalid claim record; inspect before dispatch')
    return data['active']
def release(cid):
    data=json.loads(p.read_text(encoding='utf8'))
    if not data.get('active') or data['active']['id']!=cid:
        raise ValueError('Claim ID mismatch; nothing released')
    data['history'].append(data['active']);data['active']=None
    p.write_text(json.dumps(data),encoding='utf8')
mode=sys.argv[2]
if mode=='current':current()
elif mode=='release':release(sys.argv[3])
elif mode=='claim':
    if current():raise RuntimeError('already claimed')
    raise RuntimeError('legacy claim needs a store')
'''


def git(repo,*args):
    return subprocess.check_output(['git','-C',str(repo),*args],stderr=subprocess.STDOUT).decode('utf8').strip()


def write(path,text):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(text,encoding='utf8',newline='\n')


def commit(repo,message='fixture'):
    git(repo,'add','-A')
    git(repo,'-c','user.name=Test','-c','user.email=test@example.invalid','-c','commit.gpgsign=false','commit','-qm',message)
    return git(repo,'rev-parse','HEAD')


def make_repo(root,manifest=None,extra=None):
    root.mkdir(parents=True,exist_ok=True)
    git(root,'init','-q')
    write(root/'todo/r04.md','# R04\nStatus: todo\n## Completion evidence\nPending.\n')
    write(root/'src/a.py','a\n');write(root/'src/b.py','b\n');write(root/'Tests/catalog.json','{}\n')
    if manifest is not None:
        write(root/'roadmap/worker-scopes.json',json.dumps(manifest))
    for path,text in (extra or {}).items():
        write(root/path,text)
    return commit(root)


def manifest(assignments):
    return {'schema':2,'tasks':{'R04':{'task_file':'todo/r04.md','protected':['Tests/catalog.json'],
            'assignments':assignments}}}


def seed_state(repo,state):
    path=location(repo,state);path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps({'schema':2,'active':[],'history':[],'schema2_mutated':False}),encoding='utf8')


def add_worktree(repo,path):
    git(repo,'worktree','add','--detach',str(path),'HEAD')
    return path


def child_env():
    env=dict(os.environ);env['PYTHONPATH']=str(Path(__file__).resolve().parent);return env


def run_children(specs,timeout=25):
    children=[]
    try:
        for args in specs:
            children.append(subprocess.Popen([sys.executable,*args[1:]],cwd=args[0],env=child_env(),
                                             stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL))
        return [child.wait(timeout=timeout) for child in children]
    finally:
        for child in children:
            if child.poll() is None:
                child.kill();child.wait(timeout=5)


class ClaimTests(unittest.TestCase):
    def test_prefix_ancestor_overlap_helper(self):
        root=Path('.')
        self.assertTrue(_overlaps(root,['docs/a'],['docs/a/b']))
        self.assertTrue(_overlaps(root,['docs/a/b'],['docs/a']))
        self.assertFalse(_overlaps(root,['docs/a'],['docs/ab']))

    def test_schema1_claim_migrate_and_rollback(self):
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-v1-') as temp:
            root=Path(temp)/'repo';state=Path(temp)/'state';make_repo(root)
            record=claim(root,'R04','worker',state)
            self.assertEqual(current(root,state),record)
            with self.assertRaises(RuntimeError):claim(root,'R04','other',state)
            release(root,record['id'],state);self.assertIsNone(current(root,state))
            path=location(root,state);original=path.read_bytes()
            data=json.loads(original);data['active']=None;path.write_text(json.dumps(data),encoding='utf8')
            before_rollback=path.read_bytes()
            migrated=migrate(root,state)
            self.assertTrue(Path(migrated['backup']).exists())
            rollback(root,state)
            self.assertEqual(path.read_bytes(),before_rollback)

    def test_active_legacy_migration_refused_byte_identical(self):
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-active-') as temp:
            root=Path(temp)/'repo';state=Path(temp)/'state';make_repo(root)
            claim(root,'R04','worker',state);path=location(root,state);before=path.read_bytes()
            with self.assertRaises(ValueError):migrate(root,state)
            self.assertEqual(path.read_bytes(),before)

    def test_rollback_after_schema2_mutation_refused(self):
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-rollback-') as temp:
            root=Path(temp)/'repo';state=Path(temp)/'state';make_repo(root,manifest({'a':{'files':['src/a.py']}}))
            claim(root,'R04','worker',state);path=location(root,state)
            data=json.loads(path.read_text());data['active']=None;path.write_text(json.dumps(data),encoding='utf8')
            migrate(root,state)
            base=git(root,'rev-parse','HEAD')
            record=claim_assignment(root,'R04','a','worker',base,state)
            release(root,record['claim_id'],state)
            with self.assertRaises(ValueError):rollback(root,state)

    def test_schema2_disjoint_same_task(self):
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-disjoint-') as temp:
            root=Path(temp)/'repo';second=Path(temp)/'second';state=Path(temp)/'state'
            base=make_repo(root,manifest({'a':{'files':['src/a.py']},'b':{'files':['src/b.py']}}))
            add_worktree(root,second);seed_state(root,state)
            a=claim_assignment(root,'R04','a','worker',base,state)
            b=claim_assignment(second,'R04','b','worker',base,state)
            self.assertNotEqual(a['claim_id'],b['claim_id'])
            self.assertEqual({item['assignment'] for item in active_claims(root,state)},{'a','b'})
            with self.assertRaises(ValueError):release(root,'wrong',state)
            self.assertEqual(len(active_claims(root,state)),2)
            release(root,a['claim_id'],state)
            self.assertEqual([item['assignment'] for item in active_claims(root,state)],['b'])

    def test_overlap_worktree_protected_and_bad_paths(self):
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-overlap-') as temp:
            root=Path(temp)/'repo';second=Path(temp)/'second';third=Path(temp)/'third';state=Path(temp)/'state'
            base=make_repo(root,manifest({
                'a':{'files':['src/a.py']},'overlap':{'files':['src/a.py']},
                'protected':{'files':['Tests/catalog.json']},'glob':{'files':['src/*.py']},
                'case':{'files':['SRC/A.PY']},'slash':{'files':['src//a.py']},
                'escape':{'files':['../escape.py']},'directory':{'files':['src']},
            }))
            add_worktree(root,second);add_worktree(root,third);seed_state(root,state)
            claim_assignment(root,'R04','a','worker',base,state)
            with self.assertRaises(ValueError):claim_assignment(second,'R04','a','worker',base,state)
            with self.assertRaises(ValueError):claim_assignment(root,'R04','a','worker',base,state)
            with self.assertRaises(ValueError):claim_assignment(second,'R04','overlap','worker',base,state)
            with self.assertRaises(ValueError):claim_assignment(second,'R04','protected','worker',base,state)
            with self.assertRaises(ValueError):claim_assignment(second,'R04','glob','worker',base,state)
            with self.assertRaises(ValueError):claim_assignment(second,'R04','case','worker',base,state)
            with self.assertRaises(ValueError):claim_assignment(second,'R04','slash','worker',base,state)
            with self.assertRaises(ValueError):claim_assignment(second,'R04','escape','worker',base,state)
            with self.assertRaises(ValueError):claim_assignment(second,'R04','directory','worker',base,state)

    def test_base_unknown_and_malformed_store(self):
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-invalid-') as temp:
            root=Path(temp)/'repo';state=Path(temp)/'state'
            base=make_repo(root,manifest({'a':{'files':['src/a.py']}}))
            path=location(root,state);path.parent.mkdir(parents=True,exist_ok=True)
            path.write_text('{bad json',encoding='utf8');before=path.read_bytes()
            with self.assertRaises(ValueError):claim_assignment(root,'R04','a','worker',base,state)
            self.assertEqual(path.read_bytes(),before)
            malformed=[
                {'schema':3,'active':[],'history':[]},
                {'schema':2,'active':{},'history':[],'schema2_mutated':False},
                {'schema':2,'active':[{'claim_id':'x'},{'claim_id':'x'}],'history':[],'schema2_mutated':False},
            ]
            for payload in malformed:
                path.write_text(json.dumps(payload),encoding='utf8');before=path.read_bytes()
                with self.assertRaises(ValueError):claim_assignment(root,'R04','a','worker',base,state)
                self.assertEqual(path.read_bytes(),before)
            path.write_text(json.dumps({'schema':2,'active':[],'history':[],'schema2_mutated':False}),encoding='utf8')
            with self.assertRaises(ValueError):claim_assignment(root,'R04','unknown','worker',base,state)
            with self.assertRaises(ValueError):claim_assignment(root,'R04','a','worker','0'*40,state)
            self.assertEqual(len(active_claims(root,state)),0)

    def test_old_client_failclosed_on_schema2(self):
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-old-') as temp:
            root=Path(temp)/'repo';state=Path(temp)/'state'
            base=make_repo(root,manifest({'a':{'files':['src/a.py']}}))
            path=location(root,state);path.parent.mkdir(parents=True,exist_ok=True)
            for active in ([], [{'claim_id':'x','task':'R04','owner':'w','worktree':str(root),
                                 'git_common_dir':str(Path(temp)/'repo/.git'),'base_commit':base,
                                 'files':['src/a.py']}]):
                path.write_text(json.dumps({'schema':2,'active':active,'history':[]}),encoding='utf8')
                before=path.read_bytes()
                for mode in ('current','claim','release'):
                    args=[sys.executable,'-c',LEGACY_CLIENT,str(path),mode]
                    if mode=='release':args.append('wrong')
                    completed=subprocess.run(args,capture_output=True,text=True,timeout=10)
                    self.assertNotEqual(completed.returncode,0,mode)
                    self.assertEqual(path.read_bytes(),before,mode)

    def test_concurrent_disjoint_and_overlapping_claims(self):
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-race-') as temp:
            root=Path(temp)/'repo';second=Path(temp)/'second';state=Path(temp)/'state'
            base=make_repo(root,manifest({'a':{'files':['src/a.py']},'b':{'files':['src/b.py']},
                                          'overlap':{'files':['src/a.py']}}))
            add_worktree(root,second);seed_state(root,state)
            code='import sys;from task_claim import claim_assignment;claim_assignment(sys.argv[1],"R04",sys.argv[2],"worker",sys.argv[3],sys.argv[4])'
            codes=run_children([[str(root),'-c',code,str(root),'a',base,str(state)],
                                [str(second),'-c',code,str(second),'b',base,str(state)]])
            self.assertEqual(codes,[0,0])
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-race-overlap-') as temp:
            root=Path(temp)/'repo';second=Path(temp)/'second';state=Path(temp)/'state'
            base=make_repo(root,manifest({'a':{'files':['src/a.py']},'overlap':{'files':['src/a.py']}}))
            add_worktree(root,second);seed_state(root,state)
            code='import sys;from task_claim import claim_assignment;claim_assignment(sys.argv[1],"R04",sys.argv[2],"worker",sys.argv[3],sys.argv[4])'
            codes=run_children([[str(root),'-c',code,str(root),'a',base,str(state)],
                                [str(second),'-c',code,str(second),'overlap',base,str(state)]])
            self.assertEqual(sorted(codes),[0,1])


if __name__=='__main__':unittest.main()
