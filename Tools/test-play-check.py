#!/usr/bin/env python3
"""`R-PLAY-CHECK`: play the project briefly and report what it complained about.

THE TOOL IS DELIBERATELY TWO CALLS, so this suite is written around that. A tool call runs on the editor's
game thread and PIE is ticked on that same thread inside the editor's own tick
(`EditorEngine.cpp:2169`), so a call that started a session and waited would block the thread the session
needs. Pass `seconds` to request, then call with `seconds` empty to collect.

WHAT THIS SUITE IS CAREFUL ABOUT:

  - IT STOPPES WHAT IT STARTED, AND IT CHECKS FOR A LEFTOVER FIRST. The suite must be re-runnable from any
    state, so before it does anything it drains any watch a previous run left behind. A suite that owns a
    shared resource restores it ON THE WAY IN as well as out.
  - IT NEVER REQUESTS A MAP THAT DOES NOT EXIST, and it CHECKED that this matters: measured, asking for an
    unfindable map left the editor unable to answer ANY further request. The tool now refuses such a path
    before touching the engine, and one check below is that refusal.
  - A TOOL'S OWN REPORT IS NOT EVIDENCE. The liveness check reads the MCP server through `check-mcp-server.py`,
    which is a different path from the tool's own answer.
"""

import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import Session  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
READ_TOOLSET = "TurboMCP.TurboMCPReadToolset"
TOOL = "ReadTurboMCPPlayCheck"

failures = []
skips = []
# A ONE-ELEMENT LIST RATHER THAN A MODULE-LEVEL `global` COUNTER, because `check` only ever increments and a
# closure over a list cannot be accidentally rebound the way a bare assignment inside the function could.
passed_checks = [0]


def check(name, ok, detail=""):
    if ok:
        passed_checks[0] += 1
        print("  ok   %s%s" % (name, ("  " + str(detail)) if detail else ""))
    else:
        print("  FAIL %s%s" % (name, ("  " + str(detail)) if detail else ""))
        failures.append(name)


def skip(name, why):
    print("  SKIP %s" % name)
    print("       %s" % why)
    skips.append(name)


def call(session, seconds, map_path=""):
    """One call of the tool, as JSON arguments."""
    return session.call(READ_TOOLSET, TOOL,
                        {"Seconds": seconds, "Map": map_path})


# ---------------------------------------------------------------------------------------------------
# THE ENGINE'S OWN LOG FILE, WHICH IS THE ONLY WITNESS OUTSIDE THE TOOL
# ---------------------------------------------------------------------------------------------------
#
# THE HOLE THIS SECTION EXISTS TO NARROW, STATED FIRST BECAUSE IT IS NOT CLOSED. The suite cannot yet put a
# KNOWN warning and error INSIDE the watching window, so it cannot prove the count is MEASURED rather than
# merely SHAPED. Breaking the renderer to hardcode `0 warning(s) and 0 error(s)` still passed the whole suite,
# because the suite's own session LEGITIMATELY produces zero of each and a forged zero is therefore
# indistinguishable from a true zero. FOUR MECHANISMS WERE INVESTIGATED AND EVERY ONE IS BLOCKED:
#
#   1. `PrintString` / `LogString` CANNOT EMIT A WARNING AT ALL. `UKismetSystemLibrary::PrintString` logs
#      `UE_LOGF(LogBlueprintUserMessages, Log, ...)` unconditionally - `Log` severity, with no parameter to
#      change it (KismetSystemLibrary.cpp, the `if (bPrintToLog)` branch). So no Blueprint node can put a
#      Warning in the window, whatever the fixture. MEASURED against the engine source rather than assumed.
#   2. `unreal.log_warning` / `unreal.log_error` DO EXIST (PythonScriptPlugin PyCore.cpp:2067-2068, the
#      `LogWarning` and `LogError` handlers, category `LogPython`) - but they run in whatever process executes
#      them, and the tool's device is attached to `GLog` IN THE EDITOR PROCESS. MEASURED: launching a SECOND
#      editor with `-ExecCmds="py <file>"` does NOT reach the running one. The marker appeared ZERO times in
#      the live editor's log, and the spawned process counted from frame `[  0]`.
#   3. NO C++ TEST EMITTER IS IN SCOPE. The task permits editing `TurboMCPPlayCheck.cpp/.h` and `Tools/`, and
#      a UE_LOG emitter added to a PRODUCTION tool file purely so its own test can pass would be a hook, not a
#      test.
#   4. PIE ITSELF RAISES NOTHING ON A CLEAN RUN. MEASURED: 0 warnings and 0 errors, and `PlayLevel.cpp`'s
#      warnings are on paths this project cannot reach without breaking the run.
#
# WHAT IS DONE INSTEAD, AND WHY IT IS STILL WORTH DOING: `UE_LOG` WRITES TO `GLog` **AND** TO THE LOG FILE, so
# the file is an INDEPENDENT COUNTER over the same window. It cannot manufacture the injection, but it CAN
# catch the opposite forgery - a report claiming a warning that never happened - and it turns "the count is
# right" into "the count agrees with the engine's own log", which is the cross-tool rule this project applies
# everywhere else.

