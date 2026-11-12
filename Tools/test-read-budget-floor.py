"""Check requested Blueprint detail, budget refusals and recovery without mutation calls."""

import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from tmcp import READ_TOOLSET, Session  # noqa: E402

ASSET = "/Game/Tests/BP_NodeTest"

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
    check("manifest with no budget does not claim a limit", "error:" not in control)

    # An oversized document must be refused at the requested detail, never downgraded.
    import re
    for detail in ("manifest", "signatures", "bodies", "full"):
        refused = load(session, detail, "1")
        check("budget refusal does not count as read: " + detail, "read: 0 of 1" in refused, refused[:180])
        check("refusal preserves requested detail: " + detail, "requested detail '" + detail + "'" in refused)
        needed = re.search(r"requires (\d+) characters", refused)
        check("refusal gives required size: " + detail, needed is not None)
        if needed:
            recovered = load(session, detail, needed.group(1))
            check("retry at required size succeeds: " + detail, "read: 1 of 1" in recovered, recovered[:180])
            check("retry does not downgrade: " + detail, "were dropped" not in recovered and "character-budget notes" not in recovered)

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
    print("RESULT: PASS (requested detail, budget refusal and recovery checked)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
