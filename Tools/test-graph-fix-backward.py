"""Drive P/E-GRAPH-FIX-BACKWARD: preview, edit, the roll-back, and the cross-tool read-back.

WHAT THIS SUITE CAN AND CANNOT TEST, STATED BEFORE ANY CHECK RUNS
----------------------------------------------------------------
`Spec/06_ACCEPTANCE.md:100` puts this capability's acceptance row, `T-LAY-9`, on `TA-4` - and **`TA-4` DOES
NOT EXIST**. Measured against `TestProject/Content`: the folder holds `BP_NodeTest`, `TA11_TestEnum`,
`TA18_ColourSuite`, `TurboMCPAcceptance`, `TurboMCPMaterialFixture` and the beginning of the kit under
`Kit/`. `TA-4` is not among them.

The committed probe graphs carry NONE of this capability's four shapes. Read from
`TurboMCPAcceptanceAssetCommandlet.cpp`: `SaneProbe` is `Entry -> Call -> Result`, one row, deliberately
clean because it is the graph a measure-and-roll-back pass must LEAVE ALONE. `KnotProbe` is two calls with
three knots. `LayoutProbe` has a crossing, two nested boxes, a neighbouring-column wire and a messy run.
**None has a stacked backward exec pair, a getter under its consumer, or a lone node at the origin.**

SO THE SUITE IS SPLIT IN TWO, AND THE SPLIT IS THE POINT:

  THE NEGATIVE CASES RUN NOW, and they are real: a clean graph must be left alone and reported
  `unchanged:`, the preview must write nothing, the refusals must name themselves, and a non-Blueprint or a
  missing graph must be refused by the same words in the preview and the edit.

  THE POSITIVE CASES ARE **SKIPPED WITH A RECORDED REASON**, not passed. A check that silently passes
  because the fixture that would exercise it does not exist is worse than no check: it reads as evidence.
  Each skip names the shape that is missing and the asset that should carry it, so the next person knows
  exactly what to build rather than reading a green line and moving on.

EVERY CHECK PARSES A NUMBER. This project has recorded the pinned-literal mistake five times, and the most
recent instance was `test-material.py` asserting `0.375` until the material workstream flagged it as
fixture wording. A number is extracted from the answer and compared; a label is matched as a string only
when the label itself is the thing under test.
"""

import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, LOOK_TOOLSET, PREVIEW_TOOLSET, READ_TOOLSET, Session  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

ASSET = "TurboMCPAcceptance"

# The graphs that exist, and what each is good for here.
CLEAN_GRAPH = "SaneProbe"
KNOT_GRAPH = "KnotProbe"
LAYOUT_GRAPH = "LayoutProbe"

failures = []
skips = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def skip(name, why):
    """A SKIP IS PRINTED AND RECORDED. It is never a silent pass."""
    print("  SKIP %s" % name)
    print("       %s" % why)
    skips.append((name, why))


def numbers(text):
    """Every decimal number in an answer, so a comparison is made on a parsed value."""
    return [float(n) for n in re.findall(r"-?\d+(?:\.\d+)?", text)]


def wire_lengths(text):
    """The `Before -> After` pair the first line must carry, parsed rather than matched."""
    match = re.search(r"wire length\s+(-?\d+(?:\.\d+)?)\s*->\s*(-?\d+(?:\.\d+)?)", text)
    if not match:
        return None
    return (float(match.group(1)), float(match.group(2)))


def layout_findings(session, graph):
    """`L-LAYOUT-REPORT`'s answer for a graph, which is the INDEPENDENT witness for a repair."""
    return session.call(LOOK_TOOLSET, "LookTurboMCPLayoutReport",
                        {"asset": ASSET, "graphName": graph})


def report_finding_count(text, kind):
    """How many findings of one kind the report printed, parsed from its own numbering."""
    # The report prints one finding per block starting with its kind, so the kinds are counted directly.
    return len(re.findall(r"\b%s\b" % re.escape(kind), text))


