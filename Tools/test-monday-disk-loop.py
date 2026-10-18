"""Integrator smoke: exact fixture graph edit/save/package-reload through MCP.
Only supervisor-owned TestProject fixtures; runner restores their baseline.
Not a fresh-process restart or complete widget/layout preservation certificate.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import uuid
from execution_runtime import require_context
from tmcp import Session, READ_TOOLSET, EDIT_TOOLSET, PREVIEW_TOOLSET

ROOT = Path(__file__).resolve().parents[1]

def demand(ok, message):
    if not ok:
        raise RuntimeError(message)
    print("  ok " + message, flush=True)

def main():
    context = json.loads(Path(os.environ["TURBOMCP_EXECUTION_CONTEXT"]).read_text(encoding="utf-8"))
    require_context(ROOT, context["engine"])
    demand(os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") == "1", "runner owns fixture recovery")
    session = Session()
    session.connect()
    def call(toolset, name, args):
        answer = session.call(toolset, name, args)
        print(name + ": " + str(answer), flush=True)
        demand(isinstance(answer, str) and not answer.startswith("error:"), name + " answered")
        return answer
    for asset, graph in (("BP_NodeTest", "EventGraph"), ("WBP_NodeTest", "EventGraph")):
        path = ROOT / "TestProject/Content/Tests" / (asset + ".uasset")
        demand(path.is_file() and not path.is_symlink(), "existing exact fixture " + asset)
        args = {"asset": asset, "graphName": graph, "format": "", "detail": "full", "tokenBudget": ""}
        def read():
            value = call(READ_TOOLSET, "ReadTurboMCPGraphLoad", args)
            demand(value.startswith("read: ") and "truncated" not in value.lower(), "complete reader response")
            return value
        before = read()
        headers = re.findall(r"\(Development\|PrintString ; ([^ ;\r\n]+)", before)
        if not headers and asset == "WBP_NodeTest":
            found = call(READ_TOOLSET, "ReadTurboMCPNodeFind", {"asset": asset, "graphName": graph, "words": "Tick"})
            headers = list(dict.fromkeys(re.findall(r"(?:/Game/Tests/WBP_NodeTest\.)?WBP_NodeTest:EventGraph:[0-9a-f]{6,32}", found)))
            demand(len(headers) == 1, "reader supplies unique Widget Tick entry")
        demand(bool(headers), "reader supplies existing PrintString target")
        saved = "MONDAY_SAVED_" + uuid.uuid4().hex
        unsaved = "MONDAY_UNSAVED_" + uuid.uuid4().hex
        def append(marker):
            text = '(Development|PrintString\n    InString "' + marker + '"\n    execute (-> ' + headers[-1] + ':then))'
            editargs = {"asset": asset, "graphName": graph, "text": text}
            call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphAppend", editargs)
            answer = call(EDIT_TOOLSET, "EditTurboMCPGraphAppend", editargs)
            demand(answer.startswith("changed:"), "targeted append changed graph")
        append(saved)
        compiled = call(EDIT_TOOLSET, "EditTurboMCPCompile", {"assets": asset})
        demand("1 clean, 0 with warnings, 0 with errors" in compiled, "compile reports zero errors")
        written = call(EDIT_TOOLSET, "EditTurboMCPSaveWithoutThumbnail", {"assets": "/Game/Tests/" + asset})
        demand("1 saved" in written, "explicit save wrote fixture")
        expected = read()
        demand(saved in expected, "saved marker visible")
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        append(unsaved)
        demand(unsaved in read(), "unsaved negative-control marker visible")
        demand(hashlib.sha256(path.read_bytes()).hexdigest() == digest, "negative control did not change saved bytes")
        reply = call("init_unreal.FixtureIsolationToolset", "reload_restored", {"asset_paths": ["/Game/Tests/" + asset]})
        result = json.loads(reply)
        demand(result.get("reloaded") == "1" and result.get("resolved") == "1", "package reload reported success")
        actual = read()
        demand(saved in actual and unsaved not in actual, "disk state replaced unsaved memory state")
        demand(actual == expected, "full graph reader output survived reload exactly")
        demand(hashlib.sha256(path.read_bytes()).hexdigest() == digest, "reload did not rewrite saved bytes")
    print("RESULT: PASS")
    return 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print("RESULT: FAIL " + str(exc), flush=True)
        sys.exit(1)
