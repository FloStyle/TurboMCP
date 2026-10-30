"""Exercise sequential batch planning, refusal and live edits on an unsaved fixture."""
import runpy
from pathlib import Path
from tmcp import Session, READ_TOOLSET

support = runpy.run_path(str(Path(__file__).with_name("test-widget-roundtrip.py")))
name = "TurboMCP.Batch.Sequential"
session = Session()
session.connect()
for toolset, tool in (("TurboMCP.TurboMCPPreviewToolset", "PreviewTurboMCPBatchEdit"),
                      ("TurboMCP.TurboMCPEditToolset", "EditTurboMCPBatchEdit")):
    help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp",
                             {"toolsetName": toolset, "toolName": tool})
    print(help_text)
    if help_text.startswith("error:") or len(help_text) > 2000:
        raise RuntimeError("Batch help is unavailable or exceeds its compact budget")
    for required in ("operations", "graphRepoint", "compil", "restoration", "blocked", "one asset and graph"):
        if required.lower() not in help_text.lower():
            raise RuntimeError("Batch help omits required guidance: " + required)
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
entry, report = support["parse_run_tests_report"](raw, name)
if entry["warnings"]:
    raise RuntimeError("Unexpected native warnings: " + repr(entry["warnings"]))
print("RESULT: PASS")
