---
id: "8beb2deb-3cfd-4c55-854e-25c6cb917a26"
title: "T-AUDIT-20 (P6, low) — AUDITED BY HAND 20/49: TurboMCPTraps.cpp — 202 lines, zero defects, and four failure states kept distinct"
status: backlog
priority: "P6"
tags: ["P6", "low", "audited-by-hand", "clean", "traps"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
created: "2026-10-12T10:24:52Z"
updated: "2026-10-16T19:57:32Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# T-AUDIT-20 (P6, low) — AUDITED BY HAND 20/49: TurboMCPTraps.cpp — 202 lines, zero defects, and four failure states kept distinct

| Field | Value |
|---|---|
| Card ID | `8beb2deb-3cfd-4c55-854e-25c6cb917a26` |
| Status | backlog |
| Priority | P6 |
| Tags | `P6` `low` `audited-by-hand` `clean` `traps` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Created (UTC) | 2026-10-12T10:24:52Z |
| Updated (UTC) | 2026-10-16T19:57:32Z |
| Executions recorded | 0 |

## Description

Summary
    `TurboMCPTraps.cpp`, 202 lines, zero defects found. One word in one comment is optimistic, and it is recorded as the small thing it is rather than inflated into a finding.

What the file does well
    It keeps four failure states apart that most code merges into one. `:27-32` keeps the parser separate from the loader, so a parse failure and a missing file are reported differently, because they need different fixes. `:41-45` reports "the file is not valid JSON". `:48-52` reports "the file has no 'tools' object". `:97-101` reports "the file parsed but recorded no tools". And `:152-156` reports "no traps file could be read. Tried: %s" with every path, plus the error that rejected any file that existed and did not parse (`:148`). Four distinct answers, each leading to a different action, and the answer names every path it looked at.
    `:144-146` warns about a file that exists but does not parse even when an earlier file already provided notes, because the author will otherwise never learn their file is being ignored — the failure that is easiest to hide is the one this file refuses to hide. `:174-176` parses into a temporary so a broken later file cannot erase notes that an earlier file provided. `:184-186` replaces a tool's entry outright and says why: merging would make it impossible to remove a trap that no longer applies, which is argued from what the user needs to be able to do rather than from what is convenient to implement. `:191-197` builds the table once in a function-local static, which is thread safe in C++11 and matters because tool calls can arrive on a worker thread. `:122-125` adds the engine copy only when it is not the same path as the shipped one, so a project carrying its own copy does not read the same file twice. `:110-118` builds every path through `FPaths::Combine`, so no absolute machine path appears anywhere in the file.

The one thing to change
    `:109` says "shipped notes first, so a project file can add to or correct them", but `LoadFile` replaces a tool's entry outright and the comment at `:184-186` explains why. A project file can therefore correct a shipped note and cannot add one to it. The comment at `:184` is right and the one at `:109` is more optimistic than the code. One clause to fix.

## Execution prompt

```text
Testing policy: Before editing, run the fastest relevant focused/static check. Re-run it after the change. Avoid a complete suite by default; run one only when this change spans integrations or the focused test cannot establish confidence, and record why. For audit/documentation-only work, use direct validation only; do not run a complete Unreal suite.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
