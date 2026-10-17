---
id: "c2865829-9206-4df7-8cf1-89d8d1c319bc"
title: "Release 1.0 gate: safety, build, and public release"
status: todo
priority: ""
tags: ["release-1.0", "release-last", "release-gate", "public-release"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-14T19:21:53Z"
updated: "2026-10-16T19:58:11Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# Release 1.0 gate: safety, build, and public release

| Field | Value |
|---|---|
| Card ID | `c2865829-9206-4df7-8cf1-89d8d1c319bc` |
| Status | todo |
| Priority | — |
| Tags | `release-1.0` `release-last` `release-gate` `public-release` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-14T19:21:53Z |
| Updated (UTC) | 2026-10-16T19:58:11Z |
| Executions recorded | 0 |

## Description

Purpose
Release only after all release-blocker tasks are complete.

Release blockers
1. Fix false graph output. The graph checker must accept the output.
2. Fix false zero reference output.
3. Load plugin tools from a renamed plugin folder.
4. Declare all build dependencies. Remove the known build warnings.
5. Keep the Git worktree clean after the test sequence.
6. Run the defined regression sequence. Record each result.

Release checks
1. Build from a clean worktree.
2. Run the release regression sequence.
3. Check the public source for secrets and local paths.
4. Check the public branch and release tag.
5. Publish only after all checks pass.

This card does not replace the linked product tasks. It controls release approval.

## Execution prompt

```text
Manage the Release 1.0 gate without marking it complete until every blocker passes. Use each blocker's focused verification first. The complete release regression sequence is required only at the final release-gate decision, after blockers are green.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
