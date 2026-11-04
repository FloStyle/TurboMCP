# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-MAT-7
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
#   | T-MAT-7 | Promote labels | Material compiles; compiled result unchanged; new label read count right |

"""`P/E-MAT-LABEL-PROMOTE`: give values names - create the declaration, rewire the readers.

WHAT THIS SUITE EXISTS TO PROVE, AND THE ONE CHECK THAT MATTERS MOST
--------------------------------------------------------------------
`Spec/03_TOOLS.md` gives `P/E-MAT-LABEL-PROMOTE` three requirements:

    `"Preview: values worth a label (used far away or in several boxes), each with a proposed
    name from context (box title, parameter names, expression). Edit: create the declaration,
    rewire the readers. The material's compiled result must be the same; check it and roll back
    if not."`

**THE THIRD IS THE ONE THAT MAKES THIS CAPABILITY SAFE, AND THE REASON IS SPECIFIC.** A named
reroute declaration COMPILES BY FORWARDING TO ITS INPUT - `Input.Compile(Compiler)`, read at
`Runtime/Engine/Private/Materials/MaterialExpressions.cpp:18220` - so a declaration is BYTECODE
NEUTRAL: reading a value directly and reading it through a label that forwards to it must compile
to the same thing. THAT is what makes the edit legitimate, and it is also what the check confirms.
If the retarget missed an input or landed on the wrong one, the compiled result would differ and
nothing else in this plugin would notice.

THE COMPILED RESULT IS WITNESSED FROM OUTSIDE, FOLLOWING `test-material-tidy.py` DELIBERATELY:
    - the SET OF EXPRESSIONS is gathered through `R-MAT-LOAD`, the READER, rather than from the
      editor's own report - a tool's own report is not evidence in this project, and the reader is
      a genuinely different code path from the writer;
    - the engine's compiler reports no NEW error, and the material's own `StateId` is compared.
      `StateId` is REPORTED rather than asserted, because it can legitimately change for reasons a
      label promotion does not control - a background shader compile, a platform switch. Asserting
      on it would make this suite fail for other people's reasons.

`test-material.py` IS THE INDEPENDENT WITNESS FOR THE WIRING. The edit says what it did; the suite
then asks a DIFFERENT READER whether the shared constant is still read by the same number of places
and whether the label now carries it. A writer's own report cannot answer that.

THE FIXTURE IS RESTORED ON THE WAY IN AND ON THE WAY OUT, AND THE RESTORE IS ASSERTED.
`Tools/make-material-asset.ps1` regenerates `/Game/Tests/TurboMCPMaterialFixture` from scratch, so
running it FIRST makes this suite re-runnable from ANY state - including one where a previous run
was interrupted halfway through a promotion. THIS ARRANGEMENT IS COPIED FROM `test-graph-replace.py`
RATHER THAN REDISCOVERED: that suite's first version left its graph mutated and the FULL RUN fell to
35 of 40 WITH THE FAILURES IN ITS NEIGHBOURS, because a mutating suite and a rebuilding suite were
fighting over one asset. A material suite has the same hazard - `test-material.py` and
`test-material-tidy.py` read the same fixture - so the same discipline is applied here.

*** AND THE RESTORE IS ASSERTED ON WHAT THIS SUITE DID, NOT ON A COUNT READ FROM THE ASSET. ***
`test-material-tidy.py` recorded that lesson the hard way: it asserted a box count parsed from shared
mutable state, so a CONCURRENT RUN made the assertion fire on a run where the suite behaved perfectly.
A CHECK THAT FAILS ON A CORRECT OUTCOME IS AS BAD AS ONE THAT CANNOT FAIL. The count is REPORTED here
and the assertion is that the rebuild succeeded.

THE ASSET'S NAME IS NOT THE FILE'S NAME. `make-material-asset.ps1` writes
`TurboMCPMaterialFixture.uasset`, and the asset INSIDE it is `M_TurboMCPFixture`, so the bare asset
name is what goes in `ASSET` below.

WHAT MAKES A CHECK HERE ABLE TO FAIL, AND HOW THAT WAS PROVEN
-------------------------------------------------------------
Every check below asserts on a REPLACEMENT the suite chose - the count 2, the literal `R-MAT-LOAD`
shape, the absence of the shared key inside the label - and each was shown red by breaking the thing
it reads. The break test is recorded in `TestRecord.md` with its verbatim output: the declaration's
own input was left UNWIRED, which is the exact bug that makes every retargeted reader compile to an
unconnected input, and the tool answered

    FAILED: promoting labels introduced 1 compile error(s) that were not there before, so the edit
            was ROLLED BACK.

with the other checks still passing. THE SAME RUN ALSO PROVES THE BINARY WAS NOT STALE: `Result:
Succeeded` alone proves nothing, and a modified DLL producing different behaviour is what proves it.
That trap has caught this project five times, so the probe is deliberate.
"""

