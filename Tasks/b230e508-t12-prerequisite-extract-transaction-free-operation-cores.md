---
id: "b230e508-5b3b-4de1-aad3-b832838fc2cb"
title: "T12 prerequisite — Extract transaction-free operation cores"
status: todo
priority: ""
tags: []
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
parent: "bbb63d1e-1503-4094-9376-9ae2e2578222"
permission: "workspace-write"
created: "2026-10-16T16:47:26Z"
updated: "2026-10-16T19:58:15Z"
executions: 1
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T12 prerequisite — Extract transaction-free operation cores

| Field | Value |
|---|---|
| Card ID | `b230e508-5b3b-4de1-aad3-b832838fc2cb` |
| Status | todo |
| Priority | — |
| Tags | — |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Parent card | `bbb63d1e-1503-4094-9376-9ae2e2578222` |
| Permission | `workspace-write` |
| Created (UTC) | 2026-10-16T16:47:26Z |
| Updated (UTC) | 2026-10-16T19:58:15Z |
| Executions recorded | 1 |

## Description

T12 cannot safely compose existing writers because their public Apply methods each open a transaction, compile, and save. Extract reusable plan/apply cores with caller-owned transaction and deferred compile/save, starting with graph repoint and graph swap. Preserve existing public tool behavior through thin wrappers.

## Execution prompt

```text
Refactor the existing graph repoint and graph swap implementations so a batch coordinator can invoke transaction-free apply cores without nested transactions, while existing Preview/Edit tools retain current behavior. Add a minimal internal options/context shape for caller-owned transaction and deferred compile/save. Build and run existing focused graph repoint/swap tests. Do not commit or push; report files and validation.
```

## Execution history

### Run 1 — cancelled

- Started (UTC): 2026-10-16T16:47:36Z
- Ended (UTC): 2026-10-16T16:48:14Z
- Initiated by: `session-136fa179-d6e0-4893-8894-4ca8f3b60d6a`
- Session: `session-45ebac03-8b09-49d0-b223-383b4fa4721d`

> execution closed manually by session-136fa179-d6e0-4893-8894-4ca8f3b60d6a: no verdict was recorded

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
