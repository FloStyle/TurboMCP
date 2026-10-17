---
id: "8af56e37-5290-4b95-8057-8cc3a5df0ceb"
title: "T-C3-02 (P5, low) — C3 and C-P10 overlap on 16 sites: the C-P10 card owns them, and C3 owns only the comment question at those lines"
status: backlog
priority: "P5"
tags: ["P5", "low", "C3", "pattern-C-P10", "duplication"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-12T11:47:38Z"
updated: "2026-10-16T19:57:52Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-C3-02 (P5, low) — C3 and C-P10 overlap on 16 sites: the C-P10 card owns them, and C3 owns only the comment question at those lines

| Field | Value |
|---|---|
| Card ID | `8af56e37-5290-4b95-8057-8cc3a5df0ceb` |
| Status | backlog |
| Priority | P5 |
| Tags | `P5` `low` `C3` `pattern-C-P10` `duplication` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-12T11:47:38Z |
| Updated (UTC) | 2026-10-16T19:57:52Z |
| Executions recorded | 0 |

## Description

Summary
    The comment-versus-code card and the unchecked-write card overlap on sixteen sites. Thirteen of the remaining sites on the first card are already listed, with more detail, on the card about destructive operations with no checked result. This card exists so the work is not done twice.

The overlap, established by matching line numbers on both cards
    In `KitCommandlet.cpp`: `:1377` (`SaveContainer` result discarded, four TA-1..4 graphs), `:1421` (`Save` result discarded for TA-11), `:1510` (the TA-12 pair), `:1535` (TA-13), `:1721` (`SaveContainer` discarded, TA-7 judged by `bMeasured`), `:1816` (`SaveContainer` discarded, TA-14 reports measured while absent), `:1889` (`SaveContainer` discarded for TA-20), `:2010` (curve-float half of TA-17), `:2082` (curve-table half of TA-17).
    In `AcceptanceAssetCommandlet.cpp`: `:1138` (the asset file is deleted before the save, with no undo), `:1060` (a Blueprint renamed out of its package, the `Rename` bool discarded), and `:590`, `:596`, `:627`, `:708`, `:1014` (five links skipped silently).
    In `GraphTidy.cpp`: `:1306`, `:1382` (a link broken unconditionally, an undo not checked).
    In `GraphKnots.cpp`: `:1294`, `:1373` (already recorded on the other card as section G sites).

So the destructive-operations card owns them
    It names the failure mode (a write whose result nobody checks), the fix pattern (read the return value, and verify a restore against the set that changed) and the three places in the tree that already do it right.

What that leaves on the comment card
    The sites that really are comments to re-read and have no counterpart: `CaptureSelector.cpp:782` (`ClearSelectionSet` destroys the user's panel selection), `Comment.cpp:741` (the box count against the node count around the destroy), `LocalVars.cpp:641`, `PanelProbeCommandlet.cpp:280`, `GraphTidy.cpp:36` and `GraphTidy.h:22`, plus the single-site files extracted from the board export.
    Note the nine `KitCommandlet` saves are not comments at all — they are discarded return values, which is the other card's subject. Re-reading them for a comment contradiction would be answering the wrong question.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why. For audit/documentation-only work, use direct validation only; do not run a complete Unreal suite.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