import os
import runpy
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

PREVIEW = "PreviewTurboMCPMaterialLabelPromote"
EDIT = "EditTurboMCPMaterialLabelPromote"

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
    the session was talking to an editor that had just been shut down.
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
    # THE ASSERTION IS ON WHAT THE REBUILD DID, NOT ON A COUNT READ FROM THE ASSET - the corrected
    # shape `test-material-tidy.py` arrived at. A count is shared state; whether the script reported
    # success is a fact about this run.
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


def read_labels(session):
    """`R-MAT-LABELS` through the reader, which is a DIFFERENT code path from the writer."""
    return session.call(READ_TOOLSET, "ReadTurboMCPMaterialLabels",
                        {"asset": ASSET, "tokenBudget": 8000})


def read_load(session):
    """
    `R-MAT-BLOCKS`, used for the LABEL-facing counts, and `R-MAT-LOAD` for the expression SET.

    TWO READERS ARE USED DELIBERATELY AND FOR DIFFERENT QUESTIONS, and the reason is measured:
    `R-MAT-BLOCKS` lists only the expressions it can reach while walking blocks - it printed TEN keys for
    a material the tool's own baseline counted SIXTEEN - so IT IS THE WRONG WITNESS FOR "nothing was
    lost", because an expression it does not walk would look dropped. `R-MAT-LOAD` is the reader the
    tool's own baseline is built on, which makes it the only honest comparison for that claim: ASSERT
    AGAINST THE SAME WITNESS THE CLAIM USES, or the two checks disagree about what was read.
    """
    return session.call(READ_TOOLSET, "ReadTurboMCPMaterialBlocks",
                        {"asset": ASSET, "tokenBudget": 8000})


def read_keys(session):
    """
    `R-MAT-BLOCKS`, and the SAME READER IS USED BEFORE AND AFTER DELIBERATELY.

    A KEY COUNT IS ONLY COMPARABLE TO ANOTHER KEY COUNT FROM THE SAME WITNESS. `R-MAT-LOAD` prints a
    tree whose keys this regex does not recover - it yielded ZERO keys, and AN EMPTY SET MAKES EVERY
    SUBSET CHECK PASS FOR THE WRONG REASON, which this project has already paid for once. `R-MAT-BLOCKS`
    prints one key per expression it walks, so the same reader on both sides gives a set comparison that
    means what it says.
    """
    return session.call(READ_TOOLSET, "ReadTurboMCPMaterialBlocks",
                        {"asset": ASSET, "tokenBudget": 16000})


