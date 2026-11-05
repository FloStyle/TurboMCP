"""Supervised saved data-asset recovery; refuses unowned editor execution."""
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
for toolset, tool in ((PREVIEW_TOOLSET, "PreviewTurboMCPAssetRevert"), (EDIT_TOOLSET, "EditTurboMCPAssetRevert")):
    help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolset": toolset, "tool": tool})
    for phrase in ("data-asset", "60 seconds", "confirmation"):
        if phrase not in help_text:
            raise RuntimeError(f"{tool} help lacks {phrase}: {help_text}")
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
for name in ("TurboMCP.DataAsset.SavedRecovery", "TurboMCP.DataAsset.CheckedDelete"):
    raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
    print(raw)
    support["parse_run_tests_report"](raw, name)
print("RESULT: PASS")
