"""Readiness diagnostic workflow: idle, transaction and recovery states without asset edits."""
import runpy
from pathlib import Path
from tmcp import Session, READ_TOOLSET, EDIT_TOOLSET
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
text = session.call(EDIT_TOOLSET, "ReadTurboMCPEditQueue", {})
print(text)
assert "readiness:" in text and "No edits, compilation or save" in text
help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolsetName": EDIT_TOOLSET, "toolName": "ReadTurboMCPEditQueue"})
print(help_text)
assert "does not edit, compile or save" in help_text and len(help_text) <= 2000
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
name = "TurboMCP.Tools.EditReadiness"
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
entry, report = support["parse_run_tests_report"](raw, name)
assert not entry["warnings"], entry["warnings"]
print("RESULT: PASS")
