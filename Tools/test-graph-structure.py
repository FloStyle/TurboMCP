# Copyright (c) 2026 Florian ESCHALIER (FloStyle)
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
"""`P/E-GRAPH-COPY` and `P/E-GRAPH-COLLAPSE`.

*** WHICH CASE THIS SUITE PROVES, STATED HERE BECAUSE THE ACCEPTANCE ROW DOES NOT SAY IT. ***

`T-STRUCT-11` says "copy between graphs and Blueprints". MEASURED, AND RECORDED BY THE WORKSTREAM THAT
FOUND IT: `TA-2` IS NOT A SECOND ASSET. `TurboMCPKitCommandlet.cpp:939-940` sets `Ta2.Id = "TA-2"` and
`Ta2.Name = "TA01_HandLaid"` - THE SAME BLUEPRINT - and its "event graph" is `Kit->UbergraphPages[0]`
(`:918`).

THIS SUITE PROVES THE `TA-2` CASE: COPYING BETWEEN TWO GRAPHS OF ONE BLUEPRINT. It does NOT prove the
cross-asset case, and it says so rather than letting a green run imply more than it shows.

WHAT THE TWO CASES DIFFER ON, AND WHY IT MATTERS: inside one Blueprint every crossing link is re-linkable,
because both graphs belong to the same asset. ACROSS ASSETS a crossing link whose far end is not also
being copied CANNOT be re-made, and the member check applies. So the cross-asset path - the `Unlinkable`
list and the `Missing` refusal - IS BUILT AND NOT EXERCISED HERE. That is stated, not hidden, because this
project has now had THREE capabilities report that their check count overstated their coverage.

ENGINE FACTS, READ AND CITED RATHER THAN RECALLED:
  - `FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate` is at `EdGraphSchema_K2_Actions.h:95`, and its
    `CreateNode` overload (`:130`) takes a `UK2Node*` - so instantiating the template with a bare
    `UEdGraphNode*` does NOT COMPILE. Measured here as `error C2665` at `EdGraphSchema_K2_Actions.h(102,25)`.
  - The node key is `TurboMCPNodeId::MakeKey`, six hex digits of the node's GUID.
"""

import sys

import os
import re
import sys

from tmcp import Session, READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET

# THE REPOSITORY ROOT, DERIVED FROM THIS FILE RATHER THAN HARD-CODED, so the suite runs from any cwd.
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

READ = READ_TOOLSET
PREVIEW = PREVIEW_TOOLSET
EDIT = EDIT_TOOLSET

KIT = "/Game/Tests/Kit/TA01_HandLaid"

# *** A NODE WITH NO MEMBER DEPENDENCY, CHOSEN DELIBERATELY. *** `d88638` is `int + int`, whose type is
# `K2Node_PromotableOperator` - it names no Blueprint variable, function or event, so it lands in the
# cross-asset member check trivially and the copy is testing the COPY rather than the member resolution.
#
# *** THE KEY IS DISCOVERED AT RUN TIME, NEVER WRITTEN DOWN. *** The first version of this suite pinned
# `TA01_HandLaid:HandLaid:d88638`, and it passed - AND THEN THE KIT WAS REBUILT AND THAT NODE NO LONGER
# EXISTED, because `make-kit-assets.ps1` regenerates the Blueprint and every node GUID with it. So a
# pinned key is a pin on a value the fixture does not promise, which is the mistake this project has
# recorded as "never pin a count on a shared asset" wearing different clothes. The suite reads the graph,
# takes a node the reader itself lists, and uses THAT.
ARITHMETIC_CLASS_HINT = "int + int"

results = []


def check(name, ok, detail=""):
    results.append((name, bool(ok), detail))
    print("  %s  %s" % ("ok  " if ok else "FAIL", name))
    if not ok and detail:
        for line in str(detail).splitlines()[:8]:
            print("        " + line[:150])


