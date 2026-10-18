"""Durable task claims shared by Git worktrees. Not an authorization boundary.

Schema 1 is the original single-active claim. Schema 2 is a list of
per-assignment claims. Migration is explicit and never automatic.
"""
import hashlib
import json
import os
from contextlib import contextmanager
from pathlib import Path
import re
import subprocess
import time
import uuid
from execution_runtime import ResourceLocks,canonical
from execution_recovery import atomic_json

LEGACY_SCHEMA=1
SCHEMA=2
PROTECTED_FILES={
    'Tests/catalog.json','docs/audit/capabilities.json','TODO.md','ROADMAP.md',
    'roadmap/queue.json','roadmap/worker-scopes.json','AGENTS.md','docs/worker-boundaries.md',
}
PROTECTED_PREFIXES=('todo/',)


def location(repo,state_root=None):
    common=subprocess.check_output(['git','-C',str(repo),'rev-parse','--path-format=absolute','--git-common-dir'],text=True).strip()
    # Keep default state beside shared Git metadata, not in a harness-specific
    # user profile. Git worktrees share this directory even with different envs.
    if state_root is None:
        return Path(canonical(common))/'turbomcp/task-claims.json'
    # Explicit roots are for disposable tests, never an environment override.
    key=hashlib.sha256(canonical(common).encode()).hexdigest()
    return Path(state_root)/(key+'.json')


def _git(repo,*args):
    return subprocess.check_output(['git','-C',str(repo),*args],stderr=subprocess.STDOUT).decode('utf8').strip()


def _full_sha(value):
    if not isinstance(value,str) or not re.fullmatch('[0-9a-f]{40}',value):
        raise ValueError('A full 40-hex approved base SHA is required')
    return value


@contextmanager
def _locked(path):
    # ResourceLocks is deliberately non-blocking. A simultaneous disjoint claim
    # waits briefly so it can observe the first claim instead of failing on the
    # lock itself. Only acquisition is retried; exceptions from the body are
    # never caught here.
    locks=ResourceLocks(path.parent/'locks',[str(path)])
    for _ in range(100):
        try:
            locks.__enter__()
        except RuntimeError as exc:
            if 'already owned' not in str(exc):
                raise
            time.sleep(0.02)
        else:
            try:
                yield
            finally:
                locks.__exit__(None,None,None)
            return
    raise RuntimeError('Claim store is busy; retry')


def _normal_path(raw):
    if not isinstance(raw,str) or not raw or raw!=raw.strip():
        raise ValueError('Invalid claim file entry')
    if '\\' in raw or raw.startswith('/') or raw.endswith('/') or '//' in raw:
        raise ValueError('Claim files must be relative POSIX paths: '+str(raw))
    parts=raw.split('/')
    for part in parts:
        if part in ('','.','..'):
            raise ValueError('Claim file escapes its root: '+raw)
        if ':' in part:
            raise ValueError('Claim file has a Windows drive or alternate data stream: '+raw)
        if part.endswith('.') or part.endswith(' ') or part!=part.rstrip(' .'):
            raise ValueError('Claim file has a trailing-dot or trailing-space alias: '+raw)
    if any(character in raw for character in '*?[]'):
        raise ValueError('Claim files cannot contain glob characters: '+raw)
    return '/'.join(parts)


def _canonical_file(repo,raw):
    path=(Path(repo)/raw).resolve();repo_path=Path(repo).resolve();repo_key=canonical(repo_path)
    key=canonical(path)
    if key!=repo_key and not key.startswith(repo_key+os.sep):
        raise ValueError('Claim file escapes the checkout: '+raw)
    return path,key


