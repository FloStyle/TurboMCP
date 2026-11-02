"""Focused native backward-layout safety and undo workflow."""
import runpy
from pathlib import Path
from tmcp import Session, READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
for toolset, tool in ((PREVIEW_TOOLSET, "PreviewTurboMCPGraphFixBackward"), (EDIT_TOOLSET, "EditTurboMCPGraphFixBackward")):
    help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolset": toolset, "tool": tool})
    print(help_text)
    for phrase in ("graphName", "whole named graph", "8 nodes", "no-op stays unchanged", "No automatic compilation or saving"):
        if phrase not in help_text:
            raise RuntimeError(f"{tool} help lacks {phrase}: {help_text}")
name = "TurboMCP.Graph.BackwardReview"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
support["parse_run_tests_report"](raw, name)
print("RESULT: PASS")
