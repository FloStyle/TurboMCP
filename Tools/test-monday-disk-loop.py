"""Integrator smoke: exact fixture graph edit/save/package-reload through MCP.

The Blueprint leg appends one literal PrintString. The Widget leg performs one
connected data edit: a new Concat_StrStr source is wired into a new PrintString's
InString, and the PrintString's execute input is linked to the fixture's Tick
entry. The Widget leg compares the layout report and the widget tree before and
after, then compiles, saves, reads back, and proves the saved state survives a
package reload.

Only supervisor-owned TestProject fixtures; the runner restores their baseline.
Not a fresh-process restart, a rendered-layout certificate, or proof that every
Widget node family behaves this way.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import uuid
from execution_runtime import require_context
from tmcp import Session, READ_TOOLSET, LOOK_TOOLSET, EDIT_TOOLSET, PREVIEW_TOOLSET

ROOT = Path(__file__).resolve().parents[1]

# THE SOURCE NODE'S TYPE ID COMES FROM THE ENGINE'S OWN METADATA, NOT FROM A GUESS:
# `Concat_StrStr` declares Category="Utilities|String" and DisplayName="Append", and
# the node type id is the menu category plus "|" plus the ListView title. The suite
# proves the id is usable by asking the pins reader for its ReturnValue before it
# writes any text.
CONCAT_TYPE_ID = "Utilities|String|Append"

def demand(ok, message):
    if not ok:
        raise RuntimeError(message)
    print("  ok " + message, flush=True)

def layout_state(text):
    """The layout report's counts, extent and overlap-finding count.

    Zero stored node sizes make the extent positional; that is enough to prove the
    new column is to the right and the existing corner did not move. It is not a
    per-node rectangle read, which this MCP surface does not expose.
    """
    counts = re.search(r"counts: (\d+) node\(s\), (\d+) wire\(s\)", text)
    extent = re.search(r"extent: x (-?\d+) to (-?\d+), y (-?\d+) to (-?\d+)", text)
    demand(counts is not None and extent is not None, "layout report carries counts and extent")
    overlap = re.search(r"\n\s+overlap: (\d+)", text)
    return {
        "nodes": int(counts.group(1)),
        "wires": int(counts.group(2)),
        "min_x": int(extent.group(1)),
        "max_x": int(extent.group(2)),
        "min_y": int(extent.group(3)),
        "max_y": int(extent.group(4)),
        "overlaps": int(overlap.group(1)) if overlap else 0,
    }

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
        connected = asset == "WBP_NodeTest"
        saved = "MONDAY_SAVED_" + uuid.uuid4().hex
        unsaved = "MONDAY_UNSAVED_" + uuid.uuid4().hex
        def append(marker):
            text = '(Development|PrintString\n    InString "' + marker + '"\n    execute (-> ' + headers[-1] + ':then))'
            editargs = {"asset": asset, "graphName": graph, "text": text}
            call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphAppend", editargs)
            answer = call(EDIT_TOOLSET, "EditTurboMCPGraphAppend", editargs)
            demand(answer.startswith("changed:"), "targeted append changed graph")
        if connected:
            # ONE CONNECTED WIDGET LOGIC EDIT: a new pure-value source feeds a new
            # PrintString, which is inserted after the fixture's Tick entry.
            widget_before = call(READ_TOOLSET, "ReadTurboMCPWidgetTree", {"asset": asset})
            layout_before = layout_state(call(LOOK_TOOLSET, "LookTurboMCPLayoutReport",
                                              {"asset": asset, "graphName": graph}))
            source_pins = call(PREVIEW_TOOLSET, "PreviewTurboMCPNodePins",
                               {"asset": asset, "graphName": graph, "typeId": CONCAT_TYPE_ID})
            demand("ReturnValue" in source_pins,
                   "the Concat source exposes ReturnValue in this graph")
            text = (
                '(Development|Concat_StrStr\n'
                '    ; label ConnectedSource\n'
                '    A "connected "\n'
                '    B "' + saved + '")\n'
                '(Development|PrintString\n'
                '    InString (-> ConnectedSource:ReturnValue)\n'
                '    Duration 1.5\n'
                '    execute (-> ' + headers[-1] + ':then))'
            )
            editargs = {"asset": asset, "graphName": graph, "text": text}
            preview = call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphAppend", editargs)
            demand("2 new node(s)" in preview, "connected plan adds two nodes")
            demand("wire  line" in preview, "connected plan lists its wires")
            answer = call(EDIT_TOOLSET, "EditTurboMCPGraphAppend", editargs)
            demand(answer.startswith("changed:"), "connected append changed graph")
            added = re.search(r"(\d+)\s+node\(s\) added", answer)
            demand(added is not None and int(added.group(1)) == 2,
                   "connected append reported two added nodes")
            made = re.search(r"(\d+) wire\(s\) made, of (\d+)", answer)
            demand(made is not None and made.group(1) == made.group(2) == "2",
                   "connected append reported two made wires")
            compiled = call(EDIT_TOOLSET, "EditTurboMCPCompile", {"assets": asset})
            demand("1 clean, 0 with warnings, 0 with errors" in compiled, "compile reports zero errors")
            written = call(EDIT_TOOLSET, "EditTurboMCPSaveWithoutThumbnail", {"assets": "/Game/Tests/" + asset})
            demand("1 saved" in written, "explicit save wrote fixture")
            expected = read()
            demand(saved in expected, "saved connected marker visible")
            demand("InString (-> " in expected and "ReturnValue" in expected,
                   "fresh read keeps the connected data link")
            widget_after = call(READ_TOOLSET, "ReadTurboMCPWidgetTree", {"asset": asset})
            demand(widget_after == widget_before, "widget hierarchy unchanged by the graph edit")
            layout_after = layout_state(call(LOOK_TOOLSET, "LookTurboMCPLayoutReport",
                                             {"asset": asset, "graphName": graph}))
            demand(layout_after["nodes"] == layout_before["nodes"] + 2,
                   "layout node count grew by two")
            demand(layout_after["wires"] == layout_before["wires"] + 2,
                   "layout wire count grew by two")
            demand((layout_after["min_x"], layout_after["min_y"])
                   == (layout_before["min_x"], layout_before["min_y"]),
                   "existing layout corner is unchanged")
            demand(layout_after["max_x"] > layout_before["max_x"],
                   "new nodes are placed to the right of the existing layout")
            demand(layout_after["overlaps"] == layout_before["overlaps"],
                   "no new overlap finding")
        else:
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
