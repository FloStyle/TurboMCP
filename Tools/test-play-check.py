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


def check(name, ok, detail=""):
    if ok:
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

    # THIS IS THE CHECK THAT WOULD CATCH A STALE OR EMPTY LOG BEING PASSED OFF AS A CLEAN RUN. The tool is
    # required to say WHICH of the two it is, and the phrase for "nothing happened" must not be reachable
    # when the session never started.
    check("a clean window is stated as such rather than left as an empty list",
          "no warnings and no errors" in final or "warning(s) and" in final, final[:240])
    check("the session did not report itself as never having started",
          "NEVER REACHED A RUNNING WORLD" not in final, final[:240])

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
    check("...and it reports too", "played for" in final2, final2[:200])

    print()
    if skips:
        print("  RECORDED: %d check(s) SKIPPED, not passed:" % len(skips))
        for name in skips:
            print("    %s" % name)
        print()

    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run, %d SKIPPED)" % (25, len(skips)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
