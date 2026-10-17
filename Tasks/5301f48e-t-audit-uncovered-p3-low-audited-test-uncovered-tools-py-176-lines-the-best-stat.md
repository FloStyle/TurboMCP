---
id: "5301f48e-4603-436a-b34f-54c31ab6f0a5"
title: "T-AUDIT-UNCOVERED (P3, low) — AUDITED: test-uncovered-tools.py — 176 lines, the best statement of C3 in the audit, and one weak check named"
status: backlog
priority: "P3"
tags: ["P3", "low", "audited-by-hand", "suites", "C3", "coverage"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-12T10:40:13Z"
updated: "2026-10-16T19:57:52Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-AUDIT-UNCOVERED (P3, low) — AUDITED: test-uncovered-tools.py — 176 lines, the best statement of C3 in the audit, and one weak check named

| Field | Value |
|---|---|
| Card ID | `5301f48e-4603-436a-b34f-54c31ab6f0a5` |
| Status | backlog |
| Priority | P3 |
| Tags | `P3` `low` `audited-by-hand` `suites` `C3` `coverage` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-12T10:40:13Z |
| Updated (UTC) | 2026-10-16T19:57:52Z |
| Executions recorded | 0 |

## Description

Summary
    One of the nine suites that names its failure input, and the one that closes the coverage gap. 176 lines, and it contains the best statement of the comment-versus-code problem this audit has read.

Two lessons it records about its own first versions
    1. `:54-61` — the first version asserted the wrong thing and went red on two tools that were right, because it assumed a tool named `<Toolset>Toolset` describes its toolset. Measured, neither does: `LookTurboMCPToolset` lists the live Slate windows a capture could frame, and `PreviewTurboMCPToolset` measures this plugin's help output against the 2000-character budget. So the checks assert what each tool says it does in its own doc comment, which is the only contract it has. A check must assert what the call promises, not what its name suggests.
    2. `:69-73` — assert the output, not the doc comment, because the first version of that check demanded the sentence "it does not capture an image yet" from the header, while `L-CAPTURE` arrived in phase 3 and the caption now reads "look: editing windows, N visible". The header comment is stale and the code is right. A check written from a doc comment tests the comment — and that is the best statement of the underlying problem in this whole audit: the doc and the code diverge, and a test written from the doc validates the doc.

What the suite is for
    `Tools/audit-tool-coverage.py` reads the tool declarations out of the header and the calls out of every suite, and reports the tools that no suite calls. Its first honest run found five, named with their signatures: `EditTurboMCPCompile`, `LookTurboMCPToolset`, `PreviewTurboMCPToolset`, `ReadTurboMCPEditQueue`, `ReadTurboMCPNodeResolve`. A tool that registers and answers nothing when called looks finished in `R-TOOL-INDEX` and in the panel's tool list, and nothing contradicts it — a panel lets a human try a tool, and notices nothing about a tool nobody tried. `:21-27` lists what each check proves before the checks, including that every tool answers rather than returning an empty string, so a reader can see what is and is not covered.

One weak check, named because the card should be honest about the suite it praises
    `:74-75` asserts `look.count("\n") >= 1`, which is true for any multi-line output, including an error message that spans lines. It is not in the cannot-fail class — a single-line answer would fail it — but it is weak, and the suite's own standard says what it needs: a `FAILS ON` line naming the input, for example "fails on: the listing collapsing to one line". The suite names its failures elsewhere, so this is an omission rather than a habit.

Note
    `check-fails-on.py` reports that 34 of 52 suites do not state their failing input. This one does, and it also records the two wrong versions of its own checks — the standard here is high where it is applied.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why. For audit/documentation-only work, use direct validation only; do not run a complete Unreal suite.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
