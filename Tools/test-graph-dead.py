"""Exercise real deletion and undo on an unsaved fixture, not the old shared-asset no-op.
Does not certify arbitrary node callbacks, persistence or runtime liveness.
"""
import runpy
from pathlib import Path
from tmcp import Session, READ_TOOLSET, EDIT_TOOLSET, PREVIEW_TOOLSET
support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
name = "TurboMCP.Graph.DeadCleanup"
session = Session()
session.connect()
for toolset, tool in ((PREVIEW_TOOLSET, "PreviewTurboMCPGraphDead"), (EDIT_TOOLSET, "EditTurboMCPGraphDead")):
    help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolsetName": toolset, "toolName": tool})
    print(help_text)
    if help_text.startswith("error:") or len(help_text) > 2000 or "graphName" not in help_text:
        raise RuntimeError("Cleanup help is unavailable, incomplete or over budget")
    if toolset == EDIT_TOOLSET and ("one undo transaction" not in help_text or "does not compile or save" not in help_text):
        raise RuntimeError("Cleanup help omits edit effects")
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
entry, report = support["parse_run_tests_report"](raw, name)
if entry["warnings"]:
    raise RuntimeError("Unexpected native warnings: " + repr(entry["warnings"]))
print("RESULT: PASS")
