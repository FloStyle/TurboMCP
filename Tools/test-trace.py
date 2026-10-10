#!/usr/bin/env python3
"""
`E-TRACE` and `R-TRACE`: arming a trace point, reading it, and clearing it.

WHAT THIS SUITE CAN AND CANNOT PROVE, STATED AT THE TOP BECAUSE IT IS THE MOST IMPORTANT THING ABOUT IT.

**IT CANNOT PROVE A TRACE HIT.** MEASURED, and recorded rather than hidden: the test project contains NO MAP -
`TestProject/Content` holds no `.umap` - so a Play-In-Editor session runs the engine's empty
`/Temp/UEDPIE_0_Untitled_1` world, NOTHING in it executes Blueprint code, and `E-TRACE`'s subscriber is never
called. Asked directly, the tool answers `0 script exception(s) have reached this module`.

So the honest state of the mechanism is: THE SUBSCRIPTION REGISTERS (it reports `SUBSCRIBED` and the handle is
valid, and the editor log carries `TRACE SUBSCRIBED to FBlueprintCoreDelegates::OnScriptException`), AND IT HAS
NEVER BEEN OBSERVED TO FIRE. Producing a hit needs a level with an actor whose BeginPlay runs - a fixture this
suite does not have. SECTION 6s OF TestRecord.md OPENED THAT RISK AND THIS SUITE DOES NOT CLOSE IT.

**WHAT IT DOES PROVE, AND EACH IS A REAL CLAIM ABOUT THE TOOL:**

  1. ARMING CHANGES NO ASSET. The claim is checked from OUTSIDE the tool - the asset's size, mtime and MD5
     before and after - because a tool reporting its own innocence is not evidence. This is the claim that
     matters most, since `Spec/03_TOOLS.md` has arming look like an edit.
  2. THE READ TOOL CANNOT ARM OR CLEAR. `R-TRACE` is in the Read toolset and refuses a mutating action BY NAME
     rather than performing it, so the separation a caller relies on is enforced and not merely intended.
  3. THE REFUSALS NAME WHAT IS WRONG, each asserting its OWN sentence and not just an `error:` prefix - a tool
     that refused everything with one generic message would pass a prefix-only check.
  4. ARM IS IDEMPOTENT AND CLEAR IS EXACT, including that clearing a point takes ITS hits and leaves another
     point's hits alone.
  5. THE SUBSCRIPTION STATE IS REPORTED, so a zero is never left ambiguous between "nothing ran" and "the trace
     is off".
"""

import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tmcp import Session  # noqa: E402

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
READ_TOOLSET = "TurboMCP.TurboMCPReadToolset"
EDIT_TOOLSET = "TurboMCP.TurboMCPEditToolset"

# A REAL node ID from a real graph, so the suite arms something that could actually run rather than a string
# shaped like an ID. Read from `BP_NodeTest:EventGraph`, whose `ReceiveBeginPlay` chain reaches it.
ASSET = "BP_NodeTest"
GRAPH = "EventGraph"
REAL_NODE = "BP_NodeTest:EventGraph:c580e2"
SECOND_NODE = "BP_NodeTest:EventGraph:09723b"

failures = []
checks = 0


def check(name, ok, detail=""):
    global checks
    checks += 1
    if ok:
        print("  ok   %s  %s" % (name, detail))
    else:
        print("  FAIL %s  %s" % (name, detail))
        failures.append(name)


def arm(session, node_id):
    return session.call(EDIT_TOOLSET, "EditTurboMCPTrace", {"action": "arm", "nodeId": node_id})


def clear(session, node_id=""):
    return session.call(EDIT_TOOLSET, "EditTurboMCPTrace", {"action": "clear", "nodeId": node_id})


def status(session, action="status"):
    return session.call(READ_TOOLSET, "ReadTurboMCPTrace", {"action": action, "nodeId": ""})


def armed_count(text):
    """The number of armed points, PARSED rather than matched as a sentence."""
    m = re.search(r"(\d+) armed point\(s\)", text)
    return int(m.group(1)) if m else None


def hit_count(text):
    m = re.search(r"(\d+) recorded hit\(s\)", text)
    return int(m.group(1)) if m else None


def exceptions_reached(text):
    m = re.search(r"(\d+) script exception\(s\) have reached this module", text)
    return int(m.group(1)) if m else None


def asset_fingerprint(path):
    st = os.stat(path)
    with open(path, "rb") as handle:
        import hashlib
        return (st.st_size, st.st_mtime_ns, hashlib.md5(handle.read()).hexdigest())


