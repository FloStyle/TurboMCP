---
id: "9c47994b-6b0a-49c3-a332-44e64650f199"
title: "X-27 / FT-41 (P2, medium) — preview and revert unsaved assets safely"
status: todo
priority: "P2"
tags: ["P2", "medium", "feedback-2026-10-16", "asset-safety", "revert", "FT-41"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-16T16:45:39Z"
updated: "2026-10-16T19:58:15Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# X-27 / FT-41 (P2, medium) — preview and revert unsaved assets safely

| Field | Value |
|---|---|
| Card ID | `9c47994b-6b0a-49c3-a332-44e64650f199` |
| Status | todo |
| Priority | P2 |
| Tags | `P2` `medium` `feedback-2026-10-16` `asset-safety` `revert` `FT-41` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-16T16:45:39Z |
| Updated (UTC) | 2026-10-16T19:58:15Z |
| Executions recorded | 0 |

## Description

Source: 2026-10-16 S2/S3. No tool restores an unsaved asset from disk; agents rebuilt nodes manually. Add preview and revert with dependent-unsaved assets, before/after node counts, explicit package scope and safe refusal conditions.

## Execution prompt

```text
First design safety boundaries: dirty-state provenance, loaded-package behavior, dependent preview and no-save guarantee. Add focused fixture proving revert restores on-disk node identity/state and reports before/after. Commit/push only verified work.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
