# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""Applies the S02 checkpoint to Release/PROGRESS.json.

Idempotent. Every value written here was measured in this run and is recorded in
Release/EVIDENCE/S02/client-retry.md.
"""

from __future__ import annotations

import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROGRESS = os.path.join(REPO, "Release", "PROGRESS.json")

S02_STEP = {
    "status": "verified",
    "evidence": [
        "Release/EVIDENCE/S02/client-retry.md",
        "Tools/test-client-retry.py",
        "Release/Audit/audit_retry_probe.py",
        ".agent/suite-logs/manifest.json",
    ],
    "blocker": None,
    "workUnit": (
        "S02 — the supplied client no longer replays a mutation after an ambiguous transport failure. It "
        "reconnects without repeating the request, reports outcome_unknown, and exits 2 from the CLI."
    ),
    "testsRun": (
        "L1: test-client-retry.py — 20 checks, exit 0 (C01, C02). Historical probe re-run: exit 0, "
        "simulated_effects 1 (it asserted 2 before the repair). Static suite set through the real runner: "
        "10 passed, 0 failed, 0 blocked, 0 timed out, exit 0, 140.3 s. No build required: Python only."
    ),
    "boardTaskId": None,
    "acceptanceResults": {
        "C01": "pass — one lost response produces exactly one effect, one request, one reconnect and an "
               "UnknownOutcomeError; the same loss on a Read is still retried and succeeds",
        "C02": "pass — a reconnect after a lost mutation response does not re-send the mutation; the CLI "
               "exit code for an unknown outcome is 2, distinct from 1",
    },
    "remainingLimits": [
        "No live editor test; C01/C02 are L1 with a mocked transport, as the acceptance catalog specifies.",
        "No claim that the server deduplicates a retried request. The caller is told to establish the "
        "outcome independently; safe automatic retries are not advertised.",
        "The operationId the error message refers to is not in every result line yet; completing the "
        "result contract is S04's tmcp-result-v1 migration.",
    ],
}

NEXT = (
    "S03: make the environment and builds safe (E01-E07, A11 baseline, R08-R11). Ownership checks before "
    "anything is stopped; remove global process-name termination from operational paths; validate mount "
    "targets and declared plugin dependencies; pin the supported engine identity; remove token-in-URL "
    "instructions; fail a build whose expected artifact is missing. Tests: L1 process and filesystem fakes "
    "first, then L2 and one L3 registration smoke on the current candidate."
)


def main() -> int:
    with open(PROGRESS, "r", encoding="utf-8") as handle:
        ledger = json.load(handle)

    ledger["activeStep"] = "S03"
    ledger["nextAction"] = NEXT

    for step in ledger["steps"]:
        if step["id"] == "S02":
            step.update(S02_STEP)
        elif step["id"] == "S03":
            step["status"] = "in_progress"

    for finding in ledger["findings"]:
        if finding["id"] == "R02":
            finding["disposition"] = "fixed_verified"
            finding["evidence"] = [
                "Release/EVIDENCE/S02/client-retry.md",
                "Tools/test-client-retry.py",
            ]
            finding["note"] = (
                "Repaired and verified in this run. `Session.rpc` reconnects for every method but repeats "
                "only a read; a mutation raises UnknownOutcomeError and the CLI exits 2. The classification "
                "is static and treats an unrecognised name as a mutation."
            )

    ledger["checkpoints"].append({
        "step": "S02",
        "date": "2026-10-16",
        "summary": (
            "The client no longer replays a mutation after an ambiguous failure. One lost response now "
            "produces exactly one effect; the reconnect is retained; a read is still retried. C01 and C02 "
            "pass with 20 checks. The repository's historical probe was updated to the repaired expectation "
            "and keeps its role as the evidence for the finding."
        ),
        "evidence": S02_STEP["evidence"],
        "tests": S02_STEP["testsRun"],
        "next": NEXT,
    })

    ledger["boardTracking"]["lastSync"] = (
        "2026-10-16: S01 card recorded as succeeded. S16 card 5b6a8e45-6c5f-4dbc-8bc6-0b37e6da1eae and "
        "fixture card b4d297e2-ba48-4981-b122-6c9dd48dc1ab created under the release root. An S02 card is "
        "still to be created; the S02 work is recorded here and in Release/EVIDENCE/S02/."
    )
    ledger["boardSyncQueue"].append({
        "action": "create-leaf-card",
        "step": "S02",
        "title": "S02 / R02 — the client must not replay a mutation after a lost response",
        "reason": "the S02 work unit closed without a leaf card; the ledger and evidence exist",
    })

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
