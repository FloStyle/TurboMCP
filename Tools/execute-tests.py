"""Plan or execute a bounded local test request. Never dispatch arbitrary agent commands."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import secrets
import shutil
import subprocess
import sys
import time
import uuid
from execution_runtime import ResourceLocks, canonical, run_owned
from tmcp_paths import find_engine

ROOT=Path(__file__).resolve().parents[1]


def identity(root):
    names=subprocess.check_output(['git','-C',str(root),'ls-files','--cached','--others','--exclude-standard','-z']).decode().split('\0')
    digest=hashlib.sha256()
    for name in sorted(set(names)-{''}):
        path=root/name
        if path.is_symlink() or not path.resolve().is_relative_to(root.resolve()):
            raise ValueError('Candidate contains a link outside the checkout: '+name)
        digest.update(name.encode()+b'\0')
        digest.update(hashlib.sha256(path.read_bytes()).digest() if path.is_file() else b'deleted')
    return digest.hexdigest()


def validate_request(request, root=ROOT):
    required={'schema','commit','tree_digest','profile','suites','max_seconds'}
    if set(request)!=required or request['schema']!=1:raise ValueError('Unknown or missing request fields')
    if request['profile'] not in ('fast','offline','engine'):raise ValueError('Unknown profile')
    if type(request['max_seconds']) is not int or not 1<=request['max_seconds']<=7200:raise ValueError('Run deadline must be 1..7200 seconds')
    if not isinstance(request['suites'],list) or any(not isinstance(x,str) for x in request['suites']):raise ValueError('Suites must be a list of names')
    if len(request['suites'])!=len(set(request['suites'])):raise ValueError('Duplicate suite')
    catalog=json.loads((root/'Tests/catalog.json').read_text(encoding='utf8'))['suites']
    eligible={Path(x['path']).stem for x in catalog if x['tier']=='engine'}
    if request['profile']=='engine':
        if not request['suites'] or not set(request['suites'])<=eligible:raise ValueError('Select explicit catalogued engine suites; all and arbitrary commands are refused')
    elif request['suites']:raise ValueError('Only engine requests accept suite selection')
    commit=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
    if request['commit']!=commit or request['tree_digest']!=identity(root):raise ValueError('Stale request: commit or candidate files changed; generate a new plan')


def commands(request, engine):
    if request['profile']=='fast':return [[sys.executable,'Tools/validate-repo.py'],[sys.executable,'Tools/check-language.py']]
    if request['profile']=='offline':return [[sys.executable,'Tools/validate.py']]
    pwsh=shutil.which('pwsh')
    if not pwsh:raise ValueError('PowerShell 7 is required')
    return [[sys.executable,'Tools/doctor.py'],
            [pwsh,'-NoProfile','-File','Tools/build.ps1','-Engine',engine],
            [pwsh,'-NoProfile','-File','Tools/run-suites.ps1','-Suites',','.join(request['suites'])]]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile',choices=['fast','offline','engine'],default='fast')
    parser.add_argument('--suites',default='')
    parser.add_argument('--max-seconds',type=int,default=1800)
    parser.add_argument('--request',type=Path)
    parser.add_argument('--execute',action='store_true')
    args=parser.parse_args()
    if args.request:
        request=json.loads(args.request.read_text(encoding='utf8'))
    else:
        request={'schema':1,'commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
                 'tree_digest':identity(ROOT),'profile':args.profile,'suites':[x for x in args.suites.split(',') if x], 'max_seconds':args.max_seconds}
    validate_request(request)
    if not args.execute:
        print(json.dumps(request,indent=2));return 0
    if os.name!='nt':raise ValueError('Owned execution currently requires Windows')
    if request['profile']=='engine':
        raise ValueError('Engine execution is not enabled: complete R02 ownership and R03 recovery acceptance first')
    engine=canonical(find_engine()) if request['profile']=='engine' else ''
    run=ROOT/'.local/executions'/uuid.uuid4().hex;run.mkdir(parents=True)
    # Remove inherited automation overrides and common credential variables from children.
    env={k:v for k,v in os.environ.items() if not k.startswith('TURBOMCP_') and not any(w in k.upper() for w in ('TOKEN','SECRET','PASSWORD','API_KEY','AUTHORIZATION'))}
    env['PYTHONIOENCODING']='utf-8';env['MSBUILDDISABLENODEREUSE']='1'
    context={'repo':canonical(ROOT),'engine':engine,'pid':os.getpid(),'token':secrets.token_hex(32)}
    context_path=run/'context.json';context_path.write_text(json.dumps(context),encoding='utf8')
    env['TURBOMCP_EXECUTION_CONTEXT']=str(context_path);env['TURBOMCP_EXECUTION_TOKEN']=context['token']
    env['TURBOMCP_RUNNER_LOG_ROOT']=str(run/'suites')
    env['TURBOMCP_OWNER_ID']='executor:'+str(os.getpid())
    if engine:env['TURBOMCP_ENGINE']=engine
    resources=['repo:'+canonical(ROOT)]
    if engine:resources+=['engine:'+engine,'port:8000']
    lockroot=Path(os.environ['LOCALAPPDATA'])/'TurboMCP/execution-locks'
    report={'request':request,'engine':engine,'results':[],'status':'running','privacy':'Local trusted-fixture execution; not an OS sandbox or network-isolation certificate'}
    start=time.monotonic()
    try:
        with ResourceLocks(lockroot,resources):
            validate_request(request)
            if engine:
                # Never adopt a pre-existing test editor or compete with an engine build.
                query=r"Get-CimInstance Win32_Process | Where-Object { $_.Name -match '^(UnrealEditor|UnrealEditor-Cmd|UnrealBuildTool)\.exe$' } | Select-Object ProcessId,ExecutablePath,CommandLine | ConvertTo-Json -Compress"
                result=subprocess.run(['pwsh','-NoProfile','-Command',query],capture_output=True,text=True,timeout=20,check=True)
                processes=json.loads(result.stdout) if result.stdout.strip() else []
                if isinstance(processes,dict):processes=[processes]
                for process in processes:
                    text=(str(process.get('ExecutablePath') or '')+' '+str(process.get('CommandLine') or '')).lower()
                    if not process.get('CommandLine') or engine.lower() in text:
                        raise RuntimeError('A pre-existing editor/build may use this engine. Close it explicitly; no process was stopped')
            for index,command in enumerate(commands(request,engine)):
                remaining=request['max_seconds']-(time.monotonic()-start)
                if remaining<=0:raise TimeoutError('Total run deadline exceeded')
                print('RUN: '+Path(command[1] if command[0]==sys.executable else command[3]).name,flush=True)
                log=run/f'{index:02d}.log'
                result=run_owned(command,str(ROOT),env,str(log),remaining)
                result.update(command=command,log=log.name)
                report['results'].append(result)
                if result['exit_code']!=0:
                    report['status']='timed_out' if result['timed_out'] else 'failed';break
            else:report['status']='passed'
            if identity(ROOT)!=request['tree_digest']:
                report['status']='failed';report['reason']='Candidate files changed during validation; inspect before retrying'
    except (OSError,ValueError,RuntimeError,TimeoutError,subprocess.SubprocessError) as error:
        report['status']='blocked';report['reason']=str(error)
    finally:
        report['duration_seconds']=round(time.monotonic()-start,3)
        (run/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
        context_path.unlink(missing_ok=True)
    print('RESULT: '+report['status'].upper()+'; evidence: '+str(run/'report.json'))
    return 0 if report['status']=='passed' else 1


if __name__=='__main__':
    try:sys.exit(main())
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as error:sys.exit('REFUSED: '+str(error))
