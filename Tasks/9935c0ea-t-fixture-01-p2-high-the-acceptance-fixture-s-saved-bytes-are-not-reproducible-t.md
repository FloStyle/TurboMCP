---
id: "9935c0ea-9d4a-4287-9786-6a805c38d76d"
title: "T-FIXTURE-01 (P2, high) — The acceptance fixture's saved bytes are NOT reproducible: three sizes from one source, so the tracked asset drifts on every rebuild"
status: todo
priority: "P2"
tags: ["P2", "high", "verification", "fixture", "measured", "release-1.0"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-12T12:28:05Z"
updated: "2026-10-16T19:57:52Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-FIXTURE-01 (P2, high) — The acceptance fixture's saved bytes are NOT reproducible: three sizes from one source, so the tracked asset drifts on every rebuild

| Field | Value |
|---|---|
| Card ID | `9935c0ea-9d4a-4287-9786-6a805c38d76d` |
| Status | todo |
| Priority | P2 |
| Tags | `P2` `high` `verification` `fixture` `measured` `release-1.0` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-12T12:28:05Z |
| Updated (UTC) | 2026-10-16T19:57:52Z |
| Executions recorded | 0 |

## Description

Problem
The generated acceptance fixture is tracked. Repeated writes can change its bytes.

Measured state
TestProject/Content/Tests/TurboMCPAcceptance.uasset is tracked. It is not ignored.

Conflict
Content/README.md says generated fixtures are not tracked. The Git state does not match that rule.

Cause
The runner saves the initial fixture bytes. It restores them at the end of a run. Direct fixture commands can still change the tracked asset.

Proposed fix
1. Remove only TurboMCPAcceptance.uasset from Git tracking.
2. Add its exact path to TestProject/.gitignore.
3. Do not use a wildcard. The directory contains hand-made test assets.
4. Do not add only an ignore rule. Ignore rules do not affect a tracked file.

Required checks
1. In a clean clone, run the normal test runner. Check that it creates the fixture before a dependent suite.
2. Run one mutating fixture suite. Check that git status --porcelain is empty.
3. Run direct fixture generation. Check that it creates no Git status entry.
4. Check a runner failure path and a forced editor-stop path.
5. Check that every fixture-dependent command regenerates the fixture or uses run-suites.ps1.

Do not change code until these checks have a plan.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
