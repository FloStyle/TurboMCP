"""Export an allowlisted source tree. Never pushes or copies Git history."""
import argparse, hashlib, json, pathlib, re, shutil, subprocess, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
POLICY=ROOT/'release/export-policy.json'
SECRET=re.compile(rb'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----|https?://[^\s/<>]+:[^\s/<>]+@)')
PERSONAL=re.compile(rb'[A-Za-z]:[\\/]+Users[\\/]+(?!<)[^\\/\s]+',re.I)
def source_files():
    policy=json.loads(POLICY.read_text());result=[]
    tracked=set(subprocess.check_output(['git','-C',str(ROOT),'ls-files','-z']).decode().split('\0'))
    for rule in policy['copies']:
        src=ROOT/rule['source']; dest=pathlib.PurePosixPath(rule['destination'])
        if not src.exists():raise ValueError(f'Missing required export source: {rule["source"]}')
        for p in sorted(src.rglob('*')) if src.is_dir() else [src]:
            if not p.is_file():continue
            rel=p.relative_to(ROOT).as_posix()
            # Excludes generated artifacts and untracked additions even if under an allowed directory.
            if rel not in tracked:continue
            if p.suffix.lower() not in rule['extensions']:raise ValueError(f'Unapproved export extension: {rel}')
            if p.is_symlink() or not p.resolve().is_relative_to(ROOT):raise ValueError(f'Unsafe source: {rel}')
            target=dest/p.relative_to(src).as_posix() if src.is_dir() else dest
            if target.is_absolute() or '..' in target.parts:raise ValueError('Unsafe export destination')
            data=p.read_bytes()
            if SECRET.search(data) or PERSONAL.search(data):raise ValueError(f'Sensitive-data pattern in {rel}; matching content suppressed')
            result.append((p,target,data))
    if not result:raise ValueError('Empty export')
    targets=[str(x[1]).lower() for x in result]
    if len(targets)!=len(set(targets)):raise ValueError('Duplicate export destination')
    return result
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',required=True);parser.add_argument('--preview',action='store_true');parser.add_argument('--evidence');args=parser.parse_args()
    out=pathlib.Path(args.output).resolve()
    if out.exists():raise ValueError('Output must be a new directory; existing output is never overwritten')
    if out==ROOT or ROOT.is_relative_to(out):raise ValueError('Output cannot contain the source repository')
    status=json.loads((ROOT/'release/status.json').read_text())
    head=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD'],text=True).strip()
    if not args.preview:
        dirty=subprocess.check_output(['git','-C',str(ROOT),'status','--porcelain']).strip()
        if dirty:raise ValueError('Release export requires a clean committed source tree')
        if status.get('readiness')!='ready' or not status.get('public_export_approved') or status.get('validated_tree')!=subprocess.check_output(['git','-C',str(ROOT),'rev-parse','HEAD:Plugins/TurboMCP'],text=True).strip():
            raise ValueError('Release gates require ready, owner approval, and the validated plugin tree ID')
    if not args.preview:
        if not args.evidence:raise ValueError('Release export requires --evidence from acceptance execution')
        subprocess.run([sys.executable,str(ROOT/'Tools/release-check.py'),'--evidence',args.evidence],check=True)
    files=source_files();out.mkdir(parents=True)
    entries=[]
    for p,target,data in files:
        dest=out/target;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)
        entries.append({'path':target.as_posix(),'sha256':hashlib.sha256(data).hexdigest(),'bytes':len(data)})
    # Private commit IDs and personal machine paths are intentionally omitted from the public manifest.
    (out/'export-manifest.json').write_text(json.dumps({'schema':1,'preview':args.preview,'files':entries},indent=2)+'\n')
    if args.preview:(out/'PREVIEW-NOT-A-RELEASE.txt').write_text('Review-only snapshot of unfinished development. Not approved for publication.\n')
    print(f'PASS: exported {len(files)} allowlisted files to {out}; preview={args.preview}')
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,subprocess.SubprocessError) as exc:sys.exit(f'FAIL: {exc}')
