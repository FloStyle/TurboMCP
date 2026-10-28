"""Focused function-flags workflow and live parameter help."""
import runpy
import re
from pathlib import Path
from tmcp import Session, READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
name = "TurboMCP.Workflow.FunctionFlags"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
support["parse_run_tests_report"](raw, name)
for toolset, mode in ((PREVIEW_TOOLSET, "Preview"), (EDIT_TOOLSET, "Edit")):
    help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"ToolsetName": toolset, "ToolName": mode + "TurboMCPFunctionFlags"})
    print(help_text)
    for parameter in ("Asset", "Function", "Pure", "Const"):
        if not re.search(r"(?im)^  " + parameter + r":.*\] - .+", help_text):
            raise RuntimeError("Live help omitted " + parameter)
print("RESULT: PASS")
