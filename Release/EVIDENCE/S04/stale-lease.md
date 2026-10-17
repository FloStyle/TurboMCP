# S04 evidence — the stale-lease defect found by the migration's own regression check

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**State: `implemented_unverified`. The fix is written and parse-clean but NOT yet verified end to end, and the
graph repoint suite has not been re-run to a pass.**

## What happened, in order

Round 9 migrated the graph repoint writer. This round began by running the suites that exercise it — the
regression check that migration owes — and they failed. The cause was **not** the migration:

```
  FAIL  the lease could not be taken: another owner holds it: 'start-editor:7488' (pid 7488); the owning
        process is gone. Release it with -Stop, or delete ...\.agent\turbo-mcp-lease.json if the run is over.
```

## The defect, and why it blocks a whole class of work

`start-editor.ps1` wrote the lease with the **LAUNCHER's** pid. The launcher exits as soon as the editor is up, so
by the time any later process read the lease, its recorded pid was gone — and the lease refused every start
afterwards, with advice to delete a file by hand.

The fixture-rebuilding suites call `start-editor.ps1` **from Python**, so every one of them fails on a lease that
no live process holds. MEASURED on `test-graph-repoint.py`: the suite never reached its own assertions; it
stopped at the ownership step. `test-graph-swap-call.py` failed the same way, and its "readback preserves the
migrated execute link" line is a consequence of the editor never starting, not a separate finding.

## The correction

**`start-editor.ps1`** now rewrites the lease with the **editor's** pid once the editor is up, because the world
state that matters is "is an editor of THIS project running", not "is the process that asked for it still alive".

**`Tools/ownership.ps1`** now treats a lease as follows:

| Lease | Verdict |
|---|---|
| names an `EditorPid` that is **running** | blocks, naming that editor and its owner |
| names an `EditorPid` that is **gone** | **free** — the thing the lease protects is not running, so a new start is legitimate |
| has **no** `EditorPid` (an old lease from before this field) | blocks, as before — the conservative direction for an unreadable claim is to refuse rather than to assume |

The third row is deliberate: a lease written by an older build carries no editor pid, so the strict behaviour
stays for it, and the operator's documented recovery (`-Stop`) releases it.

## What is NOT verified, and it is the important part

- **The fix has not been exercised.** A lease in the OLD format was still blocking after the change, which is the
  third row behaving as designed — but that also means the new `EditorPid` path has never run against a real
  editor. It must be: start the editor, confirm the lease carries `EditorPid`, stop the editor, and confirm the
  next start succeeds WITHOUT any manual deletion.
- **`-Stop` did not delete the lease in the old format.** MEASURED: `lease after -Stop: True`. The `-Stop` path
  contains an explicit stale-lease release (`$anyForeign.Count -eq 0`), so `$anyForeign.Count` is evaluating to
  something other than 0 for an empty collection — the same shape as `GetUndoCount() >= 0` from round 8, a test
  that cannot be false. It needs fixing and asserting.
- **`test-graph-repoint.py` and `test-graph-swap-call.py` have not been re-run to a pass.** The graph repoint
  migration's regression status is therefore UNKNOWN, and that is the single most important open question from
  this round.
- The engine writes its own `-Stop` for `UnrealEditor-Cmd` by PID from a descendant walk; that path was not
  re-exercised either.

## Evidence paths

- this record; `Tools/ownership.ps1`, `Tools/start-editor.ps1`
- the blocking lease's exact contents (old format, `Pid: 7488`, no `EditorPid`) were read and are quoted above
- tracked-diff fingerprint at the end of this round:
  `7c9bf31400391a2b3ae678fb8bf73bfa772434093706d68f5c23bcb0f0616afc`

## Next action, in order

1. Start the editor, confirm the lease carries `EditorPid`, stop it, and confirm the next start needs no manual
   deletion. If `-Stop` still leaves the lease, fix the `$anyForeign.Count` condition and assert it.
2. Re-run `test-graph-repoint.py` and `test-graph-swap-call.py` to a pass — that is the graph repoint
   migration's regression evidence, and it is currently missing.
3. Then the round-9 follow-ups: the exact wrapper shape for a migrated FString answer, C09's success half against
   a fixture whose target output is free, M01–M08, and the failing `TurboMCP.T04.QueueForCompilationDuringCompile`
   native test.
