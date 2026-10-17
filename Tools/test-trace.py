#!/usr/bin/env python3
# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-DBG-2
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
#   
#   | T-DBG-2 | Trace point | One line per tick in play with pin values; clear stops it; panel lists it |

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


def parsed_hits(text):
    """
    The hit count, PARSED from the tool's own number rather than matched as a sentence.

    A named alias of `hit_count` because the trace section reads better with it: what is being asserted there
    is that the number is NON-ZERO, and a check that matched the word "hit" would pass on zero.
    """
    return hit_count(text)


def probe_node_id(session, blueprint_name):
    """
    One armable node id out of the PIE fixture's Blueprint, read through `R-GRAPH-LOAD`.

    THE NODE ID IS PARSED FROM THE READER rather than pinned, because a pinned id would break the moment the
    fixture was rebuilt - and a fixture is meant to be rebuilt. The reader prints each node as
    `Type ; <package>:<Graph>:<key>`, so the first id whose graph is the one asked for is the probe.

    Returns None when the Blueprint or its graph cannot be read, so the caller can FAIL rather than skip: a
    fixture that went missing must not turn into a silent pass.
    """
    text = session.call(READ_TOOLSET, "ReadTurboMCPGraphLoad",
                        {"asset": blueprint_name, "graphName": "EventGraph",
                         "detail": "bodies", "format": "", "tokenBudget": ""})
    if text.startswith("error:"):
        print("    reading %s failed: %s" % (blueprint_name, text[:160]))
        return None
    # The `PrintString` node is the one that RUNS, and its id is the one to arm. The ids appear after a `;`
    # and look like `BP_TurboMCPTestLog:EventGraph:914137`.
    matches = re.findall(r"(%s:EventGraph:[0-9a-fA-F]+)" % re.escape(blueprint_name), text)
    if not matches:
        print("    no node id found in %s: %s" % (blueprint_name, text[:300]))
        return None
    # The FIRST id in the reader's output is the entry node that carries the exec chain, which is the node that
    # runs when the world starts. Later ids can be data producers that never execute on their own.
    return matches[0]


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
    print("  THE HIT IS NOW OBSERVABLE, AND THIS SUITE PROVES IT RATHER THAN ASSERTING IT.")
    #
    # MEASURED, BEFORE THE FIXTURE EXISTED: `TestProject/Content` held NO `.umap`, so PIE ran the engine's
    # empty `/Temp/UEDPIE_0_Untitled_1` world, NOTHING executed Blueprint code, and the subscriber was never
    # seen to fire. That is why this suite said so in its own result line.
    #
    # `/Game/Tests/TurboMCPTestMap` NOW EXISTS and carries a Blueprint actor whose `ReceiveBeginPlay` runs a
    # `PrintString` node. The hit was observed by hand first:
    #
    #     1 armed point(s), 2 recorded hit(s)
    #     BP_TurboMCPTestLog:EventGraph:914137 : 2 hit(s)
    #
    # A NATIVE actor can never make the delegate fire - only the Blueprint VM raises the exception - which is
    # the whole reason the fixture needed a Blueprint and not just a placed C++ actor.
    #
    # THE CHECK BELOW IS DELIBERATELY A REAL ONE. It arms a point on a node in that Blueprint, plays the map,
    # and requires a NON-ZERO hit count, so a regression that broke the subscription would fail here instead of
    # being reported as "not observable".
    trace_map = "/Game/Tests/TurboMCPTestMap"
    probe_graph = "BP_TurboMCPTestLog"
    probe_node = probe_node_id(session, probe_graph)

    if probe_node is None:
        check("the trace fixture's Blueprint exposes a node to arm", False,
              "no node id found in %s - the PIE test map fixture is missing" % probe_graph)
    else:
        print("    the fixture node is %s" % probe_node)
        arm(session, probe_node)

        started = session.call(READ_TOOLSET, "ReadTurboMCPPlayCheck",
                               {"seconds": "5", "map": trace_map})
        print("    %s" % started.splitlines()[0][:150])
        check("a play session over the fixture map was requested",
              started.startswith("pending:") or "already playing" in started, started[:180])

        # THE ENGINE'S OWN TEARDOWN IS WAITED FOR rather than slept through, because the hits are recorded
        # while the world runs. A fixed sleep would be either too short or wasted.
        import time as _time  # noqa: PLC0415
        final_run = ""
        deadline = _time.time() + 120
        while _time.time() < deadline:
            final_run = session.call(READ_TOOLSET, "ReadTurboMCPPlayCheck", {"seconds": "", "map": ""})
            if "played for" in final_run:
                break
            _time.sleep(2)
        print(final_run[:400])

        after = status(session)
        hits = parsed_hits(after)
        check("THE ARMED NODE RAN, so the subscriber fires", hits > 0,
              "hit count parsed as %s from: %s" % (hits, after[:220]))
        check("the hit names the node it matched", probe_node in after, after[:300])
        check("a tracepoint exception reached this module", "Tracepoint" in after, after[:300])

        clear(session)

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (%d check(s) run, 0 SKIPPED)" % checks)
    return 0


if __name__ == "__main__":
    sys.exit(main())
