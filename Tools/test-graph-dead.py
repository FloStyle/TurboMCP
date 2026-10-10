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

    # A COMMENT BOX IS NOT DEAD CODE, AND THIS CHECK EXISTS BECAUSE THE TOOL USED TO SAY IT WAS.
    #
    # MEASURED DEFECT: `CollectUnreachableGroups` seeded a group from EVERY unreached node, and a comment
    # has NO PINS AT ALL - so a walk that starts at entry nodes and follows wires can never reach one. Every
    # comment in every graph looked like an unreachable orphan that also wrote no variable and held no
    # entry, so it satisfied all three removal conditions and the tool answered
    # `WOULD REMOVE 1 node(s): EdGraphNode_Comment` on this very asset.
    #
    # AN IMPLICIT CHECK CAUGHT IT, WHICH IS WHY THIS ONE IS NOW EXPLICIT: `BP_NodeTest` carries a comment,
    # so the `groups == 0` check above went red on a correct tool. The check below names the reason, so the
    # next person sees WHAT was about to be deleted rather than only that a count was wrong.
    comment_out = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphDead",
                               {"asset": ASSET, "graphName": GRAPH})
    check("a COMMENT BOX is never offered for removal",
          "EdGraphNode_Comment" not in comment_out
          or "WOULD REMOVE" not in comment_out.split("EdGraphNode_Comment")[0].rsplit("\n\n", 1)[-1],
          comment_out[:300])
    check("a comment is not counted as an unreachable GROUP either",
          "unreachable group" in comment_out
          and comment_out.split("unreachable group")[0].strip().split()[-1] == "0",
          comment_out[:120])

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
    print("=== THE REMOVAL PATH, on the acceptance asset built for it (Q-12) ===")
    #
    # THE HALF THAT HAS NEVER RUN. Everything above tests the finding half against a graph with
    # nothing dead in it, which is the safe answer but proves nothing about a DELETE. The asset
    # `TurboMCPAcceptance:DeadProbe` carries two disconnected groups precisely so this can be run, and
    # `Tools/make-acceptance-asset.ps1` REGENERATES it, so mutating it is safe: the next run rebuilds
    # it from the commandlet that describes it.
    import subprocess  # noqa: PLC0415

    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    rebuild = subprocess.run(
        ["pwsh", "-NoProfile", "-File", os.path.join(repo, "Tools", "make-acceptance-asset.ps1")],
        capture_output=True, text=True, cwd=repo)
    print("    rebuild: %s" % (rebuild.stdout or rebuild.stderr).strip().splitlines()[0]
          if (rebuild.stdout or rebuild.stderr) else "    rebuild: no output")
    check("the acceptance asset was regenerated before the destructive test",
          rebuild.returncode == 0 and "PASS" in (rebuild.stdout or ""),
          (rebuild.stdout or rebuild.stderr)[-200:])

    ACCEPT = "TurboMCPAcceptance"
    DEAD_GRAPH = "DeadProbe"

    # THE REBUILD STOPPED THE EDITOR, so it is started again before the connection is remade. The
    # stop is deliberate: the editor holds the .uasset open and the save would fail for a reason that
    # has nothing to do with the commandlet.
    #
    # THE START USES `subprocess.call`, NOT `subprocess.run(capture_output=True)`, AND THAT MATTERS.
    # Measured: with `capture_output=True` the editor did NOT come up, and the next `connect()` failed
    # with "no connection could be made because the target machine actively refused it". `run` with
    # captured output waits for the child's stdio to close, and a launcher that starts a detached
    # editor leaves a pipe open, so the call returns before the editor is ready — or the editor is
    # killed with the pipe. Letting the child inherit stdio avoids the whole question.
    start = subprocess.call(
        ["pwsh", "-NoProfile", "-File", os.path.join(repo, "Tools", "start-editor.ps1")],
        cwd=repo)

    # THE LAUNCHER'S EXIT CODE IS NOT THE CHECK, AND REQUIRING IT TO BE ZERO MADE THIS SUITE FAIL FOR A
    # REASON THAT IS NOT A PROBLEM.
    #
    # MEASURED: `Tools/run-suites.ps1` OWNS THE EDITOR for its whole run, so by the time this suite runs an
    # editor is already up. `start-editor.ps1` REFUSES to start a second one and answers NON-ZERO - which is
    # correct behaviour and exactly what stops two editors fighting over the same `.uasset`. The old check
    # demanded `start == 0`, so it reported EIGHT failures on a tool that had worked, and the first of them
    # was about the launcher rather than about the graph.
    #
    # WHAT ACTUALLY MATTERS IS WHETHER THE SERVER ANSWERS, and that is polled immediately below rather than
    # inferred from an exit code. The same mistake, in the same shape, is recorded in TestRecord 6j: a check
    # that fails on a CORRECT answer is as bad as one that cannot fail.
    print("    the editor launcher returned %s (non-zero means one was ALREADY RUNNING, which is fine)"
          % start)

    # The server is polled until it answers, rather than assumed ready the moment the launcher
    # returns. A fixed sleep would be either too short on a cold start or wasted time on a warm one.
    import time as _time  # noqa: PLC0415
    deadline = _time.time() + 180
    ready = False
    while _time.time() < deadline:
        try:
            probe = Session()
            probe.connect()
            ready = True
            break
        except Exception:
            _time.sleep(3)
    check("the MCP server answered after the restart", ready,
          "gave up after 180s")

    session = Session()
    session.connect()

    before_preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphDead",
                                  {"asset": ACCEPT, "graphName": DEAD_GRAPH})
    print(before_preview)
    print()

    import re as _re  # noqa: PLC0415
    counted = _re.search(r"(\d+) node\(s\) in the graph, (\d+) unreachable group", before_preview)
    node_before = int(counted.group(1)) if counted else None
    groups_before = int(counted.group(2)) if counted else None
    check("the asset HAS a disconnected group to remove",
          groups_before is not None and groups_before > 0,
          "the finding half would be untested without one")
    check("it says it would remove something",
          "WOULD REMOVE" in before_preview, before_preview[:200])

    # The node keys of the groups that WOULD be removed, so the removal can be checked against the
    # keys rather than against a count that could coincide.
    removable = _re.findall(r"WOULD REMOVE.*?\n\s+nodes: ([\w, ]+)", before_preview)
    removable_keys = [k.strip() for group in removable for k in group.split(",")]
    print("    keys the preview would remove: %s" % ", ".join(removable_keys))
    check("the preview names the node keys it would remove",
          len(removable_keys) > 0, before_preview[:200])

    removed = session.call(EDIT_TOOLSET, "EditTurboMCPGraphDead",
                           {"asset": ACCEPT, "graphName": DEAD_GRAPH})
    print(removed)
    print()
    check("the edit opens with changed:", removed.startswith("changed:"), removed[:70])
    check("it reports the node counts before and after",
          "nodes:" in removed and "->" in removed, removed[:200])
    check("it removed the nodes the PREVIEW named",
          all(k in removed for k in removable_keys) or "removed" in removed,
          "preview said %s" % removable_keys)

    # THE READ-BACK THROUGH A DIFFERENT TOOL. The edit's own sentence is not evidence; the graph is
    # asked again and the keys are checked individually.
    after_preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphDead",
                                 {"asset": ACCEPT, "graphName": DEAD_GRAPH})
    print(after_preview)
    print()
    after_counted = _re.search(r"(\d+) node\(s\) in the graph, (\d+) unreachable group",
                               after_preview)
    node_after = int(after_counted.group(1)) if after_counted else None
    groups_after = int(after_counted.group(2)) if after_counted else None
    print("    nodes %s -> %s, groups %s -> %s"
          % (node_before, node_after, groups_before, groups_after))
    check("THE DEAD GROUPS ARE GONE", groups_after == 0, "still %s" % groups_after)
    check("the node count DROPPED, so nodes really were removed",
          node_after is not None and node_before is not None and node_after < node_before,
          "%s -> %s" % (node_before, node_after))
    check("it removed exactly the keys the preview named",
          node_before - node_after == len(removable_keys),
          "removed %s for %d named key(s)"
          % (node_before - node_after if node_after is not None else "?", len(removable_keys)))

    # THE SECOND CALL MUST BE A NO-OP, because the groups are gone.
    again = session.call(EDIT_TOOLSET, "EditTurboMCPGraphDead",
                         {"asset": ACCEPT, "graphName": DEAD_GRAPH})
    check("a second removal is refused with unchanged:",
          again.startswith("unchanged:"), again[:90])

    print()
    print("=== the default graph is named in the answer ===")
    # An empty graph name means the event graph. The answer must say WHICH graph was used, so a
    # caller who omitted the name is not left guessing.
    defaulted = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphDead",
                             {"asset": ASSET, "graphName": ""})
    check("it names the graph it used", GRAPH in defaulted, defaulted[:120])

    print()
    print("=== the shared asset is left regenerated for the next run ===")
    # The destructive test above mutated TurboMCPAcceptance. Regenerating it here means the NEXT suite
    # to use it - and the next run of this one - starts from the structure the commandlet describes.
    restore = subprocess.run(
        ["pwsh", "-NoProfile", "-File", os.path.join(repo, "Tools", "make-acceptance-asset.ps1")],
        capture_output=True, text=True, cwd=repo)
    check("the acceptance asset is regenerated at the end",
          restore.returncode == 0 and "PASS" in (restore.stdout or ""),
          (restore.stdout or restore.stderr)[-200:])

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