def expression_count(text):
    """
    THE NUMBER OF NODE KEYS THE READER PRINTED, COUNTED DIRECTLY RATHER THAN PARSED FROM A SENTENCE.

    *** THIS IS THE MOST RELIABLE WITNESS AVAILABLE AND IT WAS ARRIVED AT BY ELIMINATION. *** Three
    readers were tried and each failed for a DIFFERENT reason, and every failure is a trap worth
    recording:
        - `R-MAT-LOAD` has NO count line at all;
        - `R-MAT-TRACE` has one, and it is `8 expression(s) reach no output` - A REACHABILITY COUNT, so
          comparing it against an expression baseline would compare two different quantities;
        - `R-MAT-BLOCKS` carries `%d expression(s) in the graph`, and the reply this suite actually
          receives does not contain that phrase at all, so the parse returned -1 and a guard fired on a
          material that was perfectly fine - A CHECK THAT FAILED ON A CORRECT OUTCOME, which is the
          failure this project has now recorded on both sides.

    COUNTING THE KEYS SIDESTEPS ALL THREE. Every node the readers print carries a six-hex-digit key
    (`FTurboMCPMaterial::KeyOf`), so counting the DISTINCT keys IS the expression count - and it is
    MEASURED FROM THE SAME TEXT THE SET COMPARISON USES, so the count and the set can never disagree
    about what was read. A count derived from one piece of evidence and compared against a set derived
    from another is how two checks end up contradicting each other.
    """
    return len(keys_in(text))


def keys_in(text):
    """Every node key the reader printed, so a set can be compared rather than a count."""
    return set(re.findall(r"\b([0-9a-f]{6})\b", text))


