"""Validate repository boundaries, tasks, syntax and canonical Markdown links."""
import ast, importlib.util, json, pathlib, re, subprocess, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
def main():
    plugin_license = ROOT/'Plugins/TurboMCP/LICENSE'
    if not plugin_license.is_file() or plugin_license.read_text(encoding='utf-8') != (ROOT/'LICENSE').read_text(encoding='utf-8'):
        raise ValueError('Installable plugin must contain the complete canonical LICENSE')
    for p in ['AGENTS.md','SECURITY.md','CODE_OF_CONDUCT.md','.github/CODEOWNERS','README.md','TODO.md','engine.lock.json','Plugins/TurboMCP/TurboMCP.uplugin','TestProject/TurboMCPTest.uproject','docs/architecture.md','docs/workspace.md','release/export-policy.json']:
        if not (ROOT/p).is_file():raise ValueError(f'Missing {p}')
    root_names={p.name for p in ROOT.iterdir()}
    for p in ['Spec','Release','Tasks','HANDOVER.md','STATUS.md','PLAN.md','RELEASE_AGENT.md','Source','TurboMCP.uplugin','provenance']:
        if p in root_names:raise ValueError(f'Retired path still active: {p}')
    for p in (ROOT/'Tools').rglob('*.py'):ast.parse(p.read_text(encoding='utf-8-sig'),filename=str(p))
    for p in [ROOT/'engine.lock.json',ROOT/'Plugins/TurboMCP/TurboMCP.uplugin',ROOT/'TestProject/TurboMCPTest.uproject',ROOT/'Tests/acceptance.json',ROOT/'release/status.json',ROOT/'release/export-policy.json']:
        json.loads(p.read_text(encoding='utf-8-sig'))
    subprocess.run([sys.executable,str(ROOT/'Tools/update-todo.py'),'--check'],check=True)
    catalog=json.loads((ROOT/'Tests/catalog.json').read_text())['suites']
    paths=[x['path'] for x in catalog]
    actual={p.relative_to(ROOT).as_posix() for p in (ROOT/'Tools').glob('test-*.py')}
    if len(paths)!=len(set(paths)) or set(paths)!=actual:raise ValueError('Test catalog is incomplete or duplicated')
    for row in catalog:
        if row['tier'] not in ('offline','engine') or row['timeout_seconds']<=0:raise ValueError('Invalid test tier/deadline')
    for task in (ROOT/'todo').glob('*.md'):
        text=task.read_text(encoding='utf8')
        design=re.search(r'^Design: (.+)$',text,re.M)
        if not design or not (ROOT/design.group(1)).is_file():raise ValueError(f'{task.name}: missing design reference')
        # Completed records retain evidence, but are not pending work assignments.
        if 'Readiness: ready' in text and not re.search(r'^Status: done$', text, re.M):
            for section in ['Problem','Scope','Dependencies','Acceptance','Verification']:
                if '## '+section+'\n' not in text:raise ValueError(f'{task.name}: ready task missing {section}')
    subprocess.run([sys.executable,str(ROOT/'Tools/next-task.py'),'--check'],check=True)
    subprocess.run([sys.executable,str(ROOT/'Tools/check-task-review.py')],check=True)
    subprocess.run([sys.executable,str(ROOT/'Tools/audit-inventory.py'),'--check'],check=True)
    capabilities=json.loads((ROOT/'docs/audit/capabilities.json').read_text(encoding='utf8'))['tools']
    routing=json.loads((ROOT/'docs/audit/tool-routing.json').read_text(encoding='utf8'))['tools']
    route_ids=[row['tool'] for row in routing]
    if len(route_ids)!=len(set(route_ids)) or set(route_ids)!={row['id'] for row in capabilities}:
        raise ValueError('Tool routing must cover every declared tool exactly once')
    for row in routing:
        if row['preference'] not in ('unverified','prefer-turbomcp','prefer-native','complementary','unavailable'):
            raise ValueError('Unknown routing preference')
        if row['preference'] in ('prefer-turbomcp','prefer-native') and not row.get('evidence'):
            raise ValueError('Tool preference requires reviewable evidence')
    dispositions=json.loads((ROOT/'roadmap/task-dispositions.json').read_text(encoding='utf8'))['tasks']
    ids=[x['id'] for x in dispositions]
    if len(ids)!=len(set(ids)):raise ValueError('Duplicate historical task disposition')
    for row in dispositions:
        text=(ROOT/row['path']).read_text(encoding='utf8')
        if 'ID: '+row['id'] not in text:raise ValueError('Disposition/task mismatch')
    imported=json.loads((ROOT/'docs/task-import.json').read_text())
    for task in imported['tasks']:
        if f'ID: {task["id"]}' not in (ROOT/task['path']).read_text():raise ValueError('Imported task lost or ID changed')
    # Canonical navigation must resolve. Task quotations and source comments are explicitly historical evidence.
    docs=[ROOT/'SECURITY.md',ROOT/'CODE_OF_CONDUCT.md',ROOT/'ROADMAP.md',ROOT/'CONTRIBUTING.md',ROOT/'README.md',ROOT/'AGENTS.md',ROOT/'TODO.md',ROOT/'Tools/README.md',ROOT/'TestProject/README.md',*list((ROOT/'docs').rglob('*.md')),*list((ROOT/'skills').rglob('*.md')),*list((ROOT/'todo').glob('*.md'))]
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
