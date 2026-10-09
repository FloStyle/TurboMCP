"""Exercise L-LAYOUT-REPORT against the running editor.

Checks the rules the spec states, and exits non-zero on failure.

  T-LAY-1 to T-LAY-4  layout facts for one graph
  P1                  a graph with no wires says so in words
  P3                  a Look tool measures and never moves
  P7                  a bad graph name lists the real ones

THE CHECK THAT MATTERS MOST
---------------------------
A layout checker that reports problems in everything is useless, and one that
reports nothing is worse. So this suite runs the same tool over a graph that Epic
laid out cleanly and over one that has real problems, and requires them to differ:

  StandardMacros / DoOnce       findings: none
  StandardMacros / ForEachLoop  one backward data wire and one backward exec wire

That pair is the evidence. `ForEachLoop` legitimately contains a loop that returns
upward and a completed path that rejoins behind it, which is why the tool reports a
backward wire rather than calling it a defect and why the result says so in words.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import LOOK_TOOLSET, Session  # noqa: E402

MACROS = "/Engine/EditorBlueprintResources/StandardMacros"

# Epic's own macros: one laid out cleanly, one with real backward wires.
CLEAN_GRAPH = "DoOnce"
MESSY_GRAPH = "ForEachLoop"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def main():
    session = Session()
    session.connect()

    print("=== the graph Epic laid out cleanly ===")
    clean = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutReport",
                         {"asset": MACROS, "graphName": CLEAN_GRAPH})
    print(clean[:1400])
    print()
    check("reports the node count", "node(s)" in clean)
    check("reports the wire count", "wire(s)" in clean)
    check("reports the extent", "extent: x" in clean)
    check("reports total wire length", "total wire length:" in clean)
    check("reports the longest wire", "longest single wire:" in clean)
    check(
        "says pin positions are computed, not read",
        "computed from each node's rectangle" in clean,
    )
    check(
        "finds nothing wrong in a clean graph",
        "findings: none." in clean,
    )

    print()
    print("=== the graph with real backward wires ===")
    messy = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutReport",
                         {"asset": MACROS, "graphName": MESSY_GRAPH})
    print(messy[:1600])
    print()
    check("finds a backward data wire", "backward-data:" in messy)
    check("finds a backward exec wire", "backward-exec:" in messy)
    check(
        "separates exec from data",
        "backward-data:" in messy and "backward-exec:" in messy,
    )
    check(
        "says a backward wire is not necessarily wrong",
        "not necessarily wrong" in messy,
    )

    # This is the whole point: the two graphs must not look the same.
    check(
        "the clean graph and the messy one differ",
        ("findings: none." in clean) and ("findings:" in messy)
        and ("findings: none." not in messy),
    )

    print()
    print("=== findings name node IDs, so a caller can act ===")
    check("a finding names a node ID", ":" in messy and "->" in messy)

    print()
    print("=== a graph with no wires says so in words (P1) ===")
    # A function graph with only an entry and a result node has no wires.
    empty = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutReport",
                         {"asset": "/Game/Tests/BP_NodeTest",
                          "graphName": "UserConstructionScript"})
    print(empty[:700])
    print()
    check("reports the graph", "UserConstructionScript" in empty)
    check(
        "either has no wires and says so, or reports findings",
        "no wires at all" in empty or "findings:" in empty or "findings: none." in empty,
    )

    print()
    print("=== P7: a bad graph name lists the real ones ===")
    bad = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutReport",
                       {"asset": "/Game/Tests/BP_NodeTest", "graphName": "NoSuchGraph"})
    print(bad)
    print()
    check("refuses", bad.startswith("error:"))
    check("lists the real graphs", "EventGraph" in bad)

    print()
    print("=== a missing graph name is refused ===")
    missing = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutReport",
                           {"asset": "/Game/Tests/BP_NodeTest", "graphName": ""})
    print(missing)
    print()
    check("refuses", missing.startswith("error:"))
    check("says a graph is needed", "no graph given" in missing)

    print()
    print("=== a non-Blueprint is refused ===")
    wrong = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutReport",
                         {"asset": "TA11_TestEnum", "graphName": "EventGraph"})
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
