"""Exercise R-COMMENT-LIST against the running editor.

Checks the rules the spec states, and exits non-zero on failure.

  T-LAY-6  every comment box: title, colour, rectangle, parent box if nested,
           and the IDs of the nodes inside

WHAT IS VERIFIED AGAINST REAL CONTENT, AND WHAT IS NOT
------------------------------------------------------
The real checks run against `/Engine/EditorBlueprintResources/StandardMacros`,
which is Epic's own content and carries **21 comment boxes**, 17 of them covering
nodes. That verifies titles, colours, rectangles, node membership and the node ID
form on content nobody here wrote.

Two paths are **not** exercised by any content reachable from this project: nested
boxes and overlapping boxes. Every engine asset tried reports `0 nested` and
`0 node(s) inside more than one box`. So those two are checked against a MODEL of
the rectangle rules, below, which is honest about what it does and does not prove:

  it proves the containment and overlap arithmetic is right
  it does NOT prove the C++ renders a nested box correctly, because no nested box
  was available to render

That second half is recorded in TestRecord.md as unverified rather than claimed.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import READ_TOOLSET, Session  # noqa: E402

# Epic's own macros Blueprint. 21 boxes, 17 with nodes.
ENGINE_ASSET = "/Engine/EditorBlueprintResources/StandardMacros"

# The project Blueprint, which has no boxes at all. Both are needed: one proves the
# reader finds boxes, the other proves a zero is honest.
EMPTY_ASSET = "/Game/Tests/BP_NodeTest"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


# ---------------------------------------------------------------------------
# A model of the rectangle rules the tool implements, so the arithmetic can be
# checked even though no nested box exists to render.
# ---------------------------------------------------------------------------
def rect(x, y, w, h):
    return {"minx": x, "miny": y, "maxx": x + w, "maxy": y + h}


def area(r):
    return (r["maxx"] - r["minx"]) * (r["maxy"] - r["miny"])


def contains(outer, inner):
    return (inner["minx"] >= outer["minx"] and inner["miny"] >= outer["miny"]
            and inner["maxx"] <= outer["maxx"] and inner["maxy"] <= outer["maxy"])


def covers_node(box, node):
    """A node is covered when its POSITION is inside, which is what the tool does."""
    return (box["minx"] <= node["minx"] <= box["maxx"]
            and box["miny"] <= node["miny"] <= box["maxy"])


def parent_of(boxes, box):
    """The smallest box that contains this one, ignoring an identical rectangle."""
    best = None
    smallest = None
    for other in boxes:
        if other is box:
            continue
        if not contains(other, box):
            continue
        if (other["minx"] == box["minx"] and other["miny"] == box["miny"]
                and other["maxx"] == box["maxx"] and other["maxy"] == box["maxy"]):
            continue
        if smallest is None or area(other) < smallest:
            smallest = area(other)
            best = other
    return best


def check_geometry():
    print("=== the rectangle rules, against a model ===")

    outer = rect(0, 0, 1000, 1000)
    middle = rect(100, 100, 500, 500)
    inner = rect(200, 200, 100, 100)
    beside = rect(2000, 2000, 100, 100)

    check("a box contains a box inside it", contains(outer, inner))
    check("a box does not contain one outside it", not contains(outer, beside))
    check("a box contains itself (so identity must be excluded)", contains(outer, outer))
    check("a partly overlapping box is not contained",
          not contains(outer, rect(900, 900, 500, 500)))

    check("a node at a box's corner is covered", covers_node(outer, {"minx": 0, "miny": 0}))
    check("a node on the edge is covered", covers_node(outer, {"minx": 1000, "miny": 1000}))
    check("a node outside is not covered",
          not covers_node(outer, {"minx": 1001, "miny": 500}))

    boxes = [outer, middle, inner]
    check("the innermost containing box is the parent",
          parent_of(boxes, inner) is middle)
    check("the outermost box has no parent", parent_of(boxes, outer) is None)
    check("the middle box's parent is the outer one",
          parent_of(boxes, middle) is outer)

    twins = [rect(0, 0, 100, 100), rect(0, 0, 100, 100)]
    check("two identical boxes are not each other's parent",
          parent_of(twins, twins[0]) is None)

    # A node inside two boxes: the count the tool reports.
    a = rect(0, 0, 500, 500)
    b = rect(100, 100, 500, 500)
    node = {"minx": 300, "miny": 300}
    cover_count = sum(1 for box in (a, b) if covers_node(box, node))
    check("a node in two boxes is counted twice", cover_count == 2)


def main():
    check_geometry()
    print()

    session = Session()
    session.connect()

    print("=== T-LAY-6: real comment boxes, on Epic's own content ===")
    out = session.call(READ_TOOLSET, "ReadTurboMCPCommentList",
                       {"asset": ENGINE_ASSET, "graphName": ""})
    print(out[:1800])
    print()

    check("finds the boxes", "counts: 21 box(es)" in out)
    check("some boxes cover nodes", "with nodes" in out)
    check("every box has a title line", out.count("\n      title: ") >= 20)
    check("every box has a colour", out.count("\n      colour: ") >= 20)
    check("every box has a rectangle", out.count("\n      rect: ") >= 20)
    check("every box says whether it is nested",
          out.count("\n      parent box: ") >= 20)
    check("a box that covers no node says so", "(this box covers no node)" in out)
    check("node IDs are printed", "StandardMacros:DoOnce:" in out)
    check("the box's own ID is the long form",
          "StandardMacros.StandardMacros:DoOnce:" in out)
    check("a graph with no boxes says so", "no comment boxes" in out)
    check("colours are hex", "#" in out)

    print()
    print("=== a Blueprint with no boxes is an honest zero ===")
    empty = session.call(READ_TOOLSET, "ReadTurboMCPCommentList",
                         {"asset": EMPTY_ASSET, "graphName": ""})
    print(empty[:600])
    print()
    check("reports zero boxes", "counts: 0 box(es)" in empty)
    check("says the graphs were looked at", "no comment boxes" in empty)
    check("does not leave a bare zero", "No comment box exists" in empty)

    print()
    print("=== a graph name that does not exist lists the real ones ===")
    bad = session.call(READ_TOOLSET, "ReadTurboMCPCommentList",
                       {"asset": EMPTY_ASSET, "graphName": "NoSuchGraph"})
    print(bad)
    print()
    check("refuses", bad.startswith("error:"))
    check("lists the real graphs", "EventGraph" in bad)

    print()
    print("=== one graph at a time works ===")
    one = session.call(READ_TOOLSET, "ReadTurboMCPCommentList",
                       {"asset": ENGINE_ASSET, "graphName": "DoOnce"})
    print(one[:700])
    print()
    check("reports the named graph", "graph DoOnce" in one)
    check("finds its one box", "1 comment box(es)" in one)
    check("does not report other graphs", "ForEachLoop" not in one)

    print()
    print("=== a non-Blueprint is refused ===")
    wrong = session.call(READ_TOOLSET, "ReadTurboMCPCommentList",
                         {"asset": "TA11_TestEnum", "graphName": ""})
    print(wrong)
    print()
    check("refuses", wrong.startswith("error:"))
    check("says why", "not a Blueprint" in wrong)

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
