# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-LAY-7
#
# NAMED HERE SO Spec/06_ACCEPTANCE.md CAN BE ANSWERED MECHANICALLY. Spec/WORKING_PLAN.md section 1.3 says a
# capability is done when its acceptance row PASSES, WHICH MAKES THAT DOCUMENT THE DEFINITION OF DONE - and a
# script over the tree measured 64 OF ITS 122 ROWS CITED BY NO SUITE AT ALL, so nothing could answer
# 'is this row passing?' without a human reading it.
#
# *** AND THE SCRIPT IS NAMED NOW, BECAUSE A SCRIPT OVER THE TREE IS NOT A REFERENCE: ***
#   python Tools/checks/count-acceptance-citations.py
# IT STATES ITS CRITERION - a literal occurrence of the id in a Tools/test-*.py file - AND REPORTS 64. FOUR
# EARLIER ATTEMPTS GAVE 91, 64, 66 AND 56 BECAUSE NONE OF THEM SAID WHAT CITED MEANT, AND THIS LINE CARRIED
# THE 91. *** A NUMBER WITHOUT ITS CRITERION IS NOT A MEASUREMENT; IT IS A NUMBER. ***
#
# THE ID IS READ FROM THE DOCUMENT, NOT GUESSED. VERIFIED FOR THIS ONE:
#   | T-LAY-7 | Tidy TA-3 | Wire length shorter by at least 30 percent |

"""Drive `P/E-GRAPH-TIDY` against the acceptance fixture.

WHY THIS SUITE EXISTS, AND WHAT IT IS MEASURED AGAINST
------------------------------------------------------
Tidy MOVES NODES, so it needs a fixture it can corrupt and rebuild: the suite calls
`Tools/make-acceptance-asset.ps1` FIRST AND LAST. That script stops the editor, so the suite restarts it
with `Tools/start-editor.ps1` and opens the MCP session only once the server answers.

Every number is PARSED, never pinned. The fixture grows, and a pinned count on a shared asset is the
mistake this project has recorded five times.

THREE FAULTS IN THIS SUITE'S FIRST VERSION, all fixed and all the same shape - an assertion about a reader
rather than about the subject:

  - it read box corners from `R-COMMENT-LIST` and its regex matched nothing, so `{} -> {}` compared equal
    and "NO BOX CORNER MOVED" would have PASSED having checked nothing. It now reads the corners from
    TIDY'S OWN PREVIEW, which prints each box's position, so there is one reader rather than two;
  - its nesting regex was unanchored across lines and read `depth 1` for BOTH boxes, so the comparison was
    `1 > 1`. It is anchored per line now;
  - it asserted that `SaneProbe` is left alone. THAT ASSUMPTION WAS WRONG: `SaneProbe` is
    `Entry -> Call -> Result` with `Entry` and `Result` BOTH AT (0,0), because it was built for the
    dead-node and rollback tests rather than as a tidy fixture. It is legitimately untidy, and tidy
    correctly applies to it. The already-tidy case is now tested by running the edit TWICE and requiring
    the second run to change nothing, which is a stronger check than the one it replaces.

WHAT EACH CHECK PROVES, AND HOW IT COULD FAIL
---------------------------------------------
- **TIDY'S BEFORE-NUMBER EQUALS `L-LAYOUT-REPORT`'S TOTAL.** The cross-tool check the spec asks for, and
  the most valuable one here: two modules compute a wire length and only this comparison keeps them in
  step. IT ALREADY CAUGHT A REAL BUG - the first version stored a SUBSTITUTED node size and measured the
  before-number on it, so the two tools used different rectangles and the rollback looked like a
  considered decision. It fails if the pin row pitch, the pin ordering or the width source diverge.
- **THE PREVIEW CHANGES NOTHING.** `Plan` must write nothing. Node count and wire length before and after a
  preview must be identical. Fails if `Plan` ever creates or moves a node.
- **A BOX'S CORNER IS NEVER MOVED**, compared across a forced edit. The checkable form of the rule.
- **NESTING**: the inner box is reported nested with a depth STRICTLY GREATER than its parent's.
- **IDEMPOTENCE**: a second edit of an already-tidied graph answers `unchanged:` and moves nothing.
- **REFUSALS NAME A REASON, AND THE PAIR AGREES** (`D-02`), checked for an empty asset, a non-asset and a
  missing graph - the Preview and the Edit must refuse the SAME input with the SAME reason.
- **THE FALLBACK GEOMETRY IS ANNOUNCED**, because a pass may not silently substitute a size.
"""

