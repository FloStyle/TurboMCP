---
id: "cdbf31c9-caa6-4233-8314-1f960538ea8c"
title: "T36 (P6) — Source control next to the editor hooks"
status: todo
priority: "P6"
tags: ["P6", "roadmap", "source-control", "feedback-2026-10-13"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-13T21:05:27Z"
updated: "2026-10-16T19:58:03Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T36 (P6) — Source control next to the editor hooks

| Field | Value |
|---|---|
| Card ID | `cdbf31c9-caa6-4233-8314-1f960538ea8c` |
| Status | todo |
| Priority | P6 |
| Tags | `P6` `roadmap` `source-control` `feedback-2026-10-13` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-13T21:05:27Z |
| Updated (UTC) | 2026-10-16T19:58:03Z |
| Executions recorded | 0 |

## Description

*** SOURCE: `Feedbacks/TurboMCP_TaskList_2026-10-13.md:365`, group P6 (a table row). ***

    CONTENT, IN THE LIST'S OWN WORDS
        *** "Source control next to the editor hooks." ***
    NOTE, IN THE LIST'S OWN WORDS
        *** "REPORT 'ALREADY CHECKED OUT BY THE HOOK' AS SUCCESS. THE SAVE REPORT SAYS WHO DID THE CHECKOUT." ***

    *** AND THE NOTE IS A STATUS-CODE REQUIREMENT WITH A MEASURED PRECEDENT IN THIS TREE: `T31`'S CARD QUOTES THE
    CURRENT CHECKOUT BLOCK, WHICH ALREADY SAYS "the checkout answer NEVER gates the write ... so a provider that cannot
    take a lock cannot turn a working write into a failed one - which is the outcome `D-57` exists to prevent". ***
    *** SO THE WORK IS TO EXTEND THAT DISTINCTION TO A SECOND, LEGITIMATE REASON A CHECKOUT DID NOT HAPPEN: SOMEONE
    ELSE - A HOOK - ALREADY DID IT. TODAY THE TOOL WOULD REPORT A STATE, NOT A VERDICT, AND THE CALLER CANNOT TELL AN
    ALREADY-CHECKED-OUT FILE FROM A FILE NOBODY COULD CHECK OUT. ***
    *** AND "ALREADY CHECKED OUT" IS ALREADY A MODELLED CASE: `TurboMCPSourceControl.cpp` HANDLES A ROW ALREADY UNDER
    CHECKOUT (`:165-171`), AND `GitSourceControlModule.cpp:21` IS CITED FOR GIT'S ALREADY-CHECKED-OUT STATE. THE
    MEASUREMENT IS WHETHER THE TWO PATHS PRODUCE THE SAME SENTENCE TODAY. ***

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