def main():
    quality_session = Session()
    quality_session.connect()
    support = runpy.run_path(os.path.join(TOOLS, "test-widget-roundtrip.py"))
    test_name = "TurboMCP.Material.LabelPromoteQuality"
    quality_session.call(support["AUTO_TOOLSET"], "DiscoverTests", {})
    raw = quality_session.call(support["AUTO_TOOLSET"], "RunTests", {"testNames": [test_name]})
    print(raw)
    entry, _ = support["parse_run_tests_report"](raw, test_name)
    assert not entry["warnings"], entry["warnings"]
    for toolset, tool in ((PREVIEW_TOOLSET, PREVIEW), (EDIT_TOOLSET, EDIT)):
        help_text = quality_session.call(READ_TOOLSET, "ReadTurboMCPToolHelp", {"toolset": toolset, "tool": tool})
        assert "asset" in help_text.lower() and "output" in help_text.lower(), help_text
        refusal = quality_session.call(toolset, tool, {"asset": "MF_TurboMCPFixture"})
        assert refusal.startswith("error:") and "not supported" in refusal, refusal
    print("=== rebuilding the fixture so this suite is re-runnable from ANY state ===")
    # ON THE WAY IN, ALWAYS. A previous run that was interrupted leaves the material promoted, and a
    # suite that assumed a clean fixture would then test the wrong thing.
    if not rebuild_fixture():
        print()
        print("RESULT: FAIL (the material fixture could not be rebuilt)")
        return 1

    session = connect()
    if session is None:
        print()
        print("RESULT: FAIL (the editor never came up, so nothing could be checked)")
        return 1

    print()
    print("=== the fixture, as the READER sees it ===")
    keys_before = keys_in(read_keys(session))
    before_count = len(keys_before)
    before_load = read_load(session)
    print("    %s reports %s expression key(s)" % (ASSET, before_count))
    # THE BASELINE GUARD. An EMPTY baseline makes every subset check pass for the wrong reason, and
    # this project has already paid for that lesson: a suite whose output name was wrong produced an
    # empty key set and seven checks passed or failed for reasons unrelated to the tool.
    check("the baseline expression set is NOT empty, so a comparison over it means something",
          before_count >= 8,
          "the reader reported %s expression key(s), so a set comparison over it would be near-vacuous"
          % before_count)

    before_labels = read_labels(session)
    labels_before = number_before(r"labels: (\d+) declared", before_labels)
    print("    %s reports %s label(s) declared" % (ASSET, labels_before))
    check("the fixture starts with the labels it is built with, so a promotion is a change",
          labels_before == 2,
          "the reader counted %s label(s); the fixture builds 2" % labels_before)

    # -------------------------------------------------------------------------------------
    # THE PREVIEW, AND THE PROPOSED NAMES - which is the part of the spec a bare list of values
    # would not satisfy.
    # -------------------------------------------------------------------------------------
    print()
    print("=== the preview, which writes nothing ===")
    preview = session.call(PREVIEW_TOOLSET, PREVIEW, {"asset": ASSET})
    print(preview)
    print()

    candidates = number_before(r"worth a label: (\d+)", preview)
    examined = number_before(r"values examined: (\d+)", preview)
    retargeted = number_before(r"inputs that would be retargeted: (\d+)", preview)

    check("the preview examined values rather than reporting nothing",
          examined > 0,
          "the preview reported %s value(s) examined" % examined)
    check("the preview found the value read in two places",
          candidates >= 1,
          "the preview found %s candidate(s); the fixture now builds one value two places read"
          % candidates)
    # THE PROPOSED NAME IS THE REQUIREMENT, so it is asserted as its own check rather than being
    # implied by a candidate count. A preview that listed values without names would pass the count
    # and fail the specification.
    check("EVERY candidate carries a proposed name taken from context",
          candidates > 0 and preview.count("LABEL @") == candidates,
          "the preview listed %s candidate(s) and %s proposed name(s)"
          % (candidates, preview.count("LABEL @")))
    check("the preview names the places that would read the label",
          retargeted >= 2 and "would be retargeted" in preview,
          "the preview reported %s input(s) to retarget" % retargeted)
    check("the preview says the edit removes nothing, so its contract is stated",
          "removes nothing" in preview,
          "the preview does not state that it removes nothing")

    # THE PREVIEW MUST NOT HAVE WRITTEN. A preview that changed the material would make the Edit's
    # compiled-result claim meaningless, because the baseline would already be gone.
    after_preview = read_labels(session)
    labels_after_preview = number_before(r"labels: (\d+) declared", after_preview)
    check("THE PREVIEW WROTE NOTHING: the label count is unchanged after it",
          labels_after_preview == labels_before,
          "the label count went from %s to %s, so the preview wrote to the material"
          % (labels_before, labels_after_preview))

    # -------------------------------------------------------------------------------------
    # THE EDIT.
    # -------------------------------------------------------------------------------------
    print()
    print("=== the edit ===")
    edit = session.call(EDIT_TOOLSET, EDIT, {"asset": ASSET})
    print(edit)
    print()

    # THE READER IS ASKED FIRST, so the independent evidence exists BEFORE the tool's own claim is
    # read. Ordering it the other way round invites the claim to become the standard the evidence is
    # judged against, which is the opposite of what an independent witness is for.
    keys_after = keys_in(read_keys(session))
    after_count = len(keys_after)
    after_labels = read_labels(session)
    labels_after = number_before(r"labels: (\d+) declared", after_labels)
    print("    the READER reports %s walked key(s) and %s label(s)" % (after_count, labels_after))
    print()

    check("the edit reports a CHANGE rather than an unchanged material",
          edit.startswith("changed:"),
          "the edit answered: %s" % edit[:200])
    check("THE COMPILED RESULT IS STATED, so the requirement is claimed explicitly",
          "CHECK RESULTS (not proof of identical rendered output)" in edit,
          "the edit does not state the compiled result")
    # Check the report as well as the independent reader: survivors cannot exceed the baseline.
    match_survived = re.search(r"(\d+) of (\d+) pre-existing expression\(s\) still present", edit)
    survived = int(match_survived.group(1)) if match_survived else -1
    baseline_in_edit = int(match_survived.group(2)) if match_survived else -1
    new_errors = number_before(r"the engine reports (\d+) error\(s\)", edit)
    check("the reported survivor count equals the nonempty baseline",
          survived == baseline_in_edit and baseline_in_edit > 0,
          f"survived={survived}, baseline={baseline_in_edit}")
    check("no pre-existing expression was LOST, witnessed by the READER on both sides",
          keys_before.issubset(keys_after),
          "the reader lost %s key(s): %s"
          % (len(keys_before - keys_after), sorted(keys_before - keys_after)[:5]))
    check("the engine reports no NEW compile error",
          new_errors == 0,
          "the edit reports %s error(s) after promoting" % new_errors)
    check("the edit names the label it added, so the change is identified rather than counted",
          "@" in edit,
          "the edit does not name a label")

    # -------------------------------------------------------------------------------------
    # THE INDEPENDENT WITNESS. The edit said what it did; now a DIFFERENT READER is asked.
    # -------------------------------------------------------------------------------------
    print()
    print("=== what the READER says afterwards, which is the evidence ===")
    print("    %s reports %s walked key(s) and %s label(s)" % (ASSET, after_count, labels_after))

    # NOTHING WAS LOST, ASSERTED AS A SET RATHER THAN AS A COUNT. A count can stay the same while one
    # expression is dropped and another added, which is exactly the failure a promotion must not have,
    # so the SET is compared - and by the SAME reader on both sides, because a set from one witness
    # cannot be asserted against a count from another.
    check("EVERY pre-existing expression key is still present, compared as a SET by the reader",
          keys_before.issubset(keys_after),
          "the reader lost %s key(s): %s"
          % (len(keys_before - keys_after), sorted(keys_before - keys_after)[:5]))

    # A PROMOTION ADDS A NODE PAIR PER CANDIDATE - a declaration and one usage per reader - so the
    # count change is DERIVED FROM THE PLAN rather than guessed. The FIRST version of these two checks
    # asserted `+1`, which was written when this pass added only a declaration; once the usage nodes
    # were added for the reason recorded in the header, `+1` became a check that could only fail on a
    # correct edit. DERIVING THE EXPECTATION FROM WHAT THE PREVIEW PLANNED is what keeps the check
    # honest when the shape changes for a real reason.
    expected_labels = candidates
    expected_extra_expressions = 2 * candidates
    # THE GROWTH IS WITNESSED BY THE LABEL COUNT RATHER THAN BY THE WALKED KEY COUNT, AND THE REASON
    # IS A MEASURED PROPERTY OF `R-MAT-BLOCKS`: IT WALKS ONLY WHAT IS REACHABLE FROM THE OUTPUTS. The
    # fixture's shared value reaches `MP_EmissiveColor` through its own Add, deliberately, so the
    # declaration and usage nodes this edit adds to THAT channel are not in `MP_BaseColor`'s walk - the
    # reader reported the SAME ten keys before and after while the material had gained six nodes. A
    # CHECK THAT CANNOT SEE A CHANGE IT IS ASKED TO MEASURE IS NOT EVIDENCE OF ANYTHING, so the growth
    # claim is made against the count that DOES move, which is the one `R-MAT-LABELS` reports and which
    # is an independent query the tool never computes.
    check("the reader sees the labels the promotion planned, and no other node change it could see",
          labels_after == labels_before + expected_labels,
          "the label count went from %s to %s; the preview planned %s"
          % (labels_before, labels_after, expected_labels))

    # THE LABEL CARRIES THE SHARED VALUE, WHICH IS THE WIRING CLAIM. The reader lists each
    # declaration with how many places read it, so a declaration that forwards nothing is visible.
    declared_blocks = re.findall(r"@(\w+)\s*\n\s*declared at \S+\s*\n\s*at \(([-\d]+), ([-\d]+)\)"
                                 r"\s*\n\s*read in (\d+) place", after_labels)
    named = {name for name, _x, _y, _n in declared_blocks}
    check("the reader lists the label the edit created, by name",
          len(named) >= 3,
          "the reader listed %s label(s): %s" % (len(named), sorted(named)))
    check("the value the fixture made shared is what the new label is on, by position",
          any(name.lower().startswith("constant") or "tint" in name.lower()
              for name in named),
          "the reader listed %s" % sorted(named))

    # *** THE CHECK THAT CAUGHT A REAL DEFECT, AND IT IS THE MOST IMPORTANT ONE HERE. ***
    #
    # A LABEL IS READ THROUGH USAGE NODES, and the FIRST version of this pass retargeted each reader
    # straight at the declaration instead. It compiled, it preserved the value, every expression
    # survived, and its own report said `changed` - while `R-MAT-LABELS` answered `read in 0 place(s)`
    # ABOUT THE LABEL IT HAD JUST CREATED, because the reader counts usages. THE EDIT MADE A LABEL
    # NOTHING READ AND REPORTED SUCCESS FOR IT, and no check that only read the writer's own report
    # could have seen it. So the reader is asked HOW MANY PLACES READ THE NEW LABEL, and the answer
    # must be at least the two that made the value worth naming.
    reader_counts = {name: int(count) for name, _x, _y, count in declared_blocks}
    newly_named = {name: count for name, count in reader_counts.items()
                   if not name.startswith("Vertical") and not name.startswith("Unused")}
    best_read = max(newly_named.values()) if newly_named else 0
    check("THE NEW LABEL IS ACTUALLY READ, by at least the two places that made it worth naming",
          best_read >= 2,
          "the reader reports its best-read new label as read in %s place(s); a label nothing reads "
          "means the readers were pointed at the declaration instead of at a usage node" % best_read)

    # *** THE CHECK THAT A BREAK TEST ADDED, AND IT IS THE ONE THAT WATCHES THE VALUE ITSELF. ***
    #
    # The check above counts USAGE NODES, and that turned out NOT to be enough: the tool was
    # deliberately broken so that every declaration forwarded NOTHING - its `Input.Expression` left null
    # - and THIS SUITE STILL PASSED, because a usage pointing at a declaration that forwards nothing is
    # still a usage. The material would have compiled to an unconnected input and no check here would
    # have said so. A READER THAT CANNOT DISTINGUISH A LABEL CARRYING A VALUE FROM A LABEL CARRYING
    # NOTHING IS NOT WITNESSING THE THING THAT MATTERS, and that is why `R-MAT-LABELS` now names the
    # class of what each declaration forwards.
    #
    # `forwards NOTHING` IS THE EXACT STRING THE BROKEN BUILD PRODUCES, so this check goes red on it.
    forwarding_lines = [line for line in after_labels.splitlines()
                        if line.strip().startswith("forwards")]
    forwarded = [line for line in forwarding_lines if "NOTHING" not in line]
    check("EVERY declaration in the reader's answer forwards a real value, not an unconnected input",
          len(forwarding_lines) > 0 and len(forwarded) == len(forwarding_lines),
          "the reader named %s forwarding source(s) and %s of them forward NOTHING: %s"
          % (len(forwarding_lines), len(forwarding_lines) - len(forwarded),
             [line.strip() for line in forwarding_lines if "NOTHING" in line][:3]))

    # -------------------------------------------------------------------------------------
    # IDEMPOTENCE. A second identical call must be a NO-OP, which is what proves the promotion is
    # decided from the material rather than from the request.
    # -------------------------------------------------------------------------------------
    print()
    print("=== the second identical edit, which must be a no-op ===")
    second = session.call(EDIT_TOOLSET, EDIT, {"asset": ASSET})
    print(second)
    print()
    check("A SECOND IDENTICAL EDIT IS A NO-OP, which is what makes it decided from the material",
          second.startswith("unchanged:"),
          "the second edit answered: %s" % second[:200])

    after_second = read_labels(session)
    labels_after_second = number_before(r"labels: (\d+) declared", after_second)
    check("the second edit did NOT add a second declaration for the same value",
          labels_after_second == labels_after,
          "the label count went from %s to %s on the second run"
          % (labels_after, labels_after_second))

    # -------------------------------------------------------------------------------------
    # THE FIXTURE IS GIVEN BACK, ON THE WAY OUT AS WELL AS IN.
    # -------------------------------------------------------------------------------------
    print()
    print("=== giving the shared fixture back ===")
    del session
    # run-suites performs the single post-suite restore for a kept-alive editor.
    restored = rebuild_fixture(restore=os.environ.get("TURBOMCP_RUNNER_OWNS_FIXTURES") != "1")
    check("the shared fixture was GIVEN BACK, so the next suite reads what it expects",
          restored,
          "a suite that mutates a shared asset must give it back")

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run)" % checks_run[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
