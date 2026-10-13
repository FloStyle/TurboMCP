# ACCEPTANCE ROW THIS SUITE CLAIMS TO PROVE: T-BP-7 IS NOT THIS - this proves T01's second requirement: the reader
# refuses at the floor of its detail ladder instead of recursing until the stack dies.
"""A read whose budget cannot be met at manifest detail REFUSES, and the editor survives it.

WHAT CRASHED
    The report: "the blueprint reader stopped the editor with a stack overflow, because its main read function called
    itself repeatedly. The call was at manifest detail, with a graph filter, on a Blueprint with 38 graphs and about
    960 nodes." *** `Read` CALLED `Read` IN TWO PLACES, AND THE SECOND SET `Manifest` AGAIN FROM A CALL THAT WAS
    ALREADY AT `Manifest`: nothing changed between one call and the next, so a manifest that exceeded the budget
    recursed until the stack died. ***

WHY THE 38 GRAPHS ARE NOT NEEDED TO REACH IT
    *** THE CONDITION IS NOT THE GRAPH COUNT, IT IS `Result.Text.Len() > InRequest.TokenBudgetChars` AT `Manifest`
    DETAIL - SO ANY BLUEPRINT WHOSE MANIFEST IS LONGER THAN THE BUDGET REPRODUCES IT, AND THE 38 GRAPHS ONLY MADE THE
    MANIFEST LONGER. A BUDGET OF ONE CHARACTER REACHES THE FLOOR ON ANY ASSET. ***

WHAT IS CHECKED
    1. the control: the same read with no budget succeeds and prints its manifest, so a suite that passes because
       everything refuses cannot pass this one
    2. the crash path: at manifest detail with a budget of one character, the answer NAMES the limit rather than
       recursing - and the call RETURNING AT ALL is the evidence that the editor survived it
    3. the ladder above the floor still works: a tight budget at a higher detail reduces rather than refuses, so the
       floor has not swallowed the path that is supposed to succeed
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import READ_TOOLSET, Session  # noqa: E402

ASSET = "/Game/Tests/BP_NodeTest"

# The exact words the floor branch prints, and the reason a substring is the right test: the sentence names the
# asset and both numbers, so asserting the whole thing would make this suite fail on a wording change that is not a
# regression.
FLOOR_MARKER = "the budget cannot be met"

failures = []


def check(name, ok, detail=""):
    print("  %-4s %s%s" % ("ok" if ok else "FAIL", name, ("  " + detail) if detail else ""))
    if not ok:
        failures.append(name)


def load(session, detail, budget=""):
    return session.call(READ_TOOLSET, "ReadTurboMCPBlueprintLoad", {
        "assets": ASSET, "detail": detail, "graphFilter": "",
        "format": "", "tokenBudget": budget,
    })


def main():
    session = Session()

    # 1. The control. Without this, a reader that refused everything would pass this suite.
    control = load(session, "manifest")
    check("manifest with no budget prints the asset", "read:" in control, control.splitlines()[0][:90] if control else "")
    check("manifest with no budget does not claim a limit", FLOOR_MARKER not in control)

    # 2. The crash path. The call returning is the point: before the fix this frame recursed until the stack died.
    floor = load(session, "manifest", budget="1")
    check("manifest with a budget of one character refuses", FLOOR_MARKER in floor,
          next((line.strip() for line in floor.splitlines() if FLOOR_MARKER in line), "")[:110])
    check("the refusal names the asset", ASSET in floor)
    check("the refusal says the budget cannot be met rather than returning empty", len(floor.strip()) > 0)

    # 3. The ladder above the floor still reduces rather than refusing, so the floor has not swallowed the good path.
    reduced = load(session, "bodies", budget="2000")
    check("a tight budget above the floor still answers", "read:" in reduced)
    check("and it either reduced or fit, never refused", FLOOR_MARKER not in reduced or "reduced" in reduced,
          next((line.strip() for line in reduced.splitlines() if "reduced" in line), "")[:100])

    print("")
    if failures:
        print("RESULT: FAIL (%d check(s) failed: %s)" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (the floor refuses by name, and the ladder above it still answers)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
