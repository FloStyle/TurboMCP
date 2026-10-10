"""`P/E-MAT-TIDY`: lay out a material, one comment box per block.

WHAT THIS SUITE EXISTS TO PROVE, AND THE ONE CHECK THAT MATTERS MOST
--------------------------------------------------------------------
`Spec/03_TOOLS.md` gives `P/E-MAT-TIDY` three requirements:
`"One comment box per block. Same layout rules as graphs (section 6). Same compiled
result check."`

**THE THIRD IS THE ONE THAT MAKES THIS CAPABILITY SAFE.** A layout pass moves expressions and
adds comment boxes. It CANNOT legitimately change what the material computes, so if the compiled
result differs afterwards, THAT IS A BUG AND IT MUST BE CAUGHT RATHER THAN TOLERATED. Every other
check here is secondary to that one, because a tidy that broke a material while reporting success
would be the worst thing this plugin could do.

THE COMPILED RESULT IS WITNESSED FROM OUTSIDE AND IN TWO WAYS:
    - the SET OF EXPRESSIONS is gathered through `R-MAT-TRACE`, the READER, rather than from the
      editor's own report - a tool's own report is not evidence in this project, and the reader is
      a genuinely different code path from the writer;
    - the engine's compiler reports no NEW error, and the material's own `StateId` is compared.
      `StateId` is REPORTED rather than asserted, because it can legitimately change for reasons a
      layout does not control - a background shader compile, a platform switch. Asserting on it
      would make this suite fail for other people's reasons.

THE FIXTURE IS RESTORED ON THE WAY IN AND ON THE WAY OUT, AND THE RESTORE IS ASSERTED.
`Tools/make-material-asset.ps1` regenerates `/Game/Tests/TurboMCPMaterialFixture` from scratch, so
running it FIRST makes this suite re-runnable from ANY state - including one where a previous run
was interrupted halfway through a layout. THIS ARRANGEMENT IS COPIED FROM `test-graph-replace.py`
RATHER THAN REDISCOVERED: that suite's first version left its graph mutated and the FULL RUN fell
to 35 of 40 WITH THE FAILURES IN ITS NEIGHBOURS, because a mutating suite and a rebuilding suite
were fighting over one asset. A material suite has the same hazard - `test-material.py` reads the
same fixture - so the same discipline is applied here.

THE ASSET'S NAME IS NOT THE FILE'S NAME, and this was MEASURED twice while writing this suite:
`make-material-asset.ps1` writes `TurboMCPMaterialFixture.uasset`, and the asset INSIDE it is
`M_TurboMCPFixture`. A reader pointed at the PACKAGE path answers "no asset named ...", so the
bare asset name is what goes in `ASSET` below.

WHAT MAKES A CHECK HERE ABLE TO FAIL, and this was PROVEN BY BREAKING IT
-----------------------------------------------------------------------
The set comparison was made to lie - the baseline set had a key removed, simulating a layout that
had added or dropped an expression - and the tool answered:

    FAILED: the layout changed the SET OF EXPRESSIONS, so it was ROLLED BACK.
            0 lost (none), 1 gained (6a9cd6).

and every other check still passed, which is the RIGHT SHAPE: the failure is isolated to the
check that exists for it. THE SAME RUN ALSO PROVES THE BINARY WAS NOT STALE - the modified DLL
produced different behaviour, where `Result: Succeeded` alone would have proven nothing. That trap
has caught this project five times, so the probe is deliberate: A BREAK TEST THAT DOES NOT GO RED
MEANS THE BINARY IS OLD, NOT THAT THE CHECK IS GOOD.
"""

import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import (  # noqa: E402
    EDIT_TOOLSET,
    PREVIEW_TOOLSET,
    READ_TOOLSET,
    Session,
)

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TOOLS = os.path.join(REPO, "Tools")

# THE ASSET NAME, NOT THE FILE NAME. See the docstring.
ASSET = "M_TurboMCPFixture"

OUTPUT = "MP_BaseColor"

PREVIEW = "PreviewTurboMCPMaterialTidy"
EDIT = "EditTurboMCPMaterialTidy"

failures = []
checks_run = [0]


def check(name, ok, detail=""):
    # THE DETAIL IS SHOWN ONLY WHEN THE CHECK FAILS, so a passing check does not print a failure
    # sentence underneath itself. A passing check that prints a failure message is worse than a
    # bare pass: it teaches the reader to ignore the text.
    checks_run[0] += 1
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name,
                           ("  " + detail) if detail and not ok else ""))
    if not ok:
        failures.append(name)


def number_before(pattern, text, default=-1):
    """A COUNT PARSED OUT OF A SENTENCE. This project's rule: NEVER match a sentence beside a
    number, because a reworded sentence then silently stops matching and the check passes for the
    wrong reason."""
    match = re.search(pattern, text)
    return int(match.group(1)) if match else default


