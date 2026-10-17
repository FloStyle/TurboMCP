---
id: "5013737a-dd64-4ac3-a627-aab8c590b676"
title: "T-SUITE-03 (P4, low) — The two obsolete T03 suites are removed (a0b4556) and the final 62-suite sweep is running"
status: backlog
priority: "P4"
tags: ["P4", "low", "handoff", "build", "read-first"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-12T11:55:57Z"
updated: "2026-10-16T19:57:52Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-SUITE-03 (P4, low) — The two obsolete T03 suites are removed (a0b4556) and the final 62-suite sweep is running

| Field | Value |
|---|---|
| Card ID | `5013737a-dd64-4ac3-a627-aab8c590b676` |
| Status | backlog |
| Priority | P4 |
| Tags | `P4` `low` `handoff` `build` `read-first` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-12T11:55:57Z |
| Updated (UTC) | 2026-10-16T19:57:52Z |
| Executions recorded | 0 |

## Description

Summary
    The two obsolete suites are gone and the final 62-suite sweep was running. The previous sweep read `passed 62, failed 2, blocked 0, of 64`, and both failures were those two suites.

What was removed, and why
    `git rm Tools/test-save-option-reachable.py` — it asserted that the option is exposed to a caller, and T03's option was deliberately withdrawn after the engine's own error message proved that a defaulted parameter on a `UFUNCTION(meta=(AICallable))` is required, so the tool could not be called without it. A suite that keeps the sweep red for a feature that was chosen against is not evidence of anything.
    `Tools/test-save-without-thumbnail.py` was deleted from the worktree — untracked, and it asserted that the save names the mechanism it used, which no longer exists.
    Both paths were resolved and printed before anything was removed, and the script refused if either resolved to anything other than the expected file.

One thing worth keeping from its last run
    `test-save-without-thumbnail` connected to the editor for the first time: `varsAdd said: changed: added 1 variable(s) to /Game/Tests/BP_NodeTest, read back from the Blueprint: 1 of 1`. So T03's five connection refusals were an environment state rather than a defect in the suite — which is what the fifth attempt suspected and could not prove at the time.

Next
    The sweep was running with 62 suites. If it came back clean, this card closes with the measurement it existed to produce: the sixteen writer corrections are not merely compiled, they are observed.

Verified
    Both suites removed, both paths verified first, worktree clean, HEAD `a0b4556`, 104 commits.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why. For audit/documentation-only work, use direct validation only; do not run a complete Unreal suite.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
