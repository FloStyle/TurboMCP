# S04 evidence — the guard was built and it DID NOT catch the defect, which relocates the cause

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.
Severity: **P0, still open.** This record reports a **negative** result, and that negative is the progress.

## What was built

The plan's unused `NodeCountBefore` is now used. The success path is wrapped in its own scope so `FScopedTransaction`
is destroyed — **which is the commit on this engine** — before the guard runs, and the guard then:

- re-resolves the graph and reads `Nodes.Num()`;
- computes the expectation exactly: `NodeCountBefore + (created ? 1 : 0) - (kept ? 0 : 1)`;
- on a mismatch sets `OutcomeUnknown` / `changed = null` / `graph_nodecount_mismatch` and **reports both counts**;
- on a match reports both counts anyway.

Exact comparison, not `>=`, with both operands printed. Build: **exit 0**, artifact 3 742 720 bytes.

## The result, and it is negative

```
BEFORE  by class: {'K2Node_CallFunction': 3, 'K2Node_IfThenElse': 1, 'K2Node_VariableGet': 1}
        ids     : ['05c6ec','2cfb51','78fc9e','b9f113','db296c']
report  : changed: repointed 1 reader(s) ...; target ...:0a7db5.then; old source retained
        : compile: passed
AFTER   by class: {'K2Node_CallFunction': 2, ...}
        ids     : ['05c6ec','2cfb51','78fc9e','db296c']
```

**The guard PASSED and the tool still reported `changed ... compile: passed`** — while the count measured
afterwards is 4 where the guard expected 5, and `b9f113` is gone.

So the guard did **not** catch the defect. That is not a failure of the guard's logic: it means **the graph
satisfied the expectation at the moment the guard ran**, and the node disappeared **after** that moment.

## What that relocates the cause to — and it is the first cause-shaped evidence in this P0

The guard runs **after the commit**, inside the same call. The discrepancy appears **after the call ends**. So the
loss is caused by something that happens **between the guard and the next reader call** — and inside this window
the only things that touch the graph are:

1. **the blueprint recompile** the tool itself performs (`CompileBlueprint`), and
2. **`FBlueprintEditorUtils` cleanup / garbage collection** of the node the spawner returned.

This **excludes** every hypothesis I had already withdrawn, and it excludes the commit itself: the commit had
already happened when the guard saw a correct count.

**The single most likely reading**, stated as a reading and not a finding: the created node is registered in
`G->Nodes` but is **not owned by the blueprint's graph in a way that survives a recompile or a GC pass** — so it
counts while it is alive and is collected afterwards. The earlier observation that
`SpawnNodeOfType` calls `Spawned->AllocateDefaultPins()` **while `BuildProbeNode` does not** is now the most
interesting difference in the code, because it is the only visible difference between the path that works and the
path that does not.

## The lesson about the guard itself, which generalises

**A guard placed inside the operation cannot verify the operation's aftermath.** This is the same shape as the
round-17 baseline defect: the baseline was read at a moment when the compiler had nothing to report, so it saw
nothing. Here the count is read at a moment when the node still exists, so it sees the wrong thing — or rather,
the right thing at the wrong time.

The migrated writer already has the correct pattern for this and it is worth naming: **`EditTurboMCPGraphRepoint`
should verify by READBACK** — re-read the graph through the same code the reader uses, and confirm the node it
claims to have created is present and that the reader moved. That is the shape already used by the graph repoint
case and by the migration's own readback comparison.

## Since the guard is built and correct in itself, it stays

It is not wasted: it will catch any mismatch that IS visible at commit time, it reports the arithmetic, and it
costs one graph resolution. But it must be **paired** with a post-call verification, because the defect it was
written for is invisible from inside the call.

## Evidence paths

- this record; `Source/TurboMCP/TurboMCPGraphRepoint.cpp` (the scoped success block and the guard)
- `Release/patch_nodecount_guard.py`; build exit 0, artifact 3 742 720 bytes
- the discriminator output quoted above; editor: 0 running, no lease

## Next action, in one step

**Verify by readback inside the tool**, the way the writer already verifies elsewhere: after the guard, re-read
the graph through the reader's own path and confirm that the target the report names is **present** and that the
source's reader **moved**. Then the tool cannot report a success it cannot see. In parallel, test the
`AllocateDefaultPins` difference directly: remove that one call from `SpawnNodeOfType`'s path (or make it
conditional on pins being absent) and re-run the discriminator — if the node then survives, the cause is found
and it is one line.
