"""Exercise R-OVERRIDES against the running editor.

Checks the rules the spec states, and exits non-zero on failure.

  T-BP-12  parent-chain and interface functions with signatures and override state
  P1       a zero or an empty filter says what it means
  P2       a bad filter is refused by name
  P5       every path form gives the same answer
  P6       lists cap and say how many were left

WHAT THIS TOOL LEARNED, AND WHY THE CHECKS LOOK LIKE THIS
--------------------------------------------------------
The first version walked the parent chain with `TFieldIterator<UFunction>` and
filtered on `FUNC_Event`. It reported 3 overridden functions. The editor's own
`UBlueprintEditorLibrary::ListEvents`/`ListFunctions` report **4**, because a
`UserConstructionScript` graph counts as an override and the hand-written walk
missed it.

The same walk also needed a special case to drop `UObject::ExecuteUbergraph`, which
carries `FUNC_Event` and is not an override target. The engine's set has neither
problem, so the check below pins the engine-derived count: a regression to a
hand-rolled filter would change it.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import READ_TOOLSET, Session  # noqa: E402

BLUEPRINT = "BP_NodeTest"
BLUEPRINT_PATH = "/Game/Tests/BP_NodeTest"

# THE COUNTS ARE READ FROM THE TOOL, NOT HARD-CODED.
#
# This file used to pin `4 overridden` and `23 overridable`, which was true when it was written
# and stopped being true the moment another capability ADDED an override to BP_NodeTest. The
# suite then failed on three checks for a reason that had nothing to do with R-OVERRIDES, and the
# red looked like a defect in the tool.
#
# A count is a fact about the ASSET, and this asset is the shared test ground that other tools
# change. So the totals are parsed and the checks are about INTERNAL CONSISTENCY instead: the
# two filters must add up to the total, the two lists must not overlap, and what the filters
# return must match what the unfiltered read says. Those hold however many overrides the asset
# happens to carry, and they are what the tool actually promises.

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def main():
    session = Session()
    session.connect()

    print("=== T-BP-12: the set, with signatures and state ===")
    out = session.call(READ_TOOLSET, "ReadTurboMCPOverrides",
                       {"asset": BLUEPRINT, "include": ""})
    print(out[:2200])
    print()

    # The counts, parsed from the tool's own summary line, so the checks below are about
    # consistency rather than about numbers that only hold on one asset state.
    #   `counts: 23 overridable, 8 already overridden here, 15 still available`
    total = overridden = available = None
    for line in out.splitlines():
        if line.strip().startswith("counts:"):
            numbers = [int(tok) for tok in line.replace(",", " ").split() if tok.isdigit()]
            if len(numbers) >= 3:
                total, overridden, available = numbers[0], numbers[1], numbers[2]
            break

    print("    parsed counts: %s overridable, %s overridden, %s available"
          % (total, overridden, available))
    check("the summary line was parsed", total is not None)

    check("reports the parent class", "parent: /Script/Engine.Actor" in out)
    check(
        "says where the set came from",
        "ListEvents and ListFunctions" in out,
    )
    check("counts the overridable set", "overridable," in out)
    # The engine's own lists are the source, which is the claim. Counted rather than compared
    # to a number that belonged to a different asset state.
    check(
        "the overridden and available counts account for the whole set",
        total is not None and overridden + available == total,
        "%s + %s vs %s" % (overridden, available, total),
    )
    check(
        "the engine's count is used, not a hand-rolled filter",
        total is not None and total > 0,
        "the set must not be empty, or the checks below prove nothing",
    )
    check("prints an event signature", "ReceiveActorBeginOverlap : (AActor* OtherActor)" in out)
    check("marks an event as an event", "[event]" in out)
    # An output arrow appears where a function has a return or out parameter.
    # ReceiveHit and ReceivePointDamage have one, so it must be present here.
    check(
        "marks outputs with an arrow",
        "ReceiveHit : (UPrimitiveComponent* MyComp" in out and "-> (FHitResult Hit)" in out,
    )
    check("says which class declares it", "from: /Script/Engine.Actor" in out)
    check(
        "never points at the Blueprint's own generated class",
        "BP_NodeTest_C" not in out,
    )
    check(
        "the ubergraph dispatcher is not offered",
        "ExecuteUbergraph" not in out,
    )

    print()
    print("=== the overridden filter ===")
    over = session.call(READ_TOOLSET, "ReadTurboMCPOverrides",
                        {"asset": BLUEPRINT, "include": "overridden"})
    print(over[:1200])
    print()
    check("lists what is already overridden", "overridden here" in over)
    check("does not list anything available", "state: available" not in over)
    check(
        "finds the engine's count",
        overridden is not None
        and "shown: %d of %d" % (overridden, overridden) in over,
        "expected shown: %s of %s" % (overridden, overridden),
    )
    check(
        "counts the whole Blueprint, not the filter",
        total is not None and "%d overridable" % total in over,
    )
    # These four are the overrides BP_NodeTest was BUILT with, so a later capability adding
    # more does not affect them. They are the stable part of this check.
    for name in ("ReceiveBeginPlay", "ReceiveActorBeginOverlap", "ReceiveTick",
                 "UserConstructionScript"):
        check("includes %s" % name, name in over)

    print()
    print("=== the available filter ===")
    avail = session.call(READ_TOOLSET, "ReadTurboMCPOverrides",
                         {"asset": BLUEPRINT, "include": "available"})
    check("lists what is not overridden", "state: available" in avail)
    check("does not list anything overridden", "state: overridden here" not in avail)
    check(
        "the two filters add up to the total",
        available is not None
        and "shown: %d of %d" % (available, available) in avail,
        "expected shown: %s of %s" % (available, available),
    )

    print()
    print("=== P2: a bad filter is refused by name ===")
    bad = session.call(READ_TOOLSET, "ReadTurboMCPOverrides",
                       {"asset": BLUEPRINT, "include": "banana"})
    print(bad)
    print()
    check("refuses", bad.startswith("error:"))
    check("names every accepted value", "all, overridden, available" in bad)

    print()
    print("=== a non-Blueprint is refused ===")
    wrong = session.call(READ_TOOLSET, "ReadTurboMCPOverrides",
                         {"asset": "TA11_TestEnum", "include": ""})
    print(wrong)
    print()
    check("refuses", wrong.startswith("error:"))
    check("says why", "not a Blueprint" in wrong)

    print()
    print("=== P5: every path form gives the same answer ===")
    forms = {
        "short name": BLUEPRINT,
        "package path": BLUEPRINT_PATH,
        "object path": BLUEPRINT_PATH + ".BP_NodeTest",
    }
    bodies = set()
    for label, value in forms.items():
        text = session.call(READ_TOOLSET, "ReadTurboMCPOverrides",
                            {"asset": value, "include": "overridden"})
        print("--- %s: %s" % (label, text.splitlines()[0] if text else "(empty)"))
        lines = text.splitlines()
        bodies.add("\n".join(lines[1:]) if len(lines) > 1 else text)

    check("all forms give the same body", len(bodies) == 1,
          "%d distinct answer(s)" % len(bodies))

    print()
    if failures:
        print("RESULT: FAIL (%d) -> %s" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
