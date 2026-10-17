# S04 evidence — the reclaim rule works, and it exposed the next blocker

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**State: `implemented_unverified`. The P0 lease defect is FIXED AND VERIFIED. The graph repoint migration's
regression status is STILL UNKNOWN, and the cause has moved one step forward.**

## The reclaim rule: implemented and verified

`Tools/ownership.ps1` now treats a lease as follows:

| Lease | Verdict |
|---|---|
| no `EditorPid` (old format) | blocks — the conservative direction for an unreadable claim |
| names an `EditorPid` that is **gone** | free — the thing the lease protects is not running |
| names an `EditorPid` that is **this project's editor** | **`reclaimable`** — a same-project start takes it over, and the takeover is recorded in the new lease's `Note` |
| names an `EditorPid` that is **not this project's editor** | blocks — **a foreign editor is never pre-empted** |

The ownership of the named editor is decided from **that editor's own command line** by
`Test-TurboMCPOwnsProcess` — the same function the stop path uses — and not from the lease's own claim about
itself, which would make the rule a tautology.

**MEASURED, and it is the verification that matters:** the suite that was failing at the ownership step now gets
all the way past it.

```
ok    lease held by start-editor:7232 (pid 7232) at ...\turbo-mcp-lease.json
ok    no editor running
ok    window up after 15s: TurboMCPTest - Unreal Editor
ok    MCP server listening on port 8000
ok    all four TurboMCP toolsets answered
RESULT: PASS
```

The lease no longer refuses a start. The stale-lease P0 is closed on evidence.

## The next blocker, exposed rather than created

With the lease no longer in the way, the suite reaches its own editor start and fails there:

```
FAIL  this project's editor is already running (PID 2032)
FAIL  port 8000 is already in use, so a second MCP server would fail to bind
```

This is `start-editor.ps1` correctly refusing to start a SECOND editor for the same project — a deliberate
behaviour ("It refuses to start a second editor rather than warning. It did warn once, and started one anyway,
which left two servers racing for port 8000."). The suite calls `start-editor.ps1` **without** `-StopFirst`
after its fixture rebuild has already brought an editor up, so the launcher refuses.

**So the remaining question is a design question, not a defect in the reclaim rule:** should a start whose
target editor is ALREADY UP AND HEALTHY be an idempotent success (reuse) or a refusal? The launcher currently
refuses. The suite needs "reuse". I did **not** change this, because the behaviour is deliberate, load-bearing
(`run-suites.ps1` depends on the refusal to detect a wedged editor), and a change to it needs its own case
rather than a guess at the end of a round.

## A parse defect I made and fixed, recorded because it cost time

The first version of the reclaim block did not parse, and the parser reported the error at a **later** line than
the actual mistake — so two plausible-looking regions were inspected before the real one. The fix was to state
the strings by concatenation instead of a multi-line `-f` format expression. Worth recording: a cascading parse
error sent me to the wrong place twice, which is the same shape as every other misdiagnosis this run.

## Verified / not verified

| Item | State |
|---|---|
| The lease records the editor's pid, not the launcher's | **verified against a live editor** (round 11) |
| A lease whose editor is gone does not block | verified (round 11) |
| `-Stop` releases the lease | **verified** (round 11) |
| A lease naming THIS project's editor is reclaimable | **verified — the suite passes the ownership step** |
| A foreign editor is never pre-empted | verified in round 4 (E01) |
| `test-graph-repoint.py` passes | **NOT yet** — blocked on the second-editor refusal |
| `test-graph-swap-call.py` passes | **NOT yet** — same cause |
| The graph repoint migration's regression status | **UNKNOWN** |

## Evidence paths

- this record; `Tools/ownership.ps1`
- tracked-diff fingerprint: `7c9bf31400391a2b3ae678fb8bf73bfa772434093706d68f5c23bcb0f0616afc`
- editor at the end of the round: 0 running, 0 commandlets, no lease

## Next action, in one step

Decide and implement the reuse rule: a `start-editor.ps1` invocation whose target editor is already up, healthy
and **this project's** should succeed idempotently (the runner already reaches this conclusion through its own
health probe — see `run-suites.ps1`'s `Start-EditorAndWait`). Add a case for it. Then run
`test-graph-repoint.py` and `test-graph-swap-call.py` to a pass; nothing else in S04 can be trusted until the
graph repoint migration has regression evidence.
