"""Focused new struct and field editing workflow."""
import runpy
from pathlib import Path
from tmcp import Session, READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
for toolset, prefix in ((PREVIEW_TOOLSET, "Preview"), (EDIT_TOOLSET, "Edit")):
    for suffix in ("StructNew", "StructFields"):
        tool = prefix + "TurboMCP" + suffix
        text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolset": toolset, "tool": tool})
        print(text)
        assert "save" in text.lower() and "required" in text.lower(), tool
name = "TurboMCP.Struct.Quality"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
support["parse_run_tests_report"](raw, name)
print("RESULT: PASS")