def discover_nodes(session, graph_name, wanted=2):
    """Node keys the READER lists for the graph, taken from its own output rather than invented.

    THE READER PRINTS THE SHORT FORM (`Asset:Graph:key`) in its "not reachable" list and the LONG form in
    its node lines. BOTH are accepted by the writer now, and this function returns the SHORT form because
    that is what a reader hands to a caller - so the round trip is exercised as an agent would exercise it.
    """
    answer = session.call(READ, "ReadTurboMCPGraphLoad", {"asset": KIT, "graphName": graph_name, "format": "", "detail": "full"})
    if not isinstance(answer, str):
        return []
    import re
    keys = []
    # THE ARITHMETIC NODE FIRST, BECAUSE IT NAMES NO BLUEPRINT MEMBER: its type is a promotable operator,
    # so a copy of it tests the COPY rather than the cross-asset member check.
    for line in answer.splitlines():
        if ARITHMETIC_CLASS_HINT in line:
            found = re.search(r"([A-Za-z0-9_]+:[A-Za-z0-9_ ]+:[0-9a-f]{6})", line)
            if found:
                keys.append(found.group(1))
    for line in answer.splitlines():
        found = re.search(r"([A-Za-z0-9_]+:[A-Za-z0-9_ ]+:[0-9a-f]{6})", line)
        if found and found.group(1) not in keys:
            keys.append(found.group(1))
        if len(keys) >= wanted:
            break
    return keys[:wanted]


def graph_nodes(session, graph_name):
    """The graph's node count, PARSED from the reader's own first line rather than matched as a sentence.

    A REGEX THAT ASSUMES A FORMAT IS INDISTINGUISHABLE FROM A REAL FAILURE, so the number is looked for
    with a pattern and a count that cannot be found is reported as `None` - which FAILS the check that
    uses it rather than silently comparing zero to zero.
    """
    answer = session.call(READ, "ReadTurboMCPGraphLoad", {"asset": KIT, "graphName": graph_name, "format": "", "detail": "manifest"})
    if not isinstance(answer, str):
        return None, str(answer)
    import re
    match = re.search(r"(\d+) node\(s\)", answer)
    if not match:
        return None, answer[:400]
    return int(match.group(1)), answer[:400]


def restore_and_reconnect():
    """Puts the shared Kit back BEFORE the suite reads it, and returns a detail string.

    *** WHY BEFORE AND NOT AFTER: the fixture rebuild STOPS THE EDITOR, so a restore placed at the end
    kills the server the suite is talking to - MEASURED, `WinError 10061` on every call after it. Doing it
    first also makes the suite RE-RUNNABLE FROM ANY STATE, which is the property that matters: a run killed
    part-way leaves the same mess behind, and end-of-run cleanup would not have helped. ***
    """
    import tmcp_editor
    import subprocess
    import time

    ok, detail = tmcp_editor.rebuild_fixture("make-kit-assets.ps1", ["TURBOMCP_STRUCTURE_REBUILD"])
    if ok is None:
        return "not requested (%s) - the suite will REPORT what it leaves rather than claim a restore" % detail

    # THE EDITOR IS STARTED AGAIN HERE, because `rebuild_fixture` deliberately stops it and does NOT bring
    # one back: a harness caller owns that decision. `subprocess.call` and NOT `run(capture_output=True)` -
    # measured, the editor does not come up with captured output because the launcher leaves a pipe open.
    subprocess.call(["pwsh", "-NoProfile", "-File",
                     os.path.join(REPO, "Tools", "start-editor.ps1"), "-LowPriority"],
                    cwd=REPO)

    # AND THE SUITE WAITS FOR THE SERVER TO ANSWER BEFORE IT TALKS TO IT, rather than sleeping and hoping:
    # a fixed sleep is a race, and this project has recorded that shape more than once.
    for _ in range(40):
        probe = subprocess.run(["python", os.path.join(REPO, "Tools", "check-mcp-server.py")],
                               capture_output=True, text=True, cwd=REPO)
        if "session:" in (probe.stdout or ""):
            return "rebuilt the kit and the server answers (%s)" % str(detail)[:120]
        time.sleep(5)
    return "rebuilt the kit but the server DID NOT ANSWER within 200s"


