# T-AUDIT-20 (P6, low) — AUDITED BY HAND 20/49: TurboMCPTraps.cpp — 202 lines, zero defects, and four failure states kept distinct

ID: 8beb2deb-3cfd-4c55-854e-25c6cb917a26
Status: backlog
Priority: P6
Readiness: needs_reconciliation
Owner: unassigned
Design: docs/design/verification.md

## Roadmap disposition

Retain as regression/reference evidence; no implementation spend is authorized by a historical no-defect finding.

## Scope and acceptance

Preserve the substantive scope below. Before editing, reconcile the finding with current source and identify the focused acceptance check. Do not execute historical board/harness instructions. Record duplicates by task ID without discarding their requirements.

## Imported evidence

Historical paths and completion claims below are not current proof. This task must be made self-contained against the current source before implementation; no private archive is a prerequisite for public contributors.

> Summary
>     `TurboMCPTraps.cpp`, 202 lines, zero defects found. One word in one comment is optimistic, and it is recorded as the small thing it is rather than inflated into a finding.
>
> What the file does well
>     It keeps four failure states apart that most code merges into one. `:27-32` keeps the parser separate from the loader, so a parse failure and a missing file are reported differently, because they need different fixes. `:41-45` reports "the file is not valid JSON". `:48-52` reports "the file has no 'tools' object". `:97-101` reports "the file parsed but recorded no tools". And `:152-156` reports "no traps file could be read. Tried: %s" with every path, plus the error that rejected any file that existed and did not parse (`:148`). Four distinct answers, each leading to a different action, and the answer names every path it looked at.
>     `:144-146` warns about a file that exists but does not parse even when an earlier file already provided notes, because the author will otherwise never learn their file is being ignored — the failure that is easiest to hide is the one this file refuses to hide. `:174-176` parses into a temporary so a broken later file cannot erase notes that an earlier file provided. `:184-186` replaces a tool's entry outright and says why: merging would make it impossible to remove a trap that no longer applies, which is argued from what the user needs to be able to do rather than from what is convenient to implement. `:191-197` builds the table once in a function-local static, which is thread safe in C++11 and matters because tool calls can arrive on a worker thread. `:122-125` adds the engine copy only when it is not the same path as the shipped one, so a project carrying its own copy does not read the same file twice. `:110-118` builds every path through `FPaths::Combine`, so no absolute machine path appears anywhere in the file.
>
> The one thing to change
>     `:109` says "shipped notes first, so a project file can add to or correct them", but `LoadFile` replaces a tool's entry outright and the comment at `:184-186` explains why. A project file can therefore correct a shipped note and cannot add one to it. The comment at `:184` is right and the one at `:109` is more optimistic than the code. One clause to fix.

## Verification

Pending. Record command, result, commit, and any blocked acceptance here.
