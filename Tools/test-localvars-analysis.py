"""Check local-variable ancestry analysis on transient nodes; does not certify promotion."""
import runpy
from pathlib import Path
from tmcp import Session

support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
name = "TurboMCP.LocalVars.Ancestry"
session = Session()
session.connect()
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
entry, report = support["parse_run_tests_report"](raw, name)
if entry["warnings"]:
    raise RuntimeError("Unexpected native warnings: " + repr(entry["warnings"]))
print("RESULT: PASS")