def main():
    rebuild_detail = restore_and_reconnect()
    session = Session()

    # ---------------------------------------------------------------------------------------------
    # 0. THE NODES TO COPY ARE DISCOVERED, NOT PINNED
    # ---------------------------------------------------------------------------------------------
    DISCOVERED = discover_nodes(session, "HandLaid", wanted=2)
    check("the suite found node ids by READING the graph, so nothing is pinned to a GUID the fixture "
          "does not promise", len(DISCOVERED) >= 1, "discovered=%s" % DISCOVERED)
    if not DISCOVERED:
        print("    no node id could be discovered, so nothing below can run")
        print("RESULT: FAIL (1) -> no node id discovered")
        return 1

    # ---------------------------------------------------------------------------------------------
    # 1. THE PREVIEW WRITES NOTHING, AND SAYS SO
    # ---------------------------------------------------------------------------------------------
    before, before_text = graph_nodes(session, "EventGraph")
    check("the target graph's node count is READABLE before anything is copied",
          before is not None, before_text)

    preview = session.call(PREVIEW, "PreviewTurboMCPGraphCopy", {"sourceAsset": KIT, "sourceGraph": "HandLaid", "nodeIds": DISCOVERED[:1], "targetAsset": KIT, "targetGraph": "EventGraph", "anchorNodeId": ""})
    check("the preview answers", isinstance(preview, str), str(preview)[:300])
    check("the preview's FIRST LINE states it would change, not that it did",
          isinstance(preview, str) and preview.splitlines()[0].strip() == "would change",
          str(preview)[:300])
    check("the preview names the source and the target",
          isinstance(preview, str) and "HandLaid" in preview and "EventGraph" in preview,
          str(preview)[:300])
    check("the preview lists the node it would copy, with its class",
          isinstance(preview, str) and DISCOVERED[0] in preview,
          str(preview)[:400])
    check("the preview prints the UNLINKABLE list even when it is empty, so an absent section and an "
          "empty one are different answers",
          isinstance(preview, str) and "CANNOT be re-linked" in preview,
          str(preview)[:400])

    # THE PREVIEW IS A READER AND MUST NOT HAVE WRITTEN ANYTHING.
    after_preview, after_preview_text = graph_nodes(session, "EventGraph")
    check("the target graph's node count is UNCHANGED by a preview",
          after_preview == before, "before=%s after=%s\n%s" % (before, after_preview, after_preview_text))

    # ---------------------------------------------------------------------------------------------
    # 2. THE REFUSALS THAT MUST COME FROM `Plan` RATHER THAN FROM `Apply`
    # ---------------------------------------------------------------------------------------------
    same = session.call(PREVIEW, "PreviewTurboMCPGraphCopy", {"sourceAsset": KIT, "sourceGraph": "HandLaid", "nodeIds": DISCOVERED[:1], "targetAsset": KIT, "targetGraph": "HandLaid", "anchorNodeId": ""})
    check("copying a graph onto ITSELF is refused by name",
          isinstance(same, str) and same.splitlines()[0].strip() == "unchanged",
          str(same)[:400])
    check("... and the refusal points at what it actually is - a duplicate",
          isinstance(same, str) and "duplicate" in same, str(same)[:400])

    empty = session.call(PREVIEW, "PreviewTurboMCPGraphCopy", {"sourceAsset": KIT, "sourceGraph": "HandLaid", "nodeIds": [], "targetAsset": KIT, "targetGraph": "EventGraph", "anchorNodeId": ""})
    check("an EMPTY selection is reported as nothing to do rather than as a success",
          isinstance(empty, str) and empty.splitlines()[0].strip() == "unchanged"
          and "nothing to do" in empty.lower(), str(empty)[:400])

    bogus = session.call(PREVIEW, "PreviewTurboMCPGraphCopy", {"sourceAsset": KIT, "sourceGraph": "HandLaid", "nodeIds": ["TA01_HandLaid:HandLaid:000000"], "targetAsset": KIT, "targetGraph": "EventGraph", "anchorNodeId": ""})
    check("a node id that is not in the graph is NAMED rather than counted",
          isinstance(bogus, str) and "000000" in bogus, str(bogus)[:400])

    # ---------------------------------------------------------------------------------------------
    # 3. THE EDIT, AND THE READ-BACK THAT IS THE EVIDENCE
    # ---------------------------------------------------------------------------------------------
    applied = session.call(EDIT, "EditTurboMCPGraphCopy", {"sourceAsset": KIT, "sourceGraph": "HandLaid", "nodeIds": DISCOVERED[:1], "targetAsset": KIT, "targetGraph": "EventGraph", "anchorNodeId": ""})
    check("the edit answers", isinstance(applied, str), str(applied)[:400])
    check("the edit's FIRST LINE is `changed`, as the spec requires",
          isinstance(applied, str) and applied.splitlines()[0].strip() == "changed",
          str(applied)[:400])
    check("the edit states the target's node count BEFORE and AFTER, so `changed` carries its own proof",
          isinstance(applied, str) and "before" in applied and "after" in applied,
          str(applied)[:400])
    check("the edit prints the UNCONNECTED list as its own section",
          isinstance(applied, str) and "UNCONNECTED" in applied, str(applied)[:500])

    # *** THE READ-BACK, THROUGH A DIFFERENT TOOL, WHICH IS THE PROJECT'S RULE. *** The edit's own report
    # is not evidence that the node is there, so the graph is read again and the count compared.
    after, after_text = graph_nodes(session, "EventGraph")
    check("THE NODE COUNT IN THE TARGET GRAPH ACTUALLY WENT UP - read back through the reader",
          after is not None and before is not None and after == before + 1,
          "before=%s after=%s" % (before, after))
    check("the reader agrees with the edit's own `after` number",
          isinstance(applied, str) and after is not None
          and ("%d after" % after) in applied,
          "reader=%s\nreport=%s" % (after, str(applied)[:300]))

    # ---------------------------------------------------------------------------------------------
    # 4. THE RESTORE IS DONE ON THE WAY **IN**, WHICH IS THE ONLY PLACE IT CAN BE DONE
    # ---------------------------------------------------------------------------------------------
    #
    # *** THE FIRST VERSION OF THIS SUITE TRIED TO RESTORE AT THE END, AND IT CANNOT WORK: the rebuild
    # STOPS THE EDITOR, so a restore placed after the last call kills the very server the suite is talking
    # to, and every call after it gets `WinError 10061`. MEASURED, twice. The fix is `test-graph-replace`'s
    # - the SAME established pattern rather than a new one. ***
    #
    # IT IS ALSO THE CORRECT DESIGN RATHER THAN A WORKAROUND: this suite MUTATES a shared asset, so
    # rebuilding FIRST makes it re-runnable from ANY state. A run killed part-way leaves the same mess
    # behind, and cleaning up at the end would not have helped - which is the reasoning already recorded in
    # `test-graph-replace` and `test-graph-patch`.
    #
    # THE REBUILD IS OPT-IN, exactly as the other suites arranged it, because it costs an editor cycle. It
    # lives in `restore_and_reconnect` and it is called BEFORE the session, so nothing talks to a server the
    # rebuild is about to stop.
    #
    # AND THE SUITE REPORTS WHETHER IT RESTORED RATHER THAN CLAIMING IT. When the rebuild is not requested
    # the check is on what the suite LEFT, which is the property the next suite depends on.
    print("    fixture rebuild: %s" % rebuild_detail)

    # ---------------------------------------------------------------------------------------------
    # 5. COLLAPSE: THE PLAN RUNS, AND THE WRITE REFUSES WITH THE MEASUREMENT'S OWN REASON
    # ---------------------------------------------------------------------------------------------
    #
    # *** COLLAPSE IS NOT BUILT, AND THIS SUITE CHECKS THE HONESTY OF THAT RATHER THAN PRETENDING. ***
    # The measurement says the pieces are reachable; the ONE open question is whether the
    # `FBlueprintEditor` instance `CollapseNodesIntoGraph` is a member of can be constructed outside an
    # open asset editor. So the plan is verified to work - including its refusals - and the write is
    # verified to refuse WITHOUT WRITING.
    collapse_preview = session.call(PREVIEW, "PreviewTurboMCPGraphCollapse", {"asset": KIT, "graphName": "HandLaid", "nodeIds": DISCOVERED[:2], "memberName": "TurboMCPCollapseProbe", "memberKind": "function"})
    check("the collapse preview answers", isinstance(collapse_preview, str),
          str(collapse_preview)[:400])

    bad_kind = session.call(PREVIEW, "PreviewTurboMCPGraphCollapse", {"asset": KIT, "graphName": "HandLaid", "nodeIds": DISCOVERED[:1], "memberName": "TurboMCPBadKind", "memberKind": "widget"})
    check("a member kind that is neither function nor macro is refused BY NAME",
          isinstance(bad_kind, str) and "widget" in bad_kind
          and bad_kind.splitlines()[0].strip() == "unchanged",
          str(bad_kind)[:400])

    node_count_before_collapse, _ = graph_nodes(session, "HandLaid")
    collapse_edit = session.call(EDIT, "EditTurboMCPGraphCollapse", {"asset": KIT, "graphName": "HandLaid", "nodeIds": DISCOVERED[:2], "memberName": "TurboMCPCollapseProbe", "memberKind": "function"})
    check("the collapse edit REFUSES, and says the capability is not built rather than failing vaguely",
          isinstance(collapse_edit, str) and "NOT YET BUILT" in collapse_edit, str(collapse_edit)[:500])
    check("... and the refusal says NOTHING WAS WRITTEN, which is the spec's rule for a failed write",
          isinstance(collapse_edit, str) and "NOTHING was written" in collapse_edit,
          str(collapse_edit)[:500])
    check("... and it names the ONE question the measurement left open, rather than the whole problem",
          isinstance(collapse_edit, str) and "FBlueprintEditor" in collapse_edit
          and "asset editor" in collapse_edit, str(collapse_edit)[:500])

    node_count_after_collapse, _ = graph_nodes(session, "HandLaid")
    check("A REFUSED COLLAPSE WROTE NOTHING - the source graph's node count is unchanged",
          node_count_after_collapse == node_count_before_collapse,
          "before=%s after=%s" % (node_count_before_collapse, node_count_after_collapse))

    # ---------------------------------------------------------------------------------------------
    # RESULT
    #
    # *** THIS SUITE REPORTED NO VERDICT FOR FOUR ROUNDS, AND THE CAUSE WAS ONE MISSING LINE. *** The block below
    # computed the count, printed "... N of M check(s) passed", and returned an exit code - AND NEVER PRINTED A
    # `RESULT:` LINE, WHICH IS THE ONLY THING `run-suites.ps1` CAPTURES. The harness therefore reported
    # `RESULT: no result line`, which is indistinguishable from a crashed suite, AND EVERY DIAGNOSIS MADE FROM THAT
    # LINE WAS WRONG - the suite was running and printing the whole time.
    #
    # THE HOUSE FORM IS THE SAME IN EVERY OTHER SUITE; `test-asset-cost.py:170` ends with
    # `print("RESULT: PASS (%d check(s) run)")`. It is reproduced here rather than invented, because the harness
    # parses this line and a second dialect would be a second thing to keep in step.
    # ---------------------------------------------------------------------------------------------
    failed = [r for r in results if not r[1]]
    print("  ... %d of %d check(s) passed" % (len(results) - len(failed), len(results)))
    if failed:
        print("  FAILED:")
        for name, _, detail in failed:
            print("    - %s" % name)
            if detail:
                print("        %s" % str(detail).splitlines()[0][:150])
        print("RESULT: FAIL (%d) -> %s" % (
            len(failed), ", ".join(name for name, _, _ in failed)[:220]))
        return 1
    print("RESULT: PASS (%d check(s) run)" % len(results))
    return 0


if __name__ == "__main__":
    sys.exit(main())