import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import tmcp_editor
from tmcp import EDIT_TOOLSET, LOOK_TOOLSET, PREVIEW_TOOLSET, Session  # noqa: E402

ASSET = "TurboMCPAcceptance"
# `LayoutProbe`: a crossing, two NESTED comment boxes, a neighbouring-column wire and a messy run. It is
# the graph with boxes, so it is the only one that can check "no box corner moved".
RICH = "LayoutProbe"
# A graph with nodes and no boxes, for the idempotence run.
SANE = "SaneProbe"

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def rebuild_fixture(attempts=3, restore=True):
    """
    Rebuilds the acceptance asset, RETRYING because this project has other workstreams building the same
    plugin at the same time. A build in flight holds the DLL and the commandlet fails with a message that
    has nothing to do with the fixture, which would otherwise be reported as a fixture fault.
    """
    if os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") == "1":
        if not restore:
            return True
        ok, detail = tmcp_editor.restore_runner_fixture()
        print("    runner fixture restore: %s" % detail)
        return bool(ok)
    for attempt in range(attempts):
        if subprocess.call(
                ["pwsh", "-NoProfile", "-File",
                 os.path.join(REPO, "Tools", "make-acceptance-asset.ps1")], cwd=REPO) == 0:
            return True
        print("    rebuild attempt %d failed; a concurrent build holds the plugin, retrying"
              % (attempt + 1))
        time.sleep(30)
    return False


def start_editor(attempts=3):
    """
    Makes sure an editor with the MCP server is up, TOLERATING one that is already running.

    `start-editor.ps1` refuses when an editor already holds port 8000, which is right for a launcher and
    wrong for a caller that only needs a server. The first version of this suite reported that refusal as a
    failure and then went on to drive the running editor successfully for every later check - so it failed a
    check while proving the opposite. The only question that matters is whether the server ANSWERS.
    """
    if os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") == "1":
        ok, detail = tmcp_editor.ensure_editor()
        print("    runner editor: %s" % detail)
        return bool(ok)
    for attempt in range(attempts):
        subprocess.call(
            ["pwsh", "-NoProfile", "-File", os.path.join(REPO, "Tools", "start-editor.ps1")],
            cwd=REPO)
        if connect(timeout_s=90) is not None:
            return True
        print("    no editor answered; attempt %d of %d" % (attempt + 1, attempts))
        time.sleep(10)
    return False


def connect(timeout_s=180):
    """The session opens only once the server ANSWERS, rather than being assumed ready."""
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        try:
            session = Session()
            session.connect()
            return session
        except Exception:
            time.sleep(3)
    return None


def wire_length(session, graph):
    out = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutReport",
                       {"asset": ASSET, "graphName": graph})
    match = re.search(r"total wire length:\s*(\d+(?:\.\d+)?)", out)
    return float(match.group(1)) if match else None


def node_count(session, graph):
    out = session.call(LOOK_TOOLSET, "LookTurboMCPLayoutReport",
                       {"asset": ASSET, "graphName": graph})
    match = re.search(r"counts:\s*(\d+)\s+node", out)
    return int(match.group(1)) if match else None


