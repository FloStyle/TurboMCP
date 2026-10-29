"""Supervised save/undo workflow; refuses unowned editor execution."""
import json
import os
import runpy
from pathlib import Path
from execution_runtime import require_context
from tmcp import Session, READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET
root = Path(__file__).resolve().parents[1]
context = json.loads(Path(os.environ["TURBOMCP_EXECUTION_CONTEXT"]).read_text(encoding="utf-8"))
require_context(root, context["engine"])
if os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") != "1":
    raise RuntimeError("Runner must own fixture restoration")
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
session = Session()
session.connect()
# Each focused tool must expose its actual parameters through live discovery.
for toolset, tool, parameters in (
    (READ_TOOLSET, "ReadTurboMCPDirtyList", ("Limit",)),
    (EDIT_TOOLSET, "EditTurboMCPSave", ("Assets",)),
    (EDIT_TOOLSET, "EditTurboMCPSaveWithoutThumbnail", ("Assets",)),
    (PREVIEW_TOOLSET, "PreviewTurboMCPAssetUndo", ("Asset",)),
    (EDIT_TOOLSET, "EditTurboMCPAssetUndo", ("Asset", "Confirm")),
):
    help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolset": toolset, "tool": tool})
    print(help_text)
    for parameter in parameters:
        if parameter.lower() not in help_text.lower():
            raise RuntimeError(f"{tool} help omits {parameter}")
    if tool == "ReadTurboMCPDirtyList" and "maps" not in help_text.lower():
        raise RuntimeError("DirtyList help does not explain its map exclusion")
    if tool == "EditTurboMCPAssetUndo" and "confirmation" not in help_text.lower():
        raise RuntimeError("Undo help does not explain confirmation")
name = "TurboMCP.Workflow.SaveUndo"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
support["parse_run_tests_report"](raw, name)
print("RESULT: PASS")