def _check_ancestors(repo,parts,allow_final_absent):
    cursor=Path(repo).resolve()
    for index,part in enumerate(parts):
        cursor=cursor/part
        if cursor.is_symlink() or getattr(cursor,'is_junction',lambda:False)():
            raise ValueError('Claim file uses a symlink or junction: '+'/'.join(parts))
        final=index==len(parts)-1
        if not final and (not cursor.exists() or not cursor.is_dir()):
            raise ValueError('Claim file parent is not an ordinary directory: '+'/'.join(parts))
        if final and not allow_final_absent and (not cursor.exists() or not cursor.is_file()):
            raise ValueError('Claim file is not an ordinary file: '+'/'.join(parts))


def _normalised_for_policy(raw):
    return _normal_path(raw).lower()


def _is_protected(raw,protected):
    key=_normalised_for_policy(raw)
    for item in protected:
        try:
            candidate=_normalised_for_policy(item.rstrip('/'))
        except ValueError:
            continue
        if key==candidate or key.startswith(candidate+'/'):
            return True
    return False


def _validate_targets(repo,base,files,new_files,protected):
    if not isinstance(files,list) or not isinstance(new_files,list):
        raise ValueError('Assignment files must be lists')
    if not files and not new_files:
        raise ValueError('An assignment must have at least one file target')
    existing={_normal_path(raw) for raw in files}
    new={_normal_path(raw) for raw in new_files}
    if len(existing)!=len(files) or len(new)!=len(new_files):
        raise ValueError('Assignment contains duplicate file targets')
    if existing & new:
        raise ValueError('A file cannot be both existing and new')
    seen=set()
    for raw in sorted(existing|new):
        path,key=_canonical_file(repo,raw)
        if key in seen:
            raise ValueError('Assignment files alias the same canonical path')
        seen.add(key)
        if _is_protected(raw,protected):
            raise ValueError('Assignment file is integrator-owned: '+raw)
        if raw in new:
            _check_ancestors(repo,raw.split('/'),allow_final_absent=True)
            if path.exists():
                raise ValueError('New assignment file already exists: '+raw)
            try:
                entry=_git(repo,'ls-tree',base,'--',raw)
            except subprocess.CalledProcessError as exc:
                raise ValueError('Base tree is unreadable for '+raw) from exc
            if entry:
                raise ValueError('New assignment file already exists at the base: '+raw)
        else:
            _check_ancestors(repo,raw.split('/'),allow_final_absent=False)
            try:
                entry=_git(repo,'ls-tree',base,'--',raw).split('\t',1)[0].split()
            except subprocess.CalledProcessError as exc:
                raise ValueError('Assignment file is absent at the approved base: '+raw) from exc
            if not entry or entry[0]!='100644' or entry[1]!='blob':
                raise ValueError('Assignment file is not an ordinary repository blob at the base: '+raw)
    return sorted(existing),sorted(new)


def _validate_active_scope(repo,item):
    files=item.get('files');new_files=item.get('new_files')
    if not isinstance(files,list) or not isinstance(new_files,list):
        raise ValueError('Active claim has invalid file lists')
    protected=set(PROTECTED_FILES)|set(PROTECTED_PREFIXES)
    seen=set()
    for raw in files+new_files:
        raw=_normal_path(raw)
        if _is_protected(raw,protected):
            raise ValueError('Active claim contains an integrator-owned file: '+raw)
        _,key=_canonical_file(repo,raw)
        if key in seen:
            raise ValueError('Active claim contains duplicate files')
        seen.add(key)
        _check_ancestors(repo,raw.split('/'),allow_final_absent=True)


def _load_manifest(repo,base,task,assignment):
    try:
        manifest=json.loads(_git(repo,'show',base+':roadmap/worker-scopes.json'))
    except (subprocess.CalledProcessError,json.JSONDecodeError) as exc:
        raise ValueError('The approved base has no readable worker scope manifest') from exc
    if manifest.get('schema')!=2:
        raise ValueError('The approved base does not contain schema-2 assignment manifests')
    scope=manifest.get('tasks',{}).get(task)
    if not isinstance(scope,dict):
        raise ValueError('No approved scope for task '+task)
    assignments=scope.get('assignments',{})
    if not isinstance(assignments,dict) or assignment not in assignments:
        raise ValueError('Unknown assignment '+assignment+' for '+task)
    entry=assignments[assignment]
    if not isinstance(entry,dict) or 'files' not in entry:
        raise ValueError('Malformed assignment '+assignment)
    new_files=entry.get('new_files',[])
    protected=set(PROTECTED_FILES)|set(PROTECTED_PREFIXES)|set(scope.get('protected',[]))
    return _validate_targets(repo,base,entry['files'],new_files,protected)


