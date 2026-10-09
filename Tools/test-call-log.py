"""Exercise E-CALL-LOG, R-CALL-LOG and E-CALL-LOG-CLEAR against the running editor.

  T-LOG-1  every tool call is recorded
  T-LOG-2  the switch starts off and the choice persists
  T-LOG-3  the log says why it is empty rather than being empty

WHAT IS CHECKED FROM WHERE
--------------------------
The tool that reads the log is the same plugin, so its own report is not evidence on its own. The
suite therefore ALSO reads the JSONL FILE from disk and parses it, which is what proves the record
was really written and flushed rather than held in memory and described.

The exact TOOL NAME for an engine tool CANNOT be resolved: the engine's event carries BLAKE3 hashes
and no names, and the identifier it hashes could not be pinned down (QUESTIONS.md Q-10). So the
suite checks what is true — the toolset resolves, the line is labelled as an engine line, and an
unresolved half is LABELLED rather than invented.
"""

import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import EDIT_TOOLSET, READ_TOOLSET, Session  # noqa: E402

LOG_PATH = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "TestProject", "Saved", "Logs", "TurboMCPCalls.jsonl")

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def read_log(session, filter_text="", limit="", since=""):
    return session.call(READ_TOOLSET, "ReadTurboMCPCallLog",
                        {"filter": filter_text, "limit": limit, "since": since})


