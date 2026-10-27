"""Run the disposable Common UI inspection and preview workflow and expose its real tool transcript."""
import runpy
from pathlib import Path
from tmcp import Session, READ_TOOLSET
import re

root = Path(__file__).resolve().parents[1]
support = runpy.run_path(str(Path(__file__).with_name('test-widget-roundtrip.py')))
name = 'TurboMCP.Workflow.CommonButton'
folder = root / 'TestProject/Saved/TurboMCP/Workflow'
before = set(folder.glob('*/common-button-review.txt'))
session = Session()
session.connect()
for mode in ('Read', 'Preview', 'Edit'):
    tool = mode + 'TurboMCPCommonButtonStyle'
    help_text = session.call(READ_TOOLSET, 'ReadTurboMCPToolHelp', {'ToolsetName': 'TurboMCP.TurboMCP' + mode + 'Toolset', 'ToolName': tool})
    print(help_text)
    for parameter in ('Asset', 'Property') + (() if mode == 'Read' else ('Value',)):
        if not re.search(r'(?im)^  ' + parameter + r':.*\] - .+', help_text):
            raise RuntimeError('Missing style parameter help: ' + tool + '.' + parameter)
    if 'shared' not in help_text or 'No compile/save/runtime refresh' not in help_text:
        raise RuntimeError('Missing shared-style limits in live help: ' + tool)

session.call(support['AUTO_TOOLSET'], 'DiscoverTests', {})
raw = session.call(support['AUTO_TOOLSET'], 'RunTests', {'testNames': [name]})
print(raw)
created = set(folder.glob('*/common-button-review.txt')) - before
for path in sorted(created):
    print('WORKFLOW TRANSCRIPT:', path)
    print(path.read_text(encoding='utf-8-sig'))
entry, report = support['parse_run_tests_report'](raw, name)
if entry['warnings']:
    raise RuntimeError('Unexpected native warnings: ' + repr(entry['warnings']))
if len(created) != 1:
    raise RuntimeError('Expected exactly one fresh workflow transcript')
print('RESULT: PASS')
