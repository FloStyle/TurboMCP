"""Run the focused native node-search JSON and inventory workflow."""
import runpy
from pathlib import Path
from tmcp import Session

support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
name = "TurboMCP.Read.NodeSearchJson"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
entry, report = support["parse_run_tests_report"](raw, name)
assert not entry["warnings"], entry
print("RESULT: PASS")

