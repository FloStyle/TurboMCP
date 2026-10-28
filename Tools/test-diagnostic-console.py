"""Focused diagnostic console confirmation, refusal and readback workflow."""
import runpy
import re
from pathlib import Path
from tmcp import Session, READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
name = "TurboMCP.Diagnostics.Console"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
support["parse_run_tests_report"](raw, name)
for toolset, tool, parameters in (
    (PREVIEW_TOOLSET, "PreviewTurboMCPDiagnosticConsole", ("Target", "Command")),
    (EDIT_TOOLSET, "EditTurboMCPDiagnosticConsole", ("Target", "Command", "Confirmation")),
):
    help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"ToolsetName": toolset, "ToolName": tool})
    print(help_text)
    for parameter in parameters:
        if not re.search(r"(?im)^  " + parameter + r":.*\] - .+", help_text):
            raise RuntimeError("Live help omitted " + parameter)
query = session.call(EDIT_TOOLSET, "EditTurboMCPDiagnosticConsole", {"Target": "editor", "Command": "Log list LogTurboMCPConsoleFixture", "Confirmation": ""})
print(query)
if "LogTurboMCPConsoleFixture" not in query or "process-wide" not in query:
    raise RuntimeError("Public query omitted category or effect scope")
print("RESULT: PASS")
