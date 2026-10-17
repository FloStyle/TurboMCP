"""Record the S04 result-contract work unit in the ledger. S04 stays open."""

from __future__ import annotations

import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROGRESS = os.path.join(REPO, "Release", "PROGRESS.json")

NEXT = (
    "S04 continues. The result contract (ARC02, C03-C08) is implemented and passing. Next: the ARC03 mutation "
    "coordinator and the ARC04 fingerprint-bound diagnostic baseline as small shared helpers, then migrate the "
    "first real writer and run its M cases (M01-M08) before that leaf closes. No tool emits a result line yet, "
    "so no acceptance case that needs real metadata has passed; C09 needs the editor and a migrated tool."
)


def main() -> int:
    with open(PROGRESS, "r", encoding="utf-8") as handle:
        ledger = json.load(handle)

    ledger["activeStep"] = "S04"
    ledger["nextAction"] = NEXT

    for step in ledger["steps"]:
        if step["id"] == "S04":
            step["status"] = "implemented_unverified"
            step["evidence"] = [
                "Release/EVIDENCE/S04/result-contract.md",
                "Tools/tmcp_result.py",
                "Tools/tmcp.py",
                "Tools/test-result-contract.py",
            ]
            step["workUnit"] = (
                "S04 (part) — the ARC02 result contract: a pure formatter/parser module, the client's "
                "StructuredAnswer and call_structured, and a CLI whose exit status comes from the metadata. "
                "C03-C08 pass."
            )
            step["testsRun"] = (
                "L1: Tools/test-result-contract.py -> exit 0, 60 checks (C03-C08). "
                "Regression checks after the client change: test-client-retry.py exit 0, test-ownership.py "
                "exit 0, check-plugin.py PASS (13 checks)."
            )
            step["acceptanceResults"] = {
                "C03": "pass — refusal 1, partial save 1, failed rollback 2, success 0, incomplete 2, unknown 2; "
                       "and BOTH negative controls: alarming prose with a `changed` line exits 0, reassuring "
                       "prose with a `refused` line exits 1",
                "C04": "pass — nine malformed shapes refused; a missing line returns None (the caller decides) "
                       "while a present-but-invalid line raises",
                "C05": "pass — the final line decides; a line that is not last is not authoritative; the "
                       "compatibility body keeps the forged line visible",
                "C06": "pass — every block in order, one decoded body, the metadata parsed once",
                "C07": "pass — a native tool has no TurboMCP result and is not falsely certified; a migrated "
                       "tool with no line is a protocol failure",
                "C08": "pass — truncation reported, metadata intact and valid, the omission counted, the budget "
                       "respected exactly, and a body that fits is not truncated",
                "C09": "not run — L3, needs the editor and a migrated tool",
            }
            step["remainingLimits"] = [
                "No tool emits a result line yet, so nothing is migrated and the contract cannot be observed "
                "through the wire; C09 has not been run.",
                "The ARC03 coordinator, the ARC04 diagnostic baselines and the ARC05 transaction/rollback "
                "boundaries are not started.",
                "The measured defect that the engine's MCP adapter refuses a call omitting a declared-default "
                "parameter (inputs/outputs on at least five tools) is unchanged; it is a contract question for "
                "the migration.",
                "The CLI's own exit path has no test of its own yet; its mapping is asserted by inspection.",
            ]
            step["blocker"] = (
                "None for the result-contract half. The step cannot close until a real writer is migrated and "
                "its M cases pass, because the contract's purpose is to carry a tool's outcome and no tool "
                "produces one yet."
            )

    ledger["checkpoints"].append({
        "step": "S04",
        "date": "2026-10-16",
        "summary": (
            "ARC02's result contract is implemented and C03-C08 pass (60 checks, exit 0). The final line is "
            "authoritative, malformed metadata is a protocol failure rather than success, and the metadata "
            "survives an output budget. TWO REAL CLIENT DEFECTS were found by the cases and fixed: `body` was "
            "computed from the raw blocks so an FString-wrapped answer returned its whole wrapper as the body, "
            "and the wrapper's JSON-string form was not unwrapped at all, which would have made every migrated "
            "FString tool look like a protocol failure. THREE TEST DEFECTS were also found, one of which - an "
            "assertion that could not fail - had hidden a real off-by-one in the budget arithmetic. No tool is "
            "migrated yet, so no case needing real metadata has passed."
        ),
        "evidence": [
            "Release/EVIDENCE/S04/result-contract.md",
            "Tools/tmcp_result.py",
            "Tools/tmcp.py",
            "Tools/test-result-contract.py",
        ],
        "tests": "L1 test-result-contract.py exit 0 (60 checks, C03-C08); no regressions in test-client-retry or test-ownership.",
        "next": NEXT,
    })

    ledger["boardSyncQueue"].append({
        "action": "create-leaf-card",
        "step": "S04",
        "title": "S04 / ARC02 — the tmcp-result-v1 contract and the structured client",
        "reason": "the result-contract work unit is recorded in the ledger and evidence but has no leaf card",
    })

    with open(PROGRESS, "w", encoding="utf-8") as handle:
        json.dump(ledger, handle, indent=2, ensure_ascii=False)
        handle.write("\n")

    print("updated %s" % PROGRESS)
    print("  activeStep : %s" % ledger["activeStep"])
    print("  S04 status : %s" % next(s["status"] for s in ledger["steps"] if s["id"] == "S04"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
