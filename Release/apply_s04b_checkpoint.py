"""Record the ARC03 mutation-coordinator work unit. S04 stays open."""

from __future__ import annotations

import json
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROGRESS = os.path.join(REPO, "Release", "PROGRESS.json")

NEXT = (
    "S04 continues. ARC02 (C03-C08) passes and ARC03 compiles with its state machine verified natively. Next: "
    "the ARC04 fingerprint-bound diagnostic baseline as a small shared helper, then migrate the first real "
    "writer - the graph repoint core is the smallest that already owns a transaction - and run M01-M08 plus "
    "the tmcp-result-v1 emission for that one writer. No M case has been run and no writer uses the "
    "coordinator yet."
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
                "Release/EVIDENCE/S04/mutation-coordinator.md",
                "Release/EVIDENCE/S04/result-writer.md",
                "Release/EVIDENCE/S04/first-migration.md",
                "Tools/tmcp_result.py",
                "Tools/tmcp.py",
                "Tools/test-result-contract.py",
                "Source/TurboMCP/Public/TurboMCPMutation.h",
                "Source/TurboMCP/Public/TurboMCPResult.h",
                "Source/TurboMCP/TurboMCPResultTest.cpp",
                "Source/TurboMCP/TurboMCPGraphRepointTools.cpp",
                "Source/TurboMCP/TurboMCPMutationTest.cpp",
            ]
            step["workUnit"] = (
                "S04 (part) — ARC02's tmcp-result-v1 contract (C03-C08 pass) and ARC03's shared mutation "
                "coordinator, compiled and state-machine tested by focused native automation."
            )
            step["testsRun"] = (
                "L1: Tools/test-result-contract.py -> exit 0, 60 checks (C03-C08). "
                "L2: build.ps1 -> exit 0, 8.7 s, artifact 3,725,312 bytes. "
                "L3-native: UnrealEditor-Cmd -ExecCmds=\"Automation RunTests TurboMCP.Mutation;Quit\" -> "
                "'Found 1 automation tests', 'Test Completed. Result={Success}', and no leaked-transaction "
                "warning after the test was corrected. No regressions: test-client-retry exit 0, "
                "test-ownership exit 0, check-plugin PASS (13 checks)."
            )
            step["acceptanceResults"].update({
                "M01": "not run — needs a migrated writer and a play session",
                "M04": "not run at L3; the state machine half is asserted natively (a second Commit is "
                       "refused, a Restore after Commit is OutcomeUnknown)",
                "M05": "not run at L3; the outcome pair is asserted natively (unverified restoration -> "
                       "OutcomeUnknown/rollback_unverified, verified -> Failed)",
            })
            step["remainingLimits"] = [
                "No writer uses the coordinator, so M01-M08 have not been run and no L3 behaviour depends on it.",
                "CheckReadiness's PIE, simulation and shutdown branches are compiled but not exercised; they "
                "need a play session (M01).",
                "The ARC04 fingerprint-bound diagnostic baseline and its compile cache are not started.",
                "No tool emits a result line yet, so C09 has not been run.",
                "The measured defect that the MCP adapter refuses a call omitting a declared-default parameter "
                "(inputs/outputs on at least five tools) is unchanged.",
            ]

    ledger["checkpoints"].append({
        "step": "S04",
        "date": "2026-10-16",
        "summary": (
            "ARC03's shared mutation coordinator is implemented, compiles, and passes a focused native "
            "automation test (TurboMCP.Mutation.CoordinatorStateMachine, Result={Success}). It owns exactly one "
            "transaction, ends it exactly once, refuses an unsafe world state BEFORE any mutation, refuses a "
            "second caller rather than waiting, and reports OutcomeUnknown for a restoration it could not "
            "verify. THREE DEFECTS were found by building rather than reviewing: FScopedTransaction has no "
            "Commit() on this engine revision; UWorld::IsSimulating() does not exist; and the guard check "
            "contained `GetUndoCount() >= 0`, which is true for every unsigned count and proved nothing. The "
            "coordinator's own destructor warning then caught a leaked transaction IN THE TEST THAT VERIFIES IT, "
            "which is why a third honest teardown (Finish()) now exists."
        ),
        "evidence": [
            "Release/EVIDENCE/S04/mutation-coordinator.md",
                "Release/EVIDENCE/S04/result-writer.md",
                "Release/EVIDENCE/S04/first-migration.md",
            "Source/TurboMCP/Public/TurboMCPMutation.h",
                "Source/TurboMCP/Public/TurboMCPResult.h",
                "Source/TurboMCP/TurboMCPResultTest.cpp",
                "Source/TurboMCP/TurboMCPGraphRepointTools.cpp",
            "Source/TurboMCP/TurboMCPMutation.cpp",
            "Source/TurboMCP/TurboMCPMutationTest.cpp",
        ],
        "tests": ("L2 build exit 0; L3-native automation TurboMCP.Mutation.CoordinatorStateMachine "
                  "Result={Success} with no leaked transaction."),
        "next": NEXT,
    })

    ledger["baseline"]["lastVerifiedArtifact"] = (
        "Binaries/Win64/UnrealEditor-TurboMCP.dll, 3,703,296 bytes, 2026-10-16 - the build that carries the "
        "ARC02 result contract and the ARC03 mutation coordinator; its native automation test reports Success"
    )

    with open(PROGRESS, "w", encoding="utf-8") as handle:
        json.dump(ledger, handle, indent=2, ensure_ascii=False)
        handle.write("\n")

    print("updated %s" % PROGRESS)
    print("  activeStep : %s" % ledger["activeStep"])
    print("  S04 status : %s" % next(s["status"] for s in ledger["steps"] if s["id"] == "S04"))
    return 0


if __name__ == "__main__":
    sys.exit(main())


