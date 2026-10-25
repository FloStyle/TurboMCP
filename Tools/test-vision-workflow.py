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
s.call(support['AUTO_TOOLSET'], 'DiscoverTests', {})
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
# Start only our bounded play check; an existing user's play session is not ours.
first = s.call(READ_TOOLSET, 'ReadTurboMCPPlayCheck', {'Seconds':'8', 'Map':''})
print('PLAY START:', first)
if 'already playing' in first.lower() or first.startswith(('error:', '!!')):
    raise RuntimeError('Could not acquire an owned play-check window')
play_log = []
try:
    time.sleep(2)
    shot = s.call(LOOK_TOOLSET, 'LookTurboMCPCapture', {'Asset':'', 'GraphName':'', 'Mode':'screen', 'Target':'', 'Size':''})
    play_log.append('SCREEN DURING PLAY CHECK:\n' + shot)
finally:
    deadline = time.monotonic() + 45
    while time.monotonic() < deadline:
        last = s.call(READ_TOOLSET, 'ReadTurboMCPPlayCheck', {'Seconds':'', 'Map':''})
        if 'pending' not in last.lower() and 'still running' not in last.lower():
            break
        time.sleep(1)
    play_log.append('PLAY RESULT:\n' + last)
    for p in created:
        with p.open('a', encoding='utf-8') as out: out.write('\n' + '\n'.join(play_log))
    print('\n'.join(play_log))
if 'played for' not in last: raise RuntimeError('Owned play check did not complete')
if '0 warning(s) and 0 error(s)' not in last:
    raise RuntimeError('Owned play check reported runtime warnings or errors')
if 'image:' not in shot: raise RuntimeError('No active-window image during play check')
if 'window title:' not in shot or 'window selection: active top-level Slate window' not in shot:
    raise RuntimeError('Capture must identify the actual selected window')
print('RESULT: PASS')
