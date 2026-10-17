"""Exercise stale, missing, duplicate, blocked and altered release evidence."""
import copy,hashlib,importlib.util,pathlib,tempfile
spec=importlib.util.spec_from_file_location('release_check',pathlib.Path(__file__).with_name('release-check.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
with tempfile.TemporaryDirectory() as tmp:
    p=pathlib.Path(tmp);(p/'case.log').write_text('test output');(p/'artifact.dll').write_bytes(b'artifact')
    report={'schema':1,'source_commit':'source','engine_commit':'engine','artifact':{'path':'artifact.dll','sha256':hashlib.sha256((p/'artifact.dll').read_bytes()).hexdigest()},'cases':[{'id':'C1','status':'passed','exit_code':0,'command':['test','C1'],'log':'case.log','log_sha256':hashlib.sha256((p/'case.log').read_bytes()).hexdigest()}]}
    m.validate_evidence(report,['C1'],'source','engine',p)
    bad=[]
    for field,value in [('source_commit','old'),('cases',[]),('cases',report['cases']*2)]:
        r=copy.deepcopy(report);r[field]=value;bad.append(r)
    for field,value in [('status','blocked'),('exit_code',1),('log','../escape'),('log_sha256','wrong')]:
        r=copy.deepcopy(report);r['cases'][0][field]=value;bad.append(r)
    for r in bad:
        try:m.validate_evidence(r,['C1'],'source','engine',p)
        except ValueError:pass
        else:raise AssertionError('Invalid release evidence accepted')
    (p/'artifact.dll').write_bytes(b'changed')
    try:m.validate_evidence(report,['C1'],'source','engine',p)
    except ValueError:pass
    else:raise AssertionError('Changed artifact accepted')
print('RESULT: PASS (release evidence positive case and eight refusal controls)')
