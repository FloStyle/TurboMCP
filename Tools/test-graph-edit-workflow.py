"""Supervised graph-edit workflow; refuses unowned editor execution."""
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
for family in ("Patch", "Replace", "Repoint"):
    for toolset, prefix in ((PREVIEW_TOOLSET, "Preview"), (EDIT_TOOLSET, "Edit")):
        tool = prefix + "TurboMCPGraph" + family
        help_text = session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolset": toolset, "tool": tool})
        print(help_text)
        parameters = ("asset", "graphname", "text") if family != "Repoint" else ("asset", "graphname", "sourcenodeid", "sourcepin", "targetnodeid", "targettypeid", "targetpin", "keepsource")
        for parameter in parameters:
            if parameter not in help_text.lower():
                raise RuntimeError(f"{tool}: missing help for {parameter}")
name = "TurboMCP.Workflow.GraphEdits"
session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
raw = session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [name]})
print(raw)
support["parse_run_tests_report"](raw, name)
print("RESULT: PASS")
