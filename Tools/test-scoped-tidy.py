"""Scoped tidy preserves fixed surroundings and places connected edits locally."""
import runpy
from pathlib import Path
from tmcp import Session, READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
for toolset, tool in ((PREVIEW_TOOLSET, "PreviewTurboMCPGraphTidy"), (EDIT_TOOLSET, "EditTurboMCPGraphTidy")):
    help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolset": toolset, "tool": tool})
    for phrase in ("unselected nodes stay fixed", "first explicit MainPath", "later path nodes must be selected", "Force does not bypass", "fixed-boundary", "64..256", "no compile or save"):
        if phrase not in help_text:
            raise RuntimeError(f"{tool} help lacks {phrase}: {help_text}")
for toolset, tool in ((PREVIEW_TOOLSET, "PreviewTurboMCPGraphGroupMove"), (PREVIEW_TOOLSET, "PreviewTurboMCPGraphGroupPlacement"), (EDIT_TOOLSET, "EditTurboMCPGraphGroupMove")):
    help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolset": toolset, "tool": tool})
    for phrase in ("GraphName", "NodeIds", "case-sensitive", "fully contained"):
        if phrase not in help_text:
            raise RuntimeError(f"{tool} help lacks {phrase}: {help_text}")
    if tool.endswith("GroupMove") and "DestinationBox" not in help_text:
        raise RuntimeError(f"{tool} lacks destination guidance: {help_text}")
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
for name in ("TurboMCP.Graph.ScopedTidy", "TurboMCP.Graph.GroupMovement"):
    raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
    print(raw)
    support["parse_run_tests_report"](raw, name)
print("RESULT: PASS")
