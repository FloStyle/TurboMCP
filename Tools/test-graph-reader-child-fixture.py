"""Focused graph-reader proof for the derived Blueprint fixture.

The fixture has one linked function graph. It deliberately contains every node
shape this regression needs: function entry, a child-Blueprint parent call, a
break-struct value path, and a function result. Reading at ``full`` must name
all of them, and that exact reader output must be accepted by GraphCheck.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tmcp import PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

ASSET = "TurboMCPGraphReaderChild"
GRAPH = "GraphReaderFixture"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def main():
    session = Session()
    session.connect()

    print("=== reader lists every node in the focused child-Blueprint fixture ===")
    text = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", {
        "asset": ASSET, "graphName": GRAPH, "format": "", "detail": "full",
    })
    print(text)
    check("the reader returned the child fixture", isinstance(text, str) and text.startswith("read: "),
          str(text)[:160])

    # The entry is the function form's head. The remaining nodes are explicitly
    # rendered at full detail with their engine classes, so no node can disappear
    # behind a friendly title or a type-ID spelling change.
    for needle, label in (
        ("(fn GraphReaderFixture", "function entry"),
        ("K2Node_CallParentFunction", "child Blueprint parent call"),
        ("K2Node_BreakStruct", "break-struct node"),
        ("K2Node_FunctionResult", "function result node"),
    ):
        check("reader contains the %s" % label, needle in text, text[:1400])

    print("=== exact reader output passes GraphCheck ===")
    checked = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphCheck", {
        "asset": ASSET, "graphName": GRAPH, "text": text,
    })
    print(checked)
    check("GraphCheck accepts the reader output with zero findings",
          "0 finding(s)" in checked and "would be accepted" in checked, checked[:700])

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
