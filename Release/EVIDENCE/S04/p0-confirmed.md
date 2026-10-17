# S04 evidence — THE DISCRIMINATOR DECIDES: a node is genuinely lost, and the created target never persists

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.
Severity: **P0, CONFIRMED.** One of my own earlier claims is **withdrawn** in this record.

## The measurement, one edit, counted by class

```
BEFORE  by class: {'K2Node_CallFunction': 3, 'K2Node_IfThenElse': 1, 'K2Node_VariableGet': 1}
BEFORE  ids     : ['2ed446', 'cc1058', 'fab0dd', '554569', '9ff8f2']

SOURCE used     : ...AppendChainProbe:fab0dd
EDIT report     : changed: repointed 1 reader(s) in AppendChainProbe;
                  target ...AppendChainProbe:3c85d3.then; old source retained

AFTER   by class: {'K2Node_CallFunction': 2, 'K2Node_IfThenElse': 1, 'K2Node_VariableGet': 1}
AFTER   ids     : ['2ed446', 'cc1058', 'fab0dd', '9ff8f2']

class deltas       : {'K2Node_VariableGet': 0, 'K2Node_IfThenElse': 0, 'K2Node_CallFunction': -1}
total before/after : 5 -> 4
ids reused         : ['2ed446', '9ff8f2', 'cc1058', 'fab0dd']
```

## Withdrawn: my own round-26 finding 1, and the round-25 P0 framing

**The identity is NOT unstable across this edit.** Four of the five ids were **reused**, including the source
`fab0dd` and the unchanged branch and get nodes. I had written that a recompile "gets a different id" — that is
**disproved here for this operation**, and I withdraw it as a finding. It was a hypothesis from reading
`MakeForNode`, and one run was enough to test it.

My round-25 P0 record also said the report claimed a created node while a **source** was lost; that framing is
corrected too — the source **did** survive, exactly as the report said.

## Confirmed: a node was genuinely LOST, by class

The class count for `K2Node_CallFunction` **fell from 3 to 2**. Counting by class removes the id-churn escape
route entirely: a node of that class **is gone**. The missing id is **`554569`**.

## Confirmed: the created target NEVER PERSISTS

The report names the created target `3c85d3` and says the reader was repointed to it. **`3c85d3` is in the graph
neither before nor after** — and the target type was `Development|PrintString`, which is a `K2Node_CallFunction`,
so a persisted creation would have **raised** that class count. It fell.

**So the tool created a node, linked a reader to it, reported success with `compile: passed`, and the node is not
in the graph.** That is the defect, stated now without the id-instability speculation that confused the previous
two records.

## What IS and IS NOT established

| Claim | State |
|---|---|
| The source node survives (`keepSource=true`) | **CONFIRMED** — `fab0dd` is present after |
| The report's "old source retained" | **CONFIRMED accurate** |
| The identity is unstable across a recompile | **WITHDRAWN — disproved for this operation** |
| A `K2Node_CallFunction` node is genuinely lost | **CONFIRMED — 3 → 2, id `554569` gone** |
| The created target `3c85d3` is not in the graph | **CONFIRMED** |
| Which node `554569` was, and why it went | **NOT established** |
| The mechanism that loses the created node | **NOT established** |

L309 guards removal on `if (!P.bKeepSource)`, and `P.bKeepSource` is true here, so **`RemoveNode` did not run for
the source**. The loss is therefore somewhere else, and the created node's failure to persist is a second,
related defect. Neither has a verified cause.

## The two code paths that now deserve the next look

- **L51 `BuildProbeNode` in the plan** vs **L236 `SpawnNodeOfType` in the executor**: two nodes are built for one
  creation. If the probe node is the one linked and the spawn is abandoned — or the reverse — the reported
  `TargetId` (L413, recomputed from the executor's live pointer) names a node that the graph does not keep.
- **The transaction's completion path.** If the node is created but the transaction commits without carrying it,
  the report and the graph disagree in exactly the measured way.

## Evidence paths

- this record; `.agent/tmp/class-discriminator.py`
- the output quoted above; editor: 0 running at the end, no lease

## Next action, in one step

Print, inside `ApplyTransacted`, the node the executor actually linked and the graph's node count **immediately
after `SpawnNodeOfType` and immediately after `Commit`** — plus `FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified`
timing relative to them. That shows whether the created node exists at spawn time and disappears at commit, or
never exists at all. Do not fix by weakening the report: the report and the graph must agree, and the plan's
unused `NodeCountBefore` (L45) is the guard that should have caught this.
