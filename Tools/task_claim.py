"""Durable serial task claims shared by Git worktrees. Not an authorization boundary."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import time
import uuid
from execution_runtime import ResourceLocks,canonical
from execution_recovery import atomic_json


def location(repo,state_root=None):
    common=subprocess.check_output(['git','-C',str(repo),'rev-parse','--path-format=absolute','--git-common-dir'],text=True).strip()
    # Keep default state beside shared Git metadata, not in a harness-specific
    # user profile. Git worktrees share this directory even with different envs.
    if state_root is None:
        return Path(canonical(common))/'turbomcp/task-claims.json'
    # Explicit roots are for disposable tests, never an environment override.
    key=hashlib.sha256(canonical(common).encode()).hexdigest()
    return Path(state_root)/(key+'.json')


def current(repo,state_root=None):
    path=location(repo,state_root)
    if not path.exists():return None
    data=json.loads(path.read_text(encoding='utf8'))
    if data.get('schema')!=1 or 'active' not in data:raise ValueError('Invalid claim record; inspect before dispatch')
    return data['active']


def claim(repo,task,owner,state_root=None):
    if not re.fullmatch('[A-Za-z0-9_.-]{1,64}',owner):raise ValueError('Owner must be a short ASCII identifier')
    if subprocess.check_output(['git','-C',str(repo),'status','--porcelain']):
        raise RuntimeError('Claim from a clean worktree; preserve existing changes before dispatch')
    path=location(repo,state_root)
    with ResourceLocks(path.parent/'locks',[str(path)]):
        if current(repo,state_root):raise RuntimeError('A task is already claimed; no duplicate worker was assigned')
        data=json.loads(path.read_text(encoding='utf8')) if path.exists() else {'schema':1,'active':None,'history':[]}
        record={'id':uuid.uuid4().hex,'task':task,'owner':owner,'worktree':canonical(repo),
                'base_commit':subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip(),
                'claimed_at':time.time()}
        data['active']=record;atomic_json(path,data)
        return record


def release(repo,claim_id,state_root=None):
    path=location(repo,state_root)
    with ResourceLocks(path.parent/'locks',[str(path)]):
        data=json.loads(path.read_text(encoding='utf8'))
        if not data.get('active') or data['active']['id']!=claim_id:raise ValueError('Claim ID mismatch; nothing released')
        data['history'].append(data['active']);data['active']=None;atomic_json(path,data)


if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inspect',action='store_true',help='Show the actual store path, digest and active claim')
    parser.add_argument('--release',help='Integration owner only: release the exact claim after review or explicit recovery')
    args=parser.parse_args();repo=Path(__file__).resolve().parents[1]
    if args.release:release(repo,args.release)
    elif args.inspect:
        path=location(repo)
        print(json.dumps({'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest() if path.exists() else None,
                          'active':current(repo)},indent=2))
    else:print(json.dumps(current(repo),indent=2))
