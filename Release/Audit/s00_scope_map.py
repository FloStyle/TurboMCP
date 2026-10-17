# Copyright (c) 2026 FloStyle
# SPDX-License-Identifier: MPL-2.0
"""S00 scope reconciliation: route every supplied board card to a roadmap step and a disposition.

This is a one-shot release bookkeeping tool, not a test. It reads the supplied board snapshot from
Release/PROGRESS.json and writes Release/Audit/BOARD_MAP.json.

Rules applied, in order:

1. A card whose snapshot column is `done` is NOT trusted. Recording a card as done in the board is a
   declaration, not acceptance evidence. It is routed to the roadmap step that governs its subject so
   its claim can be checked against current behavior, with disposition `verify_existing_done`.
2. A card whose subject is a feature ARC01 excludes from this release gets `deferred_by_scope` and
   stays in backlog.
3. `T12` itself stays in backlog (ARC09); the restricted single-graph subset is a separate card.
4. Known audit/history bookkeeping cards get `out_of_release_scope` with the reason.

Nothing here is evidence that any repair works.
"""

from __future__ import annotations

import json
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
PROGRESS = os.path.join(REPO, "Release", "PROGRESS.json")
OUT = os.path.join(REPO, "Release", "Audit", "BOARD_MAP.json")

# --- feature exclusions fixed by ARC01 / ARC09 -------------------------------------------------
# Each entry: (regular expression, disposition, reason)
DEFERRED = [
    (r"T12 \(P1, high\) — Atomic multi-asset batch edit",
     "deferred_by_scope",
     "ARC09: only the existing graphRepoint operation kind in one Blueprint graph is in release "
     "scope. General multi-asset atomic editing stays backlog. The subset is tracked by its own card."),
    (r"T34 \(P6\).*Widget authoring",
     "deferred_by_scope", "ARC01: widget animation authoring is explicitly unbuilt for this release."),
    (r"T37 \(P6\).*Asset operations",
     "deferred_by_scope", "ARC01: general asset CRUD is explicitly unbuilt for this release."),
    (r"T39 \(P6\).*Curve assets",
     "deferred_by_scope", "ARC01: curve editing is explicitly unbuilt for this release."),
    (r"T41 \(P6\).*Input test loop",
     "deferred_by_scope", "ARC01: input injection is explicitly unbuilt for this release."),
    (r"T13 \(P1, medium\) — Pin-level link dump",
     "deferred_by_scope", "Not an included release capability; no acceptance case requires it."),
    (r"T14 \(P1, low\) — Session change list",
     "deferred_by_scope", "Not an included release capability; no acceptance case requires it."),
    (r"T15 \(P1, low\) — Cleanup helpers",
     "deferred_by_scope", "Not an included release capability; no acceptance case requires it."),
    (r"T25 \(P3, low\) — Data table create, with nested rows",
     "deferred_by_scope", "ARC01: broad asset creation is unbuilt for this release."),
    (r"T40 \(P6\).*Preview of the ordered steps",
     "deferred_by_scope", "Feature request beyond the fixed release boundary."),
    (r"FT-25/FT-26/FT-29",
     "deferred_by_scope", "Feature requests beyond the fixed release boundary."),
    (r"FT-32–FT-36",
     "deferred_by_scope", "Feature requests beyond the fixed release boundary."),
    (r"X-28 / TH-05 / TH-07 / X-34 / FT-43",
     "deferred_by_scope", "Bounded task-to-tool discovery is a feature request, not an included repair."),
    (r"WT-07 \(P2, medium\) — widget subtree write",
     "deferred_by_scope", "Feature request beyond the fixed release boundary."),
    (r"X-27 / FT-41",
     "deferred_by_scope", "Feature request beyond the fixed release boundary."),
    (r"X-24/X-26 / FT-37",
     "deferred_by_scope", "Feature request beyond the fixed release boundary."),
    (r"GC-10–GC-12 / X-33",
     "deferred_by_scope", "Widening the writer grammar is beyond the fixed release boundary."),
    (r"X-28|TH-05|TH-07",
     "deferred_by_scope", "Feature request beyond the fixed release boundary."),
    (r"T22 \(P3, medium\)|T23 \(P3, medium\)|T24 \(P3, medium\)|T26 \(P3, low\)",
     "deferred_by_scope", "Feature request beyond the fixed release boundary."),
    (r"T27 \(P4|T28 \(P4|T29 \(P4|T30 \(P4|T31 \(P5|T32 \(P5|T33 \(P5",
     "deferred_by_scope", "Feature request beyond the fixed release boundary."),
    (r"T11 \(P1, medium\) — Function from text",
     "deferred_by_scope", "Feature request beyond the fixed release boundary."),
    (r"T16 \(P2|T17 \(P2|T18 \(P2|T19 \(P2|T20 \(P2|T21 \(P2",
     "deferred_by_scope", "Feature request beyond the fixed release boundary."),
    (r"T35 \(P6\)|T36 \(P6\)|T38 \(P6\)",
     "deferred_by_scope", "Feature request beyond the fixed release boundary."),
    (r"T-KEEP|T-RECON|T-METHOD-01|T-SUITE-0[123]|T-X4-01|T-C3-0[12]|T-FIXTURE-01",
     "out_of_release_scope",
     "Audit and verification bookkeeping. Not a repair of an exposed capability; retained as history."),
    (r"T-AUDIT-",
     "out_of_release_scope",
     "Per-file hand-audit history. Defects with release relevance are represented by the G/D/R/X findings."),
    (r"Batch verifier — test the ten concurrent urgent fixes",
     "out_of_release_scope",
     "Written for ten parallel sessions. This release run is sequential and owns no parallel batch."),
    (r"Release 1\.0: reconcile the public branch",
     "out_of_release_scope",
     "Branch reconciliation is a publication step. Publication is not authorized in this run."),
    (r"X-17/X-16|F5-F8/RF-1|F9-F13/RF-2|X-18/X-21/X-22|NR-03/WT-04|AS-01/BL-07|BL-10 / X-30|TH-06 / GC-08|BL-09 / LT-03|GC-09 / FT-47|GL-10–GL-13|TR-01 / TR-02|RF-06 \(P1|LT-04 \(P1",
     "deferred_by_scope",
     "Feedback-batch card outside the 47-finding release ledger; its release-relevant content is "
     "owned by the mapped findings and the capability manifest."),
]

