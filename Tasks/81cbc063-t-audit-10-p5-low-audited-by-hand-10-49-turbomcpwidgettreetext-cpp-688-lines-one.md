---
id: "81cbc063-f063-4b7a-a9e6-d8633db9951d"
title: "T-AUDIT-10 (P5, low) — AUDITED BY HAND 10/49: TurboMCPWidgetTreeText.cpp — 688 lines, one comment omits the character its sibling calls decisive, and no round-trip defect"
status: backlog
priority: "P5"
tags: ["P5", "low", "audited-by-hand", "C3", "widget"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-12T10:19:06Z"
updated: "2026-10-16T19:57:32Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-AUDIT-10 (P5, low) — AUDITED BY HAND 10/49: TurboMCPWidgetTreeText.cpp — 688 lines, one comment omits the character its sibling calls decisive, and no round-trip defect

| Field | Value |
|---|---|
| Card ID | `81cbc063-f063-4b7a-a9e6-d8633db9951d` |
| Status | backlog |
| Priority | P5 |
| Tags | `P5` `low` `audited-by-hand` `C3` `widget` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-12T10:19:06Z |
| Updated (UTC) | 2026-10-16T19:57:32Z |
| Executions recorded | 0 |

## Description

Summary
    `TurboMCPWidgetTreeText.cpp`, 688 lines — the dialect's text parser and writer, and the only file in the set with no transactions, no writes and no counters. The code patterns that dominate the material and source-control writers cannot occur here. One finding, and one suspicion checked and discarded.

What was found
    The scanner's doc comment omits the one character its own sibling calls decisive. `:89` says a bare token runs "up to whitespace, a closing parenthesis or a comment", but the code at `:96-100` also stops on `(` and on `;`. The neighbouring `IsPlainName` — the function that decides what may be written bare — names the missing case as the decisive one: a space, a parenthesis, a quote, a semicolon or a line break "would end the token early or start a comment, so those are the ones that have to be quoted". One comment is therefore complete and its neighbour is not, and they describe the same rule from the two sides of one round trip. A reader who trusts `:89` and writes a value containing `(` produces text their own parser stops short on.
    Fix: one clause — "up to whitespace, an opening or closing parenthesis, or a comment".

A suspicion that was checked and discarded
    A round-trip defect looked likely: `WriteValue` at `:118-137` emits a value bare whenever `IsPlainName` allows it, and `TakeToken` stops on `(` — so a value containing `(` would be written bare and parse short. Reading `IsPlainName` settles it: it rejects `(`, `)`, `"`, `;` and every whitespace character, so such a value is quoted and the round trip holds. There is no defect there.

What is right (do not re-investigate)
    `:35-44` explains why a scanner rather than a token array, and the reason is diagnostic quality rather than speed: the faults worth naming are positional ("unclosed parenthesis at line 12", "expected a class after `:class`"), and a scanner can say where it stopped while a pre-tokenised array has to carry a position on every token to say the same thing. All 24 diagnostics name a line — every `OutReason`/`OutError` begins `line %d:` and says what was expected and what was found (`:175`, `:186`, `:203`, `:210`, `:220`, `:257`, `:271`, `:279`, `:289`, `:299`, `:346`, `:361`, `:372`, `:383`, `:395`, `:423`, `:559`, `:575`, `:584`, `:598`, `:648`). `:117-136` escapes `;` as well as `"`, because an unescaped semicolon inside a quoted value would still be read back as the start of a comment by a reader that applied the comment rule before the quoting rule; `:159-164` honours that escape. `:118-123` writes an empty value as `""` so an empty string survives the round trip. `:56-57` returns `'\0'` at the end rather than reading past it, so callers can compare without a bounds test. `:66-69` is the only place `Line` advances, so a line number cannot drift from the position. `:73-79` treats `\r` as whitespace as well as `\n`, which is what makes a CRLF input parse.

Note
    The two defect families that account for most findings in the material writers cannot occur in a pure text transform. A defect count per file is not a measure of quality: this may be the cleanest file in the set because the shape of the work removes the opportunities, not because its author was more careful.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why. For audit/documentation-only work, use direct validation only; do not run a complete Unreal suite.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
