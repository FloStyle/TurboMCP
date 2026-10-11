"""Exercise the four framing selectors added to L-CAPTURE by WORKING_PLAN row 2.1.

  selection, neighbours, box-neighbours, screen   -- 03_TOOLS.md section 18 table
  T-LOOK-7                                        -- 06_ACCEPTANCE.md line 234
  D-41  a capture never falls back to a wider frame
  D-42  a hop through a plain reroute knot is ONE hop

WHY THIS IS A NEW FILE AND NOT AN ADDITION TO `test-capture.py`
--------------------------------------------------------------
`Tools/test-capture.py` ALREADY EXISTED AND IS TRACKED (it came in with `8adfe50`).
The brief for this row named `Tools/test-capture.py` as the file to create, and
following that literally would have SILENTLY OVERWRITTEN a passing suite that tests a
different set of behaviours (the whole graph, one box, one node, the window fallback
and the `debug-window` diagnostic). The brief's own warning about
`test-widget-tree.py` is the same trap, one asset earlier. So this file is NEW, it is
named `test-capture-selectors`, and it touches nothing that already passed.

WHAT EACH CHECK NEEDS TO FAIL, NAMED
------------------------------------
Every check below has an input that reddens it, and the input is named in the comment
above it. No check is a tautology and none depends on shared mutable state: the
node IDs and the graph name are read from the running editor rather than pinned, and
the only fixture this file writes is nothing at all -- it never edits an asset.

WHAT IS MEASURED AND WHAT IS NOT
--------------------------------
Measured here: the rendering state that decides `screen`; `screen` itself, end to end,
including the PNG on disk and its colour count; `selection` refusing with nothing
selected; the `neighbours` hop contract at N=0 and N=1; the refusals for a bad hop
count, a bad node ID and a box index that does not exist; and the internal selection
probe that makes the positive `selection` branch measurable at all.

NOT measured here, and said so rather than implied: the POSITIVE `box-neighbours`
branch (a box that really links to another box). No fixture in this project has two
linked comment boxes, and creating them is an edit to a SHARED asset that this row
does not own. The negative branch is measured. See the closing note.
"""

import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import LOOK_TOOLSET, Session  # noqa: E402

BLUEPRINT = "/Game/Tests/BP_NodeTest.BP_NodeTest"
GRAPH = "EventGraph"
WIDGET = "WBP_NodeTest"

# The folder the tool prints its PNG paths from, as the editor sees it.
EDITOR_BIN = r"C:\ExampleUnrealengine\Engine\Binaries\Win64"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def refused(out):
    """True when the tool answered a refusal, in EITHER of the two shapes it uses.

    MEASURED, AND IT COST A ROUND. A mode that returns a plain string beginning
    `error:` arrives as that string. A mode that returns a multi-line answer which the
    engine's tool layer decides is an error arrives PREFIXED with `!! isError: `, so a
    check written as `out.startswith("error:")` is red even though the tool refused
    correctly. THE INPUT THAT MADE THIS VISIBLE: the `select-nodes` probe with the key
    `ffffff,nope`, whose own text began with the probe's success banner while the server
    had flagged the answer as an error.
    """
    return out.startswith("error:") or out.startswith("!! isError:")


def reason_of(out):
    """The refusal text, whichever of the two shapes carried it."""
    for marker in ("!! isError:", "error:"):
        if out.startswith(marker):
            return out.split(marker, 1)[1].strip()
    return out


def every_check_can_fail():
    """A guard on this file itself: the two shapes above must be recognised.

    FAILS ON: either marker being renamed, which would silently turn every `refused`
    check into a green one.
    """
    return (refused("error: x") and refused("!! isError: x")
            and not refused("look: x") and not refused("ok"))


def png_size(path):
    """The width and height in the PNG header, or None when it is not a PNG."""
    try:
        with open(path, "rb") as handle:
            header = handle.read(24)
    except OSError:
        return None
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    return struct.unpack(">II", header[16:24])


