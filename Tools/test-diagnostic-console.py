"""Focused diagnostic console confirmation, refusal and readback workflow."""
import runpy
import re
import time
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
# Exercise the actual MCP dispatch guard while an owned timed PIE session exists.
start = session.call(READ_TOOLSET, "ReadTurboMCPPlayCheck", {"Seconds": "12", "Map": ""})
print(start)
if "error:" in start.lower() or "already playing" in start.lower():
    raise RuntimeError("Cannot own diagnostic PIE check: " + start)
try:
    deadline = time.monotonic() + 8
    while True:
        preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPDiagnosticConsole", {"Target": "pie:0", "Command": "Log list LogTurboMCPConsoleFixture"})
        if "Query preview" in preview:
            break
        if time.monotonic() >= deadline:
            raise RuntimeError("PIE target did not become available: " + preview)
        time.sleep(0.25)
    result = session.call(EDIT_TOOLSET, "EditTurboMCPDiagnosticConsole", {"Target": "pie:0", "Command": "Log list LogTurboMCPConsoleFixture", "Confirmation": ""})
    print(result)
    if "Target: pie:0" not in result or "Command handled: yes" not in result or "LogTurboMCPConsoleFixture" not in result:
        raise RuntimeError("PIE public diagnostic dispatch failed")
finally:
    deadline = time.monotonic() + 35
    while True:
        finish = session.call(READ_TOOLSET, "ReadTurboMCPPlayCheck", {"Seconds": "", "Map": ""})
        if "played for" in finish:
            print(finish)
            break
        if time.monotonic() >= deadline:
            raise RuntimeError("Owned timed PIE did not report completion: " + finish)
        time.sleep(0.5)
print("RESULT: PASS")
