"""Exercise P-GRAPH-DEAD and E-GRAPH-DEAD against the running editor.

  T-LAY-11  remove dead copies

WHAT THIS SUITE CAN AND CANNOT PROVE, STATED UP FRONT
-----------------------------------------------------
This plugin's tools cover reading, previewing and editing; they do NOT create graph nodes. The
engine's `write_graph_dsl` does, and it REFUSES to build a disconnected group: it needs a top-level
`event` or `fn` form, and every node it makes is wired into that form's exec chain by construction.

**So the test graph contains NO dead node, and no dead node could be made with the writers
available.** That is the same wall Q-9 hit with reroute knots.

The consequence is stated rather than hidden: **the REMOVAL path is not exercised here.** What IS
exercised, and what these checks are for:

- the reachability walk finds nothing on a graph where everything IS reachable, which is the answer a
  live graph must get. A walk that over-reports would call live code dead, and that is the failure
  that destroys a user's work;
- the edit REFUSES to remove anything when there are no candidates, rather than recompiling the graph
  for nothing;
- the graph is UNCHANGED by a preview and by the refusing edit, checked by NODE IDENTITY rather than
  by a count.

The removal path is recorded as unverified in QUESTIONS.md (Q-12) with the route that would close it.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

ASSET = "BP_NodeTest"
GRAPH = "EventGraph"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def node_keys(session):
    """Every node key in the graph, from the ids the reader prints."""
    text = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                        {"asset": ASSET, "graphName": GRAPH, "detail": "full",
                         "format": "", "tokenBudget": ""})
    keys = []
    for match in re.finditer(r"%s:(\w{6})\s*;" % re.escape(GRAPH), text):
        if match.group(1) not in keys:
            keys.append(match.group(1))
    return keys, text


def main():
    session = Session()
    session.connect()

    before_keys, _ = node_keys(session)
    print("=== the graph has %d node(s) ===" % len(before_keys))

    print()
    print("=== the preview finds nothing on a graph where everything is reachable ===")
    # THE CHECK THAT MATTERS MOST. A walk that over-reports would call live code dead, so a live
    # graph answering "nothing" is the result that must be got right.
    out = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphDead",
                       {"asset": ASSET, "graphName": GRAPH})
    print(out)
    print()

    # THE COUNTS ARE PARSED FROM THE TOOL, not from a regex over the reader. The first version
    # scraped node ids out of `R-BP-LOAD` and got 8 while the graph has 11, because that reader
    # prints an id for the nodes it details and the comparison failed on a tool that was right.
    # Fifth time this project has had to learn: parse the value, do not match a string beside it.
    import re as _re  # noqa: PLC0415
    counted = _re.search(r"(\d+) node\(s\) in the graph, (\d+) unreachable group", out)
    nodes = int(counted.group(1)) if counted else None
    groups = int(counted.group(2)) if counted else None
    print("    the tool reports %s node(s) and %s unreachable group(s)" % (nodes, groups))
    check("the counts are parseable", nodes is not None and groups is not None, out[:160])
    check("it counts the graph", nodes is not None and nodes > 0, str(nodes))
    check("IT FINDS NOTHING DEAD IN A LIVE GRAPH", groups == 0, str(groups))
    check("it says WHY there is nothing, rather than being blank",
          "every node is reachable" in out, out[:240])
    # `candidates == 0` means nothing to remove. Asserted through the parsed number rather than a
    # phrase, because the phrase only appears when there ARE groups to weigh.
    check("it does not claim it would remove anything",
          "WOULD REMOVE" not in out, out[-200:])

    print()
    print("=== the preview changes nothing ===")
    after_preview, _ = node_keys(session)
    check("the node set is identical after a preview",
          sorted(after_preview) == sorted(before_keys),
          "gained %s, lost %s" % (sorted(set(after_preview) - set(before_keys)),
                                  sorted(set(before_keys) - set(after_preview))))

    print()
    print("=== the edit refuses when there is nothing safe to remove ===")
    edited = session.call(EDIT_TOOLSET, "EditTurboMCPGraphDead",
                          {"asset": ASSET, "graphName": GRAPH})
    print(edited)
    print()
    # `unchanged:` rather than `changed: removed 0`, because a caller must be able to tell a no-op
    # from a change (`P4`).
    check("it opens with unchanged:", edited.startswith("unchanged:"), edited[:70])
    check("it says nothing was deleted", "nothing was deleted" in edited, edited[:200])
    check("it points at the preview for the reasons",
          "P-GRAPH-DEAD" in edited, edited[:240])

    after_edit, _ = node_keys(session)
    check("the node set is identical after the refusing edit",
          sorted(after_edit) == sorted(before_keys),
          "gained %s, lost %s" % (sorted(set(after_edit) - set(before_keys)),
                                  sorted(set(before_keys) - set(after_edit))))

    print()
    print("=== the reachability rule is the GENEROUS one ===")
    # The graph's pure nodes — the comparison, the arithmetic — have no exec wire of their own and
    # are reachable only because a live node READS them. If the walk followed exec alone they would
    # be reported dead. So finding nothing here is evidence that DATA wires are followed.
    pure = re.findall(r"(K2Node_PromotableOperator|K2Node_CallFunction)", _)
    print("    the reader names %d node(s) and the graph has pure nodes in it" % len(pure))
    check("the graph does contain pure operator nodes, so the data wire has something to carry",
          len(pure) > 0, "found %d" % len(pure))
    check("none of them is reported dead",
          "0 unreachable group(s)" in out,
          "a pure node with a live consumer must not be called dead")

    print()
    print("=== the request is validated ===")
    for label, args, expect in (
        ("no asset", {"asset": "", "graphName": GRAPH}, "no asset given"),
        ("a non-Blueprint", {"asset": "TA18_ColourSuite", "graphName": GRAPH},
         "not a Blueprint"),
        ("a graph that does not exist", {"asset": ASSET, "graphName": "NoSuchGraph"},
         "is not a graph of this Blueprint"),
    ):
        result = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphDead", args)
        check("refuses %s" % label,
              result.startswith("error:") and expect in result, result[:120])

    print()
    print("=== the default graph is named in the answer ===")
    # An empty graph name means the event graph. The answer must say WHICH graph was used, so a
    # caller who omitted the name is not left guessing.
    defaulted = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphDead",
                             {"asset": ASSET, "graphName": ""})
    check("it names the graph it used", GRAPH in defaulted, defaulted[:120])

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