# --- subject routing, applied when no exclusion rule matched -----------------------------------
ROUTES = [
    (r"T12 prerequisite|Extract transaction-free operation cores", "S16"),
    (r"T12 \(P1, high\)", "S16"),
    (r"Batch verifier", "S16"),
    (r"Release 1\.0 gate", "S18"),
    (r"Release 1\.0: reconcile", "S18"),
    (r"^GC-06/GC-07|^FT-30/FT-31|^MI-02/FT-27|^GC-09|^GL-10|^TR-01|^RF-06|^LT-04|^X-1\b|^X-2\b|^F1/GL-1|^F3/F4|^DT-06|^GL-09|^X-20|^SF-04", "S15"),
]

DONE_ROUTE = "S15"  # every untrusted done card is re-checked in the domain-gap step


def route(title: str) -> tuple[str, str, str]:
    for pattern, disposition, reason in DEFERRED:
        if re.search(pattern, title):
            # Both exclusion dispositions park the card in backlog: an excluded subject has no
            # roadmap step, and a card with no roadmap step must never look like active work.
            return ("backlog", disposition, reason)
    for pattern, step in ROUTES:
        if re.search(pattern, title):
            return (step, "route_to_step", "Mapped to the roadmap step that governs its subject.")
    return ("S15", "route_to_step", "Subject routed to the remaining-domain-gap step for triage.")


def main() -> int:
    with open(PROGRESS, "r", encoding="utf-8") as handle:
        ledger = json.load(handle)

    cards = ledger.get("boardCards", [])
    mapped = []
    for card in cards:
        title = card.get("title", "")
        column = card.get("snapshotColumn", "")
        step, disposition, reason = route(title)

        # Rule 1 wins: an existing done declaration is never accepted as evidence.
        if column == "done" and disposition == "route_to_step":
            disposition = "verify_existing_done"
            step = DONE_ROUTE
            reason = (
                "Card declares done. That is a declaration, not acceptance evidence: the board also "
                "carries failed executions among these cards. Routed for re-verification against "
                "current behavior before any release claim."
            )

        mapped.append({
            "boardId": card.get("boardId"),
            "title": title,
            "snapshotColumn": column,
            "snapshotVerificationBypassed": card.get("snapshotVerificationBypassed"),
            "roadmapStep": step,
            "disposition": disposition,
            "reason": reason,
        })

    by_disposition: dict[str, int] = {}
    for entry in mapped:
        by_disposition[entry["disposition"]] = by_disposition.get(entry["disposition"], 0) + 1

    document = {
        "schemaVersion": 1,
        "purpose": (
            "S00 scope reconciliation. Board cards are declarations; this map routes each one to a "
            "roadmap step and a disposition. It is not evidence that any card's claim holds."
        ),
        "sourceSnapshotRevision": ledger.get("baseline", {}).get("boardSnapshotRevision"),
        "liveBoardRevisionAtReconciliation": 1742,
        "rules": [
            "A done column is not acceptance evidence.",
            "ARC01 and ARC09 exclusions are applied without reopening them.",
            "An excluded feature stays in backlog and is never counted as done.",
            "Every card maps to exactly one roadmap step or to an explicit exclusion.",
        ],
        "counts": {
            "total": len(mapped),
            "byDisposition": by_disposition,
        },
        "cards": mapped,
    }

    with open(OUT, "w", encoding="utf-8") as handle:
        json.dump(document, handle, indent=2, ensure_ascii=False)

    print("wrote %s (%d cards)" % (OUT, len(mapped)))
    for key in sorted(by_disposition):
        print("  %-24s %d" % (key, by_disposition[key]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