def engine_log_path():
    """The running editor's own log file, which is a different path from the tool's answer.

    IT NAMES THE SESSION'S OWN LOG RATHER THAN THE NEWEST FILE, AND THE FIRST VERSION GOT THIS WRONG.
    MEASURED: the log directory held `TurboMCPTest_2.log` with a LATER mtime than `TurboMCPTest.log`, because
    an earlier experiment had launched a second editor and that process rotated the files. Picking the newest
    then cross-checked the report against ANOTHER PROCESS'S LOG, which is not a cross-check at all - the two
    would disagree or agree for reasons that have nothing to do with this session.

    The editor writes its current log at `<Project>/Saved/Logs/<ProjectName>.log` and rotates previous ones
    to numbered or timestamped names, so the unsuffixed file is the one being appended to right now. The
    numbered files are still accepted as a fallback for the case where the running editor has itself already
    rotated, but the plain name wins whenever it exists.
    """
    logs = os.path.join(REPO, "TestProject", "Saved", "Logs")
    if not os.path.isdir(logs):
        return None

    plain = os.path.join(logs, "TurboMCPTest.log")
    if os.path.isfile(plain):
        return plain

    candidates = [os.path.join(logs, name) for name in os.listdir(logs)
                  if name.endswith(".log") and "backup" not in name]
    if not candidates:
        return None
    return max(candidates, key=os.path.getmtime)


def engine_log_counts(since_offset):
    """Counts Warning and Error lines the ENGINE wrote past `since_offset`.

    The line format is `[timestamp][frame]Category: Severity: text`, so the severity is the field after the
    category and is parsed rather than matched against any sentence the tool printed.
    """
    path = engine_log_path()
    if path is None or not os.path.isfile(path):
        return None, None, path
    with open(path, "r", encoding="utf-8", errors="replace") as handle:
        handle.seek(since_offset)
        text = handle.read()
    warnings = len(re.findall(r"\]\w+:\s+Warning:\s", text))
    errors = len(re.findall(r"\]\w+:\s+Error:\s", text))
    return warnings, errors, path


def server_alive():
    """Reads the server through a DIFFERENT path from the tool's own answer."""
    probe = subprocess.run(
        ["python", os.path.join(REPO, "Tools", "check-mcp-server.py")],
        capture_output=True, text=True, cwd=REPO)
    return "PASS" in (probe.stdout or "")


def drain(session, limit=40):
    """Reports until nothing is being watched, so a previous run's watch is cleared."""
    for _ in range(limit):
        out = call(session, "")
        if "pending" not in out:
            return out
        time.sleep(1)
    return "drain gave up after %d attempts" % limit


def play_and_collect(session, seconds, timeout=60):
    """Requests a session, waits for it to finish, and returns the final report."""
    first = call(session, str(seconds))
    deadline = time.time() + timeout
    last = first
    while time.time() < deadline:
        last = call(session, "")
        if "pending" not in last and "still running" not in last:
            return first, last
        time.sleep(1)
    return first, last