def resolve_printed_path(printed):
    """The tool prints a path relative to the engine's binary folder."""
    return os.path.abspath(os.path.join(EDITOR_BIN, printed))


def distinct_colours(path):
    """How many different colours the image holds, or None when unreadable.

    A flat fill is what a render that drew nothing produces, so this is the measure
    that separates a picture from a rectangle. It is NOT capped: a capped count
    saturates and then two pictures cannot be compared, which an earlier workstream
    measured on this very asset.
    """
    try:
        from PIL import Image  # noqa: PLC0415
    except ImportError:
        return None
    try:
        with Image.open(path) as image:
            pixels = list(image.convert("RGB").getdata())
    except Exception:  # noqa: BLE001
        return None
    return len(set(pixels))


def image_path_of(out):
    """The path on the `image:` line, or None. Numbers are parsed, never a sentence."""
    for line in out.splitlines():
        if line.startswith("image:"):
            return line.split(":", 1)[1].strip()
    return None


def capture(session, mode, target="", size="", asset=BLUEPRINT, graph=GRAPH):
    return session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                        {"asset": asset, "graphName": graph,
                         "mode": mode, "target": target, "size": size})


def rhi_state(session):
    """The measured rendering state, as a dict of the three flags it prints.

    Returns None when the block is not there at all, which is reported as its own
    failure rather than being read as three false values (`P1`).
    """
    out = capture(session, "rhi-state")
    print(out)
    print()
    flags = {}
    for line in out.splitlines():
        match = re.match(r"^-nullrhi on the command line: (true|false)", line)
        if match:
            flags["nullrhi"] = match.group(1) == "true"
        match = re.match(r"^FApp::CanEverRender\(\): (true|false)", line)
        if match:
            flags["can_render"] = match.group(1) == "true"
        match = re.match(r"^Slate initialized: (true|false)", line)
        if match:
            flags["slate"] = match.group(1) == "true"
    return out, (flags if len(flags) == 3 else None)


def node_key(session, words):
    """One node key from R-NODE-FIND, so no ID is pinned on a shared asset."""
    found = session.call("TurboMCP.TurboMCPReadToolset", "ReadTurboMCPNodeFind",
                         {"asset": "BP_NodeTest", "words": words, "graphName": GRAPH})
    for line in found.splitlines():
        match = re.match(r"^BP_NodeTest:%s:([0-9a-f]+)\s*$" % GRAPH, line.strip())
        if match:
            return match.group(1), line.strip()
    return None, None


def graph_node_count(session):
    """The node count from an INDEPENDENT tool, so a count can be cross-checked."""
    layout = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutReport",
                          {"asset": BLUEPRINT, "graphName": GRAPH})
    match = re.search(r"counts: (\d+) node\(s\)", layout)
    return int(match.group(1)) if match else None


