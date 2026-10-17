---
id: "3be553c8-ff93-4646-9bd7-3b3712c1b582"
title: "X-18/X-21/X-22 (P2, medium) — read-only calls must not silently dirty, save or check out assets"
status: todo
priority: "P2"
tags: ["P2", "medium", "feedback-2026-10-15", "read-only", "dirty-list", "safety"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
skip_verification: true
created: "2026-10-15T19:07:48Z"
updated: "2026-10-16T19:58:11Z"
executions: 1
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# X-18/X-21/X-22 (P2, medium) — read-only calls must not silently dirty, save or check out assets

| Field | Value |
|---|---|
| Card ID | `3be553c8-ff93-4646-9bd7-3b3712c1b582` |
| Status | todo |
| Priority | P2 |
| Tags | `P2` `medium` `feedback-2026-10-15` `read-only` `dirty-list` `safety` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-15T19:07:48Z |
| Updated (UTC) | 2026-10-16T19:58:11Z |
| Executions recorded | 1 |

## Description

Sources: 2026-10-15 S1, S2 and S3. Engine graph and property reads left assets dirty; one asset later appeared saved and checked out without a named call. Identify the dirtying call. Add a read-only guard that restores only a flag it set, and retain call provenance in dirty-list/call-log output. A save must warn before saving an asset dirtied by a read.

## Execution prompt

```text
Start with a focused reproducible read-only fixture and dirty-list checks. Record the initial dirty state, read the asset, then prove that the tool preserves prior dirtiness while clearing only dirtiness it caused. Validate call provenance directly. Do not run a complete Unreal suite by default.
```

## Execution history

### Run 1 — failed

- Started (UTC): 2026-10-15T19:20:55Z
- Ended (UTC): 2026-10-15T19:26:00Z
- Initiated by: `session-55c2b580-0161-4851-acbb-ce6620397f8d`
- Session: `session-afac35d5-6a26-47ae-94e9-85df3ba563a2`

> goal is blocked: For three consecutive goal rounds, an external UnrealEditor-Cmd acceptance-asset commandlet (PID 39700) has held the TurboMCP project while MCP port 8000 refuses connections; the focused runtime fixture cannot safely run concurrently and this session must not terminate the external process without explicit direction.

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
