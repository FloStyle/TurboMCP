"""Refuse publication unless every required acceptance case has current, intact evidence."""
import argparse,hashlib,json,pathlib,subprocess,sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
def validate_evidence(report,required,commit,engine,base):
    if report.get('schema')!=1 or report.get('source_commit')!=commit or report.get('engine_commit')!=engine:
        raise ValueError('Evidence schema/source/engine identity mismatch')
    rows=report.get('cases',[]);ids=[r['id'] for r in rows]
    if len(ids)!=len(set(ids)):raise ValueError('Duplicate evidence case')
    found={r['id']:r for r in rows}
    for case in required:
        row=found.get(case)
        if not row or row.get('status')!='passed' or type(row.get('exit_code')) is not int or row['exit_code']!=0 or not row.get('command'):
            raise ValueError('Missing or non-passing acceptance: '+case)
        rel=pathlib.Path(row.get('log',''));log=(base/rel).resolve()
        if rel.is_absolute() or not log.is_relative_to(base.resolve()) or not log.is_file():raise ValueError('Unsafe/missing evidence log: '+case)
        if not log.stat().st_size or hashlib.sha256(log.read_bytes()).hexdigest()!=row.get('log_sha256'):raise ValueError('Evidence log hash mismatch: '+case)
    artifact=report.get('artifact',{});p=(base/artifact.get('path','')).resolve()
    if not p.is_relative_to(base.resolve()) or not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=artifact.get('sha256'):
        raise ValueError('Missing or changed tested artifact')

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--evidence',required=True);args=parser.parse_args()
    status=json.loads((ROOT/'release/status.json').read_text());manifest=json.loads((ROOT/'Tests/acceptance.json').read_text())
    if status.get('readiness')!='ready' or status.get('public_export_approved') is not True:raise ValueError('Release not ready or owner approval not recorded')
    if subprocess.check_output(['git','status','--porcelain'],cwd=ROOT).strip():raise ValueError('Release requires a clean committed tree')
    cases=[r for r in manifest['cases'] if r.get('required')]
    if not cases or any(not r.get('command') or not r.get('existingEquivalent') for r in cases):raise ValueError('Required acceptance cases are not mapped to executable checks')
    head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    lock=json.loads((ROOT/'engine.lock.json').read_text());path=pathlib.Path(args.evidence).resolve()
    validate_evidence(json.loads(path.read_text()),[r['id'] for r in cases],head,lock['commit'],path.parent)
    print(f'PASS: {len(cases)} required acceptance records, source/engine identity, log and artifact hashes')
if __name__=='__main__':
    try:main()
    except (ValueError,KeyError,OSError,subprocess.SubprocessError) as exc:sys.exit('FAIL: '+str(exc))
