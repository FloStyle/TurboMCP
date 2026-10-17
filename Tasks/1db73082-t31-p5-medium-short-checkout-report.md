---
id: "1db73082-b800-4503-b6df-2d1ad761baf7"
title: "T31 (P5, medium) — Short checkout report"
status: todo
priority: "P5"
tags: ["P5", "medium", "source-control", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:05:27Z"
updated: "2026-10-16T19:57:58Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T31 (P5, medium) — Short checkout report

| Field | Value |
|---|---|
| Card ID | `1db73082-b800-4503-b6df-2d1ad761baf7` |
| Status | todo |
| Priority | P5 |
| Tags | `P5` `medium` `source-control` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:05:27Z |
| Updated (UTC) | 2026-10-16T19:57:58Z |
| Executions recorded | 0 |

## Description

Source
    `Feedbacks/TurboMCP_TaskList_2026-10-13.md:335-338`, group P5. Source IDs: X-08 (S3). This card came from the list. No one measured it on the tree.

done when
    1. Each write result has one checkout line per asset, for example "checkout: Perforce, succeeded".
    2. The full policy text appears only on request or on failure.

Why the card states both halves
    The two halves pull in opposite directions. The short line is for the common case. The full text is for the case that needs it.
    This session measured how big the full text is. The answer from `test-vcs-write` carries a checkout block whose own sentences include "the checkout answer never gates the write", "no asset file was written by this section" and "a checkout is a source control operation; the write tool's own result below or above is the file write". All three are correct. All three are noise on a successful call.

The shape already exists in the same file
    `TurboMCPSourceControlWrite.cpp:486` prints "%s: %s" for each row. The measurement is what the current success line says and how many lines it takes.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