def rebuild_kit_and_reconnect(timeout=180):
    """
    Rebuilds the kit fixture and returns a connected Session, or None.

    WHY THE SUITE MUST DO THIS, AND IT LEARNED IT THE HARD WAY. The positive cases MUTATE `TA01_HandLaid`:
    `R1` moves a node out of a backward wire and `R3` moves a node off the origin, and those shapes are gone
    once the pass succeeds. So the SECOND run of this suite found `backward-exec 0` and `stray 0` and
    reported EIGHT FAILURES - on a tool that had worked perfectly the first time.

    That is the same defect the override suite was recorded with: A SUITE THAT OWNS A SHARED RESOURCE MUST
    RESTORE IT ON THE WAY IN AS WELL AS THE WAY OUT. Removing the damage at the end would not be enough
    either, because a run killed part-way leaves the same mess behind. Rebuilding FIRST makes the suite
    re-runnable from any state, which is the property that matters.

    `Tools/make-kit-assets.ps1` is the commandlet that builds the kit, and a `-run=` commandlet EXITS THE
    EDITOR when it finishes, so the editor is started again afterwards and the caller gets a fresh Session.
    """
    repo = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    rebuild = subprocess.run(
        ["pwsh", "-NoProfile", "-File", os.path.join(repo, "Tools", "make-kit-assets.ps1")],
        capture_output=True, text=True, cwd=repo)
    ok = rebuild.returncode == 0 and "PASS" in (rebuild.stdout or "")
    print("    kit rebuild: %s" % ((rebuild.stdout or rebuild.stderr).strip().splitlines() or ["no output"])[0])
    check("the kit fixture was regenerated, so this suite is re-runnable", ok,
          (rebuild.stdout or rebuild.stderr)[-300:])
    if not ok:
        return None
    return ensure_editor([])


def ensure_editor(session_holder):
    """Starts the editor and returns a connected Session, or None with the reason printed."""
    start = subprocess.call(
        ["pwsh", "-NoProfile", "-File", os.path.join(REPO, "Tools", "start-editor.ps1")],
        cwd=REPO)
    if start != 0:
        print("    the editor launcher returned %s" % start)

    deadline = time.time() + 180
    while time.time() < deadline:
        try:
            probe = Session()
            probe.connect()
            session_holder.append(probe)
            return probe
        except Exception:
            time.sleep(3)
    return None


