# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: NONE (T-DBG-1 is claimed by `test-call-log`; this adds the file-level
# invariants T02 asks for, which no row names)
"""The call log file: a size is never a 0 that means "unknown", and a start with no end is still a record.

WHY THIS IS A SUITE AND NOT A READING
    T02's report was "Agent calls show 0 characters and 0 tokens", and the fix was to make an unmeasured size `null`
    in the log rather than a default `0` that reads as a measurement. *** THE LOG FILE IS THE ONE ARTEFACT A SUITE CAN
    READ THAT THE EDITOR'S OWN CODE WRITES, SO THIS CHECKS THE FILE RATHER THAN THE FUNCTIONS AROUND IT. ***

    AND IT IS THE CRASH CASE THAT MAKES IT WORTH RUNNING: *** A `start` RECORD WITH NO `end` IS THE CALL THAT WAS IN
    FLIGHT WHEN SOMETHING WENT WRONG. THE FILE MUST KEEP IT - a log that only records completed calls cannot name the
    call that killed the session, which is the whole reason the start record exists. ***

WHAT IT CHECKS
    1. THE FILE EXISTS AND HAS RECORDS. *** THE CALLS ARE MADE BY THIS SUITE, SO AN EMPTY FILE IS A FAILURE AND NOT A
       SKIP - A CHECK THAT PASSES WHEN IT FOUND NOTHING TO CHECK IS THE PASS THIS PROJECT BLOCKS SUITES FOR. ***
    2. every line is a JSON object, because the file is appended line by line and one malformed line would make every
       reader of it unreliable.
    3. NO ENGINE-SOURCED RECORD CARRIES A ZERO SIZE. `null` is the honest value for a size the engine's event does not
       carry; `0` is the value this suite exists to catch. *** THE CHECK IS CONDITIONAL ON AN ENGINE RECORD EXISTING
       AND THE SUITE SAYS SO RATHER THAN PASSING QUIETLY WHEN THERE IS NONE. ***
    4. THE START/END ORDERING HOLDS: every `start` has an `end` after it, UNLESS IT IS THE LAST RECORD IN THE FILE,
       which is exactly the shape a crash leaves.
"""

import json
import os
import pathlib
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import READ_TOOLSET, Session  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[1]
LOG_NAME = "TurboMCPCalls.jsonl"
ASSET = "/Game/Tests/BP_NodeTest"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def find_log():
    """The log, wherever the project put it. The path is `Saved/Logs/TurboMCPCalls.jsonl` under the project."""
    for candidate in ROOT.glob("*/Saved/Logs/" + LOG_NAME):
        return candidate
    return None


def main():
    session = Session()

    # Make the calls this suite then reads. Five, as T02's verify step asks, and each one is a real tool call.
    for index in range(5):
        session.call(READ_TOOLSET, "ReadTurboMCPBlueprintLoad",
                     {"assets": ASSET, "detail": "manifest", "graphFilter": "",
                      "format": "json", "tokenBudget": ""})

    log = find_log()
    check("the log file exists where the code says it writes", log is not None,
          str(log) if log else "looked for */Saved/Logs/" + LOG_NAME)
    if log is None:
        print("")
        print("RESULT: FAIL (no log file, so nothing below could be checked)")
        return 1

    lines = [line for line in log.read_text(encoding="utf-8", errors="replace").splitlines() if line.strip()]
    check("the file has records, and this suite made some of them", len(lines) > 0, "%d line(s)" % len(lines))
    if not lines:
        print("")
        print("RESULT: FAIL (the log is empty after five calls)")
        return 1

    records = []
    malformed = 0
    for line in lines:
        try:
            records.append(json.loads(line))
        except ValueError:
            malformed += 1
    check("every line is a JSON object", malformed == 0, "%d malformed" % malformed)

    # 3. The fix: an engine-sourced record has no size, and says so with null.
    engine = [r for r in records if r.get("source") == "engine"]
    zero_sizes = [r for r in engine
                  if r.get("result_chars") == 0 or r.get("token_estimate") == 0]
    if engine:
        check("no engine-sourced record reports a size of zero", not zero_sizes,
              "%d with a zero out of %d" % (len(zero_sizes), len(engine)))
        check("and the engine-sourced records carry null instead",
              all(r.get("result_chars") is None for r in engine),
              "result_chars values: %s" % sorted({repr(r.get("result_chars")) for r in engine}))
    else:
        # Not a silent pass: the run says which half of the check it could not make.
        print("  --   no engine-sourced record in this file, so the null-instead-of-zero check was not exercised")

    # 4. Every start has an end, unless it is the last record - which is what a crash leaves.
    starts = [i for i, r in enumerate(records) if r.get("phase") == "start"]
    ends = [i for i, r in enumerate(records) if r.get("phase") == "end"]
    orphans = [i for i in starts if not any(e > i for e in ends) and i != len(records) - 1]
    check("every start is followed by an end, or is the last record", not orphans,
          "%d orphaned start(s) at %s" % (len(orphans), orphans[:4]))
    check("the suite's own calls left records", len(records) >= 5, "%d record(s)" % len(records))
    # *** THE DESIGN, MEASURED: THE PLUGIN PATH WRITES A `start` AND THE AGENT PATH CANNOT - the engine's event
    # carries the duration after the call and nothing before it. So a record with no start is NOT a lost start, it is
    # the other path, AND `RenderTimeline()` NOW SAYS SO ON THE ROW. THIS CHECK ASSERTS THE STATED DESIGN; THE ONE IT
    # REPLACED ASSERTED FIVE STARTS, WHICH THE MEASUREMENT SAYS CANNOT HAPPEN. ***
    startless = [r for i, r in enumerate(records)
                 if r.get("phase") == "end" and not any(s < i for s in starts)]
    check("every record with no start is the engine's path, not a lost one",
          all(r.get("source") == "engine" for r in startless),
          "%d start(s), %d start-less end(s)" % (len(starts), len(startless)))

    print("")
    if failures:
        print("RESULT: FAIL (%d check(s) failed: %s)" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d record(s): every line parses, no zero stands for an unknown size, and no start was lost)"
          % len(records))
    return 0


if __name__ == "__main__":
    sys.exit(main())