def _read_store(path,repo):
    if not path.exists():
        return None
    try:
        data=json.loads(path.read_text(encoding='utf8'))
    except json.JSONDecodeError as exc:
        raise ValueError('Malformed claim store; inspect before dispatch') from exc
    schema=data.get('schema')
    if schema==LEGACY_SCHEMA:
        if 'active' not in data or not isinstance(data.get('history'),list):
            raise ValueError('Invalid schema-1 claim record')
        active=data['active']
        if active is not None:
            if not isinstance(active,dict) or not active:
                raise ValueError('Invalid schema-1 active claim')
            for key in ('id','task','owner','worktree','base_commit','claimed_at'):
                if key not in active:
                    raise ValueError('Schema-1 active claim is missing '+key)
            _full_sha(active['base_commit'])
            if not isinstance(active['claimed_at'],(int,float)):
                raise ValueError('Schema-1 active claim has an invalid timestamp')
        return data
    if schema==SCHEMA:
        if not isinstance(data.get('active'),list) or not isinstance(data.get('history'),list):
            raise ValueError('Invalid schema-2 claim record')
        if not isinstance(data.get('schema2_mutated'),bool):
            raise ValueError('Invalid schema-2 mutation marker')
        claims=data['active']
        if claims and not data['schema2_mutated']:
            raise ValueError('Schema 2 has active claims but says it was never mutated')
        ids=[]
        for item in claims:
            if not isinstance(item,dict):
                raise ValueError('Invalid schema-2 active claim')
            for key in ('claim_id','task','assignment','owner','worktree','git_common_dir','base_commit','claimed_at'):
                if key not in item:
                    raise ValueError('Schema-2 active claim is missing '+key)
            _full_sha(item['base_commit'])
            if not isinstance(item['claimed_at'],(int,float)):
                raise ValueError('Schema-2 active claim has an invalid timestamp')
            for key in ('worktree','git_common_dir'):
                if not isinstance(item[key],str) or not Path(item[key]).is_absolute():
                    raise ValueError('Schema-2 active claim has an invalid '+key)
            _validate_active_scope(repo,item)
            ids.append(item['claim_id'])
        if len(ids)!=len(set(ids)):
            raise ValueError('Duplicate schema-2 claim ID')
        return data
    raise ValueError('Unsupported claim schema; inspect before dispatch')


def _write_store(path,data):
    atomic_json(path,data)


def current(repo,state_root=None):
    """Read a schema-1 active claim. Schema 2 requires active_claims()."""
    data=_read_store(location(repo,state_root),repo)
    if data is None:
        return None
    if data.get('schema')!=LEGACY_SCHEMA:
        raise ValueError('Claim store is schema 2; use active_claims or migrate explicitly')
    return data['active']


def active_claims(repo,state_root=None):
    data=_read_store(location(repo,state_root),repo)
    if data is None:
        return []
    if data.get('schema')==LEGACY_SCHEMA:
        active=data.get('active')
        if not active:
            return []
        return [dict(active,claim_id=active.get('id'),assignment=None,files=None,new_files=None,legacy=True)]
    return list(data['active'])


