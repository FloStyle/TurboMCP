---
id: "c4485ba2-aa66-4413-b519-722823b1263f"
title: "T-AUDIT-12 (P1, medium) — AUDITED BY HAND 12/49: TurboMCPOverrideRemove.cpp — 182 lines, the count that caught its worst bug is not in the answer"
status: backlog
priority: "P1"
tags: ["P1", "medium", "audited-by-hand", "pattern-C-P1", "override"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-12T10:20:28Z"
updated: "2026-10-16T19:57:32Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-AUDIT-12 (P1, medium) — AUDITED BY HAND 12/49: TurboMCPOverrideRemove.cpp — 182 lines, the count that caught its worst bug is not in the answer

| Field | Value |
|---|---|
| Card ID | `c4485ba2-aa66-4413-b519-722823b1263f` |
| Status | backlog |
| Priority | P1 |
| Tags | `P1` `medium` `audited-by-hand` `pattern-C-P1` `override` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-12T10:20:28Z |
| Updated (UTC) | 2026-10-16T19:57:32Z |
| Executions recorded | 0 |

## Description

Summary
    `TurboMCPOverrideRemove.cpp`, 182 lines — the shortest file in the set, and good code. It names the measurement that once caught its own worst bug, then does not report that measurement.

What was found
    `:88-95` records what the first version did: it called `RemoveGraph` on the event's graph, and for an event override that graph holds every other event and the whole body of the Blueprint — so removing one override destroyed the other three and the graph with them, taking the overridden count from 4 to 1. It was caught only because the count after the removal was read back and compared with the count before.
    The delivered answer carries no count. `:111-124` reads `bStillImplemented` through the editor's own list and prints "the editor now reports it as implemented: %s"; `:141-153` does the same on the function-graph path. So the one measurement credited with catching the worst defect in this file's history is not in the answer. A regression of that shape today would be caught only by `bStillImplemented` going false, which is weaker: deleting the event graph also removes the override, so it cannot tell "the override went" from "the override and three others went".
    Fix: `UBlueprintEditorLibrary::ListEvents` and `ListFunctions` are already called at `:113` and `:142`, so the before/after count is two lines away. Print it the way `TurboMCPMaterialInstance.cpp:359-365` does — "looked at %d level(s) in the chain, each for: %s".

What is right (a lot for 182 lines)
    `:71-72` names both shapes before looking for either: an event override is a node in the ubergraph and a function override is a graph of its own, and both are looked for because a caller does not know which one they made. `:80` walks the node loop backwards, which is the only safe way to destroy from a container while iterating it. `:97-103` performs the same three steps the editor's own node deletion performs (`Modify`, `DestroyNode`, `MarkPackageDirty`). `:111` does the read-back through the editor's own list, so the answer is the engine's rather than the tool's own bookkeeping. `:125` and `:154` both say the change is in memory and not saved, and to call `E-SAVE`. `:159-161` lists the overridden names when nothing matched, so a caller corrects itself in one step. `:52` and `:67` both begin with `error:` and name what is wrong.

Note
    The file uses no `FScopedTransaction` at all — the undo step is the editor's own, through `Modify()` and `DestroyNode()` — and it has no unchecked cancel. The 28 found across the tree are confined to the three material writers.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why. For audit/documentation-only work, use direct validation only; do not run a complete Unreal suite.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