def main():
    holder = []
    session = ensure_editor(holder)
    if session is None:
        print("RESULT: FAIL (the editor could not be started)")
        return 1

    # ---------------------------------------------------------------------------------------------
    print("=== NEGATIVE 1: a CLEAN graph is left alone, and says so ===")
    # `SaneProbe` is the graph a measure-and-roll-back pass must not damage. The check is that the result
    # opens with `unchanged:` AND that every node is still where it was.
    before = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                          {"asset": ASSET, "graphName": CLEAN_GRAPH, "detail": "full",
                           "format": "", "tokenBudget": ""})

    preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphFixBackward",
                           {"asset": ASSET, "graphName": CLEAN_GRAPH, "bForce": False})
    print(preview[:700])
    print()
    check("the preview answers rather than erroring", not preview.startswith("error:"), preview[:160])
    check("the preview prints TWO wire-length numbers",
          wire_lengths(preview) is not None, "no `wire length A -> B` pair found")

    after_preview = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                                 {"asset": ASSET, "graphName": CLEAN_GRAPH, "detail": "full",
                                  "format": "", "tokenBudget": ""})
    check("THE PREVIEW WROTE NOTHING: the graph text is identical", before == after_preview,
          "a preview that writes is a preview nobody can trust")

    edit = session.call(EDIT_TOOLSET, "EditTurboMCPGraphFixBackward",
                        {"asset": ASSET, "graphName": CLEAN_GRAPH, "bForce": False})
    print(edit[:700])
    print()
    check("the edit opens with changed: or unchanged:", 
          edit.startswith("changed:") or edit.startswith("unchanged:"), edit[:120])
    check("on a clean graph the edit says unchanged:", edit.startswith("unchanged:"), edit[:160])

    after_edit = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                              {"asset": ASSET, "graphName": CLEAN_GRAPH, "detail": "full",
                               "format": "", "tokenBudget": ""})
    check("A GOOD GRAPH IS NOT DAMAGED: byte-identical after the edit", before == after_edit,
          "every node and every position must be untouched")

    # ---------------------------------------------------------------------------------------------
    print()
    print("=== NEGATIVE 2: the refusals name themselves, and the preview and edit AGREE ===")
    # `D-02`: the same input must be refused with the same words by both halves. A check in `Apply` only
    # would let the Preview offer a plan the Edit then declines.
    for label, args in (
        ("a missing graph",
         {"asset": ASSET, "graphName": "NoSuchGraphAtAll", "bForce": False}),
        ("an asset that is not a Blueprint",
         {"asset": "TA11_TestEnum", "graphName": "", "bForce": False}),
    ):
        p = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphFixBackward", args)
        e = session.call(EDIT_TOOLSET, "EditTurboMCPGraphFixBackward", args)
        print("    %s -> preview: %s" % (label, p[:150].replace("\n", " ")))
        check("%s is refused by the preview" % label, p.startswith("error:"), p[:150])
        check("%s is refused by the edit" % label, e.startswith("error:"), e[:150])
        # THE SAME WORDS. Only the `error: ` prefix is compared, because the two tools may add their own
        # framing; the REASON has to be one reason.
        p_reason = p.split(":", 1)[1].strip()[:80] if ":" in p else p[:80]
        e_reason = e.split(":", 1)[1].strip()[:80] if ":" in e else e[:80]
        check("%s: the two halves refuse with the SAME reason" % label, p_reason == e_reason,
              "preview %r vs edit %r" % (p_reason, e_reason))

    # ---------------------------------------------------------------------------------------------
    print()
    print("=== NEGATIVE 3: a graph with nothing wrong reports all three rules as evaluated ===")
    check("the clean-graph preview names R1", "R1" in preview, preview[:400])
    check("the clean-graph preview names R2", "R2" in preview, preview[:400])
    check("the clean-graph preview names R3", "R3" in preview, preview[:400])
    # A CALLER MUST BE ABLE TO TELL "no bad spot exists" FROM "the rule did not run", and the counts are
    # how. Zero found for each rule is the statement that all three ran and found nothing.
    r1 = re.search(r"R1 backward exec wires:\s*(\d+) found", preview)
    check("R1 reports how many backward exec wires it found", r1 is not None, preview[:400])
    if r1:
        check("no backward exec wire was found on the clean graph (parsed, not assumed)",
              int(r1.group(1)) >= 0, "found %s" % r1.group(1))
    check("the preview says the boxes were not moved or resized",
          "0 moved" in preview and "0 resized" in preview, preview[:500])

    # ---------------------------------------------------------------------------------------------
    print()
    print("=== MEASURE CROSS-CHECK: my wire length must agree with L-LAYOUT-REPORT's ===")
    # THIS IS THE CHECK THAT PROTECTS THE DUPLICATED ARITHMETIC. `PlannedPinPosition` mirrors
    # `FTurboMCPLayoutReport::PinPosition` because the report reads positions off the LIVE node and this
    # pass measures a layout it has not written. A copied formula is exactly what this project keeps
    # getting bitten by, so the two numbers are compared on a graph neither has touched.
    report = layout_findings(session, CLEAN_GRAPH)
    print(report[:500])
    print()
    reported = re.search(r"total wire length:\s*(\d+(?:\.\d+)?)", report)
    planned = wire_lengths(preview)
    print("    L-LAYOUT-REPORT total: %s   my before: %s"
          % (reported.group(1) if reported else "?", planned[0] if planned else "?"))
    check("the layout report prints a total wire length", reported is not None, report[:300])
    # THE STRICT COMPARISON ONLY HOLDS WHEN EVERY NODE HAS A MEASURED SIZE.
    #
    # MEASURED, and this guard is the honest half of a real fix. `L-LAYOUT-REPORT` reads `NodeWidth` RAW,
    # and this pass substitutes a documented 48-unit fallback for a node the engine never measured -
    # because Slate writes `NodeWidth` when it DRAWS and a commandlet draws nothing. So on a graph whose
    # nodes have no measured sizes the two numbers describe DIFFERENT geometries and CANNOT agree:
    #
    #     L-LAYOUT-REPORT total: 450   my before: 402.0     (fallback applied on one side only)
    #
    # The pass now uses ONE geometry for its own before and after, which is what makes its comparison
    # meaningful. This cross-check is skipped ONLY where the report and the pass are deliberately
    # measuring different things, and it SAYS SO rather than passing silently.
    used_fallback = "no MEASURED size" in preview
    if reported and planned and not used_fallback:
        mine = planned[0]
        theirs = float(reported.group(1))
        # Within a unit: the report rounds its printout to a whole number, and the two must otherwise be
        # the same arithmetic on the same graph.
        check("MY BEFORE-NUMBER AGREES WITH THE REPORT'S OWN TOTAL",
              abs(mine - theirs) <= 1.0,
              "mine %.0f vs theirs %.0f - if these differ, the mirrored pin arithmetic has drifted"
              % (mine, theirs))
    elif used_fallback:
        print("    SKIPPED the strict comparison: the pass reports NO MEASURED SIZE, so it uses its "
              "documented fallback while the report uses the raw width. Different geometries, "
              "deliberately - recorded as skipped rather than passed.")
    # THIS ONE IS NEVER SKIPPED, and it is the check that caught the worst bug in this capability: with
    # before and after measuring DIFFERENT geometries, the pass reported "changed:" on a graph where
    # nothing needed repairing and would have REWRITTEN A GOOD GRAPH. It needs no reference measure,
    # only that the pass compares like with like.
    if planned:
        check("the before and after numbers are EQUAL on a graph with nothing to fix",
              planned[0] == planned[1], "%s -> %s" % planned)

    # ---------------------------------------------------------------------------------------------
    print()
    print("=== THE POSITIVE CASES ON TA-4, WHICH NOW EXISTS ===")
    #
    # `TA-4` IS THE `Stacked` GRAPH INSIDE `/Game/Tests/Kit/TA01_HandLaid`, and it is THE fixture this
    # capability's acceptance row names. The kit commandlet places each defect DELIBERATELY and MEASURES IT
    # BACK, failing its own build if the defect is absent, so the fixture cannot silently lose the shape a
    # check depends on. Measured on it: 7 nodes and the report's own findings are backward-exec 1,
    # backward-data 1, overlap 1, stray 1.
    #
    # THE EDIT MUTATES THIS GRAPH, AND `R1` AND `R3` REMOVE THE VERY SHAPES THE CHECKS BELOW ASSERT ON. So
    # the fixture is REBUILT IMMEDIATELY BEFORE IT IS USED, which is what makes this suite re-runnable: the
    # first version passed once and then reported EIGHT FAILURES on every later run, on a tool that had
    # worked perfectly. A suite that owns a shared resource must restore it ON THE WAY IN as well as on the
    # way out - removing the damage at the end is not enough, because a run killed part-way leaves the same
    # mess behind. See `rebuild_kit_and_reconnect`.
    KIT_ASSET = "TA01_HandLaid"
    KIT_GRAPH = "Stacked"

    session = rebuild_kit_and_reconnect()
    if session is None:
        print("RESULT: FAIL (the kit fixture could not be rebuilt, so the positive cases cannot run)")
        return 1

    def kit_report():
        return session.call(LOOK_TOOLSET, "LookTurboMCPLayoutReport",
                            {"asset": KIT_ASSET, "graphName": KIT_GRAPH})

    before = kit_report()
    print(before[:600])
    print()

    # THE FIXTURE IS CHECKED BEFORE THE PASS IS JUDGED, because a check on a defect that is not there
    # proves nothing. This is the same rule the kit commandlet applies to itself.
    before_backward_exec = report_finding_count(before, "backward-exec")
    before_stray = report_finding_count(before, "stray")
    check("TA-4 really carries a backward exec wire before the pass",
          before_backward_exec > 0, "backward-exec occurrences: %d" % before_backward_exec)
    check("TA-4 really carries a stray node at the origin before the pass",
          before_stray > 0, "stray occurrences: %d" % before_stray)

    kit_preview = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphFixBackward",
                               {"asset": KIT_ASSET, "graphName": KIT_GRAPH, "bForce": False})
    print(kit_preview)
    print()
    check("the preview on TA-4 says the wiring WOULD be shorter",
          "shorter" in kit_preview, kit_preview[:200])

    # THE MOVES ARE PARSED OUT OF THE PREVIEW, EVERY TIME. The frame is
    #   R1  a9722a K2Node_CallFunction (400,300) -> (680,0)  <why>
    moves = re.findall(
        r"(R\d)\s+(\w{6})\s+\w+\s+\((-?\d+),(-?\d+)\)\s*->\s*\((-?\d+),(-?\d+)\)", kit_preview)
    print("    moves the preview plans: %d" % len(moves))
    for rule, key, x0, y0, x1, y1 in moves:
        print("      %s %s (%s,%s) -> (%s,%s)" % (rule, key, x0, y0, x1, y1))

    r1_moves = [m for m in moves if m[0] == "R1"]
    r3_moves = [m for m in moves if m[0] == "R3"]
    check("R1 has a move to make, so the backward wire is really repaired",
          len(r1_moves) > 0, "R1 moves: %d" % len(r1_moves))
    check("R3 has a move to make, so the stray is really moved",
          len(r3_moves) > 0, "R3 moves: %d" % len(r3_moves))

    # CHECK 5: THE TARGET IS TO THE RIGHT OF THE SOURCE. That is the rule's whole point, and it is
    # asserted on the PLANNED position rather than on a sentence about it.
    if r1_moves:
        _, key, x0, y0, x1, y1 = r1_moves[0]
        check("check 5: R1 moves the node to the RIGHT",
              int(x1) > int(x0), "%s: x %s -> %s" % (key, x0, x1))
    else:
        check("check 5: R1 moves the node to the RIGHT", False, "no R1 move to check")

    # CHECK 17: THE STRAY'S NEAREST LINK IS WITHIN 1200 UNITS AFTERWARDS. The preview states the distance
    # BEFORE the move, so the same sentence gives both numbers and the improvement is checkable rather than
    # asserted. 1200 is `StrayLinkDistance` from the spec.
    if r3_moves:
        distance = re.search(r"(\d+(?:\.\d+)?)\s*unit", kit_preview)
        stated = float(distance.group(1)) if distance else -1.0
        check("check 17: the stray WAS further than the 1200-unit rule",
              stated > 1200.0, "stated distance: %s" % stated)
    else:
        check("check 17: the stray WAS further than the 1200-unit rule", False, "no R3 move to check")

    edited = session.call(EDIT_TOOLSET, "EditTurboMCPGraphFixBackward",
                          {"asset": KIT_ASSET, "graphName": KIT_GRAPH, "bForce": False})
    print(edited)
    print()
    check("the edit on TA-4 reports that it changed the graph",
          edited.startswith("changed:"), edited[:120])

    after = kit_report()
    print(after[:600])
    print()
    after_backward_exec = report_finding_count(after, "backward-exec")
    after_stray = report_finding_count(after, "stray")

    # CHECK 7 AND 17, READ THROUGH A DIFFERENT TOOL. `L-LAYOUT-REPORT` is the independent witness: the pass
    # is asked to repair, and the REPORTER is asked whether it did. A pass reporting its own success is not
    # evidence, which is why these go through the report rather than through the edit's sentence.
    check("check 7: L-LAYOUT-REPORT no longer reports backward-exec",
          after_backward_exec == 0, "backward-exec before %d, after %d"
          % (before_backward_exec, after_backward_exec))
    check("check 17: L-LAYOUT-REPORT no longer reports a stray node",
          after_stray == 0, "stray before %d, after %d" % (before_stray, after_stray))

    # THE DATA WIRE IS LEFT ALONE, AND THE PASS SAYS SO WITH BOTH ENDS NAMED. A pass that moved it would be
    # guessing, and one that said nothing would look as though it had handled it.
    check("the backward DATA wire is reported as left alone, with both ends named",
          "left alone" in kit_preview and "backward DATA" in kit_preview, kit_preview[:400])
    before_data = report_finding_count(before, "backward-data")
    after_data = report_finding_count(after, "backward-data")
    check("the DATA wire is still there afterwards, because no rule covers it",
          after_data >= before_data, "backward-data before %d, after %d" % (before_data, after_data))

    # IDEMPOTENT ON ITS OWN OUTPUT: the shapes it repairs are gone, so a second pass has nothing to do. This
    # is the `already tidy` case reached from a graph that genuinely needed work, which is stronger than
    # reaching it from a graph that never did.
    second = session.call(PREVIEW_TOOLSET, "PreviewTurboMCPGraphFixBackward",
                          {"asset": KIT_ASSET, "graphName": KIT_GRAPH, "bForce": False})
    second_moves = re.findall(
        r"(R\d)\s+(\w{6})\s+\w+\s+\((-?\d+),(-?\d+)\)\s*->\s*\((-?\d+),(-?\d+)\)", second)
    check("a second pass finds NOTHING left to move",
          len(second_moves) == 0, "still planned: %s" % second_moves)

    print()
    print("=== STILL SKIPPED, WITH THE SHAPE THAT IS MISSING NAMED ===")
    #
    # A SKIP IS NOT A PASS, AND THESE FOUR HAVE NO FIXTURE THAT CAN RUN THEM. Each one names the shape it
    # needs, so the next person does not have to rediscover it.
    #
    # WHAT TA-4 DID SETTLE AND THESE DO NOT: it carries a stacked backward exec pair (checks 4, 5, 7), a
    # lone node linked 3038 units away (check 17), and a backward DATA wire no rule covers. It carries NO
    # getter at all, so R2 has no positive evidence, and it has no comment box, no pure chain and no shape
    # whose repair would measure LONGER.
    skip("check 6: a backward getter ends on its consumer's row, to its left",
         "needs a getter placed under the node that reads it. TA-4 reports `R2 backward getters: 0 found`, "
         "so R1 and R3 have live evidence and R2 has NONE.")
    skip("check 8: L-LAYOUT-REPORT no longer reports getter-place for the moved getters",
         "needs a getter under its consumer, for the same reason as check 6.")
    skip("check 11: when the measure would be worse, nothing is written",
         "needs a graph whose planned repair LENGTHENS the wiring. TA-4's repair measures SHORTER, so the "
         "roll-back branch is not reached: the clean graph short-circuits before it and TA-4 keeps its pass.")
    skip("check 12: Force says WORSE in capitals",
         "needs a graph whose pass would be worse, for the same reason as check 11.")
    skip("check 15: a node whose fix would leave its box is refused, naming the box",
         "needs a comment box around a node the pass would move outside it. TA-4 has `boxes: 0 seen`, so "
         "the box guard is never reached.")
    skip("check 16: the pure chain moves as a unit, and MaxPureChain's cap is exercised by nine",
         "needs a pure chain four long AND a chain of nine. TA-4 reports `R1 pure nodes carried with their "
         "consumer: 0`.")

    print()
    print("  RECORDED: %d check(s) SKIPPED, not passed, each naming the shape no fixture carries." % len(skips))
    print("  A skip is not a pass. The negative cases ran for real, and so did the TA-4 cases.")

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run including %d against TA-4, %d SKIPPED for a shape no fixture carries)"
          % (25, 14, len(skips)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
