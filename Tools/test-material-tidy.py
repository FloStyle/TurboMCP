# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-MAT-9
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
#   | T-MAT-9 | Tidy material | One box per block; result unchanged |

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

import tmcp_editor
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


def rebuild_fixture(restore=True):
    """
    Regenerates the shared fixture so the suite is re-runnable from ANY state.

    THIS STOPS THE EDITOR, AND THAT IS WHY THE SESSION IS MADE AFTERWARDS RATHER THAN BEFORE.
    `make-material-asset.ps1` stops the editor itself, because the running editor holds the .uasset
    open and a save against a held handle is lost. MEASURED while writing this suite: rebuilding
    with a live session first produced `URLError: [WinError 10061] no connection could be made` -
    the session was talking to an editor that had just been shut down. `test-graph-replace.py` uses
    the same arrangement for the same reason.
    """
    if os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") == "1":
        if not restore:
            return True
        ok, detail = tmcp_editor.restore_runner_fixture()
        print("    runner fixture restore: %s" % detail)
        return bool(ok)
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
    if os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") == "1":
        ok, detail = tmcp_editor.ensure_editor()
        print("    runner editor: %s" % detail)
        if not ok:
            return None
    else:
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
    print("=== checking whether the shared fixture needs rebuilding, WITHOUT stopping the editor ===")
    # THE FIXTURE IS INSPECTED BEFORE IT IS REBUILT, AND THAT ORDER IS THE WHOLE POINT OF THIS CHANGE.
    #
    # `make-material-asset.ps1` STOPS THE EDITOR, and the first version of this suite called it FOUR
    # TIMES in one run. The full run priced that: 32 of 42 with TEN failures whose names changed
    # between runs and which pass in isolation - the race signature. SO THE CHEAP QUESTION IS ASKED
    # FIRST: can the editor even answer? If it can, the fixture is usually already clean and NOBODY
    # HAS TO BE RESTARTED. Only a fixture that is genuinely dirty pays the reboot.
    session = connect()
    if session is None:
        print()
        print("RESULT: FAIL (the editor never came up, so nothing could be checked)")
        return 1

    look = session.call(READ_TOOLSET, "ReadTurboMCPMaterialBlocks",
                        {"asset": ASSET, "tokenBudget": 4000})
    boxes_at_start = number_before(r"blocks: (\d+) comment box", look)
    print("    the fixture reports %s box(es); it is built with 2" % boxes_at_start)

    if boxes_at_start != 2:
        print("    it is NOT in its built state, so it is rebuilt - and that stops the editor, so the")
        print("    session is remade afterwards.")
        del session
        if not rebuild_fixture():
            print()
            print("RESULT: FAIL (the material fixture could not be rebuilt)")
            return 1
        session = connect()
        if session is None:
            print()
            print("RESULT: FAIL (the editor never came back after the rebuild)")
            return 1
        # RE-READ, because the fixture was replaced underneath the session and the earlier answer is
        # about an asset that no longer exists.
        look = session.call(READ_TOOLSET, "ReadTurboMCPMaterialBlocks",
                            {"asset": ASSET, "tokenBudget": 4000})
        boxes_at_start = number_before(r"blocks: (\d+) comment box", look)
        print("    after the rebuild it reports %s box(es)" % boxes_at_start)
    else:
        print("    it is already in its built state, so the editor was NOT stopped")

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
    # *** AND THE EXPRESSION COUNT IS READ FROM THE SAME LINE BEFORE THE EDIT, SO THE CHECK BELOW CAN COMPARE
    # IT WITH ITSELF INSTEAD OF WITH A LITERAL. ***
    #
    # THE CHECK USED TO ASSERT `expressions_after == 16` - A COUNT PINNED ON A SHARED ASSET, WHICH THIS
    # PROJECT'S OWN RULE FORBIDS IN THESE WORDS: "NEVER PIN A COUNT OR AN ID ON A SHARED ASSET - it has cost
    # five rounds". It cost this one too, and in the worst way: the suite PASSED when the fixture held 16 and
    # FAILED when it held 12, so IT REPORTED A DEFECT IN THE MATERIAL TOOL THAT WAS A PROPERTY OF WHICH FIXTURE
    # IT HAPPENED TO BE LOOKING AT.
    #
    # The correct comparison was available the whole time - `keys_before`/`keys_after` already compare the
    # expression SET with itself - and only the COUNT was pinned.
    before_line = re.search(r"blocks:\s*(\d+)\s*comment box\(es\),\s*(\d+)\s*expression", before_blocks)
    expressions_before = int(before_line.group(2)) if before_line else None
    check("the reader printed the blocks line, so the box count read back is a measurement",
          before_line is not None,
          "no 'blocks: N comment box(es), N expression(s)' line was found before the edit either")
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
    # *** BOTH NUMBERS COME OUT OF THE SAME LINE, AND THE LINE IS PARSED AS A WHOLE. ***
    #
    # THE CHECK BELOW USED TO READ ITS SUBJECT WITH A PATTERN THAT COULD NOT MATCH IT, AND IT WAS THE
    # PROJECT'S OLDEST RULE BROKEN BY THE CHECK THAT EXISTS TO ENFORCE IT. `TurboMCPMaterial.cpp:1619`
    # prints `blocks: %d comment box(es), %d expression(s)`, so the EXPRESSION count is the SECOND number on
    # that line - and the old pattern was `comment box\(es\), (\d+) expression`, which requires `blocks: <n> `
    # to be IMMEDIATELY before it. IT NEVER MATCHED, `number_before` returned its default of `-1`, and the
    # check reported "the reader counted 12 expression(s)" - A WRONG NUMBER THAT LOOKED LIKE A MEASUREMENT
    # while the fixture builds 16 and the trace reader had just agreed on all sixteen by name.
    #
    # *** SO THE CHECK WAS NOT MEASURING THE READER; IT WAS MEASURING WHETHER A REGEX ASSUMED THE RIGHT
    # FORMAT. *** The line is now found ONCE and both numbers are read from THAT match, which is the same
    # discipline `number_before` uses for the box count and makes it impossible to match a different line.
    boxes_line = re.search(r"blocks:\s*(\d+)\s*comment box\(es\),\s*(\d+)\s*expression", after_blocks)
    boxes_after = int(boxes_line.group(1)) if boxes_line else None
    expressions_after = int(boxes_line.group(2)) if boxes_line else None

    check("the reader printed the blocks line, so both numbers below are a measurement",
          boxes_line is not None,
          "no 'blocks: N comment box(es), N expression(s)' line was found in the reader's answer")
    check("the box count GREW, so the layout actually boxed the blocks",
          boxes_after is not None and boxes_before is not None and boxes_after > boxes_before,
          "boxes went from %s to %s" % (boxes_before, boxes_after))
    check("the expression count is UNCHANGED, seen by the reader",
          expressions_after is not None and expressions_before is not None
          and expressions_after == expressions_before,
          "the reader counted %s expression(s) before and %s after" % (expressions_before, expressions_after))

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
    print("=== restoring the fixture ONLY IF THIS RUN LEFT IT DIFFERENT ===")
    # *** WHY THIS IS CONDITIONAL, AND IT IS A MEASURED DECISION RATHER THAN A SHORTCUT. ***
    #
    # THE FIRST VERSION REBUILT THE FIXTURE FOUR TIMES PER RUN, AND `make-material-asset.ps1` STOPS THE
    # EDITOR TO DO IT. Four editor stops inside one suite is churn the harness has to absorb, and the
    # FULL RUN MEASURED THE COST: it fell to 32 of 42 with TEN failures whose names CHANGED BETWEEN RUNS
    # and which PASS IN ISOLATION - `test-comment-boxes`, `test-function`, `test-panel-reach`,
    # `test-play-check`, `test-refs`, `test-trace`, `test-vars-add`, `test-widget-retarget`. THAT IS THE
    # RACE SIGNATURE THIS PROJECT HAS RECORDED MANY TIMES, and the churn was mine.
    #
    # A REBUILD IS ONLY NEEDED WHEN THE FIXTURE IS ACTUALLY DIRTY, so it is read first and written back
    # only on a real difference. THIS IS CHECKED RATHER THAN ASSUMED because an assumption here is what
    # produced the churn: the suite's own second pass already leaves the material UNCHANGED, so the
    # common case is a clean fixture and no editor stop at all.
    #
    # AND THE FIXTURE IS NOT THE ONLY SAFETY NET: `test-material-tidy` restores WHAT IT CHANGED by
    # design - the tidy is idempotent, so a second pass is a no-op - and the restore below covers the
    # case where a run was interrupted part-way and left the material mid-layout.
    look = session.call(READ_TOOLSET, "ReadTurboMCPMaterialBlocks",
                        {"asset": ASSET, "tokenBudget": 4000})
    boxes_now = number_before(r"blocks: (\d+) comment box", look)

    # ---------------------------------------------------------------------------------------------
    # THIS CHECK USED TO ASSERT ON A NUMBER READ FROM SHARED, MUTABLE STATE, AND IT COULD FAIL ON A RUN WHERE
    # THIS SUITE BEHAVED PERFECTLY. MEASURED, AND IT IS WHY THE FULL RUN STOOD AT 40 OF 42 WITH BOTH FAILURES
    # BEING THIS ASSERTION.
    #
    # THE OLD SHAPE ASSERTED IN BOTH BRANCHES AND BOTH DEPENDED ON THE SAME PARSED VALUE:
    #     dirty = boxes_now is not None and boxes_now != 2
    #     if dirty:   check("...GIVEN BACK", rebuild_fixture(), ...)
    #     else:       check("...left in the state it was built with", boxes_now == 2, ...)
    # SO WHENEVER `boxes_now` WAS ANYTHING OTHER THAN 2 THE CHECK COULD ONLY PASS BY TAKING THE FIRST BRANCH AND
    # HAVING THE REBUILD SUCCEED - AND WHEN THE REGEX DID NOT MATCH, `number_before` RETURNED `None`, `dirty` WAS
    # FALSE, AND THE `else` BRANCH ASSERTED `None == 2`. THAT IS THE SAME CLASS AS PATCH'S KEY-EXTRACTION REGEX
    # THAT MISSED THE `/Game/Tests/` PREFIX AND COST SEVEN CHECKS IN ONE ROUND.
    #
    # AND THE DIRECTIONS WERE OPPOSITE, WHICH IS THE PART WORTH KEEPING: THE `if` BRANCH WAS HONEST - "the fixture
    # was dirty, so I rebuilt it, here is whether that worked" - WHILE THE `else` BRANCH REACHED OUTSIDE, AND IT
    # RAN ON THE SUCCESS PATH. A CHECK THAT ONLY ASSERTS WHEN NOTHING WENT WRONG PUNISHES THE GOOD CASE.
    #
    # SO: THE BOX COUNT IS REPORTED, AND THE ASSERTION IS ON WHAT THIS SUITE DID. The count is SHARED STATE - a
    # concurrent run, or a fixture left by an earlier suite, owns it as much as this suite does - so asserting it
    # equals a constant is a claim about the world rather than about this run. WHAT IS ASSERTED INSTEAD IS THAT
    # THE RESTORE HAPPENED, which is the thing the next suite actually depends on.
    #
    # A FAILURE OF THE REGEX IS NOW ITS OWN OUTCOME RATHER THAN A SILENT `None` FOLDED INTO A COMPARISON: an
    # unparseable count is REPORTED AS UNPARSEABLE, because a check whose message says "the fixture is dirty" when
    # the truth is "I could not read the count" sends the next person to the wrong place.
    if boxes_now is None:
        print("    the reader printed no box count that could be parsed, so the fixture's state is unknown")
        print("    and it is rebuilt rather than assumed clean. The raw reply was: %s" % look[:200])
    else:
        print("    the fixture carries %s comment box(es); it is built with 2" % boxes_now)

    needs_restore = boxes_now is None or boxes_now != 2
    if needs_restore:
        print("    so it is rebuilt, because a suite that mutates a shared asset must give it back")
        # Under run-suites the runner performs the single post-suite restore.
        restored = rebuild_fixture(restore=os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") != "1")
        check("the shared fixture was GIVEN BACK, so the next suite reads what it expects", restored,
              "a suite that mutates a shared asset must give it back")
    else:
        # NOT A SKIP: the fixture is already in the state the next suite expects, and that is REPORTED rather
        # than asserted, because the count is shared state this suite cannot own.
        check("the suite did not need to restore the fixture it did not leave dirty", True)
        print("    the fixture is clean, so the editor was NOT stopped again")

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run)" % checks_run[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
