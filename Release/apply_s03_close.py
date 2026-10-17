"""Close S03: every acceptance case passes, including the focused L3 suite that was open."""

from __future__ import annotations

import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROGRESS = os.path.join(REPO, "Release", "PROGRESS.json")

NEXT = (
    "S04: implement the fixed shared contracts (C03-C08, DOC1-DOC5, R05-R07, G05, G07, G11, G17). The typed "
    "internal result and the ARC02 `tmcp-result-v1` formatter/parser; the small shared mutation coordinator "
    "from ARC03; fingerprint-bound diagnostic baselines from ARC04; owned transaction and rollback "
    "verification from ARC05. The first migrated real writer must pass its M cases before its leaf closes. "
    "Carry forward the measured finding that the engine's MCP adapter REFUSES a call that omits a "
    "declared-default parameter (`inputs`/`outputs` on at least five tools), which belongs to this shared "
    "contract rather than to five separate patches."
)


def main() -> int:
    with open(PROGRESS, "r", encoding="utf-8") as handle:
        ledger = json.load(handle)

    ledger["activeStep"] = "S04"
    ledger["nextAction"] = NEXT

    for step in ledger["steps"]:
        if step["id"] == "S03":
            step["status"] = "verified"
            step["blocker"] = None
            step["evidence"] = [
                "Release/EVIDENCE/S03/environment-safety.md",
                "Tools/ownership.ps1",
                "Tools/test-ownership.py",
                "Tools/check-plugin.py",
                "Tools/test-function.py",
            ]
            step["testsRun"] = (
                "L1: Tools/test-ownership.py -> exit 0, 41 checks, 10.2 s. "
                "L1: check-plugin.py -> RESULT: PASS (13 checks), up from 11. "
                "L3: start-editor.ps1 -NullRHI -Offscreen -LowPriority -> PASS; check-mcp-server.py "
                "--editor-pid -> PASS, 4 TurboMCP toolsets of 28; ReadTurboMCPStatus answered live; the loaded "
                "module's SHA-256 equals the built artifact's. "
                "L3: Tools/test-function.py -> exit 0, RESULT: PASS, 0.8 s, and the asset is left as found. "
                "L3: start-editor.ps1 -Stop -> PASS, 0 editors remain, lease released."
            )
            step["acceptanceResults"]["E07"] = (
                "pass — the client's unknown-outcome behaviour is asserted in test-client-retry.py (20 checks, "
                "exit 0) and the module-identity half is measured by E06: the loaded module's SHA-256 equals "
                "the built artifact's."
            )
            step["remainingLimits"] = [
                "The `inputs`/`outputs` requiredness defect applies to at least five tools and is carried into "
                "S04's shared contract, not patched per tool.",
                "Tools/ still holds the one-shot patch_*.py scripts written this session; S17 must keep them "
                "out of the user package.",
                "Only the function suite has been run at L3 so far; the graph, material, data and runtime "
                "families are untouched.",
            ]

    for finding in ledger["findings"]:
        if finding["id"] in ("R09", "R10", "R11", "R08"):
            finding["disposition"] = "fixed_verified"

    ledger["checkpoints"].append({
        "step": "S03",
        "date": "2026-10-16",
        "summary": (
            "S03 VERIFIED. E01-E07 pass. The last open item was the two-input signature case in "
            "Tools/test-function.py, and its cause was measured: the case drove every trial through ONE "
            "function name and re-added it after removal, and the engine refuses to reuse a name the package "
            "still holds - 'the engine would not create a function named TStructFiveProbeSigned and made "
            "TStructFiveProbeSigned_0 instead'. The TOOL was right to refuse and right to say why; the CASE was "
            "reusing a name. Each trial now uses its own name, so the case measures the signature instead of "
            "the package's name lifetime, and a second trial proves the two parameters work twice. The suite "
            "exits 0 and leaves the asset as found. Also fixed: `-Stop` now releases a stale lease, because a "
            "refusal that tells an operator to delete a file by hand is a dead end."
        ),
        "evidence": [
            "Release/EVIDENCE/S03/environment-safety.md",
            "Tools/test-function.py",
            "Tools/start-editor.ps1",
        ],
        "tests": ("L3 test-function.py exit 0 (RESULT: PASS, 0.8 s, asset left as found); "
                  "editor start/stop through the ownership path, 0 editors remaining, lease released."),
        "next": NEXT,
    })

    # The S03 remainder card is satisfied by this record.
    ledger["boardSyncQueue"].append({
        "action": "record-external-outcome",
        "step": "S03",
        "taskId": "93191536-bbed-4cd5-9a34-995be0066bc9",
        "result": "succeeded",
        "reason": ("the signature case now passes; the cause was a name collision the case created, not a tool "
                   "defect - each trial uses its own name now"),
    })

    with open(PROGRESS, "w", encoding="utf-8") as handle:
        json.dump(ledger, handle, indent=2, ensure_ascii=False)
        handle.write("\n")

    verified = [f["id"] for f in ledger["findings"] if f["disposition"] == "fixed_verified"]
    print("updated %s" % PROGRESS)
    print("  activeStep        : %s" % ledger["activeStep"])
    print("  S03 status        : %s" % next(s["status"] for s in ledger["steps"] if s["id"] == "S03"))
    print("  findings verified : %d of %d" % (len(verified), len(ledger["findings"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
