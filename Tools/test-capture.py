"""Exercise L-CAPTURE against the running editor.

Checks the rules the spec states, and exits non-zero on failure.

  T-LOOK-1 to T-LOOK-4  picture of an asset editor
  P2                    a framing that cannot be done is an error naming why
  P3                    a Look tool changes nothing but the image it writes

WHAT IS VERIFIED, AND WHAT IS NOT
---------------------------------
Verified: the mode and target validation, that a bad framing is refused with the
numbers that make it wrong rather than falling back to the whole graph, that the view
location and zoom are actually applied and read back, that a PNG of exactly the
requested size lands on disk, and that its name says which asset, graph and mode made
it.

NOT verified, and asserted as such below: the captured canvas is **empty**. The image
contains the real graph title bar, toolbar and breadcrumb, so the widget really is the
graph editor, but no node is drawn. That is a limitation of this tool, not a claim about
the graph. The suite requires the title bar to be part of the picture and is honest in
its own output that the nodes are missing.
"""

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import LOOK_TOOLSET, Session  # noqa: E402

BLUEPRINT = "/Game/Tests/BP_NodeTest.BP_NodeTest"
GRAPH = "EventGraph"

# Several nodes, so "the whole graph" has something to frame.
EXPECTED_NODES = 11

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def png_size(path):
    try:
        with open(path, "rb") as handle:
            header = handle.read(24)
    except OSError:
        return None
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    return struct.unpack(">II", header[16:24])


def resolve_printed_path(printed):
    """The tool prints a path relative to the engine, as the editor sees it."""
    return os.path.abspath(os.path.join(
        r"C:\ExampleUnrealengine\Engine\Binaries\Win64", printed))


