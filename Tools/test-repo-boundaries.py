"""Exercise export boundaries with private files and injected sensitive data."""
import importlib.util,json,pathlib,subprocess,sys,tempfile
ROOT=pathlib.Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('public_export',ROOT/'Tools/export-public.py')
export=importlib.util.module_from_spec(spec);spec.loader.exec_module(export)
def run(*args,cwd):subprocess.run(args,cwd=cwd,check=True,capture_output=True)
with tempfile.TemporaryDirectory(prefix='tmcp-export-boundary-') as work:
    root=pathlib.Path(work);run('git','init',cwd=root)
    (root/'plugin').mkdir();(root/'private').mkdir()
    (root/'plugin/good.cpp').write_text('// source\n');(root/'plugin/untracked.cpp').write_text('// not approved\n')
    (root/'private/tasks.md').write_text('Private task content\n')
    policy={'copies':[{'source':'plugin','destination':'Plugins/TurboMCP/Source','extensions':['.cpp']}]}
    (root/'policy.json').write_text(json.dumps(policy))
    run('git','add','plugin/good.cpp','private/tasks.md',cwd=root)
    export.ROOT=root;export.POLICY=root/'policy.json'
    files=export.source_files()
    assert [str(x[1]) for x in files]==['Plugins/TurboMCP/Source/good.cpp'],files
    print('PASS: untracked files and tracked private tasks are excluded')
    (root/'plugin/good.cpp').write_text('// '+ 'ghp_'+'A'*36+'\n')
    try:export.source_files()
    except ValueError as exc:assert 'Sensitive-data' in str(exc)
    else:raise AssertionError('Token pattern was exported')
    print('PASS: token pattern is refused without printing it')
    (root/'plugin/good.cpp').write_text('// C:/Users/Example/private\n')
    try:export.source_files()
    except ValueError as exc:assert 'Sensitive-data' in str(exc)
    else:raise AssertionError('Personal path was exported')
    print('PASS: personal user path is refused')
    (root/'plugin/good.cpp').write_text('// source\n')
    (root/'plugin/binary.dll').write_bytes(b'MZ');run('git','add','plugin/binary.dll',cwd=root)
    try:export.source_files()
    except ValueError as exc:assert 'extension' in str(exc)
    else:raise AssertionError('Unapproved binary was exported')
    print('PASS: unexpected tracked binary is refused')
print('RESULT: PASS (public export boundaries and negative controls)')
