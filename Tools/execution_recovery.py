"""Durable local fixture recovery and conservative failure-report collection."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import uuid


def atomic_json(path, data):
    path=Path(path)
    temporary=path.with_suffix(path.suffix+'.tmp')
    with temporary.open('w',encoding='utf8',newline='\n') as handle:
        json.dump(data,handle,indent=2);handle.write('\n')
        handle.flush();os.fsync(handle.fileno())
    os.replace(temporary,path)


def files(root):
    root=Path(root)
    if root.is_symlink() or root.is_junction():
        raise ValueError('Linked fixture roots are refused')
    result={}
    if not root.exists(): return result
    for path in root.rglob('*'):
        if path.is_symlink() or path.is_junction() or not path.resolve().is_relative_to(root.resolve()):
            raise ValueError('Linked fixture content is refused')
        if path.is_file(): result[path.relative_to(root).as_posix()]=path
    return result


def digest(path):
    with Path(path).open('rb') as handle:
        return hashlib.file_digest(handle,'sha256').hexdigest()


def snapshot(repo,run):
    repo=Path(repo).resolve();run=Path(run).resolve()
    content=repo/'TestProject/Content'
    if not content.resolve().is_relative_to(repo):raise ValueError('Fixture boundary escapes the checkout')
    source=files(content)
    target=run/'fixture-backup';target.mkdir()
    hashes={}
    for name,path in source.items():
        destination=target/name;destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(path,destination);hashes[name]=digest(destination)
        if digest(path)!=hashes[name]:raise RuntimeError('Fixture changed while snapshotting')
    record={'schema':1,'repo':str(repo),'boundary':'TestProject/Content','state':'pending',
            'owner_pid':os.getpid(),'files':hashes}
    atomic_json(run/'recovery.json',record)
    return record


def restore(repo,run):
    """Caller must hold engine/repository locks and prove no editor is alive."""
    repo=Path(repo).resolve();run=Path(run).resolve()
    record=json.loads((run/'recovery.json').read_text(encoding='utf8'))
    if record['repo']!=str(repo) or record['boundary']!='TestProject/Content':
        raise ValueError('Recovery belongs to another checkout or boundary')
    if record.get('schema')!=1 or record.get('state')!='pending':
        raise ValueError('Only a pending recovery can be restored; completed runs cannot overwrite new work')
    content=repo/record['boundary']
    if not content.resolve().is_relative_to(repo):raise ValueError('Fixture boundary escapes the checkout')
    current=files(content)
    backup=files(run/'fixture-backup')
    expected=record['files']
    # Verify the entire backup before touching any candidate file.
    if set(backup)!=set(expected) or any(digest(backup[n])!=expected[n] for n in expected):
        raise ValueError('Recovery backup is incomplete or changed; nothing restored')
    for name in expected:
        destination=content/name
        if not destination.resolve().is_relative_to(content.resolve()):
            raise ValueError('Unsafe recovery destination')
    for name in expected:
        destination=content/name;destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(backup[name],destination)
    for name in set(current)-set(expected):
        destination=current[name]
        if not destination.resolve().is_relative_to(content.resolve()):
            raise ValueError('Unsafe fixture cleanup destination')
        destination.unlink()
    actual=files(content)
    if set(actual)!=set(expected) or any(digest(actual[n])!=expected[n] for n in expected):
        raise RuntimeError('Fixture restoration did not match the snapshot')
    record['state']='restored';atomic_json(run/'recovery.json',record)
    return len(expected)


def pending(repo):
    root=Path(repo)/'.local/executions'
    result=[]
    for path in root.glob('*/recovery.json'):
        try:
            data=json.loads(path.read_text(encoding='utf8'))
            if data.get('state')!='restored':result.append(path.parent.name)
        except (OSError,ValueError):result.append(path.parent.name)
    return sorted(result)


def collect_failure(repo,run,report):
    """Keep raw artifacts local. The draft contains only controlled fields."""
    repo=Path(repo);run=Path(run)
    if (run/'feedback-draft.md').exists():raise FileExistsError('Feedback already exists; preserve the original report')
    destination=run/'private-evidence'/uuid.uuid4().hex;destination.mkdir(parents=True)
    copied=[];errors=[];total_bytes=0
    for relative in ('TestProject/Saved/Crashes','TestProject/Saved/Logs','.agent/logs'):
        try:
            if not (repo/relative).resolve().is_relative_to(repo.resolve()):raise ValueError('Evidence boundary escapes the checkout')
            for name,path in files(repo/relative).items():
                size=path.stat().st_size
                if total_bytes+size>512*1024*1024:
                    errors.append('Local evidence limit reached; original artifact retained in Saved')
                    continue
                total_bytes+=size
                target=destination/relative/name;target.parent.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(path,target)
                copied.append({'path':target.relative_to(run).as_posix(),'sha256':digest(target),'bytes':target.stat().st_size})
        except (OSError,ValueError) as error:
            errors.append(type(error).__name__+': artifact collection incomplete')
    atomic_json(run/'private-artifacts.json',{'files':copied,'errors':errors,
        'warning':'Local raw artifacts may contain proprietary data. Never upload automatically. Existing artifacts may predate this run.'})
    status=report.get('status','unknown')
    if status not in ('failed','timed_out','blocked','interrupted'):status='unknown'
    # Never embed raw exception text, paths, tool arguments or logs in this draft.
    text=('# TurboMCP test-session failure\n\nStatus: '+status+'\n'
          'Cause: Not confirmed.\n'
          'Mutation outcome: Unknown unless a separate read-back proves it.\n'
          'Automatic mutation retry: Not performed.\n'
          'Reproduction: Maintainer must supply synthetic steps after local review.\n'
          'Crash confirmed: Not inferred from a nonzero exit.\n'
          'Raw evidence: Retained locally; review and sanitize before sharing.\n')
    path=run/'feedback-draft.md'
    with path.open('x',encoding='ascii',newline='\n') as handle:handle.write(text)
    return {'artifact_count':len(copied),'collection_errors':errors,'feedback':'feedback-draft.md'}
