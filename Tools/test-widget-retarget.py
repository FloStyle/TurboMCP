"""Check retarget binding preservation, diagnostics and refusal without saving."""
import runpy
from pathlib import Path
from tmcp import Session, READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET

support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
name = "TurboMCP.Widget.RetargetQuality"
session = Session()
session.connect()
for toolset, tool in ((PREVIEW_TOOLSET, "PreviewTurboMCPWidgetRetarget"), (EDIT_TOOLSET, "EditTurboMCPWidgetRetarget")):
    help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolset": toolset, "tool": tool})
    print(help_text)
    for phrase in ("asset", "delegate", "signature"):
        assert phrase in help_text.lower(), f"{tool} help lacks {phrase}"
    assert "save" in help_text.lower() or "saving" in help_text.lower(), f"{tool} help lacks save behavior"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
entry, report = support["parse_run_tests_report"](raw, name)
if entry["warnings"]:
    raise RuntimeError("Unexpected native warnings: " + repr(entry["warnings"]))
print("RESULT: PASS")