def main():
    session = Session()
    session.connect()

    # ------------------------------------------------------------------
    print("=== this suite's own refusal detector ===")
    # FAILS ON: either refusal shape being renamed, which would silently turn every
    # `refused` check in this file green. This is the check ON the checks.
    check("both refusal shapes are recognised, and a success is not a refusal",
          every_check_can_fail())
    print("        'error: x' -> %s | '!! isError: x' -> %s | 'look: x' -> %s"
          % (refused("error: x"), refused("!! isError: x"), refused("look: x")))

    # ------------------------------------------------------------------
    print()
    print("=== the measured rendering state, and whether screen can work ===")
    # FAILS ON: the block not being printed (flags is None), any "true"/"false" heading
    # being renamed or removed, or the caveat about GUsingNullRHI being dropped.
    state, flags = rhi_state(session)
    check("the three state flags are all reported", flags is not None, str(flags))
    check("it says whether screen can work here",
          "can mode screen work here:" in state)
    check("it names the engine flag it CANNOT read",
          "GUsingNullRHI" in state and "CANNOT read it" in state,
          "the caveat that this is a proxy, not the engine flag")
    check("it says no dependency was added", "No dependency was added" in state)

    screen_should_work = bool(flags) and flags.get("can_render") and flags.get("slate") \
        and not flags.get("nullrhi")
    print("        screen_should_work=%s from %s" % (screen_should_work, flags))

    # ------------------------------------------------------------------
    print()
    print("=== screen: the whole editor window ===")
    # FAILS ON: the PNG path missing, the size in the text disagreeing with the PNG
    # header on disk, a flat fill where a window was asked for, or the mode claiming to
    # have framed a graph. The asset is irrelevant to this mode on purpose.
    shot = capture(session, "screen", asset=WIDGET)
    print(shot)
    print()

    if not screen_should_work:
        check("refuses when the process does not render",
              refused(shot))
        check("the refusal names the engine line",
              "LaunchEngineLoop.cpp:3343" in shot)
        check("the refusal says it will not write a black image",
              "black image" in shot)
        check("no image path is printed", image_path_of(shot) is None)
    else:
        check("does not refuse when the process does render",
              not refused(shot), shot.splitlines()[0] if shot else "")
        # FAILS ON: the mode claiming anything other than the full uncropped window. The
        # FIRST VERSION OF THIS CHECK ASSERTED ONLY THE PREFIX `framed: the whole editor
        # window` AND PASSED AGAINST A BROKEN BUILD THAT SAID `cropped to the graph` --
        # it was found by the break test rather than by review. The whole sentence is
        # asserted now, because a prefix match cannot tell an uncropped window from a
        # frame cut out of it.
        check("says the WHOLE window was framed, uncropped, not a graph",
              "framed: the whole editor window, uncropped, at its current size" in shot)
        check("says it is not the level viewport",
              "NOT the level viewport frame" in shot)

        window_match = re.search(r"window: (\d+)x(\d+), rendered at (\d+)x(\d+)", shot)
        check("reports the window size and the rendered size", window_match is not None)
        size_match = re.search(r"^size: (\d+)x(\d+)$", shot, re.M)
        check("reports an image size", size_match is not None)

        path = image_path_of(shot)
        check("prints an image path", path is not None)
        if path:
            absolute = resolve_printed_path(path)
            check("the image exists on disk", os.path.isfile(absolute), absolute)
            if os.path.isfile(absolute):
                actual = png_size(absolute)
                check("it is a real PNG", actual is not None, str(actual))
                if actual and size_match:
                    claimed = (int(size_match.group(1)), int(size_match.group(2)))
                    check("the PNG on disk is the size the tool claims",
                          actual == claimed, "disk %s vs text %s" % (actual, claimed))
                colours = distinct_colours(absolute)
                if colours is None:
                    check("the image could be read back", False,
                          "Pillow is missing, so the canvas could not be checked")
                else:
                    # THE CHECK THAT SEPARATES A PICTURE FROM A RECTANGLE. A black
                    # image, which is what a null renderer produces, has ONE colour.
                    check("the picture is not a flat fill", colours >= 500,
                          "%d distinct colours" % colours)

    # ------------------------------------------------------------------
    print()
    print("=== selection: refuses with nothing selected, and says which selection ===")
    # FAILS ON: a fallback to the whole graph, a refusal with an empty reason, or the
    # definition block being dropped from the output.
    nothing = capture(session, "selection")
    print(nothing)
    print()
    check("refuses when nothing is selected", refused(nothing))
    check("says nothing is selected", "nothing is selected" in nothing)
    check("says it is NOT a fallback to the whole graph",
          "not a fallback to the whole graph" in nothing)
    check("prints a reason rather than an empty error",
          len(reason_of(nothing)) > 40)

    # ------------------------------------------------------------------
    print()
    print("=== neighbours: the hop contract at N=0 and N=1 ===")
    # FAILS ON: a root that cannot be found, a hop count that is ignored, a knot being
    # framed, or the printed definition losing the D-42 rule.
    root_key, root_id = node_key(session, "PrintString")
    check("a root node was found to walk from", root_key is not None, str(root_id))

    if root_key:
        total = graph_node_count(session)
        print("        the graph holds %s node(s), from an independent tool" % total)

        zero = capture(session, "neighbours", target=root_id, size="0")
        print(zero)
        print()
        one = capture(session, "neighbours", target=root_id, size="1")
        print(one)
        print()

        zero_match = re.search(r"framed: node .* within 0 hop\(s\), (\d+) node\(s\)", zero)
        one_match = re.search(r"framed: node .* within 1 hop\(s\), (\d+) node\(s\)", one)
        check("N=0 reports a node count", zero_match is not None)
        check("N=1 reports a node count", one_match is not None)

        zero_count = int(zero_match.group(1)) if zero_match else None
        one_count = int(one_match.group(1)) if one_match else None

        # N=0 IS THE NODE ALONE. This is the check that proves the hop count is
        # honoured: a tool that ignored `size` would report the same number for both.
        check("N=0 frames exactly the one node", zero_count == 1, "reported %s" % zero_count)
        check("N=1 frames more than N=0", one_count is not None and zero_count is not None
              and one_count > zero_count, "%s vs %s" % (one_count, zero_count))

        # The count is cross-checked against the graph, so a number that is larger than
        # the graph is impossible rather than merely suspicious.
        check("N=1 frames no more nodes than the graph holds",
              one_count is not None and total is not None and one_count <= total,
              "%s <= %s" % (one_count, total))

        # NO NODE IS LISTED TWICE, and every hop number is within the count asked for.
        # This is what stops a breadth first walk that revisits nodes from passing.
        hop_lines = re.findall(r"^(BP_NodeTest:%s:[0-9a-f]+) at hop (\d+)$" % GRAPH,
                               one, re.M)
        ids = [line[0] for line in hop_lines]
        hops = [int(line[1]) for line in hop_lines]
        check("every framed node is listed with its hop", len(hop_lines) == (one_count or -1),
              "%d listed vs %s reported" % (len(hop_lines), one_count))
        check("no node is listed twice", len(ids) == len(set(ids)),
              "%d ids, %d unique" % (len(ids), len(set(ids))))
        check("no hop exceeds the count asked for", all(h <= 1 for h in hops), str(hops))
        check("the root is at hop 0", hops.count(0) == 1, str(hops))

        # D-42 IN THE OUTPUT, which is the requirement that the definition travels with
        # the answer. FAILS ON: the definition text being removed or reworded.
        check("prints the D-42 knot rule in its own output",
              "a plain reroute knot is therefore ONE hop and not two" in one)
        check("names the decision it follows", "D-42" in one)
        check("says a knot is never framed",
              "a knot is NEVER framed" in one)

    # ------------------------------------------------------------------
    print()
    print("=== neighbours: the refusals ===")
    # FAILS ON: each bad input being accepted instead of refused.
    bad_size = capture(session, "neighbours", target=root_id or "BP_NodeTest:EventGraph:000000",
                       size="banana")
    print(bad_size)
    print()
    check("a non-numeric hop count is refused", refused(bad_size))
    check("the refusal says what Size means here",
          "whole number of hops" in bad_size)

    no_target = capture(session, "neighbours", target="", size="1")
    check("a missing target is refused", refused(no_target))
    check("the refusal asks for a node ID", "needs a target node ID" in no_target)

    missing = capture(session, "neighbours", target="BP_NodeTest:EventGraph:ffffff", size="1")
    check("a key that matches nothing is refused", refused(missing))
    check("the refusal points at R-NODE-FIND", "R-NODE-FIND" in missing)

    huge = capture(session, "neighbours", target=root_id or "BP_NodeTest:EventGraph:000000",
                   size="99999")
    check("an absurd hop count is refused", refused(huge))
    check("the refusal names the limit", "limit" in huge)

    # ------------------------------------------------------------------
    print()
    print("=== box-neighbours: the refusals, and the box index contract ===")
    # FAILS ON: a box index that does not exist being accepted, a non-numeric index
    # being accepted, or the refused-combinations paragraph being dropped.
    no_box = capture(session, "box-neighbours", target="9", size="1")
    print(no_box)
    print()
    check("a box that does not exist is refused", refused(no_box))
    check("the refusal gives the real box count", "comment box(es)" in no_box)
    check("the refusal names what was asked for", "asked for box 9" in no_box)
    check("it does NOT fall back to a wider frame",
          "framed:" not in no_box)

    bad_box = capture(session, "box-neighbours", target="banana", size="1")
    check("a non-numeric box index is refused", refused(bad_box))
    check("the refusal says an index is wanted", "not a comment box index" in bad_box)

    empty_box = capture(session, "box-neighbours", target="", size="1")
    check("a missing box index is refused", refused(empty_box))

    # The refusals this mode documents are asserted as TEXT, because they are the
    # answer to "which combinations do you not support" and a caller reads them.
    # FAILS ON: that paragraph being removed from the output.
    defined = capture(session, "box-neighbours", target="0", size="0")
    print(defined)
    print()
    if refused(defined):
        check("with no boxes, box 0 is refused rather than framed",
              "comment box(es)" in defined)
    else:
        check("the definition block is printed", "box-neighbours:" in defined)
        check("the space and the bounds are stated",
              "GRAPH SPACE rectangles" in defined and "SMALLEST box" in defined)
        check("the refused combinations are stated in the answer",
              "refused combinations" in defined
              and "CURRENT SELECTION" in defined and "PRIOR CAPTURE" in defined)

    # ------------------------------------------------------------------
    print()
    print("=== the internal selection probe, and what it proves ===")
    # FAILS ON: the probe refusing a resolvable key, the engine not confirming the
    # selection, or the probe leaving the editor's selection changed.
    probe = capture(session, "select-nodes", target=root_key or "ffffff")
    print(probe)
    print()
    check("the probe reports what was selected BEFORE", "selected BEFORE:" in probe)
    check("the probe reports what the engine holds after",
          "selected AFTER asking for" in probe)
    check("the probe reads back through a SECOND call than the setter",
          "GetBoundsForNode found" in probe)
    accepted = re.search(r"selected AFTER asking for (\d+) node\(s\): (\d+) node\(s\)", probe)
    check("the probe reports the asked-for count and the engine's count",
          accepted is not None)
    check("the engine reported the same number it was asked for",
          accepted is not None and accepted.group(1) == accepted.group(2),
          accepted.group(0) if accepted else "")
    check("the engine accepted at least one node",
          accepted is not None and int(accepted.group(2)) >= 1,
          accepted.group(0) if accepted else "")
    check("the probe CLEARED the selection and says so",
          "selected after the probe cleared it: 0 node(s)" in probe)
    check("the probe says it is not the contract",
          "the PROBE, not the contract" in probe)
    check("the probe worked", not refused(probe))

    # A key that resolves to nothing changes NOTHING, which is the write-tool rule the
    # probe borrows: a bad input leaves the editor as it was.
    # FAILS ON: the probe selecting something anyway.
    probe_bad = capture(session, "select-nodes", target="ffffff,nope")
    print(probe_bad)
    print()
    check("a key that resolves to no node is refused", refused(probe_bad))
    check("the refusal says NOTHING was selected", "NOTHING was selected" in probe_bad)
    check("the refusal names the bad keys", "ffffff" in probe_bad)

    # The selection mode reads what the probe is about to clear, so after a probe the
    # refusal must still be the refusal rather than an empty success.
    after_probe = capture(session, "selection")
    check("selection still refuses after the probe cleared the selection",
          refused(after_probe) and "nothing is selected" in after_probe)

    # ------------------------------------------------------------------
    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
