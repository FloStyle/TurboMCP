---
id: "abdd2c08-6544-4730-b183-e4d856aad868"
title: "T22 (P3, medium) — One field syntax for all field tools"
status: todo
priority: "P3"
tags: ["P3", "medium", "schema", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:04:45Z"
updated: "2026-10-16T19:57:58Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T22 (P3, medium) — One field syntax for all field tools

| Field | Value |
|---|---|
| Card ID | `abdd2c08-6544-4730-b183-e4d856aad868` |
| Status | todo |
| Priority | P3 |
| Tags | `P3` `medium` `schema` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:04:45Z |
| Updated (UTC) | 2026-10-16T19:57:58Z |
| Executions recorded | 0 |

## Description

Source
    `Feedbacks/TurboMCP_TaskList_2026-10-13.md:270-277`, group P3. Source IDs: X-10 and SF-03 (S4), DT-01 (S3), SN-02 (S4). This card came from the list. No one measured it on the tree.

done when
    1. Struct new, struct fields, variables add, function signatures and table rows all accept "Name: Type option=value". They all use the same parser.
    2. The table tools accept display names first, then internal names. A refusal lists the display names.
    3. "float" maps to a double-precision real, and the result says so. "real" and "double" are accepted.
    4. Each help text has one example with options, including "force" for a removal.

Why clause 2 is spelled out
    A display name is what a person sees. An internal name is what the engine stores. This is the only place in the project where two names exist for one field. `C-P7` says that a refusal must lead to the next step, so a refusal has to name the spelling the caller typed.

Clause 3 is already partly measured
    `test-vars-add.py:30-32` uses `Speed: float default=250 category=Movement`, and the tool accepts it. `test-types` has a `kind` question on record. The first measurement is therefore a grep of every field parser, not a read of one.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