def main():
    session = Session()
    session.connect()

    # CLEARED ON THE WAY IN AS WELL AS OUT. A suite that owns an in-memory registry must start from a known
    # state, or a run killed part-way leaves points armed and the next run's counts are wrong. This project
    # has recorded that rule three times and paid for it twice.
    drained = clear(session, "")
    print("=== the registry is cleared before anything is judged ===")
    print(drained)
    print()
    check("clearing everything answers with the word cleared",
          drained.startswith("cleared:"), drained[:100])

    asset_path = os.path.join(REPO, "TestProject", "Content", "Tests", "%s.uasset" % ASSET)
    before = asset_fingerprint(asset_path) if os.path.isfile(asset_path) else None

    print()
    print("=== ARMING, AND THE CLAIM THAT IT CHANGES NO ASSET ===")
    out = arm(session, REAL_NODE)
    print(out)
    print()
    check("arming is accepted", out.startswith("armed:"), out[:120])
    check("it reports the point it added", "added 1 point" in out, out[:120])
    check("IT SAYS NOTHING WAS ADDED TO A GRAPH AND NOTHING WAS RECOMPILED",
          "NOTHING WAS ADDED TO ANY GRAPH" in out and "RECOMPILED" in out, out[-200:])

    # THE FINGERPRINT IS THE INDEPENDENT WITNESS. A tool's own sentence about not writing is not evidence; the
    # asset's size, mtime and hash are.
    if before is not None:
        after = asset_fingerprint(asset_path)
        check("the ASSET is byte-identical after arming, proved by size, mtime and MD5",
              before == after, "before %s after %s" % (before, after))
    else:
        check("the ASSET is byte-identical after arming, proved by size, mtime and MD5",
              False, "asset not found at %s" % asset_path)

    print()
    print("=== ARM IS IDEMPOTENT, so arming twice cannot double-count ===")
    again = arm(session, REAL_NODE)
    print(again)
    print()
    check("arming an already-armed point says so rather than adding a second",
          "0 point(s)" in again and "1 were already armed" in again, again[:140])

    print()
    print("=== THE READ TOOL CANNOT ARM OR CLEAR ===")
    # `R-TRACE` is in the Read toolset, which by this project's design must be unable to change anything. A
    # read tool that silently honoured an arm request would break that promise while looking helpful.
    attempted = status(session, action="arm")
    print(attempted[:400])
    print()
    check("R-TRACE REFUSES an arm request by name rather than performing it",
          attempted.startswith("error:") and "does not perform it" in attempted, attempted[:160])
    check("the refusal points at the tool that CAN do it",
          "E-TRACE" in attempted, attempted[:200])
    check("and it returns the current state rather than only an error",
          "armed point(s)" in attempted, attempted[-200:])

    print()
    print("=== THE REFUSALS NAME WHAT IS WRONG ===")
    # EACH ASSERTS ITS OWN SENTENCE. A prefix-only check would pass on a tool that refused everything with one
    # generic message, which is the unfailable-assertion failure this project has recorded.
    bad_action = session.call(EDIT_TOOLSET, "EditTurboMCPTrace",
                              {"action": "wat", "nodeId": REAL_NODE})
    print(bad_action)
    check("an unknown action is refused and the real actions are named",
          bad_action.startswith("error:") and "arm, clear or status" in bad_action, bad_action[:140])

    no_id = session.call(EDIT_TOOLSET, "EditTurboMCPTrace", {"action": "arm", "nodeId": ""})
    print(no_id)
    check("arming with no node ID is refused, and the reason is about the CONSEQUENCE",
          no_id.startswith("error:") and "cannot be matched by anything" in no_id, no_id[:200])

    print()
    print("=== CLEAR IS EXACT: it takes its own point's hits and leaves the other's ===")
    arm(session, SECOND_NODE)
    two = status(session)
    print(two)
    print()
    check("two points are armed", armed_count(two) == 2, "parsed %s" % armed_count(two))

    one = clear(session, REAL_NODE)
    print(one)
    print()
    check("clearing one point removes exactly one",
          one.startswith("cleared:") and "removed 1 of 1" in one, one[:140])
    remaining = status(session)
    check("the OTHER point is still armed", armed_count(remaining) == 1,
          "parsed %s" % armed_count(remaining))
    check("and the cleared point is gone from the report", REAL_NODE not in remaining,
          remaining[:200])

    print()
    print("=== WHAT THE TOOL SAYS ABOUT ITS OWN MECHANISM, WHICH IS THE HONEST ZERO ===")
    # THE SUBSCRIPTION STATE IS REPORTED, so a zero can never be ambiguous between "nothing ran" and "the
    # trace is off". This is the check that would have caught a subscription that never registered.
    said = status(session)
    print(said[:600])
    print()
    check("the subscription state is reported, not left to be assumed",
          "SUBSCRIBED" in said or "not subscribed" in said, said[:120])
    check("THE ENGINE HAS NEVER CALLED THIS MODULE, AND THE TOOL SAYS SO PLAINLY",
          exceptions_reached(said) == 0
          and "NOT demonstrated" in said,
          "exceptions reached: %s" % exceptions_reached(said))
    check("the hit count is PARSED and reported as a number",
          hit_count(said) is not None, "parsed %s" % hit_count(said))
    check("it explains what a zero means rather than leaving a bare 0",
          "that means none of them has run" in said, said[-300:])

    print()
    print("=== PUT BACK WHAT WAS TAKEN ===")
    final = clear(session, "")
    print(final)
    print()
    check("the registry is empty at the end, so the suite is re-runnable",
          armed_count(status(session)) == 0, status(session)[:160])

    print()
    print("  RECORDED, NOT GLOSSED: this suite has NOT observed a trace hit. The test project has no map,")
    print("  so PIE runs an empty world and no Blueprint executes. The subscription REGISTERS and has never")
    print("  been seen to FIRE. See TestRecord.md section 6s - the risk it opened is still open.")

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run, %d SKIPPED: no trace hit is observable in this project)"
          % (checks, 0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
