"""Validate repository boundaries, tasks, syntax and canonical Markdown links."""
import ast, importlib.util, json, pathlib, re, subprocess, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
    for p in ['AGENTS.md','README.md','TODO.md','engine.lock.json','Plugins/TurboMCP/TurboMCP.uplugin','TestProject/TurboMCPTest.uproject','docs/architecture.md','docs/workspace.md','release/export-policy.json']:
        if not (ROOT/p).is_file():raise ValueError(f'Missing {p}')
    root_names={p.name for p in ROOT.iterdir()}
    for p in ['Spec','Release','Tasks','HANDOVER.md','STATUS.md','PLAN.md','RELEASE_AGENT.md','Source','TurboMCP.uplugin']:
        if p in root_names:raise ValueError(f'Retired path still active: {p}')
    for p in (ROOT/'Tools').rglob('*.py'):ast.parse(p.read_text(encoding='utf-8-sig'),filename=str(p))
    for p in [ROOT/'engine.lock.json',ROOT/'Plugins/TurboMCP/TurboMCP.uplugin',ROOT/'TestProject/TurboMCPTest.uproject',ROOT/'Tests/acceptance.json',ROOT/'release/status.json',ROOT/'release/export-policy.json']:
        json.loads(p.read_text(encoding='utf-8-sig'))
    subprocess.run([sys.executable,str(ROOT/'Tools/update-todo.py'),'--check'],check=True)
    imported=json.loads((ROOT/'docs/task-import.json').read_text())
    for task in imported['tasks']:
        if f'ID: {task["id"]}' not in (ROOT/task['path']).read_text():raise ValueError('Imported task lost or ID changed')
    # Canonical navigation must resolve. Task quotations and source comments are explicitly historical evidence.
    docs=[ROOT/'README.md',ROOT/'AGENTS.md',ROOT/'TODO.md',ROOT/'Tools/README.md',ROOT/'TestProject/README.md',*list((ROOT/'docs').rglob('*.md'))]
    for p in docs:
        for target in re.findall(r'\]\(<?([^\s)>]+)>?\)',p.read_text(encoding='utf8')):
            if re.match(r'^[a-zA-Z]+:|^#',target):continue
            target=target.split('#')[0]
            if target and not (p.parent/target).exists():raise ValueError(f'Broken link in {p.relative_to(ROOT)}: {target}')
    tracked=subprocess.check_output(['git','-C',str(ROOT),'ls-files','-z']).decode().split('\0')
    for p in tracked:
        if any(x in pathlib.PurePosixPath(p).parts for x in ['Binaries','Intermediate','Saved','.agent','.local']):raise ValueError(f'Generated/private file tracked: {p}')
    print(f'PASS: repository structure, Python syntax, JSON, {imported["count"]} imported tasks, and canonical links')
if __name__=='__main__':
    try:main()
    except (ValueError,OSError,SyntaxError,subprocess.SubprocessError) as exc:sys.exit(f'FAIL: {exc}')
