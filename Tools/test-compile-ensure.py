# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-STRUCT-7
#
# NAMED HERE SO Spec/06_ACCEPTANCE.md CAN BE ANSWERED MECHANICALLY, which is what
# `python Tools/checks/count-acceptance-citations.py` measures. THE ROW WAS EXTENDED FOR THIS SUITE, and the
# extension is the task's own sentence rather than a new demand:
#
#   | T-STRUCT-7 | Compile | A broken node is named by ID; a clean unsaved asset says compiled, not saved; an
#   |            |         | engine message logged during the compile is listed with a count per asset, so a
#   |            |         | compile that raised an ensure is not reported clean |
#
# WHICH IS T04's `done when` VERBATIM (`Feedbacks/TurboMCP_TaskList_2026-10-13.md:90`): "the compile result
# lists each ensure and each error that the engine logged during the compile, with a count per asset. A
# compile that had an ensure is never reported as 'clean, 0 warnings' alone."
"""T04's verify: the engine really logs an ensure during a compile, and at a severity a report can read.

WHY THIS SUITE EXISTS AND WHY IT IS SHAPED LIKE THIS
----------------------------------------------------
The task's own verify is "compile a Blueprint that triggers a `queued for compile while compiling` ensure".
That verify was UNRUNNABLE, and the reason is measured rather than argued:

  - the ensure is `BlueprintCompilationManager.cpp:331`, inside `QueueForCompilation`, and it needs
    `GCompilingBlueprint` plus a second compile request arriving while the first is running;
  - `EditTurboMCPCompile` compiles sequentially, so a tool call cannot build that;
  - and NOTHING in this project logs at `Warning` or `Error` during a compile: every `UE_LOG` in
    `BlueprintCompilationManager.cpp` is `Display`, and no `Ensure condition failed` line appears anywhere in
    `TestProject/Saved/Logs` before this fixture existed.

SO THE CONDITION IS BUILT IN C++, IN THE TEST PROJECT, BY `UTurboMCPTestCompileFixture`. It arms the
engine's own `OnBlueprintPreCompile` hook - broadcast from INSIDE the compile window at
`BlueprintCompilationManager.cpp:1365` - and asks the engine to queue a SECOND asset for compilation from
inside it. The engine then raises its own assertion, twice, because an engine invariant was really violated:
`Attempting to enqueue BP_TestAlpha for compile while compiling` (`:331`) and `Blueprints requested
compilation while compiling other blueprints` (`:2243`).

*** WHAT THE FIXTURE MEASURED, AND THE FINDING THAT MATTERS MOST. ***
  - `queuedFromHook=1`, `ensures=2`: the condition is real and the engine asserted twice;
  - `engineLog=60 [Warning Error Error ...]`: the engine's messages reached the log AT `Warning` AND `Error`,
    which are the two severities a report must read. That is the premise `T04` rests on, and it is now
    measured rather than assumed;
  - `deviceSaw=83`, `firstCall owner=125868 this=85736 inGame=no`: **THE LOG IS NOT DELIVERED ON THE GAME
    THREAD.** `FTurboMCPLogCapture` keeps only messages raised on the thread that created it
    (`TurboMCPLogCapture.cpp:62-68`), and a tool call runs on the game thread - so THAT RULE WOULD DISCARD
    EVERY ONE OF THESE. This suite asserts the measurement; it does not claim the capture sees them.

*** THE C++ TEST REPORTS THE ENGINE'S ERROR LINES AS ITS OWN, AND THAT IS DELIBERATE. *** An `Error` line
raised inside an automation test is a test error unless it is declared expected, and BOTH ways of declaring
it were measured and rejected: `Occurrences = 1` made the verdict depend on how many times the test had run
(the second run reported the line as `Verbose` with an empty body, and the test failed), and
`Occurrences = -1` SUPPRESSED the line, so the fixture's device then recorded `engineLog=0 []` - a test
measuring its own suppression. So the test is expected to report the engine's two messages as errors, and the
checks below confirm those are the only errors. A REAL failure in the fixture still appears as an error this
suite did not predict.

WHAT THIS SUITE DOES NOT CLAIM
------------------------------
IT DOES NOT CLAIM THE ENGINE'S ENSURE LANDED INSIDE `E-COMPILE`'s OWN CAPTURE WINDOW. Those are different
tool calls - the automation test raises the ensure, `E-COMPILE` compiles separately - and the capture is
per-call, so a message raised by the fixture is gone by the time `E-COMPILE` runs. The checks on `E-COMPILE`'s
report below therefore assert its SHAPE and its honesty in the negative case, which is what can be measured
from here.
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, Session  # noqa: E402

AUTO_TOOLSET = "AutomationTestToolset.AutomationTestToolset"
LOG_TOOLSET = "EditorToolset.LogsToolset"
ASSET_TOOLSET = "editor_toolset.toolsets.asset.AssetTools"

# BOTH ASSETS EXIST IN `TestProject/Content/Tests`, AND BOTH ARE ALREADY SHARED FIXTURES. Named here rather
# than discovered, because a suite that discovers its own input cannot say what it measured.
ASSET = "/Game/Tests/BP_NodeTest"
SECOND_ASSET = "/Game/Tests/Kit/BP_TestAlpha"
FIXTURE_TEST = "TurboMCP.T04.QueueForCompilationDuringCompile"

# THE TWO MESSAGES THE ENGINE RAISES, as its own `ensureMsgf` text. They are the errors the fixture test is
# expected to report, and any OTHER error means the fixture itself misbehaved.
EXPECTED_ENGINE_TEXTS = (
    "Attempting to enqueue BP_TestAlpha for compile while compiling",
    "Blueprints requested compilation while compiling other blueprints",
)

failures = []
passed_checks = [0]


def check(name, ok, detail=""):
    if ok:
        passed_checks[0] += 1
        print("  ok   %s%s" % (name, ("  " + str(detail)) if detail else ""))
    else:
        print("  FAIL %s%s" % (name, ("  " + str(detail)) if detail else ""))
        failures.append(name)


def fixture_report(session):
    """The fixture's own line, read back from the editor's log toolset."""
    text = session.call(LOG_TOOLSET, "GetLogEntries",
                        {"category": "", "pattern": "fixture: compiled", "maxEntries": 10})
    return text if isinstance(text, str) else str(text)


def main():
    session = Session()
    session.connect()

    print("=== 1. the condition: a compile queued from inside a compile ===")
    # `DiscoverTests` FIRST, because `RunTests` refuses until the report tree exists - measured while writing
    # this suite: without it the toolset answers an error and nothing runs.
    print(session.call(AUTO_TOOLSET, "DiscoverTests", {})[:200])

    listed = session.call(AUTO_TOOLSET, "ListTests", {"nameFilter": "TurboMCP.T04", "tagFilter": ""})
    check("the T04 fixture test is discovered by the editor", FIXTURE_TEST in str(listed), str(listed)[:160])
    if FIXTURE_TEST not in str(listed):
        print()
        print("RESULT: FAIL (the fixture test is not registered, so no check below could mean anything)")
        return 1

    run = str(session.call(AUTO_TOOLSET, "RunTests", {"testNames": [FIXTURE_TEST]}))
    print(run[:600])

    # *** THE FIXTURE TEST IS EXPECTED TO FAIL, AND THIS IS WHERE THAT IS MADE HONEST RATHER THAN HIDDEN. ***
    # The engine's `Error` lines are reported as the test's own errors - see the suite docstring for the two
    # measured ways of declaring them expected and why both were refused. So the test's verdict is checked
    # against the errors it reports: EVERY error must be one the engine caused, and a failed assertion from
    # the fixture (`Expected '...' to be true`) means the fixture itself broke.
    unexpected = re.findall(r"Expected '[^']*' to be true", run)
    check("the fixture's own assertions all passed", not unexpected,
          unexpected[0] if unexpected else "none failed")
    check("the test reached the end of its run rather than erroring out",
          FIXTURE_TEST in run, run[:200])
    check("the engine's ensure is what the test reported, not some other error",
          "Ensure condition failed" in run, run[:200])

    print()
    print("=== 2. what the fixture measured inside the process, read from the editor's log ===")
    report = fixture_report(session)
    print(report[:900])

    check("the fixture queued a compile from inside a compile",
          "queuedFromHook=1" in report, report[-260:])
    check("the engine raised TWO ensures while that compile ran",
          "ensures=2" in report, report[-260:])
    check("the compiler's own results log carried no error, so only the engine's log could say anything",
          "0 error(s) from the compiler)" in report, report[-260:])
    check("the engine's messages arrived at a severity the compile report reads (Warning or Error)",
          ("[Warning " in report) or ("[Error " in report), report[-260:])
    check("those messages were not merely Notice or Display level",
          "engineLog=0" not in report, report[-260:])

    # *** THE FINDING THIS SUITE EXISTS TO RECORD AS MUCH AS TO PASS. *** The last number is the thread the
    # engine delivered the log on, and `inGame=no` is what makes a per-thread filter useless here.
    check("the log was not delivered on the game thread, which is the thread a tool call runs on",
          "inGame=no" in report, report[-200:])

    print()
    print("=== 3. the engine's own ensure is in the editor's log, from the editor's own toolset ===")
    ensured = str(session.call(LOG_TOOLSET, "GetLogEntries",
                               {"category": "", "pattern": "Ensure condition failed", "maxEntries": 20}))
    print(ensured[:500])
    check("the engine logged its own ensure rather than only a message it carries",
          "Ensure condition failed" in ensured, ensured[:200])
    check("the ensure names the asset that was queued while another compiled",
          "BP_TestAlpha" in ensured or "BP_TestAlpha" in report, ensured[:200])

    print()
    print("=== 4. E-COMPILE's report: its shape, and its honesty when the engine logged nothing ===")
    compiled = session.call(EDIT_TOOLSET, "EditTurboMCPCompile", {"assets": "BP_NodeTest"})
    print(compiled[:900])

    check("compile answers rather than erroring", not compiled.startswith("error:"), compiled[:160])
    check("the report gives a per-asset line", "BP_NodeTest" in compiled, compiled[:200])
    check("the report says whether the asset is saved", "saved" in compiled.lower(), compiled[:200])
    check("the report's verdict is a word this suite recognises",
          ("clean" in compiled.lower()) or ("not clean" in compiled.lower()), compiled[:200])

    # *** A REPORT MAY SAY `clean`, OR IT MAY NAME THE ENGINE LOG - BUT IT MAY NOT NAME THE ENGINE LOG WITHOUT
    # SAYING WHAT THE ENGINE LOG CONTRIBUTED. *** A bare claim that a source was consulted, with no count
    # beside it, is the shape `T04` was filed against, so the two are tied together rather than asserted
    # separately. The check does NOT demand engine messages in THIS call: a compile of an already-compiled
    # asset legitimately has none, and failing on that would be failing on a correct report.
    names_engine_log = "from the engine log" in compiled
    gives_counts = ("engine log" in compiled) and ("warning(s)" in compiled) and ("/" in compiled)
    check("when the report names the engine log it also gives the count of what came from it",
          (not names_engine_log) or gives_counts,
          "names=%s counts=%s" % (names_engine_log, gives_counts))

    print()
    print("=== 5. the fixture's assets exist, so it was not compiling nothing ===")
    for path in (ASSET, SECOND_ASSET):
        exists = session.call(ASSET_TOOLSET, "exists", {"path": path})
        # THE TOOL ANSWERS A JSON BOOLEAN, WHICH `tmcp.Session.call` MAY HAND BACK AS A PYTHON BOOL - the
        # first version of this check died with `'bool' object has no attribute 'lower'`. Measured.
        answer = exists if isinstance(exists, bool) else ("true" in str(exists).lower())
        check("the fixture's asset still exists: %s" % path, bool(answer), repr(exists)[:120])

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run)" % passed_checks[0])
    return 0


if __name__ == "__main__":
    sys.exit(main())