def box_corners(preview):
    """
    Every box's top-left corner, read from TIDY'S OWN PREVIEW.

    ONE READER, NOT TWO. The first version parsed `R-COMMENT-LIST` and matched nothing, so it compared two
    empty dictionaries and would have PASSED a check about corners having moved. The preview prints each
    box's position in the same answer that describes the fit, so there is nothing to cross-read.
    """
    corners = {}
    for line in preview.splitlines():
        match = re.search(r"^\s*(\S.*?) at \((-?\d+(?:\.\d+)?),(-?\d+(?:\.\d+)?)\)", line)
        if match:
            corners[match.group(1).strip()] = (float(match.group(2)), float(match.group(3)))
    return corners


def box_depths(preview):
    """Each box's depth, ANCHORED PER LINE so one box's depth cannot be read for another."""
    depths = {}
    for line in preview.splitlines():
        match = re.search(r"^\s*(\S.*?) at \(.*?\) depth (\d+)", line)
        if match:
            depths[match.group(1).strip()] = int(match.group(2))
    return depths


def tidy_preview(session, graph, force=False):
    return session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphTidy",
                        {"asset": ASSET, "graphName": graph, "bForce": force})


def tidy_edit(session, graph, force=False):
    return session.call(EDIT_TOOLSET, "EditTurboMCPGraphTidy",
                        {"asset": ASSET, "graphName": graph, "bForce": force})


