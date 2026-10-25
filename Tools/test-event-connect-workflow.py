"""Run the disposable multi-event connection workflow and expose its real tool transcript."""
import runpy
from pathlib import Path
from tmcp import Session

root = Path(__file__).resolve().parents[1]
support = runpy.run_path(str(Path(__file__).with_name('test-widget-roundtrip.py')))
name = 'TurboMCP.Workflow.EventConnect'
folder = root / 'TestProject/Saved/TurboMCP/Workflow'
before = set(folder.glob('*/event-connect-review.txt'))
session = Session()
session.connect()
session.call(support['AUTO_TOOLSET'], 'DiscoverTests', {})
raw = session.call(support['AUTO_TOOLSET'], 'RunTests', {'testNames': [name]})
print(raw)
created = set(folder.glob('*/event-connect-review.txt')) - before
for path in sorted(created):
    print('WORKFLOW TRANSCRIPT:', path)
    print(path.read_text(encoding='utf-8-sig'))
entry, report = support['parse_run_tests_report'](raw, name)
if entry['warnings']:
    raise RuntimeError('Unexpected native warnings: ' + repr(entry['warnings']))
if len(created) != 1:
    raise RuntimeError('Expected exactly one fresh workflow transcript')
print('RESULT: PASS')
