# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""Applies the S01 checkpoint to Release/PROGRESS.json.

Idempotent. Every value written here was measured in this run and is recorded in
Release/EVIDENCE/S01/verification-verdicts.md.
"""

from __future__ import annotations

import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROGRESS = os.path.join(REPO, "Release", "PROGRESS.json")

S01_STEP = {
    "status": "verified",
    "evidence": [
        "Release/EVIDENCE/S01/verification-verdicts.md",
        "Tools/run-suites-lib.ps1",
        "Tools/test-runner-verdicts.py",
        "Tools/test-coverage-audit.py",
        ".agent/suite-logs/manifest.json",
    ],
    "blocker": None,
    "workUnit": (
        "S01 — the release gate cannot accept an incomplete or failed suite; the coverage audit's "
        "parameter parser no longer fabricates TEXT(\"\")."
    ),
    "testsRun": (
        "L1: test-runner-verdicts.py (30 checks, exit 0, 56.9 s); test-coverage-audit.py (15 checks, "
        "exit 0). L2: build.ps1 exit 0, 5 actions, 14.4 s, DLL 3,680,256 bytes at 2026-10-16 22:14:17. "
        "L1 static set through the real runner: 8 PASS, 2 FAIL (one corrected and re-run to exit 0), "
        "150.2 s."
    ),
    "boardTaskId": "436bef33-0252-4ee7-bb60-8ffe3584b177",
    "acceptanceResults": {
        "H01": "pass — child printed RESULT: PASS and exited 7; runner recorded exit 7 and refused",
        "H02": "pass — blocked-only run refused, blocked counted as blocked (1 blocked, 0 failed)",
        "H03": "pass — unknown name and valid+unknown both refused with exit 2, nothing started",
        "H04": "pass — zero-match selection and duplicate name both refused with exit 2",
        "H05": "pass — timeout is a state, refused, partial output in the preserved log, no leaked grandchild",
        "H06": "pass — exit 0 with no verdict refused, missing verdict named",
        "H07": "pass — valid verdict with exit 0 accepted, one executed case, runner exit 0",
        "H08": "pass — defaulted FString yields its own name; both assertion styles are not false positives; "
               "a suite with no way to fail is still reported",
        "H09": "pass — the inserted uncalled tool is detected and named; a call for it clears the finding",
    },
    "remainingLimits": [
        "audit-tool-coverage still fails on two real inherited findings: the two batch tools are called by "
        "no suite. Owned by the S16 card 5b6a8e45-6c5f-4dbc-8bc6-0b37e6da1eae.",
        "Tools/test-batch-edit.py has not been executed; it needs an editor and belongs to S16.",
        "Tools/test-function.py's new inputs/outputs cases and the corrected fixture commandlet guard are "
        "not exercised at L3.",
        "TestProject/Content/Tests/TurboMCPAcceptance.uasset and TurboMCPGraphReaderChild.uasset differ "
        "from their committed bytes after the run; recorded on card b4d297e2-ba48-4981-b122-6c9dd48dc1ab.",
    ],
}

NEXT = (
    "S02: remove ambiguous mutation replay from the supplied client (Tools/tmcp.py). C01/C02. The client "
    "catches every exception, reconnects and repeats the original request, including a committed mutation "
    "whose response was lost. R02 was confirmed with a mocked-I/O probe that produced two effects from one "
    "lost response. Report outcome_unknown, reconnect without replay, and do not replay project-code "
    "execution as if it were a pure read."
)


def main() -> int:
    with open(PROGRESS, "r", encoding="utf-8") as handle:
        ledger = json.load(handle)

    ledger["activeStep"] = "S02"
    ledger["nextAction"] = NEXT

    for step in ledger["steps"]:
        if step["id"] == "S01":
            step.update(S01_STEP)
        elif step["id"] == "S02":
            step["status"] = "in_progress"

    # R01 and R04 are repaired and verified. Nothing else changes disposition.
    for finding in ledger["findings"]:
        if finding["id"] in ("R01", "R04"):
            finding["disposition"] = "fixed_verified"
            finding["evidence"] = [
                "Release/EVIDENCE/S01/verification-verdicts.md",
                "Tools/test-runner-verdicts.py",
                "Tools/test-coverage-audit.py",
            ]
            finding["note"] = (
                "Repaired and verified in this run. R01: the runner preserves the child exit code, treats a "
                "timeout as a state, refuses an unmatched selection before starting anything, exits nonzero "
                "for blocked and timed-out suites, and writes a machine-readable manifest. R04: the coverage "
                "audit splits parameters on top-level commas so a defaulted FString keeps its own name, and "
                "the suite-quality heuristic recognises every shape that can record a failure. "
                "audit-tool-coverage still fails on the two batch tools, which is R03/G12 and belongs to S16."
            )

    ledger["checkpoints"].append({
        "step": "S01",
        "date": "2026-10-16",
        "summary": (
            "The release gate no longer accepts an incomplete or failed suite. Child exit codes are "
            "preserved, a timeout is a state, an unmatched selection is refused with exit 2 before anything "
            "starts, blocked and timed-out suites force a nonzero exit, each suite's complete log is kept, "
            "and a machine-readable manifest is written. The coverage audit's defaulted-parameter parser no "
            "longer fabricates TEXT(\"\"); the suite-quality heuristic no longer calls two suites that can "
            "fail unable to fail; the guard checker no longer calls if(!X) unguarded. Two real unguarded "
            "engine texture lookups were corrected. H01-H09 pass."
        ),
        "evidence": S01_STEP["evidence"],
        "tests": S01_STEP["testsRun"],
        "next": NEXT,
    })

    ledger["boardTracking"] = {
        "rootTaskId": "5f2cee78-ed31-487e-bfa6-6452eaaf738f",
        "activeLeafTaskId": None,
        "lastSync": (
            "2026-10-16: S01 card 436bef33-0252-4ee7-bb60-8ffe3584b177 recorded as succeeded with an "
            "external outcome. Created S16 card 5b6a8e45-6c5f-4dbc-8bc6-0b37e6da1eae (ARC09 batch subset) "
            "and fixture card b4d297e2-ba48-4981-b122-6c9dd48dc1ab."
        ),
    }

    with open(PROGRESS, "w", encoding="utf-8") as handle:
        json.dump(ledger, handle, indent=2, ensure_ascii=False)
        handle.write("\n")

    fixed = [f["id"] for f in ledger["findings"] if f["disposition"] == "fixed_verified"]
    open_count = [f["id"] for f in ledger["findings"] if f["disposition"] == "open"]
    print("updated %s" % PROGRESS)
    print("  activeStep        : %s" % ledger["activeStep"])
    print("  findings verified : %d (%s)" % (len(fixed), ", ".join(fixed)))
    print("  findings open     : %d" % len(open_count))
    return 0


if __name__ == "__main__":
    sys.exit(main())
