"""Exercise row edits and refusal on a disposable DataTable."""
import runpy
from pathlib import Path
from tmcp import Session, READ_TOOLSET
import re

support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
name = "TurboMCP.DataTable.OwnedRows"
session = Session()
session.connect()
for mode, suffix, parameters in [('Read','DataTable',('Asset','Include','Columns')),('Preview','DataTableRows',('Asset','Rows')),('Edit','DataTableRows',('Asset','Rows'))]:
    tool=mode+'TurboMCP'+suffix
    text=session.call(READ_TOOLSET,'ReadTurboMCPToolHelp',{'ToolsetName':'TurboMCP.TurboMCP'+mode+'Toolset','ToolName':tool})
    print(text)
    for parameter in parameters:
        if not re.search(r'(?im)^  '+parameter+r':.*\] - .+',text):
            raise RuntimeError('Missing live parameter help: '+tool+'.'+parameter)

session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
entry, report = support["parse_run_tests_report"](raw, name)
if entry["warnings"]:
    raise RuntimeError("Unexpected native warnings: " + repr(entry["warnings"]))
transcript=Path(__file__).resolve().parents[1]/'TestProject/Saved/TurboMCP/Workflow/datatable-public-review.txt'
print(transcript.read_text(encoding='utf-8-sig'))
print("RESULT: PASS")
