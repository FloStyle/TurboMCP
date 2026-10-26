"""Run disposable vision fixtures, then capture an owned short play-check window."""
import runpy
import time
import re
from pathlib import Path
from tmcp import Session, READ_TOOLSET, LOOK_TOOLSET
root = Path(__file__).resolve().parents[1]
support = runpy.run_path(str(Path(__file__).with_name('test-widget-roundtrip.py')))
folder = root / 'TestProject/Saved/TurboMCP/Workflow'
before = set(folder.glob('*/vision-review.txt'))
s = Session(); s.connect()
# Audit the live registered help, not just C++ comments or generated docs.
help_specs = {
    'InterfaceAdd': ['Asset', 'Interface'],
    'InterfaceRemove': ['Asset', 'Interface'],
    'LocalVariableAdd': ['Asset', 'Function', 'Name', 'Type', 'Default'],
    'DispatcherAdd': ['Asset', 'Name', 'Inputs'],
    'MergeMembers': ['SourceAsset', 'TargetAsset'],
    'GraphTransfer': ['SourceAsset', 'SourceGraph', 'TargetAsset', 'TargetGraph'],
    'WidgetSubtreeCopy': ['SourceAsset', 'SourceWidget', 'TargetAsset', 'Parent'],
}
help_evidence = []
for suffix, parameters in help_specs.items():
    for mode in ('Preview', 'Edit'):
        name = mode + 'TurboMCP' + suffix
        expected = parameters + (['Confirm'] if suffix == 'InterfaceRemove' and mode == 'Edit' else [])
        text = s.call(READ_TOOLSET, 'ReadTurboMCPToolHelp', {'ToolsetName': 'TurboMCP.TurboMCP' + mode + 'Toolset', 'ToolName': name})
        help_evidence.append(name + '\n' + text)
        for parameter in expected:
            if not re.search(r'(?im)^  ' + parameter + r':.*\] - .+', text):
                raise RuntimeError('Missing live parameter description: ' + name + '.' + parameter + '\n' + text)
name = 'EditTurboMCPMenuActivate'
text = s.call(READ_TOOLSET, 'ReadTurboMCPToolHelp', {'ToolsetName': 'TurboMCP.TurboMCPEditToolset', 'ToolName': name})
help_evidence.append(name + '\n' + text)
for parameter in ('Target', 'Widget'):
    if not re.search(r'(?im)^  ' + parameter + r':.*\] - .+', text):
        raise RuntimeError('Missing live menu parameter description: ' + parameter)
folder.mkdir(parents=True, exist_ok=True)
(folder / 'oct26-tool-help.txt').write_text('\n\n'.join(help_evidence), encoding='utf-8')
print('HELP: all 15 new tools expose individual parameter descriptions')
s.call(support['AUTO_TOOLSET'], 'DiscoverTests', {})
# Select only the new focus scenario for iteration; default retains all vision checks.
def common_play():
    name = 'TurboMCP.Workflow.CommonPlay'
    evidence = folder / 'common-play.txt'
    if evidence.exists(): evidence.unlink()
    result = s.call(support['AUTO_TOOLSET'], 'RunTests', {'testNames':[name]})
    print(result)
    support['parse_run_tests_report'](result, name)
    if not evidence.exists(): raise RuntimeError('Fresh CommonUI evidence missing')
    print(evidence.read_text(encoding='utf-8-sig'))
    print('RESULT: PASS CommonUI controller workflow')
import os
if os.environ.get('TURBOMCP_VISION_SCENARIO') == 'commonui':
    common_play()
    raise SystemExit(0)

raw = s.call(support['AUTO_TOOLSET'], 'RunTests', {'testNames':['TurboMCP.Workflow.Vision']})
print(raw)
created = set(folder.glob('*/vision-review.txt')) - before
for p in sorted(created):
    print('WORKFLOW TRANSCRIPT:', p); print(p.read_text(encoding='utf-8-sig'))
entry, _ = support['parse_run_tests_report'](raw, 'TurboMCP.Workflow.Vision')
# Opening the stock UMG editor registers this known conflicting shortcut.
# Preserve it in evidence; every other native warning still fails this workflow.
known = "LogSlate: The command 'UMGEditor.OpenAnimDrawer' has the same default chord as 'EditorViewport.Next' [Ctrl+Shift+Space Bar]"
unexpected = [warning for warning in entry['warnings'] if warning != known]
if unexpected: raise RuntimeError(repr(unexpected))
if entry['warnings']: print('KNOWN ENGINE SHORTCUT WARNING:', entry['warnings'])
if len(created) != 1: raise RuntimeError('Expected one fresh transcript')
# Return control to the editor between captures so asynchronous shader work can finish.
transcript = next(iter(created))
match = re.search(r"=== Material fixture path ===\s*([^\s]+)", transcript.read_text(encoding='utf-8-sig'))
if not match: raise RuntimeError('Material fixture path missing')
deadline = time.monotonic() + 45
while True:
    time.sleep(2)
    material_shot = s.call(LOOK_TOOLSET, 'LookTurboMCPCapture', {'Asset':match[1], 'GraphName':'', 'Mode':'asset-editor', 'Target':'fit', 'Size':''})
    with transcript.open('a', encoding='utf-8') as out: out.write('\n=== Material after editor ticks ===\n' + material_shot + '\n')
    if 'material shaders: available' in material_shot: break
    if 'material shaders: compiling' not in material_shot or time.monotonic() >= deadline:
        raise RuntimeError('Material preview did not reach shader availability: ' + material_shot)
if 'image:' not in material_shot: raise RuntimeError('No delayed material image')
print('MATERIAL AFTER EDITOR TICKS:', material_shot)
# Shader availability does not guarantee that the newly activated viewport has drawn.
# Keep this editor active across normal ticks and retain a second image for visual review.
time.sleep(3)
settled_shot = s.call(LOOK_TOOLSET, 'LookTurboMCPCapture', {'Asset':match[1], 'GraphName':'', 'Mode':'asset-editor', 'Target':'fit', 'Size':''})
with transcript.open('a', encoding='utf-8') as out: out.write('\n=== Material after active viewport ticks ===\n' + settled_shot + '\n')
if 'image:' not in settled_shot: raise RuntimeError('No settled material image')
print('MATERIAL AFTER ACTIVE VIEWPORT TICKS:', settled_shot)
# Keep the entire play/input loop in one native latent test. Starting a second
# automation test tears down an existing PIE session in Unreal's test framework.
name = 'TurboMCP.Workflow.PlayMenu'
result = s.call(support['AUTO_TOOLSET'], 'RunTests', {'testNames':[name]})
support['parse_run_tests_report'](result, name)
menu_evidence = folder / 'play-menu.txt'
if not menu_evidence.exists(): raise RuntimeError('Menu workflow evidence missing')
menu_text = menu_evidence.read_text(encoding='utf-8-sig')
with transcript.open('a', encoding='utf-8') as out: out.write('\n=== Owned play and menu loop ===\n' + result + '\n' + menu_text)
print(menu_text)
print('RESULT: PASS')

common_play()
