"""Bounded editor output history regression."""
import runpy
import re
from pathlib import Path
from tmcp import Session, READ_TOOLSET
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
name = "TurboMCP.Diagnostics.EditorLog"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
support["parse_run_tests_report"](raw, name)
live = session.call(READ_TOOLSET, "ReadTurboMCPEditorLog", {"Category": "LogTurboMCPEditorLogFixture", "Contains": "editor-log-live-", "Since": "", "Cursor": "", "Limit": 10})
print(live)
if "editor-log-live-" not in live or "nextCursor:" not in live or "not tool-call history" not in live:
    raise RuntimeError("Public editor log result lacks live marker or cursor contract")
help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"ToolsetName": READ_TOOLSET, "ToolName": "ReadTurboMCPEditorLog"})
print(help_text)
for parameter in ("Category", "Contains", "Since", "Cursor", "Limit"):
    if not re.search(r"(?im)^  " + parameter + r":.*\] - .+", help_text):
        raise RuntimeError("Live help omitted " + parameter)
print("RESULT: PASS")
