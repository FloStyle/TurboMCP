"""Copy the complete tracked source tree without Git history or path rewriting."""
import argparse, hashlib, json, pathlib, re, shutil, subprocess, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
POLICY=ROOT/'release/export-policy.json'
SECRET=re.compile(rb'(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----|https?://[^\s/<>]+:[^\s/<>]+@)')
PERSONAL=re.compile(rb'[A-Za-z]:[\\/]+Users[\\/]+(?!<)[^\\/\s]+',re.I)
def source_files():
    policy=json.loads(POLICY.read_text());result=[]
    if policy.get('schema')!=2 or policy.get('mode')!='complete-tracked-tree':raise ValueError('Unsupported full-tree export policy')
    # Export Git blob bytes, independent of checkout line-ending conversion.
    # A preview uses the staged candidate tree. Unstaged tracked edits must be staged first.
    if subprocess.run(['git','-C',str(ROOT),'diff','--quiet'],check=False).returncode:
        raise ValueError('Unstaged tracked changes: stage the candidate before previewing')
    records=subprocess.check_output(['git','-C',str(ROOT),'ls-files','--stage','-z']).decode('utf8').split('\0')
    for record in records:
        if not record:continue
        meta,rel=record.split('\t',1);mode,oid,stage=meta.split();path=pathlib.PurePosixPath(rel)
        if stage!='0' or mode not in ('100644','100755'):raise ValueError('Unmerged or non-regular source: '+rel)
        if path.is_absolute() or '..' in path.parts:raise ValueError('Unsafe source path: '+rel)
        if any(x in policy['forbidden_components'] for x in path.parts) or path.name in policy['forbidden_names'] or path.suffix.lower() in policy['forbidden_suffixes']:
            raise ValueError('Forbidden tracked content: '+rel)
        data=subprocess.check_output(['git','-C',str(ROOT),'cat-file','blob',oid])
        if SECRET.search(data) or PERSONAL.search(data):raise ValueError('Sensitive-data pattern in '+rel+'; matching content suppressed')
        result.append((ROOT/rel,path,data))
    if not result:raise ValueError('Empty export')
    names=[str(x[1]).casefold() for x in result]
    if len(names)!=len(set(names)):raise ValueError('Case-colliding export paths')
    blobs = {path.as_posix(): data for _, path, data in result}
    if not blobs.get('LICENSE') or blobs.get('Plugins/TurboMCP/LICENSE') != blobs['LICENSE']:
        raise ValueError('Export requires matching root and installable-plugin LICENSE files')
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',required=True);parser.add_argument('--preview',action='store_true');parser.add_argument('--evidence');args=parser.parse_args()
    out=pathlib.Path(args.output).resolve()
    manifest=out.with_name(out.name+'.manifest.json')
    if manifest.exists():raise ValueError('Manifest output already exists')
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
    # Sidecar metadata keeps the copied source tree exactly equal to the Git tree.
    manifest.write_text(json.dumps({'schema':2,'preview':args.preview,'files':entries},indent=2)+'\n')
    print(f'PASS: copied all {len(files)} tracked files unchanged; preview={args.preview}; manifest={manifest}')
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,subprocess.SubprocessError) as exc:sys.exit(f'FAIL: {exc}')
