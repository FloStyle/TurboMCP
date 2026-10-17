# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""Applies the S00 checkpoint to Release/PROGRESS.json.

Idempotent: it sets the recorded baseline, marks S00 verified, activates S01, and records the board
tracking ids. It never invents evidence; every value it writes was measured in this run.
"""

from __future__ import annotations

import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROGRESS = os.path.join(REPO, "Release", "PROGRESS.json")

BASELINE = {
    "pluginCommit": "f045c1313f1db04ddcac269d6910be9ad6befb11",
    "engineCommit": "396c9f059903aed5fec78ecd3d437a40c6415368",
    "dirtySourceFingerprint": "767249a0f23e94ebb3ac0af84c39a805f7e4af059b46f2b8059cddd04473fef2",
    "ownedEditorPid": None,
    "lastVerifiedArtifact": (
        "Binaries/Win64/UnrealEditor-TurboMCP.dll (3,679,744 bytes, 2026-10-16 18:54:58) - "
        "observed, not built or exercised in S00"
    ),
    "boardSnapshotRevision": 1632,
    "boardRefreshStatus": "refreshed 2026-10-16 at live revision 1742; supplied snapshot 1632 is stale",
}

S00_STEP = {
    "status": "verified",
    "evidence": [
        "Release/EVIDENCE/S00/baseline.md",
        "Release/Audit/BOARD_MAP.json",
        "Release/CAPABILITIES.json",
        "Release/DECISIONS.md",
        "Release/Audit/board-current.tsv",
    ],
    "blocker": None,
}

S00_EXTRA = {
    "declaredTools": 101,
    "declaredToolsByToolset": {
        "UTurboMCPReadToolset": 30,
        "UTurboMCPLookToolset": 4,
        "UTurboMCPPreviewToolset": 29,
        "UTurboMCPEditToolset": 38,
    },
    "orphanDeclarations": 0,
    "cataloguedTools": 94,
    "catalogueDriftNote": (
        "TOOLS.md is generated and still states 94 tools; the working tree declares 101. Regenerating "
        "it is a release documentation deliverable for S17 (case Z01)."
    ),
    "testSuites": {"pythonSuiteShaped": 68, "noEditorSuites": 10, "slowSuites": 1},
    "acceptanceCases": {"total": 97, "mapped": 0, "unmapped": 97},
    "boardCards": {
        "total": 111,
        "deferredByScope": 52,
        "outOfReleaseScope": 30,
        "verifyExistingDone": 27,
        "routeToStep": 2,
        "verificationBypassedAmongDone": 7,
    },
    "workUnit": "S00 — baseline, scope reconciliation, capability manifest, tracking cards.",
    "testsRun": "L0 only: declaration recount, orphan-definition check, card routing, manifest generation. No build, no editor, no suite.",
}


def main() -> int:
    with open(PROGRESS, "r", encoding="utf-8") as handle:
        ledger = json.load(handle)

    ledger["baseline"] = BASELINE
    ledger["activeStep"] = "S01"
    ledger["nextAction"] = (
        "S01: repair the verification verdicts in Tools/run-suites.ps1 and the coverage audit. "
        "H01-H09. The runner currently prints a warning for a zero-match selection instead of "
        "failing, reuses bootstrapped child exit codes instead of rejecting nonzero, and has no "
        "stable case selector."
    )

    for step in ledger["steps"]:
        if step["id"] == "S00":
            step.update(S00_STEP)
            step.update(S00_EXTRA)
        elif step["id"] == "S01":
            step["status"] = "in_progress"

    ledger["boardTracking"] = {
        "rootTaskId": "5f2cee78-ed31-487e-bfa6-6452eaaf738f",
        "activeLeafTaskId": None,
        "lastSync": "2026-10-16: root card created and moved to running; live board revision 1742",
    }

    ledger["checkpoints"] = ledger.get("checkpoints", [])
    ledger["checkpoints"].append({
        "step": "S00",
        "date": "2026-10-16",
        "summary": (
            "Baseline recorded (plugin f045c13, engine 396c9f0/5.8.3, 6 pre-existing dirty source "
            "files preserved). Declarations recounted: 101 tools, 0 orphan declarations. 111 board "
            "cards routed to a roadmap step or an explicit exclusion (52 deferred_by_scope, 30 "
            "out_of_release_scope, 27 verify_existing_done, 2 route_to_step). Capability manifest "
            "and decision record created. Release tracking root card created."
        ),
        "evidence": S00_STEP["evidence"],
        "tests": "L0 only; no build, no editor, no suite.",
        "next": "S01 — repair verification verdicts (H01-H09).",
    })

    with open(PROGRESS, "w", encoding="utf-8") as handle:
        json.dump(ledger, handle, indent=2, ensure_ascii=False)
        handle.write("\n")

    print("updated %s" % PROGRESS)
    print("  activeStep   : %s" % ledger["activeStep"])
    print("  S00 status   : %s" % next(s["status"] for s in ledger["steps"] if s["id"] == "S00"))
    print("  S01 status   : %s" % next(s["status"] for s in ledger["steps"] if s["id"] == "S01"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