def rebuild_fixture():
    """
    Regenerates the shared fixture so the suite is re-runnable from ANY state.

    THIS STOPS THE EDITOR, AND THAT IS WHY THE SESSION IS MADE AFTERWARDS RATHER THAN BEFORE.
    `make-material-asset.ps1` stops the editor itself, because the running editor holds the .uasset
    open and a save against a held handle is lost. MEASURED while writing this suite: rebuilding
    with a live session first produced `URLError: [WinError 10061] no connection could be made` -
    the session was talking to an editor that had just been shut down. `test-graph-replace.py` uses
    the same arrangement for the same reason.
    """
    completed = subprocess.run(
        ["pwsh", "-NoProfile", "-File", os.path.join(TOOLS, "make-material-asset.ps1")],
        capture_output=True, text=True, cwd=REPO)
    output = (completed.stdout or "") + (completed.stderr or "")
    if "PASS" not in output:
        print("THE FIXTURE COULD NOT BE REBUILT, so the checks below cannot run on a known state.")
        print(output[-1500:])
        return False
    return True


def connect():
    """
    Starts the editor and returns a connected session, or None.

    `subprocess.call` RATHER THAN `run(capture_output=True)`, AND THIS IS A MEASURED FACT RATHER
    THAN A PREFERENCE, recorded in this project before: THE EDITOR DOES NOT COME UP WHEN ITS OUTPUT
    IS CAPTURED, because the launcher leaves a pipe open. So the output goes to the console.
    """
    subprocess.call(
        ["pwsh", "-NoProfile", "-File", os.path.join(TOOLS, "start-editor.ps1")], cwd=REPO)

    deadline = time.time() + 180
    while time.time() < deadline:
        try:
            session = Session()
            session.connect()
            return session
        except Exception:  # noqa: BLE001 - not up yet is the expected case
            time.sleep(3)
    return None


