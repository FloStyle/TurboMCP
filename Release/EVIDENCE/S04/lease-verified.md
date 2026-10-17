# S04 evidence — the lease, verified against a real editor, and the defect it still has

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**State: `implemented_unverified`, and the graph repoint migration's regression status is still UNKNOWN.** Two
corrections from the previous record, one verified fix, and one remaining defect that blocks the suites.

## Corrections to the previous record, both measured

**1. `-Stop` DOES release the lease.** The previous record said it left an old-format lease in place. Re-measured:

```
lease before: True
  releasing a lease left by 'start-editor:7488' (pid 7488); no editor of this project is running
RESULT: PASS
lease after : False
```

The earlier `True` was read after a run whose `-Stop` had already been followed by other activity. **The
round-10 note was wrong, and it is corrected here rather than left standing.** The `$anyForeign.Count -eq 0`
condition does what it says.

**2. The stale-lease fix is real and now observed.** MEASURED on a live editor:

```json
{ "OwnerId": "start-editor:27724", "Pid": 27724, ..., "EditorPid": 23192 }
actual editor pid: 23192
```

The lease now names the **editor** (23192), not the launcher that exited (27724). That is the fix working.

## The remaining defect, and it is what blocks the suites

The corrected lease now blocks a start whenever its named editor is alive — correctly, in isolation — but **the
fixture-rebuilding workflow needs exactly the opposite**. MEASURED, twice, on `test-graph-repoint.py`:

```
ok    lease held by start-editor:2408 (pid 2408) at ...\turbo-mcp-lease.json
ok    no editor running
ok    window up after 15s: TurboMCPTest - Unreal Editor
ok    MCP server listening on port 8000
ok    all four TurboMCP toolsets answered
RESULT: PASS                      <- the suite's own fixture rebuild starts an editor successfully
FAIL  the lease could not be taken: an editor of this project is already running (pid 22720),
      held by 'start-editor:2408'   <- the SUITE's next start is refused by the lease its own rebuild created
RESULT: FAIL
```

So the sequence the suite performs — rebuild the fixture (which starts an editor) and then run its own checks —
now fails at the second start. **The lease is working as designed and the design does not fit the workflow.**

The fix is narrow and I did not have the context budget to make and test it: a **reclaim rule**. A lease whose
named editor is alive AND belongs to THIS project (which the coordinator can establish from that editor's own
command line, exactly as `Test-TurboMCPOwnsProcess` already does) should be **taken over** by a new start of the
same project, recording the displaced owner in the lease's `Note`. That keeps the property that matters — a
FOREIGN editor is still never pre-empted — while letting this repository restart its own editor freely.

## What is verified, what is not

| Item | State |
|---|---|
| The lease records the editor's pid, not the launcher's | **verified against a live editor** |
| A lease whose editor is gone does not block | implemented; the path was exercised by the old-format lease test |
| `-Stop` releases the lease | **verified** |
| A foreign editor is never owned or pre-empted | verified in round 4 (E01) |
| A same-project restart is allowed | **NOT implemented — this is the blocker** |
| `test-graph-repoint.py` passes | **NOT run to a pass** |
| `test-graph-swap-call.py` passes | **NOT run to a pass** |
| The graph repoint migration's regression status | **UNKNOWN** |

## Honest summary of the round

The migration's regression check was attempted and **did not complete**. Two of the previous round's claims were
corrected by measurement. One fix was verified against a real editor. One defect was found that blocks the check,
and it is recorded with the exact narrow change it needs rather than a guess. The suites were left failing, not
reclassified.

## Evidence paths

- this record; `Tools/start-editor.ps1`, `Tools/ownership.ps1`
- the lease's exact contents at both states are quoted above
- editor at the end of the round: 0 running, 0 commandlets

## Next action, in one step

Implement the reclaim rule for a lease whose editor belongs to THIS project, then run
`test-graph-repoint.py` and `test-graph-swap-call.py` to a pass. Nothing else in S04 can be trusted until the
graph repoint migration has its regression evidence.
