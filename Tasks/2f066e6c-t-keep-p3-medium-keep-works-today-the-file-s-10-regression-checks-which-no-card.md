---
id: "2f066e6c-8c34-412f-b1b4-451389996086"
title: "T-KEEP (P3, medium) — `Keep: works today`: the file's 10 regression checks, which no card owned until now"
status: todo
priority: "P3"
tags: ["P3", "medium", "keep", "regression", "feedback-2026-10-13", "release-1.0"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T23:25:41Z"
updated: "2026-10-16T19:58:03Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-KEEP (P3, medium) — `Keep: works today`: the file's 10 regression checks, which no card owned until now

| Field | Value |
|---|---|
| Card ID | `2f066e6c-8c34-412f-b1b4-451389996086` |
| Status | todo |
| Priority | P3 |
| Tags | `P3` `medium` `keep` `regression` `feedback-2026-10-13` `release-1.0` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T23:25:41Z |
| Updated (UTC) | 2026-10-16T19:58:03Z |
| Executions recorded | 0 |

## Description

Purpose
Map the ten release regression checks to current suites.

Current mapping
| Check | Suite or command | Gap |
| --- | --- | --- |
| Edit queue | test-uncovered-tools | None found. |
| Compile | test-uncovered-tools and test-compile-ensure | No multi-asset check. |
| Save | test-save-thumbnail-option | No multi-asset check. |
| Node resolve | test-uncovered-tools | None found for the basic check. |
| Node-type lookup | test-batch-tools and test-node-pins-owner | No cross-Blueprint call ID check. No compile-needed check. |
| Struct create and fields | test-struct | None found for the basic check. |
| Variables add | test-vars-add | None found. |
| Function add and remove | test-function | None found. |
| References | test-refs | None found for the current reference task. |
| Dead-node preview | test-graph-dead | No repeated-live-logic check. |

Suggested command
pwsh -File Tools\run-suites.ps1 -Suites test-uncovered-tools,test-compile-ensure,test-save-thumbnail-option,test-batch-tools,test-node-pins-owner,test-struct,test-vars-add,test-function,test-refs,test-graph-dead

Do not run this command while code agents build or change fixtures.

Done when
1. Add or document every missing check.
2. Run the sequence from a clean worktree.
3. Record the result on this task.

## Execution prompt

```text
Map the ten Keep regression rows to their current targeted checks and document a minimal selected-suite command. Validate each mapping directly. Do not run a complete Unreal suite; run only the selected checks when execution is needed.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