def main():
    session = Session()
    session.connect()

    print("=== the suite starts from a known state ===")
    # RE-RUNNABLE FROM ANY STATE. If a previous run died mid-session, a watch may still be armed, and every
    # start would then be refused. Draining it first is the way in.
    drained = drain(session)
    print("    %s" % drained.splitlines()[0] if drained else "    (no output)")
    check("nothing is left being watched before the suite starts",
          "pending" not in drained, drained[:200])

    print()
    print("=== the refusals, which decide before they touch the engine ===")
    #
    # EVERY REFUSAL IS CHECKED FOR ITS OWN SENTENCE, not merely for the `error:` prefix. A tool that refused
    # everything with one generic message would pass a prefix-only check while telling a caller nothing.
    bad_number = call(session, "abc")
    print(bad_number[:200])
    check("a non-numeric `seconds` is refused", bad_number.startswith("error:"), bad_number[:120])
    check("...and the refusal names the value that was wrong", "abc" in bad_number, bad_number[:160])

    zero = call(session, "0")
    print(zero[:200])
    check("zero seconds is refused", zero.startswith("error:"), zero[:120])
    check("...because the window would be empty",
          "greater than zero" in zero or "empty" in zero, zero[:160])

    negative = call(session, "-5")
    print(negative[:200])
    check("a negative `seconds` is refused", negative.startswith("error:"), negative[:120])

    huge = call(session, "100000")
    print(huge[:200])
    check("an absurd `seconds` is refused by the cap", huge.startswith("error:"), huge[:120])
    check("...and the cap is named so a caller can retry", "600" in huge, huge[:200])

    # THE MAP GUARD, WHICH IS THE MOST IMPORTANT REFUSAL HERE. Measured: an unfindable map left the editor
    # unable to answer ANY further request, so the tool must refuse before it asks. The check after it is
    # that the editor is STILL ALIVE, which is the thing that actually matters.
    bad_map = call(session, "3", "/Game/DoesNotExist/NoMap")
    print(bad_map[:260])
    check("a map that does not exist is refused", bad_map.startswith("error:"), bad_map[:120])
    check("...and the refusal names the path", "/Game/DoesNotExist/NoMap" in bad_map, bad_map[:200])
    check("...and says nothing was requested", "NOT requested" in bad_map, bad_map[:200])
    check("AND THE EDITOR STILL ANSWERS afterwards, which is what the guard is for",
          server_alive(), "check-mcp-server.py did not pass")

    print()
    print("=== nothing is being watched when no session is running ===")
    empty = call(session, "")
    print(empty[:200])
    check("a report with no watch and no session is refused rather than answered vaguely",
          empty.startswith("error:") or empty.startswith("already playing:"), empty[:160])
    check("...and it says how to start one", "seconds" in empty, empty[:200])

    print()
    print("=== THE GOLDEN PATH: a real session, started and stopped by this tool ===")
    # THE ENGINE'S OWN LOG IS READ FROM BEFORE THE WATCH STARTS, so the counts below cover the same window.
    log_path = engine_log_path()
    log_offset = 0
    if log_path is not None and os.path.isfile(log_path):
        log_offset = os.path.getsize(log_path)
    first, final = play_and_collect(session, 3)
    print("    start said: %s" % first.splitlines()[0])
    print(final)
    print()

    # THE START MUST NOT PRETEND TO HAVE A RESULT. This is the whole reason the tool is two calls, so it is
    # asserted rather than assumed: a tool that returned an empty log here would look like a clean run.
    check("the start call ANSWERS pending rather than reporting a result it cannot have",
          first.startswith("pending:"), first[:160])
    check("...and it explains why it cannot wait",
          "thread" in first or "next editor tick" in first, first[:240])

    # THE FINAL REPORT. Its shape is asserted from the tool's own numbers, parsed rather than matched.
    counts = re.search(r"(\d+) warning\(s\) and (\d+) error\(s\)", final)
    check("the report gives a parsed warning and error count",
          counts is not None, final[:200])
    check("...and it says the window it covered", "played for" in final, final[:200])

    # A CLEAN WINDOW MUST BE STATED AS SUCH, AND THE OLD CHECK HERE COULD NOT FAIL.
    #
    # IT USED TO READ: `"no warnings and no errors" in final or "warning(s) and" in final`. The SECOND
    # DISJUNCT IS TRUE FOR EVERY REPORT THE TOOL CAN PRODUCE, because `warning(s) and` is part of the count
    # line itself - so the check passed whether the window was clean, noisy, or reported by a renderer that
    # had been rewritten to lie. THIS PROJECT HAS RECORDED "an assertion that cannot fail" in its
    # cannot-be-SATISFIED form; this was the other form, and the disjunct is now gone.
    #
    # The tool prints ONE of two mutually exclusive sentences and the check now demands the one the MEASURED
    # state produces: a clean 3-second session raises nothing (MEASURED 0 and 0, and no PIE path this project
    # can reach raises anything), so the report must say so in the tool's own words for it.
    check("a clean window is stated as such, in the tool's own sentence for it",
          "The session raised no warnings and no errors." in final, final[:260])
    check("...and a clean window does NOT also claim to have listed records",
          "warn  " not in final and "ERROR " not in final, final[:260])

    # THE REPORT MUST NOT CONTRADICT ITS OWN NUMBERS, AND THIS CHECK WAS ADDED BECAUSE A BREAK-TEST SHOWED THE
    # TOOL COULD. Hardcoding the count to a non-zero value produced a report that said
    # `5 warning(s) and 0 error(s) in the window` AND `The session raised no warnings and no errors.` IN THE
    # SAME ANSWER - the count and the sentence disagreed, and only the cross-check below noticed.
    #
    # A reader who trusts the sentence is told the run was clean while the number says otherwise, and the two
    # come from ONE computation that is supposed to have a single outcome. This makes that impossible: whichever
    # of the two a future edit gets wrong, the other one contradicts it here.
    if counts is not None:
        warned, errored = int(counts.group(1)), int(counts.group(2))
        claims_clean = "The session raised no warnings and no errors." in final
        check("the report does not contradict its own counts: a non-zero count and a clean sentence "
              "cannot both appear",
              not (claims_clean and (warned > 0 or errored > 0)),
              "counts %d/%d, claims clean=%s" % (warned, errored, claims_clean))
        check("...and a window with counts does list the records it counted",
              (warned == 0 and errored == 0) or ("warn  " in final or "ERROR " in final),
              "counts %d/%d with no record lines" % (warned, errored))
    check("the session did not report itself as never having started",
          "NEVER REACHED A RUNNING WORLD" not in final, final[:240])

    # THE CROSS-CHECK: THE ENGINE'S OWN LOG OVER THE SAME WINDOW. This cannot manufacture the injection the
    # hole needs, but it catches the OPPOSITE forgery - a report claiming a warning that never happened - and
    # it makes the count agree with the log rather than only with itself. The counts are required to MATCH
    # rather than merely to be non-negative, which is why the offsets are taken around the same window.
    if log_path is None:
        skip("the engine's own log was cross-checked against the report",
             "no editor log file was found under TestProject/Saved/Logs, so there is nothing to cross-check.")
    else:
        log_warnings, log_errors, _ = engine_log_counts(log_offset)
        reported_warnings = int(counts.group(1)) if counts else -1
        reported_errors = int(counts.group(2)) if counts else -1
        print("    the engine's own log over the same window: %s warning(s), %s error(s)"
              % (log_warnings, log_errors))
        check("the report's counts AGREE with the engine's own log over the same window",
              log_warnings == reported_warnings and log_errors == reported_errors,
              "tool said %s/%s, the log says %s/%s"
              % (reported_warnings, reported_errors, log_warnings, log_errors))

    print()
    print("=== the watch is released, so the suite is re-runnable ===")
    after = call(session, "")
    print(after[:200])
    # NOT `pending`: the report was handed over and the watch closed. Without this the second run of the
    # suite would be refused at its first start.
    check("after reporting, nothing is being watched any more",
          "pending" not in after and "still running" not in after, after[:200])

    print()
    print("=== a second full cycle works, which is the re-runnability check ===")
    first2, final2 = play_and_collect(session, 2)
    check("a SECOND session can be started straight away",
          first2.startswith("pending:"), first2[:160])
    # COUNTED FROM ITS OWN NUMBERS. `"played for" in final2` would pass on a report whose counts were missing
    # or unparseable, so the count is parsed here too the way the first window's is.
    counts2 = re.search(r"(\d+) warning\(s\) and (\d+) error\(s\)", final2)
    check("...and it reports with a parseable count, not merely the words",
          counts2 is not None, final2[:240])
    check("...and the second window is clean too, since nothing was injected into it",
          counts2 is not None and counts2.group(1) == "0" and counts2.group(2) == "0", final2[:240])

    print()
    if skips:
        print("  RECORDED: %d check(s) SKIPPED, not passed:" % len(skips))
        for name in skips:
            print("    %s" % name)
        print()

    # THE COUNT IS COMPUTED, NOT PINNED. A hard-coded total drifts the moment a check is added or moved, and
    # this suite is the one place where "never pin a count" cannot apply to the RESULT LINE either.
    total = passed_checks[0] + len(failures)
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run, %d SKIPPED)" % (total, len(skips)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
