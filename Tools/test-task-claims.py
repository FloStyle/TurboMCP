"""Disposable Git worktree tests for serial and per-assignment claims."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from task_claim import _locked,_overlaps,active_claims,claim,claim_assignment,current,location,migrate,release,rollback

REPO=Path(__file__).resolve().parents[1]
LEGACY_BASE='682ebe1f13ce0256003bce74c5ac8e5d33af13fa'


def legacy_source():
    return subprocess.check_output(['git','-C',str(REPO),'show',LEGACY_BASE+':Tools/task_claim.py'],
                                   stderr=subprocess.STDOUT).decode('utf8')


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


TASK_TEXT = """ID: {task}
Status: {status}
Readiness: ready
## Problem
Synthetic task.
## Scope
Synthetic scope.
## Dependencies
None.
## Acceptance
Synthetic acceptance.
## Verification
Synthetic verification.
## Stop conditions
Stop.
## Deliverable
Synthetic deliverable.
## Completion evidence
Pending.
"""


def make_cli_repo(root,task='R04',depends=None,status='todo'):
    root.mkdir(parents=True,exist_ok=True)
    git(root,'init','-q')
    for name in ('task_claim.py','next-task.py'):
        (root/'Tools').mkdir(parents=True,exist_ok=True)
        shutil.copy2(Path(__file__).with_name(name),root/'Tools'/name)
    assignments={'a':{'files':['src/a.py']},'b':{'files':['src/b.py']}}
    manifest_data={'schema':2,'tasks':{task:{'task_file':'todo/%s.md'%task.lower(),'assignments':assignments}}}
    write(root/'roadmap/worker-scopes.json',json.dumps(manifest_data))
    write(root/'roadmap/queue.json',json.dumps({'tasks':[{'id':task,'path':'todo/%s.md'%task.lower(),
        'depends_on':depends or [],'track':'v1'}]}))
    write(root/'.gitignore','__pycache__/\n')
    write(root/('todo/%s.md'%task.lower()),TASK_TEXT.format(task=task,status=status))
    write(root/'src/a.py','a\n');write(root/'src/b.py','b\n')
    return commit(root)


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

    def test_malformed_backup_and_false_marker_refuse(self):
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-backup-') as temp:
            root=Path(temp)/'repo';state=Path(temp)/'state';make_repo(root)
            claim(root,'R04','worker',state);path=location(root,state)
            data=json.loads(path.read_text());data['active']=None;path.write_text(json.dumps(data),encoding='utf8')
            migrate(root,state);backup=path.with_name(path.name+'.schema1.bak')
            backup.write_text('{}',encoding='utf8');before=path.read_bytes()
            with self.assertRaises(ValueError):rollback(root,state)
            self.assertEqual(path.read_bytes(),before)
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-marker-') as temp:
            root=Path(temp)/'repo';state=Path(temp)/'state';make_repo(root)
            base=git(root,'rev-parse','HEAD');path=location(root,state);path.parent.mkdir(parents=True,exist_ok=True)
            record={'claim_id':'x','task':'R04','assignment':'a','owner':'w','worktree':str(root),
                    'git_common_dir':str(root/'.git'),'base_commit':base,'files':['src/a.py'],
                    'new_files':[],'claimed_at':1}
            path.write_text(json.dumps({'schema':2,'active':[record],'history':[],'schema2_mutated':False}),encoding='utf8')
            before=path.read_bytes()
            with self.assertRaises(ValueError):rollback(root,state)
            self.assertEqual(path.read_bytes(),before)

    def test_malformed_schema1_and_schema2_records(self):
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-malformed-') as temp:
            root=Path(temp)/'repo';state=Path(temp)/'state';base=make_repo(root,manifest({'a':{'files':['src/a.py']}}))
            path=location(root,state);path.parent.mkdir(parents=True,exist_ok=True)
            bad=[
                {'schema':1,'active':[],'history':[]},
                {'schema':1,'active':{},'history':[]},
                {'schema':1,'active':None},
                {'schema':2,'active':[{'claim_id':'x'}],'history':[],'schema2_mutated':True},
            ]
            for payload in bad:
                path.write_text(json.dumps(payload),encoding='utf8');before=path.read_bytes()
                with self.assertRaises(ValueError):claim_assignment(root,'R04','a','worker',base,state)
                self.assertEqual(path.read_bytes(),before)

    def test_new_files_and_alias_protection(self):
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-new-') as temp:
            root=Path(temp)/'repo';second=Path(temp)/'second';state=Path(temp)/'state'
            base=make_repo(root,manifest({
                'new':{'files':[],'new_files':['src/new.py']},
                'protected_case':{'files':['Tests/CATALOG.JSON']},
                'ads':{'files':['src/a.py:stream']},
                'dot':{'files':['src/a.py.']},
            }))
            add_worktree(root,second);seed_state(root,state)
            record=claim_assignment(root,'R04','new','worker',base,state)
            self.assertEqual(record['new_files'],['src/new.py'])
            self.assertEqual(active_claims(root,state)[0]['new_files'],['src/new.py'])
            for assignment in ('protected_case','ads','dot'):
                with self.assertRaises(ValueError):claim_assignment(second,'R04',assignment,'worker',base,state)

    def test_body_exception_not_retried(self):
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-body-') as temp:
            path=Path(temp)/'store.json';calls=[]
            with self.assertRaises(RuntimeError):
                with _locked(path):
                    calls.append(1)
                    raise RuntimeError('already owned from body')
            self.assertEqual(calls,[1])

    def test_next_task_cli_same_task_assignments_and_refusals(self):
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-cli-') as temp:
            root=Path(temp)/'repo';second=Path(temp)/'second';state=None;base=make_cli_repo(root)
            add_worktree(root,second)
            seed_state(root,state)
            def run(path,*extra):
                return subprocess.run([sys.executable,str(path/'Tools/next-task.py'),*extra],
                                      cwd=path,env=child_env(),capture_output=True,text=True,timeout=20)
            first=run(root,'--claim','worker1','--assignment','a','--base',base)
            self.assertEqual(first.returncode,0,first.stderr)
            second_result=run(second,'--claim','worker2','--assignment','b','--base',base)
            self.assertEqual(second_result.returncode,0,second_result.stderr)
            self.assertEqual({item['assignment'] for item in active_claims(root,state)},{'a','b'})
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-cli-blocked-') as temp:
            root=Path(temp)/'repo';base=make_cli_repo(root,task='R05',depends=['R99'])
            result=subprocess.run([sys.executable,str(root/'Tools/next-task.py'),'--claim','worker','--assignment','a','--base',base],
                                  cwd=root,env=child_env(),capture_output=True,text=True,timeout=20)
            self.assertNotEqual(result.returncode,0)
            self.assertTrue(result.stdout or result.stderr)
        with tempfile.TemporaryDirectory(prefix='tmcp-claim-cli-status-') as temp:
            root=Path(temp)/'repo';base=make_cli_repo(root,task='R06',status='blocked')
            result=subprocess.run([sys.executable,str(root/'Tools/next-task.py'),'--claim','worker','--assignment','a','--base',base],
                                  cwd=root,env=child_env(),capture_output=True,text=True,timeout=20)
            self.assertNotEqual(result.returncode,0)

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
            legacy=Path(temp)/'legacy_task_claim.py';write(legacy,legacy_source())
            path=location(root,state);path.parent.mkdir(parents=True,exist_ok=True)
            active=[{'claim_id':'x','task':'R04','assignment':'a','owner':'w','worktree':str(root),
                     'git_common_dir':str(Path(temp)/'repo/.git'),'base_commit':base,
                     'files':['src/a.py'],'new_files':[],'claimed_at':1}]
            for claims in ([],active):
                path.write_text(json.dumps({'schema':2,'active':claims,'history':[],'schema2_mutated':bool(claims)}),encoding='utf8')
                before=path.read_bytes()
                for mode in ('current','claim','release'):
                    code=("import sys;sys.path.insert(0,sys.argv[1]);import legacy_task_claim as l;"
                          "mode=sys.argv[2];repo=sys.argv[3];state=sys.argv[4];"
                          "l.current(repo,state) if mode=='current' else "
                          "(l.claim(repo,'R04','worker',state) if mode=='claim' else l.release(repo,'wrong',state))")
                    completed=subprocess.run([sys.executable,'-c',code,str(Path(temp)),mode,str(root),str(state)],
                                             capture_output=True,text=True,timeout=10,env=child_env())
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