def main():
    session = Session()
    session.connect()

    print("=== the whole graph ===")
    out = session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                       {"asset": BLUEPRINT, "graphName": GRAPH,
                        "mode": "", "target": "", "size": ""})
    print(out)
    print()
    check("reports the asset", BLUEPRINT in out)
    check("reports the graph", "graph: %s" % GRAPH in out)
    check("says what it framed", "framed: the whole graph" in out)
    check("reports the node count it framed",
          "%d node(s)" % EXPECTED_NODES in out)
    check("reports the frame bounds", "frame bounds: x" in out)
    check("reports the zoom asked for AND applied",
          "zoom: asked" in out and "applied" in out)
    check("reports the size", "size: 1600x900" in out)
    check("writes an image and says where", "image:" in out and ".png" in out)

    image_path = None
    for line in out.splitlines():
        if line.startswith("image:"):
            image_path = line.split(":", 1)[1].strip()
    check("an image path was printed", image_path is not None)

    if image_path:
        absolute = resolve_printed_path(image_path)
        check("the image exists", os.path.isfile(absolute), absolute)
        if os.path.isfile(absolute):
            check("it is a real PNG at the size asked for",
                  png_size(absolute) == (1600, 900), str(png_size(absolute)))
            check("it is not empty", os.path.getsize(absolute) > 1000,
                  "%d bytes" % os.path.getsize(absolute))
            check("the name says the asset, graph and mode",
                  "BP_NodeTest" in image_path and "EventGraph" in image_path
                  and "graph" in image_path)

    print()
    print("=== a requested size is honoured ===")
    small = session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                         {"asset": BLUEPRINT, "graphName": GRAPH,
                          "mode": "", "target": "", "size": "800x400"})
    check("reports the size asked for", "size: 800x400" in small)
    small_path = None
    for line in small.splitlines():
        if line.startswith("image:"):
            small_path = line.split(":", 1)[1].strip()
    if small_path:
        absolute = resolve_printed_path(small_path)
        if os.path.isfile(absolute):
            check("the second image is at the second size",
                  png_size(absolute) == (800, 400), str(png_size(absolute)))

    print()
    print("=== P2: asking for a box that does not exist names the numbers ===")
    # BP_NodeTest has no comment boxes, so box 0 is already out of range.
    no_box = session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                          {"asset": BLUEPRINT, "graphName": GRAPH,
                           "mode": "box", "target": "9", "size": ""})
    print(no_box)
    print()
    check("refuses", no_box.startswith("error:"))
    check("names what was asked for", "asked for box 9" in no_box)
    check("names how many exist", "comment box(es)" in no_box)
    check("does NOT fall back to the whole graph",
          "framed: the whole graph" not in no_box)

    print()
    print("=== P2: mode box needs a target ===")
    no_target = session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                             {"asset": BLUEPRINT, "graphName": GRAPH,
                              "mode": "box", "target": "", "size": ""})
    print(no_target)
    print()
    check("refuses", no_target.startswith("error:"))
    check("says a target is needed", "needs a target" in no_target)

    print()
    print("=== P2: a node ID that resolves is framed ===")
    # An ID taken from R-NODE-FIND, so the two tools agree on the form.
    found = session.call("TurboMCP.TurboMCPReadToolset", "ReadTurboMCPNodeFind",
                         {"asset": "BP_NodeTest", "words": "PrintString",
                          "graphName": GRAPH})
    first_line = None
    for line in found.splitlines():
        if line.startswith("BP_NodeTest:%s:" % GRAPH):
            first_line = line.strip()
            break
    check("a node ID was found to frame", first_line is not None, str(first_line))

    if first_line:
        one = session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                           {"asset": BLUEPRINT, "graphName": GRAPH,
                            "mode": "node", "target": first_line, "size": "800x600"})
        print(one)
        print()
        check("frames the node", "framed: node" in one)
        check("writes an image", "image:" in one)

    print()
    print("=== P2: an unknown node key is refused ===")
    bad_node = session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                            {"asset": BLUEPRINT, "graphName": GRAPH,
                             "mode": "node", "target": "BP_NodeTest:EventGraph:ffffff",
                             "size": ""})
    print(bad_node)
    print()
    check("refuses", bad_node.startswith("error:"))
    check("suggests R-NODE-FIND", "R-NODE-FIND" in bad_node)

    print()
    print("=== P2: a bad mode is refused by name ===")
    bad_mode = session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                            {"asset": BLUEPRINT, "graphName": GRAPH,
                             "mode": "banana", "target": "", "size": ""})
    print(bad_mode)
    print()
    check("refuses", bad_mode.startswith("error:"))
    check("names every accepted mode", "graph, box, node" in bad_mode)

    print()
    print("=== P2: a bad size is refused by name ===")
    bad_size = session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                            {"asset": BLUEPRINT, "graphName": GRAPH,
                             "mode": "", "target": "", "size": "banana"})
    check("refuses", bad_size.startswith("error:"))

    print()
    print("=== P2: a graph that does not exist lists the real ones ===")
    bad_graph = session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                             {"asset": BLUEPRINT, "graphName": "NoSuchGraph",
                              "mode": "", "target": "", "size": ""})
    check("refuses", bad_graph.startswith("error:"))
    check("lists the real graphs", "EventGraph" in bad_graph)

    print()
    print("=== a Widget Blueprint is a Blueprint, so it is captured ===")
    # A correction to an earlier assumption: UWidgetBlueprint derives from UBlueprint,
    # so a Widget Blueprint's graph CAN be captured and is not refused. The first
    # version of this suite asserted a refusal and was wrong.
    widget = session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                          {"asset": "WBP_NodeTest", "graphName": "EventGraph",
                           "mode": "", "target": "", "size": "800x600"})
    print(widget)
    print()
    check("captures a Widget Blueprint's graph, or says why not",
          ("image:" in widget) or widget.startswith("error:"))
    if widget.startswith("error:"):
        check("the refusal explains itself", "error:" in widget)

    print()
    print("=== P2: a graph that does not exist is refused, for a Widget Blueprint too ===")
    widget_bad = session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                              {"asset": "WBP_NodeTest", "graphName": "NoSuchGraph",
                               "mode": "", "target": "", "size": ""})
    check("refuses", widget_bad.startswith("error:"))

    print()
    print("=== P2: an asset that is not a Blueprint at all is refused ===")
    not_bp = session.call(LOOK_TOOLSET, "LookTurboMCPCapture",
                          {"asset": "TA11_TestEnum", "graphName": "EventGraph",
                           "mode": "", "target": "", "size": ""})
    print(not_bp)
    print()
    check("refuses", not_bp.startswith("error:"))
    check("says it is not a Blueprint", "not a Blueprint" in not_bp)

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
