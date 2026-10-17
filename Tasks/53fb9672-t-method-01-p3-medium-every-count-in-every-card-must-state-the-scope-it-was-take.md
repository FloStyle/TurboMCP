---
id: "53fb9672-b301-4d18-877a-9bf737c0ad6b"
title: "T-METHOD-01 (P3, medium) — Every count in every card must state the scope it was taken over — 161 is a floor, not a total"
status: backlog
priority: "P3"
tags: ["P3", "medium", "verification", "counts", "method"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-12T10:09:02Z"
updated: "2026-10-16T19:57:32Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-METHOD-01 (P3, medium) — Every count in every card must state the scope it was taken over — 161 is a floor, not a total

| Field | Value |
|---|---|
| Card ID | `53fb9672-b301-4d18-877a-9bf737c0ad6b` |
| Status | backlog |
| Priority | P3 |
| Tags | `P3` `medium` `verification` `counts` `method` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-12T10:09:02Z |
| Updated (UTC) | 2026-10-16T19:57:32Z |
| Executions recorded | 0 |

## Description

Summary
    A count was published from an estimate and then measured. The mechanism named first was wrong; the conclusion is the same and the numbers are now real. The rule this card leaves behind: every count must state the scope it was taken over.

What was claimed, and why it was wrong
    The first version said 68 of 99 reports survived and 31 were lost, because the workflow caps each report at 3,500 characters and a truncated report can lose its lines from the tail. The cap is real — it was set by hand in the workflow script — but it is not what removed the reports. The spill file holds the workflow's return value, and the tool truncates that value to fit its output limit. The 3,500-character cap affected content, not count.

What was actually measured
    `src-batch1-raw.txt` has 12 `FILE:` markers and 10 `FINDINGS:` lines, with 2 reports cut mid-sentence. `src-batch2-raw.txt` has 28 and 27, with 1 cut mid-sentence. So 9 of the 68 saved reports are complete-but-short rather than truncated, and the real loss is the reports the spill never carried at all: 40 audited in batch 1 with 12 saved, 59 in batch 2 with 28 saved.
    The surviving numbers are larger than the card's own figure suggested: the 10 complete batch-1 reports declare 168 findings between them — not the ~50 lines that fitted the cap — and the 27 complete batch-2 reports declare 118, which matches.

The conclusion, now measured
    "161 C3 sites" is not the total of the saved reports either. It is the total of the lines that fitted. 161 is a floor.

The lesson
    It is sharper than the first version, because the same mistake was made again inside the correction: a mechanism was named from plausibility instead of from measurement. The plausibility was good — the cap was set by hand — and it was still not the cause.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why. For audit/documentation-only work, use direct validation only; do not run a complete Unreal suite.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