def claim(repo,task,owner,state_root=None):
    """Original single-claim path, kept for schema-1 clients."""
    if not re.fullmatch('[A-Za-z0-9_.-]{1,64}',owner):
        raise ValueError('Owner must be a short ASCII identifier')
    if subprocess.check_output(['git','-C',str(repo),'status','--porcelain']):
        raise RuntimeError('Claim from a clean worktree; preserve existing changes before dispatch')
    path=location(repo,state_root)
    with _locked(path):
        data=_read_store(path,repo)
        if data is not None and data.get('schema')==SCHEMA:
            raise RuntimeError('Claim store is schema 2; use claim_assignment')
        if data is None:
            data={'schema':LEGACY_SCHEMA,'active':None,'history':[]}
        if data.get('active'):
            raise RuntimeError('A task is already claimed; no duplicate worker was assigned')
        record={'id':uuid.uuid4().hex,'task':task,'owner':owner,'worktree':canonical(repo),
                'base_commit':_git(repo,'rev-parse','HEAD'),'claimed_at':time.time()}
        data['active']=record;_write_store(path,data)
        return record


def _worktree_identity(repo,claims):
    worktree=canonical(repo)
    common=canonical(_git(repo,'rev-parse','--path-format=absolute','--git-common-dir'))
    registered=set()
    for line in _git(repo,'worktree','list','--porcelain').splitlines():
        if line.startswith('worktree '):
            registered.add(canonical(line[len('worktree '):]))
    if worktree not in registered:
        raise ValueError('The worktree is not registered with Git: '+worktree)
    if canonical(_git(repo,'rev-parse','--show-toplevel'))!=worktree:
        raise ValueError('The canonical worktree path does not match its Git root')
    for item in claims:
        if canonical(item.get('worktree',''))==worktree:
            raise ValueError('This worktree already holds an active claim')
        if canonical(item.get('git_common_dir',''))!=common:
            raise ValueError('Active claims must share the same Git common directory')
    return worktree,common


def _overlaps(repo,old_files,new_files):
    for old in old_files:
        old_key=canonical(Path(repo)/old)
        for raw in new_files:
            key=canonical(Path(repo)/raw)
            if old_key==key or old_key.startswith(key+os.sep) or key.startswith(old_key+os.sep):
                return True
    return False


def claim_assignment(repo,task,assignment,owner,base,state_root=None):
    if not re.fullmatch('[A-Za-z0-9_.-]{1,64}',owner):
        raise ValueError('Owner must be a short ASCII identifier')
    if not re.fullmatch('[A-Za-z0-9_.-]{1,128}',assignment or ''):
        raise ValueError('Assignment must be a short ASCII identifier')
    base=_full_sha(base)
    if _git(repo,'rev-parse','HEAD')!=base:
        raise ValueError('The approved base must be the current HEAD of the clean worktree')
    if subprocess.check_output(['git','-C',str(repo),'status','--porcelain']):
        raise RuntimeError('Claim from a clean worktree; preserve existing changes before dispatch')
    path=location(repo,state_root)
    with _locked(path):
        data=_read_store(path,repo)
        if data is None or data.get('schema')!=SCHEMA:
            raise ValueError('Schema 2 is required before assignment claims; migrate explicitly')
        claims=data['active']
        if any(item.get('legacy') for item in claims):
            raise ValueError('An active legacy claim has no recorded scope; new claims are refused')
        worktree,common=_worktree_identity(repo,claims)
        files,new_files=_load_manifest(repo,base,task,assignment)
        targets=files+new_files
        for item in claims:
            if item.get('task')==task and item.get('assignment')==assignment:
                raise ValueError('That assignment is already claimed')
            if _overlaps(repo,(item.get('files') or [])+(item.get('new_files') or []),targets):
                raise ValueError('Assignment overlaps active claim '+item['claim_id'])
        record={'claim_id':uuid.uuid4().hex,'task':task,'assignment':assignment,'owner':owner,
                'worktree':worktree,'git_common_dir':common,'base_commit':base,
                'files':files,'new_files':new_files,'claimed_at':time.time()}
        claims.append(record);data['schema2_mutated']=True;_write_store(path,data)
        return record


