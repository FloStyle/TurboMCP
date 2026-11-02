"""Focused existing-function delegate binding, preservation and undo workflow."""
import runpy
from pathlib import Path
from tmcp import Session, READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
help_errors = []
for toolset, tool in ((PREVIEW_TOOLSET, "PreviewTurboMCPBindFunction"), (EDIT_TOOLSET, "EditTurboMCPBindFunction")):
    help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolset": toolset, "tool": tool})
    print(help_text)
    for phrase in ("graphName", "functionName", "save"):
        if phrase not in help_text:
            help_errors.append(f"{tool} help lacks {phrase}")
name = "TurboMCP.Graph.BindFunctionReview"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
support["parse_run_tests_report"](raw, name)
if help_errors:
    raise RuntimeError("; ".join(help_errors))
print("RESULT: PASS")