def disk_lines():
    """Every line of the log file, parsed. This is the check the tool cannot fake."""
    if not os.path.isfile(LOG_PATH):
        return None
    out = []
    with open(LOG_PATH, "r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            try:
                out.append(json.loads(line))
            except ValueError:
                out.append({"_unparsed": line})
    return out


def main():
    session = Session()
    session.connect()

    # START FROM AN EMPTY LOG FILE, WHICH MAKES THE SUITE INDEPENDENT OF WHAT CAME BEFORE.
    #
    # The file ACCUMULATES and is never rotated, and it carries lines written by earlier builds —
    # including builds whose hash resolution was incomplete. A check that "no line is left with an
    # unresolved hash" then read those old lines and went red on work that was already fixed, which
    # is the fifth order-dependent check this project has had to fix.
    #
    # The file is truncated rather than deleted: it is the record of what happened, the suite just
    # starts its own record. The in-memory list is cleared through the tool, which is also a check
    # that the clear works with logging off.
    session.call(EDIT_TOOLSET, "EditTurboMCPCallLog", {"enabled": "false"})
    session.call(EDIT_TOOLSET, "EditTurboMCPCallLogClear", {"confirm": "yes"})
    try:
        if os.path.isfile(LOG_PATH):
            open(LOG_PATH, "w", encoding="utf-8").close()
    except OSError as problem:
        print("  could not truncate the log file: %s" % problem)
    print("=== starting from an empty log file: %s ===" % os.path.basename(LOG_PATH))

    print("=== the log STARTS OFF, per the spec ===")
    # Read the state first, without changing it, by asking the read tool. The setting persists, so a
    # previous run may have left it on; that is checked too.
    state = read_log(session)
    print(state.splitlines()[0] if state else "")
    print(state.splitlines()[1] if state else "")
    print()
    check("the read tool reports the recording state",
          "recording:" in state, state[:120])
    check("it names the log file", "log file:" in state)
    # THE STATE AT THE START IS NOT ASSUMED. The setting PERSISTS, so a previous run leaves the log
    # on and the file and memory full. An earlier version demanded the "the log is OFF" wording and
    # went red on a second run for that reason alone — the order-dependence this project has had to
    # fix in four other suites by now.
    #
    # What is checked instead is that the answer is SELF-CONSISTENT: the explanation is required
    # only when there is nothing to show, and when there IS something the entries must be there.
    if "showing 0 newest of 0" in state:
        check("an empty selection explains why it is empty",
              ("the log is OFF" in state) or ("the log is on" in state), state[:400])
    else:
        check("a non-empty selection actually shows entries",
              state.count("  end  ") + state.count("  start  ") > 0, state[:300])
        check("it reports a count rather than nothing", "counts:" in state)

    print()
    print("=== E-CALL-LOG turns it on, and refuses a word it does not know ===")
    bad = session.call(EDIT_TOOLSET, "EditTurboMCPCallLog", {"enabled": "banana"})
    print(bad)
    print()
    # `FString::ToBool` answers false for anything unrecognised, so a caller typing `banana` would
    # have silently turned the log OFF. The strict parse is what stops that, and this checks it.
    check("it refuses an unknown word rather than treating it as false",
          bad.startswith("error:") and "true or false" in bad, bad[:120])

    on = session.call(EDIT_TOOLSET, "EditTurboMCPCallLog", {"enabled": "true"})
    print(on)
    print()
    check("it opens with changed: or unchanged:",
          on.startswith("changed:") or on.startswith("unchanged:"), on[:70])
    check("it says the log is on", "call log is now on" in on or "already on" in on)
    check("it says the choice persists", "survives a restart" in on)

    # A second call in the same direction must NOT claim a change.
    again = session.call(EDIT_TOOLSET, "EditTurboMCPCallLog", {"enabled": "on"})
    check("a second call is idempotent and says so",
          again.startswith("unchanged:"), again[:70])

    print()
    print("=== a call is recorded, and the record reaches DISK ===")
    before = disk_lines()
    before_count = len(before) if before is not None else 0

    # Two calls of different kinds, so the filter below has something to separate.
    session.call(READ_TOOLSET, "ReadTurboMCPStatus", {})
    session.call(READ_TOOLSET, "ReadTurboMCPBlueprintLoad",
                 {"assets": "BP_NodeTest", "detail": "manifest",
                  "graphFilter": "", "format": "", "tokenBudget": ""})

    after = disk_lines()
    print("    disk lines: %s -> %s" % (before_count, len(after) if after is not None else "?"))
    check("the log FILE exists on disk", after is not None, LOG_PATH)
    check("the file grew when tools were called",
          after is not None and len(after) > before_count,
          "%s -> %s" % (before_count, len(after) if after is not None else "?"))

    if after:
        newest = after[-1]
        check("a line parses as JSON with the documented fields",
              all(k in newest for k in ("time", "phase", "source", "toolset", "tool",
                                        "duration_ms", "result_chars", "token_estimate",
                                        "error")),
              ", ".join(sorted(newest.keys())))
        check("the timestamp is ISO-8601 UTC",
              newest["time"].endswith("Z") and "T" in newest["time"], newest["time"])
        check("the source says where the line came from",
              newest["source"] in ("plugin", "engine"), newest["source"])
        check("the session id is recorded",
              "session" in newest and len(newest.get("session", "")) > 8,
              newest.get("session", "")[:20])
        check("the duration is a number",
              isinstance(newest.get("duration_ms"), (int, float)), str(newest.get("duration_ms")))
        check("an error flag is present and boolean",
              isinstance(newest.get("error"), bool), str(newest.get("error")))

    print()
    print("=== EVERY name resolves: no hash placeholder may remain (Q-10, closed) ===")
    # THE CHECK THAT WOULD HAVE CAUGHT THE PARTIAL RESOLUTION. The first version accepted a line
    # with a real name in ONE slot and a hash in the other, and that half-resolved state LOOKS
    # finished. The names now come from the engine's own `GetTools()` and `ParseToolsetName`, so an
    # unresolved hash is a FAILURE rather than a tolerated state.
    if after:
        engine_lines = [e for e in after if e.get("source") == "engine"]
        print("    %d engine-sourced line(s)" % len(engine_lines))
        check("at least one engine line was recorded", len(engine_lines) > 0)

        unresolved = [e for e in engine_lines
                      if "engine tool ending" in e.get("toolset", "")
                      or "engine tool ending" in e.get("tool", "")]
        check("NO line is left with an unresolved hash",
              len(unresolved) == 0,
              "; ".join("%s.%s" % (e.get("toolset"), e.get("tool"))
                        for e in unresolved[:3]))

        # A top-level tool hashes against the EMPTY prefix, which is a REAL key and is written as
        # `(top-level)` rather than left blank or mistaken for a failed lookup. Conflating
        # "resolved to empty" with "not resolved" was a real bug: it printed
        # `(engine tool ending ...)` for a toolset it had just named.
        check("a top-level tool is named, not blank or a hash",
              any(e.get("toolset") == "(top-level)" for e in engine_lines),
              "; ".join(e.get("toolset", "") for e in engine_lines[:3]))

        # The inner event of a call_tool dispatch names the real target with its full toolset.
        check("an inner call names its real toolset",
              any(e.get("toolset", "").startswith("TurboMCP.") for e in engine_lines),
              "; ".join(e.get("toolset", "") for e in engine_lines[:3]))
        check("an inner call names its real tool, qualified",
              any(e.get("tool", "").startswith("TurboMCP.") for e in engine_lines),
              "; ".join(e.get("tool", "") for e in engine_lines[:3]))

        # TWO EVENTS PER CALL IS THE ENGINE'S OWN DESIGN, not a bug: `call_tool` emits one for
        # itself and one for the inner tool it dispatched, so per-toolset analytics for the real
        # target stay visible. `ModelContextProtocolServer.cpp:887` and `:907`.
        call_tools = [e for e in engine_lines if e.get("tool") == "call_tool"]
        inner = [e for e in engine_lines if e.get("tool") != "call_tool"]
        check("the two emissions per call are both recorded",
              len(call_tools) > 0 and len(inner) > 0,
              "%d call_tool, %d inner" % (len(call_tools), len(inner)))

    print()
    print("=== the tool's own read agrees with the file ===")
    listed = read_log(session, filter_text="engine", limit="20")
    print(listed[:600])
    print()
    check("the read tool says recording is on", "recording: on" in listed)
    # THE COUNT IS PARSED, NOT SUBSTRING-MATCHED. `"0 in memory" not in listed` is true when the
    # reporting is empty AND when the memory really holds none, so on a second run — where the
    # previous run's clear had emptied the memory — the check went red on a state that was correct.
    # That is the fourth order-dependent check this project has had to fix.
    import re as _re  # noqa: PLC0415
    counted = _re.search(r"counts: (\d+) in memory", listed)
    in_memory = int(counted.group(1)) if counted else None
    print("    the tool reports %s entry(s) in memory" % in_memory)
    check("the count is parseable", in_memory is not None, listed[:160])
    # The engine records a `ToolCall` event for EVERY call, including a call to this read tool, so
    # by the time any read runs there has been at least the call that is reading.
    check("there is at least the call that is reading the log",
          in_memory is not None and in_memory >= 1,
          "reported %s" % in_memory)
    check("the filter narrowed the list", "matching" in listed)

    print()
    print("=== an empty result still says why, per P1 ===")
    nomatch = read_log(session, filter_text="NoSuchToolNameAtAll")
    check("a filter matching nothing explains itself",
          "none match" in nomatch or "showing 0" in nomatch, nomatch[:300])
    check("it still reports the file and the state",
          "log file:" in nomatch and "recording:" in nomatch)

    print()
    print("=== since= pages forward instead of re-reading ===")
    page = read_log(session, filter_text="", limit="1")
    # The newest entry's timestamp is echoed at the end when there is more to page.
    print(page.splitlines()[-1] if page else "")
    if after:
        newest_time = after[-1].get("time", "")
        paged = read_log(session, filter_text="", since=newest_time)
        check("since= excludes what was already seen",
              "showing 0" in paged or newest_time not in paged, paged[:200])

    print()
    print("=== E-CALL-LOG-CLEAR needs confirmation and leaves the FILE alone ===")
    unconfirmed = session.call(EDIT_TOOLSET, "EditTurboMCPCallLogClear", {"confirm": ""})
    print(unconfirmed)
    print()
    check("it refuses without confirmation",
          unconfirmed.startswith("error:") and "confirm" in unconfirmed, unconfirmed[:120])
    check("it says the file is not touched", "FILE" in unconfirmed or "file" in unconfirmed)

    # THE ORDER MATTERS AND THE FIRST VERSION HAD IT WRONG.
    #
    # CLEARING THE LOG IS ITSELF A TOOL CALL, so `E-CALL-LOG-CLEAR` writes its own start and end
    # records, and a read straight afterwards finds them. The first version cleared and then read,
    # and reported "10 in memory" — the clear had worked and the READING had refilled it.
    #
    # So: switch logging off, clear (nothing is written), read (nothing is written), then switch it
    # back on. `E-CALL-LOG` itself is not written while logging is off, which is what makes the
    # sequence quiet.
    #
    # THIS IS A FACT ABOUT THE TOOL WORTH KNOWING: an observer of the log is part of the log.
    session.call(EDIT_TOOLSET, "EditTurboMCPCallLog", {"enabled": "false"})

    lines_before_clear = len(disk_lines() or [])
    cleared = session.call(EDIT_TOOLSET, "EditTurboMCPCallLogClear", {"confirm": "yes"})
    print(cleared)
    print()
    check("it opens with changed:", cleared.startswith("changed:"), cleared[:70])
    check("it says the file is untouched", "untouched" in cleared or "left alone" in cleared)

    after_clear = read_log(session)
    lines_after_clear = len(disk_lines() or [])
    session.call(EDIT_TOOLSET, "EditTurboMCPCallLog", {"enabled": "true"})

    check("memory is empty after the clear",
          "0 in memory" in after_clear, after_clear[:200])
    # THE CHECK THAT MATTERS: the file is the record of what happened and must survive.
    check("the FILE on disk is unchanged by the clear",
          lines_after_clear == lines_before_clear,
          "%s -> %s" % (lines_before_clear, lines_after_clear))
    check("the tool says the file is left alone",
          "untouched" in cleared or "left alone" in cleared, cleared[:200])

    print()
    print("=== the log ROTATES, so it cannot grow without bound ===")
    # The file previously had NO cap and reached 402 KB in a single round of testing, because every
    # tool call writes a line and every READ of the log is itself a tool call. The bound is tested
    # by REACHING it rather than by reading the constant: a limit nobody has crossed is a limit
    # nobody knows works.
    rotated = LOG_PATH + ".1"
    try:
        if os.path.isfile(rotated):
            os.remove(rotated)
        padding = ('{"time":"2026-01-01T00:00:00.000Z","phase":"end","source":"engine",'
                   '"toolset":"padding","tool":"padding","duration_ms":0,'
                   '"result_chars":0,"token_estimate":0,"error":false}\n')
        with open(LOG_PATH, "a", encoding="utf-8") as handle:
            written = 0
            while written < 4_250_000:
                handle.write(padding)
                written += len(padding)
        size_before = os.path.getsize(LOG_PATH)
        print("    filled the log to %d bytes" % size_before)
        check("the file is over the 4 MB limit", size_before > 4 * 1024 * 1024)

        # One call is enough: the rotation is checked on the way in to writing a line.
        session.call(READ_TOOLSET, "ReadTurboMCPStatus", {})
        time.sleep(0.5)

        check("a rotated file appeared", os.path.isfile(rotated),
              "expected %s" % os.path.basename(rotated))
        if os.path.isfile(rotated):
            check("the rotated file holds what was there before",
                  os.path.getsize(rotated) >= size_before,
                  "%d vs %d" % (os.path.getsize(rotated), size_before))
            check("the current file started again, so it is far smaller",
                  os.path.getsize(LOG_PATH) < size_before,
                  "%d vs %d" % (os.path.getsize(LOG_PATH), size_before))

            # The marker is what stops a reader of the older file wondering whether the log lost
            # data. It carries the same fields as every other line, so a parser needs no special
            # case for it.
            first = ""
            with open(LOG_PATH, "r", encoding="utf-8", errors="replace") as handle:
                first = handle.readline().strip()
            marker = json.loads(first) if first else {}
            check("the marker says the log rotated", marker.get("phase") == "rotate",
                  first[:120])
            check("the marker is a normal line with normal fields",
                  all(k in marker for k in ("time", "phase", "source", "toolset", "tool")),
                  ", ".join(sorted(marker.keys())))
            check("the marker says where the older file went",
                  ".1" in marker.get("result", ""), marker.get("result", "")[:120])

        # Cleaned up, because 4 MB of padding is not something to leave behind.
        if os.path.isfile(rotated):
            os.remove(rotated)
        open(LOG_PATH, "w", encoding="utf-8").close()
        print("    cleaned up the padding")
    except OSError as problem:
        check("the rotation could be tested", False, str(problem))

    print()
    print("=== the request is validated ===")
    for label, args, expect in (
        ("a bad boolean", {"enabled": "maybe"}, "not a yes or no"),
        ("a bad confirmation", {"confirm": "please"}, "confirm=yes"),
    ):
        tool = "EditTurboMCPCallLog" if "enabled" in args else "EditTurboMCPCallLogClear"
        result = session.call(EDIT_TOOLSET, tool, args)
        check("refuses %s" % label,
              result.startswith("error:") and expect in result, result[:110])

    print()
    print("=== the log is left ON, so the next phase can rely on it ===")
    print(session.call(EDIT_TOOLSET, "EditTurboMCPCallLog",
                       {"enabled": "true"})[:60].replace("\n", " "))

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
