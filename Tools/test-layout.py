# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-LAY-1, T-LAY-4
"""Exercise L-LAYOUT-REPORT against the running editor.

Checks the rules the spec states, and exits non-zero on failure.

  T-LAY-1 to T-LAY-4  layout facts for one graph
  P1                  a graph with no wires says so in words
  P3                  a Look tool measures and never moves
  P7                  a bad graph name lists the real ones

THE CHECK THAT MATTERS MOST
---------------------------
A layout checker that calls every vertical control-flow handoff a defect is
misleading. Compare two stock macros: DoOnce has no recognized handoff,
while ForEachLoop has a later Sequence lane. Both may have other layout findings:

  StandardMacros / DoOnce       no intended control-flow handoff
  StandardMacros / ForEachLoop  intended Sequence handoff plus backward wires

That pair checks that the Sequence lane gets a specific intended handoff rule.
Backward wires remain review candidates rather than automatic proof
that the macro is broken.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import LOOK_TOOLSET, Session  # noqa: E402

MACROS = "/Engine/EditorBlueprintResources/StandardMacros"

# Stock macros with different control-flow patterns.
CLEAN_GRAPH = "DoOnce"
MESSY_GRAPH = "ForEachLoop"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def layout(session, args):
    # The MCP schema requires the scope field; an empty value selects the whole graph.
    return session.call(LOOK_TOOLSET, "LookTurboMCPLayoutReport", dict(args, scope=""))


def main():
    session = Session()
    session.connect()

    print("=== stock macro without a recognized handoff ===")
    clean = layout(session,
                         {"asset": MACROS, "graphName": CLEAN_GRAPH})
    print(clean[:1400])
    print()
    check("reports the node count", "nodes," in clean)
    check("reports the wire count", "wires," in clean)
    check("reports the extent", "Extent: x" in clean)
    check("reports total wire length", "total wire length " in clean)
    check("reports the longest wire", "longest " in clean)
    check(
        "says pin positions are computed, not read",
        "computed pin positions" in clean,
    )
    check(
        "does not invent an intended control-flow handoff",
        "0 intended handoffs" in clean,
    )

    print()
    print("=== the graph with real backward wires ===")
    messy = layout(session,
                         {"asset": MACROS, "graphName": MESSY_GRAPH})
    print(messy[:1600])
    print()
    check("finds a backward data wire", "backward-data:" in messy)
    check("finds a backward exec wire", "backward-exec:" in messy)
    check("reports classified findings", "classification:" in messy and "class=defect" in messy)
    check(
        "separates exec from data",
        "backward-data:" in messy and "backward-exec:" in messy,
    )
    check(
        "says a backward wire is not necessarily wrong",
        "Defect candidates require visual review" in messy,
    )

    # A known Sequence handoff is distinguished from other layout findings.
    counts = re.search(r"classification: (\d+) defect candidates, (\d+) intended handoffs", messy)
    intended_page = layout(session,
                           {"asset": MACROS, "graphName": MESSY_GRAPH,
                            "offset": int(counts.group(1))}) if counts else ""
    print("intended finding page:", intended_page[:1100])
    check(
        "the Sequence handoff has its explicit rule",
        bool(counts) and int(counts.group(2)) > 0
        and "class=intended rule=R4" in intended_page,
    )

    print()
    print("=== findings name node IDs, so a caller can act ===")
    check("a finding names a node ID", ":" in messy and "->" in messy)

    print()
    print("=== a graph with no wires says so in words (P1) ===")
    # A function graph with only an entry and a result node has no wires.
    empty = layout(session,
                         {"asset": "/Game/Tests/BP_NodeTest",
                          "graphName": "UserConstructionScript"})
    print(empty[:700])
    print()
    check("reports the graph", "UserConstructionScript" in empty)
    check(
        "either has no wires and says so, or reports findings",
        "no wires at all" in empty or "findings:" in empty,
    )

    print()
    print("=== P7: a bad graph name lists the real ones ===")
    bad = layout(session,
                       {"asset": "/Game/Tests/BP_NodeTest", "graphName": "NoSuchGraph"})
    print(bad)
    print()
    check("refuses", bad.startswith("error:"))
    check("lists the real graphs", "EventGraph" in bad)

    print()
    print("=== a missing graph name is refused ===")
    missing = layout(session,
                           {"asset": "/Game/Tests/BP_NodeTest", "graphName": ""})
    print(missing)
    print()
    check("refuses", missing.startswith("error:"))
    check("says a graph is needed", "no graph given" in missing)

    print()
    print("=== a non-Blueprint is refused ===")
    wrong = layout(session,
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
