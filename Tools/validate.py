"""Run the bounded offline validation tier and record machine-readable evidence."""
import datetime,json,pathlib,subprocess,sys,uuid
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
    run=ROOT/'.local/validation'/uuid.uuid4().hex;run.mkdir(parents=True)
    catalog=json.loads((ROOT/'Tests/catalog.json').read_text())
    checks=[{'id':n,'path':'Tools/'+n+'.py','timeout_seconds':300} for n in ['validate-repo','check-language','check-unreachable']]
    checks += [x for x in catalog['suites'] if x['tier']=='offline']
    results=[]
    for check in checks:
        cmd=[sys.executable,str(ROOT/check['path'])]
        try:
            result=subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=check['timeout_seconds'])
            code=result.returncode;output=result.stdout
        except subprocess.TimeoutExpired as exc:code=124;output=exc.stdout or b''
        log=run/(check['id']+'.log');log.write_bytes(output)
        results.append({'id':check['id'],'command':['python',check['path']],'exit_code':code,'log':log.relative_to(ROOT).as_posix()})
        print(('PASS' if code==0 else 'FAIL')+': '+check['id'],flush=True)
    report={'schema':1,'utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),'dirty':bool(subprocess.check_output(['git','status','--porcelain'],cwd=ROOT)),'tier':'offline','results':results}
    (run/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Evidence: '+str(run/'report.json'))
    return int(any(x['exit_code'] for x in results))
if __name__=='__main__':sys.exit(main())
