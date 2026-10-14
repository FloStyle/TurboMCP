"""Regression: a connected SetText-style target pin round-trips through reader and checker."""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tmcp import PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

ASSET = "TurboMCPAcceptance"
GRAPH = "TargetPinRoundTripProbe"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def main():
    session = Session()
    session.connect()

    print("=== connected Target/self pin remains an ordinary pin line ===")
    text = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad", {
        "asset": ASSET, "graphName": GRAPH, "format": "", "detail": "bodies",
    })
    print(text)
    check("the reader returned the fixture", isinstance(text, str) and text.startswith("read: "),
          str(text)[:160])
    check("the connected self pin is printed", re.search(r"^\s+self\s+\(", text, re.M) is not None,
          text[:900])
    check("the getter/source node is in the body",
          re.search(r"Variables\|Default\|GetRoundTripTextTarget", text) is not None, text[:1200])
    check("the reader does not report the getter disconnected",
          "not reachable from any entry point" not in text, text[-400:])

    print("=== reader and dead-node preview share all-wire reachability ===")
    dead = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphDead", {
        "asset": ASSET, "graphName": GRAPH,
    })
    print(dead)
    check("dead preview finds no unreachable group", "0 unreachable group(s)" in dead, dead[:300])

    print("=== exact reader output is accepted by GraphCheck ===")
    checked = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphCheck", {
        "asset": ASSET, "graphName": GRAPH, "text": text,
    })
    print(checked)
    check("GraphCheck reports zero findings", "0 finding(s)" in checked and "would be accepted" in checked,
          checked[:500])

    # Graph readers resolve graph names without case sensitivity. The preview
    # tools must select the same graph when they receive reader output.
    case_checked = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphCheck", {
        "asset": ASSET, "graphName": GRAPH.lower(), "text": text,
    })
    check("GraphCheck resolves the reader graph case-insensitively",
          "0 finding(s)" in case_checked and "would be accepted" in case_checked, case_checked[:300])

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