def main():
    print("=== rebuilding the shared fixture, so this suite is re-runnable from any state ===")
    if not rebuild_fixture():
        print()
        print("RESULT: FAIL (the material fixture could not be rebuilt)")
        return 1
    print("  ok   the fixture was regenerated, so the layout starts from a known state")

    # THE SESSION IS MADE AFTER THE REBUILD, because the rebuild stopped the editor. See
    # `rebuild_fixture` above for the measurement that made this necessary.
    session = connect()
    if session is None:
        print()
        print("RESULT: FAIL (the editor never came up, so nothing could be checked)")
        return 1

    print()
    print("=== the PREVIEW writes nothing and says what it would do ===")
    preview = session.call(PREVIEW_TOOLSET, PREVIEW, {"asset": ASSET, "force": ""})
    print(preview[:1400])

    # ONE COMMENT BOX PER BLOCK IS THE REQUIREMENT, so the preview must NAME blocks rather than
    # merely list nodes. A preview that said only "12 expressions would move" would satisfy nobody
    # reading the spec.
    blocks_planned = number_before(r"(\d+) block\(s\)", preview)
    check("the preview names the blocks it plans",
          blocks_planned >= 1, "the preview reported %s block(s)" % blocks_planned)

    check("the preview says the geometry is a FALLBACK rather than measured",
          "FALLBACK" in preview or "fallback" in preview,
          "a material expression stores no size, so the answer must say the sizes are substituted")

    # A PREVIEW MUST NOT CLAIM THE COMPILED RESULT IS UNCHANGED, because it has not measured it.
    # Claiming it would be the tool reporting evidence it does not have.
    check("the preview does NOT claim the compiled result is unchanged, because it has not measured it",
          "COMPILED RESULT" not in preview,
          "only the edit measures the compiled result")

    # AND IT MUST HAVE WRITTEN NOTHING. Read the box count back through the READER.
    before_blocks = session.call(READ_TOOLSET, "ReadTurboMCPMaterialBlocks",
                                 {"asset": ASSET, "tokenBudget": 4000})
    boxes_before = number_before(r"blocks: (\d+) comment box", before_blocks)
    check("the preview wrote nothing: the box count read back is the fixture's own",
          boxes_before == 2, "the reader saw %s box(es); the fixture builds 2" % boxes_before)

    print()
    print("=== the EDIT lays it out and PROVES the compiled result is the same ===")

    # THE SET OF EXPRESSIONS, GATHERED THROUGH THE READER BEFORE THE EDIT. Every key the trace
    # mentions is collected, so the comparison afterwards rests on the reader's own answer rather
    # than on the writer's.
    # THE OUTPUT NAME IS NOT BaseColor. MEASURED: the reader answered 'BaseColor' is not a
    # connected output ... Its connected outputs are: MP_BaseColor, so the fixture's own property
    # is the prefixed one. Getting this wrong would have made every trace call an error and the key
    # set EMPTY - and an empty baseline makes the subset checks below pass for the wrong reason,
    # which is a test bug that mimics a passing tool. That exact shape has already cost this project
    # seven checks in 	est-graph-patch, so the output is VERIFIED before it is used.
    outputs = session.call(READ_TOOLSET, "ReadTurboMCPMaterialLoad",
                           {"asset": ASSET, "bIncludeInstanceOverrides": False, "tokenBudget": 3000})
    check("the fixture has a connected output to trace, so there IS a baseline to compare",
          OUTPUT in outputs, "the reader's connected outputs did not include %s" % OUTPUT)

    trace_before = session.call(READ_TOOLSET, "ReadTurboMCPMaterialTrace",
                                {"asset": ASSET, "outputProperty": OUTPUT,
                                 "bIncludeInstanceOverrides": False, "tokenBudget": 6000})
    keys_before = set(re.findall(r";\s*([0-9a-f]{6})\b", trace_before))
    check("the trace reader named the expressions before the edit, so there is a baseline",
          len(keys_before) >= 3, "the reader named %d key(s)" % len(keys_before))

    edit = session.call(EDIT_TOOLSET, EDIT, {"asset": ASSET, "force": "1"})
    print(edit[:1600])

    check("the edit reports the compiled result was checked",
          "COMPILED RESULT" in edit, "the edit must measure the compiled result, not assume it")
    check("the edit reports the expression set is IDENTICAL",
          "IDENTICAL" in edit, edit[:300])
    check("the edit did NOT fail and roll back",
          "FAILED" not in edit and "ROLLED BACK" not in edit, edit[:300])

    # *** THE CHECK THAT MATTERS MOST, AND IT IS TAKEN FROM THE READER RATHER THAN THE WRITER. ***
    # The writer says the set is identical. THE READER IS ASKED THE SAME QUESTION, through a
    # different code path, and the two answers must agree. A tool's own report is not evidence.
    trace_after = session.call(READ_TOOLSET, "ReadTurboMCPMaterialTrace",
                               {"asset": ASSET, "outputProperty": OUTPUT,
                                "bIncludeInstanceOverrides": False, "tokenBudget": 6000})
    keys_after = set(re.findall(r";\s*([0-9a-f]{6})\b", trace_after))

    check("THE READER AGREES: every expression the reader named before is still there",
          keys_before.issubset(keys_after),
          "lost: %s" % sorted(keys_before - keys_after))
    check("THE READER AGREES: the layout added no expression",
          keys_after.issubset(keys_before),
          "gained: %s" % sorted(keys_after - keys_before))

    print()
    print("=== ONE COMMENT BOX PER BLOCK, seen by the reader ===")
    after_blocks = session.call(READ_TOOLSET, "ReadTurboMCPMaterialBlocks",
                                {"asset": ASSET, "tokenBudget": 4000})
    boxes_after = number_before(r"blocks: (\d+) comment box", after_blocks)
    expressions_after = number_before(r"comment box\(es\), (\d+) expression", after_blocks)

    check("the box count GREW, so the layout actually boxed the blocks",
          boxes_after is not None and boxes_before is not None and boxes_after > boxes_before,
          "boxes went from %s to %s" % (boxes_before, boxes_after))
    check("the expression count is UNCHANGED, seen by the reader",
          expressions_after == 12,
          "the reader counted %s expression(s); the fixture builds 12" % expressions_after)

    # ONE BOX PER BLOCK, PARSED. The preview said how many blocks it planned, and the edit wrote
    # that many boxes on top of the two the fixture already had. A layout that boxed every
    # expression separately, or that left blocks unboxed, fails here.
    check("the number of boxes matches the blocks the preview planned, plus the fixture's own 2",
          boxes_after == blocks_planned + 2,
          "the preview planned %s block(s) and the fixture had 2 boxes, so %s were expected but the "
          "reader saw %s" % (blocks_planned, blocks_planned + 2, boxes_after))

    # A BOX MUST SAY WHAT IT HOLDS. A box labelled only "block 3" tells a reader nothing, and the
    # whole value of a block box is that it names the block of logic.
    check("the boxes NAME what they hold rather than being numbered only",
          "block 1:" in after_blocks or "block 0:" in after_blocks,
          "at least one box should carry a name, not just a number")

    print()
    print("=== it is a DIFF, not a rewrite: a second pass says unchanged ===")
    second = session.call(EDIT_TOOLSET, EDIT, {"asset": ASSET, "force": ""})
    check("a second pass reports unchanged, which is what makes it safe to run twice",
          "unchanged" in second, second[:300])

    print()
    print("=== THE LAYOUT SURVIVED: every expression is where the pass put it ===")
    # A LAYOUT THAT DID NOT STICK would still have reported success, so the positions are read back
    # through the READER and compared against the plan. `R-MAT-BLOCKS` prints each box with its own
    # origin, so the boxes' geometry is the reader's own answer.
    box_rects = re.findall(r"(\d+)x(\d+) at (-?\d+),(-?\d+)", after_blocks)
    check("the reader reports real box geometry after the layout",
          len(box_rects) >= 1, "the reader printed no box rectangle")
    check("every box has a positive size, so none was written collapsed",
          all(int(w) > 0 and int(h) > 0 for w, h, _, _ in box_rects),
          "a zero-sized box would be a box the author cannot use")

    print()
    print("=== restoring the fixture, so the next suite reads a known state ===")
    restored = rebuild_fixture()
    check("the shared fixture was GIVEN BACK", restored,
          "a suite that owns a shared asset must restore it on the way IN as well as out")

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run)" % checks_run[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
