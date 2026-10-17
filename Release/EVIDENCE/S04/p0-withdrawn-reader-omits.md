# S04 evidence — **P0 RESOLVED: there was never any data loss. The READER omits nodes from its output.**

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.
Severity: the P0 as stated is **withdrawn**. A different, real defect is established in its place.

## The diff, and it is decisive

The executor printed **every id in `G->Nodes`**, in the same id form the reader emits:

```
IDS before: 6 -> a45f75, 3bae0e, 35da20, fae497, fdd181, 199be2
IDS after : 7 -> a45f75, 3bae0e, 35da20, fae497, fdd181, 199be2, d938b0
```

The reader, on the same graph and the same call:

```
READING 0 (after rebuild): 5 node(s), 3 call  ['fae497', 'fdd181', '199be2']
READING 4 (after EDIT)   : 4 node(s), 2 call  ['fae497', '199be2']
```

## What this proves

1. **The graph contains SIX nodes. The reader prints THREE of them.** The omitted ids are `a45f75`, `3bae0e`,
   `35da20` — every time.
2. **`fdd181` — the node this P0 called "lost" on every run for ten rounds — is in `G->Nodes` BEFORE and
   AFTER.** It was never lost, never removed, never consumed. **The reader simply stopped printing it.**
3. **The executor's arithmetic was correct the whole time**: 6 → 7, one node created, nothing removed. The
   `SpawnNodeOfType` call, the transaction, the pin-move loop, the rollback and the report were **all correct**.
4. **The "count falls by exactly one" — the single most consistent symptom across ten rounds — is the reader's
   OMISSION SET changing**, not a node disappearing.

**THE P0 IS WITHDRAWN.** There was no data loss, no unpersisted created node, and no destructive edit.

## The real defect, which is serious

**`ReadTurboMCPGraphLoad` returns an INCOMPLETE graph and does not say so.** Six nodes in, three out, no
indication in the answer that anything was withheld.

- Every agent that reads a graph through this tool sees **half the picture** — and it is the half that excludes
  `a45f75`, `3bae0e`, `35da20` without explanation.
- The set it omits **changes after an edit**, so a caller diffing two reads concludes that nodes appeared or
  disappeared when **only the reporting changed**. That is precisely the false conclusion this P0 spent ten
  rounds reaching.
- **ARC02 already defines the fields for this**: `truncated` and `omitted`. A reader that withholds content must
  **say so**. This is not a new requirement; it is an existing one not being met.

## What caused the ten-round detour, stated plainly

**My instrument counted printed lines and I treated them as the graph.** A regex over a reader's text output is a
measurement of *the reader*, not of *the graph*, and I used it as ground truth for ten rounds — through **eight
disproved mechanisms**: identity instability, the source lost, the probe orphan, the unregistered node, the
transaction commit, `AllocateDefaultPins`, the two resolvers, and (as it turned out) the premise itself.

The one run that settled it — diffing the executor's ids against the reader's — was available in round 25.

## Corrected position on every earlier record

| Record claimed | Corrected |
|---|---|
| "the created target never persists" | **false** — it is `d938b0`, present in `G->Nodes` after the edit (`CompileBlueprint` had not yet run at the "after" read; the reader never showed it either way) |
| "a node is genuinely lost (3 → 2)" | **false** — the class count fell because the reader omitted a different node |
| "the source survives / 'retained' is accurate" | **true** |
| "the tool reports success while destroying data" | **false** — the tool's arithmetic was correct |
| the guard "saw a correct count" | **correct, and it was right to pass** |

The `NodeCountBefore` guard should **stay**: it is a correct check, and it passed because the state was correct.

## The two defects that DO stand, both now established

1. **The reader omits nodes without saying so.** It must report `omitted`/`truncated` per ARC02, and the omission
   set must not change silently after an edit. **This is the P0, correctly stated.**
2. **Two graph resolvers search different arrays** (3 vs 4, nothing comparing them). Proven not to be the cause
   here; still a latent identity defect of the ARC06 family.

## Evidence paths

- this record; the log lines quoted above from `TestProject\Saved\Logs\TurboMCPTest.log`
- `.agent/tmp/three-readings.py`; `Source/TurboMCP/TurboMCPGraphRepoint.cpp` (`LogNodeIds`)
- `LogNodeIds`, `LogGraphResolution`, `LogNodeCount` are **still in the tree** and must be removed or cvar-gated
  before S17

## Next action, in one step

Establish **why** `ReadTurboMCPGraphLoad` prints three of six nodes, and make it **declare what it withheld** —
`omitted`/`truncated` — for every read. Then the P0 card becomes "the reader is incomplete and silent", which is
testable directly: read a graph, and assert that the answer either lists every node in `G->Nodes` or names the
count it omitted.
