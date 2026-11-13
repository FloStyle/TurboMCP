"""Run only the transient Blueprint soft-default and reference-scope fixture."""
from pathlib import Path
import runpy

from tmcp import Session

support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
name = "TurboMCP.Read.BlueprintReferenceDefault"
session = Session()
session.connect()
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
entry, report = support["parse_run_tests_report"](raw, name)
if entry["state"] != "Success" or entry["errors"] or entry["warnings"]:
    raise AssertionError("Blueprint reference/default fixture failed: " + repr(entry))
print("RESULT: PASS", report["total"], "focused test")
