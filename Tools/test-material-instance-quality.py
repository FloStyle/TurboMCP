"""Check material instance inheritance and override identity."""
import runpy
from pathlib import Path
from tmcp import Session, READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET

support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
name = "TurboMCP.Material.InstanceQuality"
session = Session()
session.connect()
for toolset, tool in ((READ_TOOLSET, "ReadTurboMCPMaterialInstance"), (EDIT_TOOLSET, "EditTurboMCPMaterialInstanceSet")):
    help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolset": toolset, "tool": tool})
    print(help_text)
    for phrase in ("asset", "instance"):
        assert phrase in help_text.lower(), f"{tool} help lacks {phrase}"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
entry, report = support["parse_run_tests_report"](raw, name)
if entry["warnings"]:
    raise RuntimeError("Unexpected native warnings: " + repr(entry["warnings"]))
print("RESULT: PASS")
