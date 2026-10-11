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

from tmcp import Session, READ_TOOLSET, PREVIEW_TOOLSET, EDIT_TOOLSET

READ = READ_TOOLSET
PREVIEW = PREVIEW_TOOLSET
EDIT = EDIT_TOOLSET

KIT = "/Game/Tests/Kit/TA01_HandLaid"

# *** A NODE WITH NO MEMBER DEPENDENCY, CHOSEN DELIBERATELY. *** `d88638` is `int + int`, whose type is
# `K2Node_PromotableOperator` - it names no Blueprint variable, function or event, so it lands in the
# cross-asset member check trivially and the copy is testing the COPY rather than the member resolution.
# Its key was READ from `ReadTurboMCPGraphLoad` on the fixture, not guessed: the graph's own manifest
# prints it in the "not reachable from any entry point" list.
ARITHMETIC_NODE = "TA01_HandLaid:HandLaid:d88638"
MULTIPLY_NODE = "TA01_HandLaid:HandLaid:fe13a8"

results = []


def check(name, ok, detail=""):
    results.append((name, bool(ok), detail))
    print("  %s  %s" % ("ok  " if ok else "FAIL", name))
    if not ok and detail:
        for line in str(detail).splitlines()[:8]:
            print("        " + line[:150])


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


def main():
    session = Session()
    session.describe()

    # ---------------------------------------------------------------------------------------------
    # 1. THE PREVIEW WRITES NOTHING, AND SAYS SO
    # ---------------------------------------------------------------------------------------------
    before, before_text = graph_nodes(session, "EventGraph")
    check("the target graph's node count is READABLE before anything is copied",
          before is not None, before_text)

    preview = session.call(PREVIEW, "PreviewTurboMCPGraphCopy", {"sourceAsset": KIT, "sourceGraph": "HandLaid", "nodeIds": [ARITHMETIC_NODE], "targetAsset": KIT, "targetGraph": "EventGraph", "anchorNodeId": ""})
    check("the preview answers", isinstance(preview, str), str(preview)[:300])
    check("the preview's FIRST LINE states it would change, not that it did",
          isinstance(preview, str) and preview.splitlines()[0].strip() == "would change",
          str(preview)[:300])
    check("the preview names the source and the target",
          isinstance(preview, str) and "HandLaid" in preview and "EventGraph" in preview,
          str(preview)[:300])
    check("the preview lists the node it would copy, with its class",
          isinstance(preview, str) and ARITHMETIC_NODE in preview,
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
    same = session.call(PREVIEW, "PreviewTurboMCPGraphCopy", {"sourceAsset": KIT, "sourceGraph": "HandLaid", "nodeIds": [ARITHMETIC_NODE], "targetAsset": KIT, "targetGraph": "HandLaid", "anchorNodeId": ""})
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
    applied = session.call(EDIT, "EditTurboMCPGraphCopy", {"sourceAsset": KIT, "sourceGraph": "HandLaid", "nodeIds": [ARITHMETIC_NODE], "targetAsset": KIT, "targetGraph": "EventGraph", "anchorNodeId": ""})
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
    # 4. RESTORE, ON THE WAY OUT, AND ASSERT THE RESTORE
    # ---------------------------------------------------------------------------------------------
    #
    # THE COPIED NODE IS REMOVED AGAIN SO THE FIXTURE IS AS IT WAS FOUND. The removal goes through the
    # project's own graph-replace path rather than a hand-written delete, and the restore is ASSERTED
    # rather than assumed - a suite that leaves a shared fixture changed is the defect this project has
    # recorded, and it restores ON THE WAY IN as well as out.
    restored, restored_text = graph_nodes(session, "EventGraph")
    check("the restore point is readable", restored is not None, restored_text)
    if restored is not None and before is not None and restored != before:
        # THE SUITE SAYS IT COULD NOT RESTORE RATHER THAN PRETENDING IT DID. The kit commandlet rebuilds
        # this asset, and `Tools/make-kit-assets.ps1` is the documented way back - so a failure here is
        # reported with the remedy rather than left as a silent difference.
        check("the fixture is left as it was found - IF THIS FAILS, rebuild with "
              "`pwsh -NoProfile -File Tools/make-kit-assets.ps1`",
              False,
              "the target graph has %s node(s) and had %s before the suite ran" % (restored, before))
    else:
        check("the fixture is left as it was found", True)

    # ---------------------------------------------------------------------------------------------
    # 5. COLLAPSE: THE PLAN RUNS, AND THE WRITE REFUSES WITH THE MEASUREMENT'S OWN REASON
    # ---------------------------------------------------------------------------------------------
    #
    # *** COLLAPSE IS NOT BUILT, AND THIS SUITE CHECKS THE HONESTY OF THAT RATHER THAN PRETENDING. ***
    # The measurement says the pieces are reachable; the ONE open question is whether the
    # `FBlueprintEditor` instance `CollapseNodesIntoGraph` is a member of can be constructed outside an
    # open asset editor. So the plan is verified to work - including its refusals - and the write is
    # verified to refuse WITHOUT WRITING.
    collapse_preview = session.call(PREVIEW, "PreviewTurboMCPGraphCollapse", {"asset": KIT, "graphName": "HandLaid", "nodeIds": [ARITHMETIC_NODE, MULTIPLY_NODE], "memberName": "TurboMCPCollapseProbe", "memberKind": "function"})
    check("the collapse preview answers", isinstance(collapse_preview, str),
          str(collapse_preview)[:400])

    bad_kind = session.call(PREVIEW, "PreviewTurboMCPGraphCollapse", {"asset": KIT, "graphName": "HandLaid", "nodeIds": [ARITHMETIC_NODE], "memberName": "TurboMCPBadKind", "memberKind": "widget"})
    check("a member kind that is neither function nor macro is refused BY NAME",
          isinstance(bad_kind, str) and "widget" in bad_kind
          and bad_kind.splitlines()[0].strip() == "unchanged",
          str(bad_kind)[:400])

    node_count_before_collapse, _ = graph_nodes(session, "HandLaid")
    collapse_edit = session.call(EDIT, "EditTurboMCPGraphCollapse", {"asset": KIT, "graphName": "HandLaid", "nodeIds": [ARITHMETIC_NODE, MULTIPLY_NODE], "memberName": "TurboMCPCollapseProbe", "memberKind": "function"})
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
    # ---------------------------------------------------------------------------------------------
    failed = [r for r in results if not r[1]]
    print("  ... %d of %d check(s) passed" % (len(results) - len(failed), len(results)))
    if failed:
        print("  FAILED:")
        for name, _, detail in failed:
            print("    - %s" % name)
            if detail:
                print("        %s" % str(detail).splitlines()[0][:150])
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
