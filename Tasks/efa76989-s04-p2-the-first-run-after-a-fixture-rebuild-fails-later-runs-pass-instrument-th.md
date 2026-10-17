---
id: "efa76989-7e08-4673-8ac1-901c709cdc04"
title: "S04 / P2 — the first run after a fixture rebuild fails; later runs pass. Instrument the FAILING run."
status: todo
priority: "P2"
tags: ["release-1.0", "P2", "test-hygiene", "order-dependent", "S04"]
workspace: "7d139a89-61a3-4566-b367-cf7a89b9969b"
parent: "5f2cee78-ed31-487e-bfa6-6452eaaf738f"
created: "2026-10-16T21:56:59Z"
updated: "2026-10-16T21:56:59Z"
executions: 0
board_revision: 1803
exported: "2026-10-17T09:13:25Z"
---

# S04 / P2 — the first run after a fixture rebuild fails; later runs pass. Instrument the FAILING run.

| Field | Value |
|---|---|
| Card ID | `efa76989-7e08-4673-8ac1-901c709cdc04` |
| Status | todo |
| Priority | P2 |
| Tags | `release-1.0` `P2` `test-hygiene` `order-dependent` `S04` |
| Workspace | `7d139a89-61a3-4566-b367-cf7a89b9969b` |
| Parent card | `5f2cee78-ed31-487e-bfa6-6452eaaf738f` |
| Created (UTC) | 2026-10-16T21:56:59Z |
| Updated (UTC) | 2026-10-16T21:56:59Z |
| Executions recorded | 0 |

## Description

A defect that has appeared twice and vanished twice. Evidence: Release/EVIDENCE/S04/swap-order-dependent.md
MEASURED: four consecutive runs this round - test-graph-swap-call.py exit 0 PASS, PASS, PASS, and test-graph-repoint.py exit 0 PASS. But in TWO earlier rounds the swap suite failed `readback preserves the migrated execute link`, both times on the run immediately following a FIXTURE REBUILD.

=== WHY IT STILL MATTERS THOUGH PASSES ARE REPRODUCIBLE ===
A DISAPPEARING DEFECT IS NOT A FIXED ONE. The FAILURE is reproducible too - on the run after a rebuild - and it has now occurred twice. A release cannot ship a suite that fails on its first run and passes on its second, because the first run is the one a fresh checkout performs.
It also cost real rounds: round 22 read it as stale state from earlier refusals, round 39 read it as the swap tool's own un-unified graph resolver, and this round DISPROVED the second of those by running it. That is the ninth mechanism disproved in the graph sequence.

=== THE INSTRUMENT, AND IT IS THE POINT ===
INSTRUMENT THE FAILING RUN, NOT THE PASSING ONES. Concretely: run the suite four times and RECORD WHICH RUN FAILS; then read the state on that run only.
Two candidate readings, NEITHER established:
  * the suite's `rebuild_fixture` + `start-editor` sequence leaves the editor with an asset that has not finished loading or compiling when the first read is taken;
  * the case's first assertion reads state that its own setup invalidates - `rebuild_fixture` is DOCUMENTED to stop the editor and not start it, and test-graph-swap-call.py restarts it, so the first read after a cold start is the one that differs.

=== DO NOT ===
Do not close this because four passes were observed. Do not close it by loosening the check. And do not propose a tenth mechanism from reading code - measure the failing run.

=== ALSO DUE, SEPARATE ===
Six local graph resolvers remain to unify (Comment, GraphAppend, GraphPatch, GraphReplace, GraphStructure x2, GraphSwapCall) onto TurboMCPNodeId::FindGraphByName, plus making the shared resolver report a MULTIPLE MATCH so an ambiguous graph name is refused rather than silently resolved.

## Execution prompt

```text
S04: the first run after a fixture rebuild fails. Instrument the failing run. Do not run from the board.
```

## Execution history

_No execution record. The card state is a board declaration, not run evidence._

---

_Exported from the DSH task board, revision 1803, at 2026-10-17T09:13:25Z. The board is authoritative; this file is a snapshot._
