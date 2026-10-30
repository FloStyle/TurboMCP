"""Exercise real deletion and undo on an unsaved fixture, not the old shared-asset no-op.
Does not certify arbitrary node callbacks, persistence or runtime liveness.
"""
import runpy
from pathlib import Path
from tmcp import Session
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
name = "TurboMCP.Graph.DeadCleanup"
session = Session()
session.connect()
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
entry, report = support["parse_run_tests_report"](raw, name)
if entry["warnings"]:
    raise RuntimeError("Unexpected native warnings: " + repr(entry["warnings"]))
print("RESULT: PASS")
