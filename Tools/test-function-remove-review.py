"""Focused function removal, refusal and preservation workflow."""
import runpy
from pathlib import Path
from tmcp import Session, READ_TOOLSET, EDIT_TOOLSET
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolset": EDIT_TOOLSET, "tool": "EditTurboMCPFunctionRemove"})
print(help_text)
for phrase in ("name", "asset", "undo", "save", "callers"):
    assert phrase in help_text.lower(), f"Help lacks {phrase}"
name = "TurboMCP.Function.RemoveReview"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
support["parse_run_tests_report"](raw, name)
print("RESULT: PASS")
