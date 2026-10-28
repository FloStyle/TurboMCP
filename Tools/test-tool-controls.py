"""Focused personal tool-policy rules; restart and dispatch checks are separate."""
import runpy
from pathlib import Path
from tmcp import Session
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
name = "TurboMCP.Workflow.ToolControls"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
support["parse_run_tests_report"](raw, name)
print("RESULT: PASS")