def release(repo,claim_id,state_root=None):
    path=location(repo,state_root)
    with _locked(path):
        data=_read_store(path,repo)
        if data is None:
            raise ValueError('Claim ID mismatch; nothing released')
        if data.get('schema')==LEGACY_SCHEMA:
            if not data.get('active') or data['active'].get('id')!=claim_id:
                raise ValueError('Claim ID mismatch; nothing released')
            data['history'].append(data['active']);data['active']=None
        else:
            found=[item for item in data['active'] if item.get('claim_id')==claim_id]
            if not found:
                raise ValueError('Claim ID mismatch; nothing released')
            data['history'].append(found[0]);data['active'].remove(found[0]);data['schema2_mutated']=True
        _write_store(path,data)


def migrate(repo,state_root=None):
    path=location(repo,state_root)
    with _locked(path):
        data=_read_store(path,repo)
        if data is None or data.get('schema')!=LEGACY_SCHEMA:
            raise ValueError('Migration requires a schema-1 store')
        if data.get('active') is not None:
            raise ValueError('Migration requires schema-1 active explicitly null; the store is unchanged')
        backup=path.with_name(path.name+'.schema1.bak')
        if backup.exists():
            raise ValueError('A schema-1 backup already exists; inspect before migration')
        backup.write_bytes(path.read_bytes())
        migrated={'schema':SCHEMA,'migrated_from':LEGACY_SCHEMA,'schema2_mutated':False,
                  'active':[],'history':data.get('history',[])}
        _write_store(path,migrated)
        return {'backup':str(backup),'store':str(path)}


def rollback(repo,state_root=None):
    path=location(repo,state_root)
    with _locked(path):
        data=_read_store(path,repo)
        if data is None or data.get('schema')!=SCHEMA:
            raise ValueError('Rollback requires a schema-2 store')
        if data.get('active'):
            raise ValueError('Rollback cannot erase active schema-2 claims')
        if data.get('schema2_mutated'):
            raise ValueError('Rollback is forbidden after any schema-2 mutation')
        backup=path.with_name(path.name+'.schema1.bak')
        if not backup.exists():
            raise ValueError('No schema-1 backup exists')
        restored=_read_store(backup,repo)
        if restored is None or restored.get('schema')!=LEGACY_SCHEMA or restored.get('active') is not None:
            raise ValueError('Schema-1 backup is not an empty legacy store; nothing restored')
        temporary=path.with_suffix(path.suffix+'.rollback.tmp')
        temporary.write_bytes(backup.read_bytes());os.replace(temporary,path)
        return {'restored':str(path)}


if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inspect',action='store_true',help='Show the actual store path, digest and active claims')
    parser.add_argument('--release',help='Integrator only: release the exact claim ID')
    parser.add_argument('--migrate',action='store_true',help='Migrate an empty schema-1 store; never active')
    parser.add_argument('--rollback',action='store_true',help='Restore the schema-1 backup before any schema-2 mutation')
    parser.add_argument('--claim-assignment',metavar='ASSIGNMENT')
    parser.add_argument('--task');parser.add_argument('--owner');parser.add_argument('--base')
    args=parser.parse_args();repo=Path(__file__).resolve().parents[1]
    if args.release:release(repo,args.release)
    elif args.migrate:print(json.dumps(migrate(repo)))
    elif args.rollback:print(json.dumps(rollback(repo)))
    elif args.claim_assignment:
        if not (args.task and args.owner and args.base):raise ValueError('--task, --owner and --base are required')
        print(json.dumps(claim_assignment(repo,args.task,args.claim_assignment,args.owner,args.base),indent=2))
    elif args.inspect:
        path=location(repo);data=_read_store(path,repo)
        print(json.dumps({'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None,
                          'schema':data.get('schema') if data else None,'active':active_claims(repo)},indent=2))
    else:print(json.dumps(active_claims(repo),indent=2))
