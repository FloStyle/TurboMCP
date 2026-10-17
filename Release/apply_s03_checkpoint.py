# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""Applies the round-4 S03 checkpoint to Release/PROGRESS.json.

E01 now passes; the first L3 observation of this run was taken (E06). S03 stays OPEN because the focused
function suite fails five checks on its signature case, and because R08's finding needed a broad fix that is
recorded rather than patched.
"""

from __future__ import annotations

import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROGRESS = os.path.join(REPO, "Release", "PROGRESS.json")

NEXT = (
    "Fix the signature case in Tools/test-function.py: capture `before` after any leftover probe is removed, "
    "and establish which pin type and name combination the resolver accepts for a two-input signature. Then "
    "run the focused graph family for S05. Record the `inputs`/`outputs` requiredness defect - a declared "
    "default the engine's MCP adapter refuses to omit, on at least five tools - against S04's shared contract "
    "rather than patching each tool."
)


def main() -> int:
    with open(PROGRESS, "r", encoding="utf-8") as handle:
        ledger = json.load(handle)

    ledger["activeStep"] = "S03"
    ledger["nextAction"] = NEXT

    for step in ledger["steps"]:
        if step["id"] == "S03":
            step["status"] = "implemented_unverified"
            step["evidence"] = [
                "Release/EVIDENCE/S03/environment-safety.md",
                "Tools/ownership.ps1",
                "Tools/test-ownership.py",
                "Tools/check-plugin.py",
                "Tools/test-function.py",
            ]
            step["blocker"] = (
                "Tools/test-function.py fails 5 checks, all in the signature case that supplies `inputs` and "
                "`outputs`: the two-pin signature is refused or its pins are not reported, and one assertion "
                "compares against a stale `before` list. The suite was already failing before this round "
                "(27 checks) because the engine's MCP adapter refuses a call that omits the defaulted "
                "`inputs`/`outputs` parameters; that is a public-contract defect on at least five tools and is "
                "recorded for S04 rather than patched here."
            )
            step["workUnit"] = (
                "S03 — E01 passes (41 of 41). R08's undeclared plugin dependencies are declared and now "
                "checked in both directions by check-plugin.py, which reports 13 checks. R09's engine version "
                "mismatch is refused. E06 was taken: the editor loaded the built artifact byte for byte and "
                "registered all four toolsets."
            )
            step["testsRun"] = (
                "L1: Tools/test-ownership.py -> exit 0, 41 checks, 10.2 s (E01 now passes). "
                "L1: check-plugin.py -> RESULT: PASS (13 checks), up from 11. "
                "L3: start-editor.ps1 -NullRHI -Offscreen -LowPriority -> PASS, 16.4 s, editor pid 39780; "
                "check-mcp-server.py --editor-pid 39780 -> PASS, 4 TurboMCP toolsets of 28; ReadTurboMCPStatus "
                "answered live; the loaded module's SHA-256 equals the built artifact's; start-editor.ps1 -Stop "
                "-> PASS, 0 editors remain. "
                "L3: Tools/test-function.py -> 27 failures reduced to 5, all in the signature case."
            )
            step["acceptanceResults"] = {
                "E01": "pass — 41 of 41 checks. A foreign editor is never owned, a name alone is not "
                       "ownership, an unreadable command line is not ownership, and a different engine's "
                       "editor is not owned.",
                "E02": "pass — the lease is exclusive; a second owner and a different project on the same port "
                       "are refused; the same owner may re-take it",
                "E03": "pass — the lease records project, engine, port and fixture set; an unreadable lease "
                       "blocks instead of reading as free",
                "E04": "pass — a mount to another checkout is foreign and not replaced; the token-in-URL "
                       "detector fires on the old instruction and setup-engine.ps1 no longer contains one",
                "E05": "pass — absent, zero-byte and undersized artifacts fail; a present artifact is hashed; "
                       "an UP-TO-DATE artifact still passes; build.ps1 exits 1 on a missing artifact",
                "E06": "pass — the loaded module's SHA-256 equals the built artifact's, the four toolsets are "
                       "registered, and a live tool call answered",
                "E07": "partial — the client's unknown-outcome behaviour is asserted in test-client-retry.py; "
                       "the module-identity half is covered by E06's measurement",
            }
            step["remainingLimits"] = [
                "The signature case in Tools/test-function.py fails 5 checks and is not accepted.",
                "No other L3 suite was run this round.",
                "The `inputs`/`outputs` requiredness defect applies to at least five tools; the shared contract "
                "is S04 work.",
                "Tools/ still holds the one-shot patch_*.py scripts written this session; S17 must keep them "
                "out of the user package.",
            ]

    for finding in ledger["findings"]:
        if finding["id"] == "R08":
            finding["disposition"] = "fixed_verified"
            finding["evidence"] = [
                "Release/EVIDENCE/S03/environment-safety.md",
                "Tools/check-plugin.py",
                "TurboMCP.uplugin",
            ]
            finding["note"] = (
                "Repaired and verified. EditorScriptingUtilities and PythonScriptPlugin are declared in "
                "TurboMCP.uplugin, and check-plugin.py now verifies both directions - every engine plugin an "
                "include refers to must be declared, and every declared plugin must have an engine descriptor "
                "- reporting 13 checks where it reported 11. A required fixture-only module split is NOT done; "
                "the release declares the dependencies instead, which ARC13 prefers."
            )
        elif finding["id"] in ("R09", "R10"):
            finding["disposition"] = "fixed_verified"
            finding["evidence"] = [
                "Release/EVIDENCE/S03/environment-safety.md",
                "Tools/ownership.ps1",
                "Tools/test-ownership.py",
                "Tools/setup-engine.ps1",
            ]
            finding["note"] = (
                ("R10: ownership is decided from a process's command line, never its name; the launcher and the "
                 "suite runner stop only this repository's editor; a lease gives one owner for the project, "
                 "engine, port and fixture set; the commandlets are cleared by PID from the editor's own "
                 "descendant walk. E01-E04 pass, and the editor was started and stopped through the new path "
                 "with 0 processes left behind." if finding["id"] == "R10" else
                 "R09: a foreign mount is refused and never replaced; the engine version mismatch is refused "
                 "by name with an explicit override; the token-in-URL instruction is gone and a detector "
                 "prevents its return. The reproducible-install procedure itself is S17 work.")
            )

    ledger["checkpoints"].append({
        "step": "S03",
        "date": "2026-10-16",
        "summary": (
            "E01 passes: 41 of 41 checks. The cause was measured at last - `payload, out = owns(...)` bound the "
            "probe's EXIT CODE to `payload`, so seven assertions evaluated the integer 0 while the module's "
            "correct answer sat in `out`. R08's undeclared plugin dependencies are declared and now checked in "
            "both directions (check-plugin.py, 13 checks). R09's engine version mismatch is refused. E06 was "
            "taken: the loaded module's SHA-256 equals the built artifact's, all four toolsets are registered, "
            "a live call answered, and the stop left 0 editors. A focused L3 suite found a real public-contract "
            "defect - the adapter refuses a call that omits the defaulted inputs/outputs parameters - and also "
            "found a regression this session had introduced; its failures went from 27 to 5. S03 stays open on "
            "the signature case."
        ),
        "evidence": [
            "Release/EVIDENCE/S03/environment-safety.md",
            "Tools/ownership.ps1",
            "Tools/test-ownership.py",
            "Tools/check-plugin.py",
        ],
        "tests": ("L1 test-ownership.py 41/41 exit 0; check-plugin.py PASS 13 checks; "
                  "L3 editor start PASS 16.4 s, 4 toolsets registered, module hash equals the artifact, "
                  "stop PASS with 0 remaining; L3 test-function.py 27 -> 5 failures."),
        "next": NEXT,
    })

    ledger["baseline"]["lastVerifiedArtifact"] = (
        "Binaries/Win64/UnrealEditor-TurboMCP.dll, 3,680,256 bytes, 2026-10-16 22:14:17, "
        "sha256 07B277CA56FB194EEF31C7FFA3FB66EACA13004D67081CD5D024B69FE620FB6A - loaded by editor pid "
        "39780 through the Marketplace mount with the identical hash"
    )
    ledger["baseline"]["boardRefreshStatus"] = ledger["baseline"]["boardRefreshStatus"]

    with open(PROGRESS, "w", encoding="utf-8") as handle:
        json.dump(ledger, handle, indent=2, ensure_ascii=False)
        handle.write("\n")

    fixed = [f["id"] for f in ledger["findings"] if f["disposition"] == "fixed_verified"]
    unverified = [f["id"] for f in ledger["findings"] if f["disposition"] == "implemented_unverified"]
    print("updated %s" % PROGRESS)
    print("  findings verified   : %d (%s)" % (len(fixed), ", ".join(sorted(fixed))))
    print("  findings unverified : %d (%s)" % (len(unverified), ", ".join(sorted(unverified)) or "none"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
