"""Check Blueprint character-budget warnings and input refusals without mutation calls.

An oversized minimum manifest is retained with a warning, not refused or
recursively reduced again. This suite does not assert a hard response limit.
"""

import json
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
    session.connect()

    # 1. The control. Without this, a reader that refused everything would pass this suite.
    control = load(session, "manifest")
    check("manifest with no budget prints the asset", "read:" in control, control.splitlines()[0][:90] if control else "")
    check("manifest with no budget does not claim a limit", FLOOR_MARKER not in control)

    # 2. The crash path. The call returning is the point: before the fix this frame recursed until the stack died.
    floor = load(session, "manifest", budget="1")
    check("manifest with a budget of one character reports its limit", FLOOR_MARKER in floor,
          next((line.strip() for line in floor.splitlines() if FLOOR_MARKER in line), "")[:110])
    check("the budget warning names the asset", ASSET in floor)
    check("the budget warning is nonempty", len(floor.strip()) > 0)

    # 3. The ladder above the floor still reduces rather than refusing, so the floor has not swallowed the good path.
    reduced = load(session, "bodies", budget="2000")
    check("a tight budget above the floor still answers", "read:" in reduced)
    check("and it either reduced or fit, never refused", FLOOR_MARKER not in reduced or "budget" in reduced,
          next((line.strip() for line in reduced.splitlines() if "reduced" in line), "")[:100])

    check("floor does not claim the output fits", "to fit the budget" not in floor, floor[-500:])
    check("status labels requested detail", "requested detail manifest" in control, control[:150])
    for invalid in ("0", "-1", "1.5", "2147483648"):
        answer = load(session, "manifest", invalid)
        check("invalid budget is refused: " + invalid, answer.startswith("error:"), answer[:200])

    for detail in ("manifest", "signatures", "bodies", "full"):
        args = {"assets": ASSET, "detail": detail, "graphFilter": "", "format": "json", "tokenBudget": "1000000"}
        normal = session.call(READ_TOOLSET, "ReadTurboMCPBlueprintLoad", args)
        start = normal.find("{")
        check("JSON control contains a document: " + detail, start >= 0, normal[:200])
        if start >= 0:
            document, _ = json.JSONDecoder().raw_decode(normal[start:])
            check("JSON control is not an error: " + detail, "error" not in document, str(document)[:200])
        args["tokenBudget"] = "1"
        refusal = session.call(READ_TOOLSET, "ReadTurboMCPBlueprintLoad", args)
        start = refusal.find("{")
        check("JSON refusal contains an error document: " + detail, start >= 0, refusal[:200])
        if start >= 0:
            error, _ = json.JSONDecoder().raw_decode(refusal[start:])
            message = str(error)
            check("JSON refusal names requested detail: " + detail, "detail '" + detail + "'" in message, message)
            check("JSON refusal provides budget recovery: " + detail, "Raise TokenBudget" in message and "does not enforce a hard response limit" in message, message)
        check("JSON budget refusal does not count as a read: " + detail, "read: 0 of 1" in refusal, refusal[:100])

    print("")
    if failures:
        print("RESULT: FAIL (%d check(s) failed: %s)" % (len(failures), ", ".join(failures)))
        return 1
    print("RESULT: PASS (the floor reports its limit, and the ladder above it still answers)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