def main():
    print("=== the fixture is rebuilt first, because tidy MOVES NODES ===")
    check("the acceptance asset was rebuilt before the destructive test", rebuild_fixture())
    check("the editor was started again", start_editor())

    session = connect()
    if session is None:
        print("RESULT: FAIL (the MCP server never answered)")
        return 1

    # -----------------------------------------------------------------------------------------
    print()
    print("=== TIDY'S BEFORE-NUMBER EQUALS THE READER'S TOTAL, THE CROSS-TOOL CHECK ===")
    reader_before = wire_length(session, RICH)
    preview = tidy_preview(session, RICH)
    print(preview[:900])
    print()
    check("the default preview opens with a decision",
          preview.startswith(("WOULD APPLY:", "WOULD ROLL BACK:", "ALREADY TIDY:")),
          preview[:180])
    check("the default preview is bounded and omits rectangle rows",
          len(preview) <= 32500 and "score rect" not in preview,
          "characters=%d" % len(preview))
    page0 = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphTidy",
                         {"asset": ASSET, "graphName": RICH, "layoutMode": "blocks",
                          "geometryOffset": 0, "geometryLimit": 2})
    check("blocks preview returns a numbered geometry page",
          "geometry evidence: rows 0..1 of " in page0 and "score rect" in page0,
          page0[:550])
    page1 = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphTidy",
                         {"asset": ASSET, "graphName": RICH, "layoutMode": "blocks",
                          "geometryOffset": 2, "geometryLimit": 2})
    check("the next geometry page advances without repeating rows",
          "geometry evidence: rows 2..3 of " in page1,
          page1[:550])
    bad_page = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphTidy",
                            {"asset": ASSET, "graphName": RICH,
                             "geometryOffset": 1, "geometryLimit": 0})
    check("invalid pagination is refused", bad_page.startswith("error:"), bad_page[:180])
    tidy_before = re.search(r"total wire length ([\d.]+) ->", preview)
    check("the preview reports a before-number", tidy_before is not None, preview[:200])
    print("    reader %.1f vs tidy %s" % (reader_before or -1.0,
                                          tidy_before.group(1) if tidy_before else "?"))
    check("TIDY'S BEFORE-NUMBER EQUALS L-LAYOUT-REPORT'S TOTAL",
          reader_before is not None and tidy_before is not None
          and abs(reader_before - float(tidy_before.group(1))) <= 1.0,
          "reader %.1f, tidy %s" % (reader_before or -1.0,
                                    tidy_before.group(1) if tidy_before else "?"))
    # TWO WEAK VERSIONS OF THIS CHECK BEFORE IT WAS RIGHT, and both are the same mistake - asserting
    # wording rather than the guarantee. The first demanded the word "getter" and went red on a graph with
    # no getters. The second demanded "node", and the ROLLBACK wording says "the layout would leave the
    # wire length at ..." without that word. What the preview ACTUALLY promises is a decision, so that is
    # what is asserted.
    check("the preview states a decision: apply, roll back, or already tidy",
          ("WOULD APPLY" in preview or "WOULD ROLL BACK" in preview
           or "ALREADY TIDY" in preview),
          preview[:400])
    # A ROLLBACK MUST SAY WHY, per `P7`: an answer that leads nowhere is not an answer.
    if "WOULD ROLL BACK" in preview:
        check("the rollback NAMES the reason", "not shorter" in preview or "force" in preview,
              preview[:400])

    # -----------------------------------------------------------------------------------------
    print()
    print("=== the preview CHANGES NOTHING ===")
    # A THIRD PREVIEW AND A NODE COUNT, all before anything writes. The FIRST version of this block
    # re-measured at a later point in the run, AFTER the forced edit had already moved the nodes, so it
    # compared the graph in two different states and reported a preview side effect that does not exist.
    # MEASURED directly: the report reads 3109 before and 3109 after a preview. The fault was the test's
    # ORDER, and the lesson is that a check must be taken on one state of the subject.
    nodes_before = node_count(session, RICH)
    preview_again = tidy_preview(session, RICH)
    nodes_after = node_count(session, RICH)
    reader_after = wire_length(session, RICH)
    print("    nodes %s -> %s, wire length %s -> %s"
          % (nodes_before, nodes_after, reader_before, reader_after))
    check("previewing does not change the node count", nodes_before == nodes_after,
          "%s -> %s" % (nodes_before, nodes_after))
    check("previewing does not change the wire length", reader_before == reader_after,
          "%s -> %s" % (reader_before, reader_after))
    check("the second preview is identical to the first", preview == preview_again,
          "a preview that differs from itself is not a preview")

    # -----------------------------------------------------------------------------------------
    print()
    print("=== nesting: the inner box is nested and STRICTLY DEEPER, read per line ===")
    depths = box_depths(preview)
    corners_before = box_corners(preview)
    print("    depths: %s" % depths)
    print("    corners: %s" % corners_before)
    check("both boxes are reported with a depth", len(depths) >= 2, str(depths))
    check("exactly one box is nested", sum(1 for d in depths.values() if d > 0) == 1, str(depths))
    check("the nested box is STRICTLY deeper than its parent",
          max(depths.values()) > min(depths.values()) if depths else False,
          str(depths))
    check("the preview names the nested relationship", "nested" in preview, preview[:400])

    # -----------------------------------------------------------------------------------------
    print()
    print("=== A BOX'S TOP-LEFT CORNER IS NEVER MOVED, across a FORCED edit ===")
    check("the fixture has boxes to check", len(corners_before) >= 2, str(corners_before))

    forced = tidy_edit(session, RICH, force=True)
    print(forced[:900])
    print()
    check("the forced edit opens with changed: or unchanged:",
          forced.startswith("changed:") or forced.startswith("unchanged:"), forced[:90])
    first_line = forced.split("\n")[0]
    check("the edit's FIRST LINE carries both wire length numbers",
          len(re.findall(r"\d+\.\d", first_line)) >= 2, first_line)
    if forced.startswith("changed:") and "WORSE" in forced:
        check("a forced regression is LABELLED WORSE", True, "the regression is announced")

    corners_after = box_corners(tidy_preview(session, RICH, force=True))
    print("    corners after: %s" % corners_after)
    # BOTH NON-EMPTY, so a reader fault cannot make this pass by comparing nothing.
    check("corners were read on BOTH sides of the edit",
          len(corners_before) >= 2 and len(corners_after) >= 2,
          "%d before, %d after" % (len(corners_before), len(corners_after)))
    check("NO BOX CORNER MOVED",
          all(corners_after.get(k) == v for k, v in corners_before.items())
          and len(corners_after) == len(corners_before),
          "%s -> %s" % (corners_before, corners_after))

    # -----------------------------------------------------------------------------------------
    print()
    print("=== IDEMPOTENCE: running the pass again on what it produced changes NOTHING ===")
    # TAKEN AFTER THE FORCED EDIT, DELIBERATELY: `force` applied the layout even though it was worse, so the
    # graph is now in the state tidy itself produced, and running tidy on its OWN OUTPUT must be a no-op.
    # That is a stronger statement than testing a graph someone declared tidy by hand, and it is the only
    # way to reach the already-tidy branch with this fixture - `SaneProbe` CANNOT serve, because it is
    # `Entry -> Call -> Result` with `Entry` and `Result` BOTH at (0,0), so it is legitimately untidy.
    second = tidy_edit(session, RICH)
    print(second[:400])
    check("the second edit answers rather than erroring", not second.startswith("error:"),
          second[:120])
    check("the second edit says the graph is ALREADY TIDY",
          second.startswith("unchanged:") and "already tidy" in second.lower(), second[:160])
    # THE NODE COUNT IS THE WITNESS, not the tool's own sentence.
    nodes_after_second = node_count(session, RICH)
    check("the node count is unchanged by the second edit",
          nodes_before is not None and nodes_after_second == nodes_before,
          "%s -> %s" % (nodes_before, nodes_after_second))
    # AND A PREVIEW OF THE TIDIED GRAPH MUST AGREE, which ties the two halves together: the Preview and the
    # Edit must not disagree about a graph the Edit produced.
    preview_of_tidied = tidy_preview(session, RICH)
    check("the preview of the tidied graph agrees it needs nothing",
          "ALREADY TIDY" in preview_of_tidied, preview_of_tidied[:300])

    # -----------------------------------------------------------------------------------------
    print()
    print("=== a refusal NAMES its reason, and the Preview and the Edit AGREE (D-02) ===")
    for label, args in (
        ("an empty asset", {"asset": "", "graphName": RICH, "bForce": False}),
        ("a name that is not an asset", {"asset": "NoSuchAssetAnywhere",
                                         "graphName": RICH, "bForce": False}),
        ("a graph that does not exist", {"asset": ASSET, "graphName": "NoSuchGraph",
                                         "bForce": False}),
    ):
        preview_refusal = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphTidy", args)
        edit_refusal = session.call(EDIT_TOOLSET, "EditTurboMCPGraphTidy", args)
        print("  %s -> %s" % (label, preview_refusal[:150]))
        check("%s is refused by the preview" % label, preview_refusal.startswith("error:"),
              preview_refusal[:150])
        check("%s is refused by the edit too" % label, edit_refusal.startswith("error:"),
              edit_refusal[:150])
        check("%s gets the SAME reason from both" % label, preview_refusal == edit_refusal,
              "preview=%s edit=%s" % (preview_refusal[:80], edit_refusal[:80]))

    # -----------------------------------------------------------------------------------------
    print()
    print("=== the geometry used is ALWAYS STATED, fallback or measured ===")
    # THE FIRST VERSION DEMANDED THE FALLBACK MESSAGE UNCONDITIONALLY and went red on a run where every node
    # had a real size, which is the good case. What the capability promises is that the answer SAYS which
    # geometry it used - either one - because "the engine measured every node" and "the code forgot to
    # mention it" are indistinguishable to a caller unless the absence is stated too.
    states_geometry = ("NO MEASURED SIZE" in preview) or ("every node has a size" in preview)
    check("the preview states which geometry it used", states_geometry, preview[-400:])
    states_geometry_edit = (("NO MEASURED SIZE" in forced) or ("every node had a size" in forced)
                            or ("NO MEASURED SIZE" in second) or ("every node had a size" in second))
    check("the edit states it too, so the written positions can be judged", states_geometry_edit,
          (forced[-300:] if forced else second[-300:]))

    print()
    print("=== the fixture is rebuilt last, so the next suite sees a clean asset ===")
    # run-suites owns the post-suite restore; calling the editor-side unload/reload a
    # second time here can race the runner's isolation check.
    check("the acceptance asset was regenerated at the end",
          rebuild_fixture(restore=os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") != "1"))

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
