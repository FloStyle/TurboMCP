"""Restore one interrupted local test run after refusing live owners and editors."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import sys
from execution_runtime import ResourceLocks,canonical
from execution_recovery import atomic_json,restore,collect_failure
from engine_execution import preflight,finish_lease
from tmcp_paths import find_engine

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_id');parser.add_argument('--execute',action='store_true')
    args=parser.parse_args()
    if not re.fullmatch('[0-9a-f]{32}',args.run_id):raise ValueError('Use the exact run ID; paths are refused')
    run=ROOT/'.local/executions'/args.run_id
    if run.is_symlink() or run.is_junction() or not run.resolve().is_relative_to(ROOT.resolve()):
        raise ValueError('Linked recovery run refused')
    owner=json.loads((run/'owner.json').read_text(encoding='utf8'))
    engine=canonical(find_engine())
    if engine!=owner['engine']:raise ValueError('Recovery engine differs from configured engine')
    if not args.execute:
        print('PLAN: inspect the owner, acquire checkout/engine/port locks, preserve evidence, restore owned fixtures');return 0
    locks=Path(os.environ['LOCALAPPDATA'])/'TurboMCP/execution-locks'
    with ResourceLocks(locks,['repo:'+canonical(ROOT),'engine:'+engine,'port:8000']):
        env=dict(os.environ,TMCP_RECOVERY_PID=str(int(owner['pid'])))
        result=subprocess.run(['pwsh','-NoProfile','-Command',
            "if(Get-Process -Id ([int]$env:TMCP_RECOVERY_PID) -ErrorAction SilentlyContinue){exit 9}"],env=env,timeout=20)
        if result.returncode:raise RuntimeError('Recorded owner PID is alive; recovery refused')
        preflight(engine)
        lease=ROOT/'.agent/turbo-mcp-lease.json'
        if lease.exists() and json.loads(lease.read_text(encoding='utf-8-sig')).get('OwnerId')!=owner['owner']:
            raise RuntimeError('Foreign lease preserved; recovery refused')
        report={'status':'interrupted','reason':'Supervisor ended before final recovery evidence'}
        report_path=run/'report.json'
        if report_path.exists():
            report=json.loads(report_path.read_text(encoding='utf8'));report['status']='interrupted'
        if not (run/'feedback-draft.md').exists():report['failure_evidence']=collect_failure(ROOT,run,report)
        report['restored_files']=restore(ROOT,run)
        finish_lease(ROOT,owner['owner'])
        report['recovery']='completed; interrupted test remains failed'
        atomic_json(run/'recovered-report.json',report)
    print('RECOVERED: fixtures restored; the interrupted test is not a pass')
    return 0


if __name__=='__main__':
    try:sys.exit(main())
    except (OSError,ValueError,RuntimeError,KeyError,subprocess.SubprocessError) as error:sys.exit('REFUSED: '+str(error))
