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
    key=hashlib.sha256(canonical(common).encode()).hexdigest()
    root=Path(state_root) if state_root else Path(os.environ['LOCALAPPDATA'])/'TurboMCP/task-claims'
    return root/(key+'.json')


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
    parser.add_argument('--release',help='Integration owner only: release the exact claim after review or explicit recovery')
    args=parser.parse_args();repo=Path(__file__).resolve().parents[1]
    if args.release:release(repo,args.release)
    else:print(json.dumps(current(repo),indent=2))
