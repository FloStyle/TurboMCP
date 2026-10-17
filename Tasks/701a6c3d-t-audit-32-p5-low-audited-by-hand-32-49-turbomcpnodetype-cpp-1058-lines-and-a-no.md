---
id: "701a6c3d-90d5-403d-84f1-5f2eacc2113d"
title: "T-AUDIT-32 (P5, low) — AUDITED BY HAND 32/49: TurboMCPNodeType.cpp — 1058 lines, and a note with a written contract that fourteen assignments fill and no caller reads"
status: backlog
priority: "P5"
tags: ["P5", "low", "audited-by-hand", "pattern-C-P2", "pattern-C-P7", "nodetype"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-12T10:34:02Z"
updated: "2026-10-16T19:57:32Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-AUDIT-32 (P5, low) — AUDITED BY HAND 32/49: TurboMCPNodeType.cpp — 1058 lines, and a note with a written contract that fourteen assignments fill and no caller reads

| Field | Value |
|---|---|
| Card ID | `701a6c3d-90d5-403d-84f1-5f2eacc2113d` |
| Status | backlog |
| Priority | P5 |
| Tags | `P5` `low` `audited-by-hand` `pattern-C-P2` `pattern-C-P7` `nodetype` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-12T10:34:02Z |
| Updated (UTC) | 2026-10-16T19:57:32Z |
| Executions recorded | 0 |

## Description

Summary
    `TurboMCPNodeType.cpp`, 1058 lines — the largest reader in the tree. One finding, and it is the fifth instance of the pattern this audit keeps finding: a correct idea that is not wired to the place that needs it.

What was found
    An `FString& OutNote` carries a written contract, is filled fourteen times, and is read by nobody. `TurboMCPNodeType.h:140` documents it: "@param OutNote why it could not be built, when it could not. Never empty on failure." `:180` repeats the contract for `SpawnNodeOfType` with one stated exception ("never empty unless Spawned"). The implementation assigns it fourteen times — `:274` "no Blueprint", `:279` "no graph", `:337` "a transient graph for the probe could not be created", plus `:349`, `:358`, `:380`, `:385`, `:440`, `:461`, `:468`, and two `OutNote.Empty()` calls at `:270` and `:376`. A `git grep` for `OutNote` finds only two files in the whole repository: the .cpp with its 14 uses and the .h with 4 (the two declarations and their documentation). No caller passes it and no line reads it — a contract with no counterparty. The implementation does the work the doc promises; the string is discarded.
    The file itself shows where the note belongs: `:469-470` builds the refusal from `ESpawnResult` alone, so the caller is told *that* the spawn failed and never *why*.
    Fix: append the note where the failure is turned into an answer. One line, and the doc already describes what it should say.

Second item, recorded so it is not re-reported
    The transient probe graph. `:334` creates a `UEdGraph` with `RF_Transient`, which invites the "does a read change something?" question, and `:328-333` answers it with the reason: its outer is the Blueprint so a node that walks its outer chain finds one, it is not added to the Blueprint's graph lists, and so nothing in the asset is touched. `:335-338` tests it for null; `:135-137` states the caller's obligation, that the real graph's node count does not change and the caller owns the result. There is no `Modify()`, no `Transaction` and no `MarkPackageDirty` anywhere in the file, so it cannot leave the asset dirty.

Note
    That makes five instances of one pattern at five scales: a `kind` that `KnownKinds` does not accept (`TurboMCPTypes.cpp`), `NormaliseKey` used by two of its four callers (`TurboMCPNodeId.cpp`), a `mutable` documented in `TurboMCPPanelToolRunner.h` that the code replaced with a `const_cast`, a template rule that only 9 of 52 suites follow (`Tools/make-tools-doc.py`), and this one. The defect is the same every time and so is the repair: wire it, or delete it. What must not remain is a contract, a guard, a field or a rule that exists in one place and is consulted in none.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why. For audit/documentation-only work, use direct validation only; do not run a complete Unreal suite.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
