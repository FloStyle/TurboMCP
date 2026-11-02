"""Focused native backward-layout safety and undo workflow."""
import runpy
from pathlib import Path
from tmcp import Session, READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolset": EDIT_TOOLSET, "tool": "EditTurboMCPGraphPositions"})
print(help_text)
for phrase in ("graphName", "positions", "JSON array", "not tidy", "only its box", "booleans"):
    if phrase not in help_text:
        raise RuntimeError(f"Positions help lacks {phrase}: {help_text}")
name = "TurboMCP.Graph.PositionsReview"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
support["parse_run_tests_report"](raw, name)
print("RESULT: PASS")
