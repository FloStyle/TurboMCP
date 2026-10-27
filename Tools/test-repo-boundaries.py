"""Verify complete-tree export, staged blob identity and fail-closed exclusions."""
import importlib.util,json,pathlib,subprocess,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('public_export',ROOT/'Tools/export-public.py');export=importlib.util.module_from_spec(spec);spec.loader.exec_module(export)
policy=json.loads((ROOT/'release/export-policy.json').read_text())
def run(*args,cwd):subprocess.run(args,cwd=cwd,check=True,capture_output=True)
with tempfile.TemporaryDirectory(prefix='tmcp-export-boundary-') as work:
    root=pathlib.Path(work);run('git','init',cwd=root);run('git','config','core.autocrlf','false',cwd=root)
    for rel,data in [('LICENSE',b'License fixture\n'),('Plugins/TurboMCP/LICENSE',b'License fixture\n'),('plugin/good.cpp',b'// source\n'),('todo/task.md',b'# Task\n'),('Tests/fixture.uasset',b'fixture\x00'),('docs/design.md',b'# Design\n')]:
        p=root/rel;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
    notice={'license':'MPL-2.0','copyright':'Fixture','status':'repository_notice','provenance':'Synthetic test fixture'}
    (root/'Plugins/TurboMCP/NOTICES.json').write_text(json.dumps({'schema':1,'files':{'LICENSE':notice,'NOTICES.json':notice}}))
    (root/'policy.json').write_text(json.dumps(policy));run('git','add','.',cwd=root)
    (root/'untracked.txt').write_text('local scratch')
    export.ROOT=root;export.POLICY=root/'policy.json'
    files=export.source_files();expected={'Plugins/TurboMCP/NOTICES.json','LICENSE','Plugins/TurboMCP/LICENSE','plugin/good.cpp','todo/task.md','Tests/fixture.uasset','docs/design.md','policy.json'}
    assert {str(x[1]) for x in files}==expected
    for _,rel,data in files:assert data==subprocess.check_output(['git','show',':'+str(rel)],cwd=root)
    print('PASS: every tracked path and blob preserved; untracked scratch excluded')
    license_path=root/'Plugins/TurboMCP/LICENSE'
    for data in [None,b'Different license\n']:
        if data is None:license_path.unlink()
        else:license_path.write_bytes(data)
        run('git','add','-A',cwd=root)
        try:export.source_files()
        except ValueError as exc:assert 'matching root' in str(exc)
        else:raise AssertionError('Missing or mismatched plugin license exported')
    license_path.write_bytes(b'License fixture\n');run('git','add','-A',cwd=root)
    print('PASS: missing and mismatched plugin licenses refused')
    p=root/'plugin/good.cpp';p.write_text('// modified\n')
    try:export.source_files()
    except ValueError as exc:assert 'Unstaged' in str(exc)
    else:raise AssertionError('Unstaged edit silently ignored')
    for bad in ['// '+'ghp_'+'A'*36, '// '+'C:'+'/Users/'+'example-person/private']:
        p.write_text(bad);run('git','add',str(p),cwd=root)
        try:export.source_files()
        except ValueError as exc:assert 'Sensitive-data' in str(exc)
        else:raise AssertionError('Sensitive content exported')
    p.write_text('// source\n');run('git','add',str(p),cwd=root)
    (root/'binary.dll').write_bytes(b'MZ');run('git','add','binary.dll',cwd=root)
    try:export.source_files()
    except ValueError as exc:assert 'Forbidden' in str(exc)
    else:raise AssertionError('Forbidden tracked file silently excluded or exported')
print('RESULT: PASS (full-tree fidelity and refusal controls)')
