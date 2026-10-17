---
id: "1f5fe672-f84d-40f1-98c0-adc77a8a8816"
title: "T26 (P3, low) — Show the dependents of a struct change"
status: todo
priority: "P3"
tags: ["P3", "low", "struct", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:04:46Z"
updated: "2026-10-16T19:57:58Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T26 (P3, low) — Show the dependents of a struct change

| Field | Value |
|---|---|
| Card ID | `1f5fe672-f84d-40f1-98c0-adc77a8a8816` |
| Status | todo |
| Priority | P3 |
| Tags | `P3` `low` `struct` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:04:46Z |
| Updated (UTC) | 2026-10-16T19:57:58Z |
| Executions recorded | 0 |

## Description

Source
    `Feedbacks/TurboMCP_TaskList_2026-10-13.md:298-301`, group P3. Source IDs: FT-08 (S3). This card came from the list. No one measured it on the tree.

done when
    1. After a struct edit, the result lists the assets that the change made unsaved or broke. It offers to compile them.

Why the card names both words
    "Broke" is the measurable half. "Unsaved" is the cheap half. The dirty set is a package flag, and a scan finds it. The broken set needs a compile.
    This session measured what a compile verdict can and cannot see. T04 found that a compile's `FCompilerResultsLog` carries only what the compiler reported. An ensure during the compile was invisible until the verdict also read the log capture (`7153502`). A dependent list built from the compiler's own log alone will miss exactly that class.

"Offers to compile them" is a sentence, not an action
    Compiling is a write. The project's rule is that a tool says what it did and what it did not do. An offer is therefore a sentence the tool prints, not something it does.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
