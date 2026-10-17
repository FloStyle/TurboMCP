# S04 evidence — PRODUCT DEFECT: the created-target edit LOSES a node and reports success

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.
Severity: **P0.** This is not a test defect and it is not a fixture question.

## The measurement, around ONE edit and nothing else

Fixture rebuilt, editor started, state read, **one** created-target edit, state read again:

```
BEFORE any edit: 5 node(s)
    444ca0  K2Node_IfThenElse
    44b45e  K2Node_VariableGet
    fb5390  K2Node_CallFunction
    2e8c29  K2Node_CallFunction
    d6ba11  K2Node_CallFunction
BEFORE call nodes: ['fb5390', '2e8c29', 'd6ba11']

--- the edit's report ---
changed: repointed 1 reader(s) in AppendChainProbe; target ...AppendChainProbe:848287.then; old source retained
compile: passed (compared against the pre-edit diagnostics, scoped to graph 'AppendChainProbe'; no new
         diagnostic naming it, 6 other diagnostic(s) in this asset out of scope)

AFTER one edit: 4 node(s)
    444ca0  K2Node_IfThenElse
    44b45e  K2Node_VariableGet
    fb5390  K2Node_CallFunction
    d6ba11  K2Node_CallFunction
AFTER call nodes: ['fb5390', 'd6ba11']

CLAIMED created node: 848287
  present BEFORE: False
  present AFTER : False
  count before/after: 5 -> 4  (delta -1)
```

## What this says, and every part of it is factual

1. The tool reports it **created** a target node, `848287`, and **repointed a reader to it**.
2. **`848287` is not in the graph before the edit, and not in it after.** It is nowhere.
3. The node count **fell by one** (5 → 4) — so the edit **removed** a node.
4. The node that disappeared is **`2e8c29`**, the one used as the **source** of the edit.
5. The report says **`old source retained`** while the source node is **gone**.
6. The report says **`compile: passed`** and **`changed`** — so a destructive edit is presented as a clean
   success.

**This is the worst class of defect in the whole contract this step exists to build:** a caller is told a change
succeeded, on a compiled asset, on the created-target path — the very path an agent uses when it does not know
what to repoint to — while the graph lost a node and the node it claims to have created does not exist.

## Where it most likely lives, stated as a hypothesis and not a conclusion

The reported target id is a **runtime object id** for a node that is not in the saved graph, and the source node
is destroyed. The two readings that fit:

- **the transaction does not contain what the report claims.** `ApplyTransacted` creates the target before or
  outside `Tx`, or the node is created and linked but never registered into the graph — so `FScopedTransaction`
  reports success while the object the report names is not part of the persistent graph; or
- **the created node is transient and is collected**, and the *reader* is right about the graph while the
  *writer* is right only about a temporary object.

The round-9 migration note is directly relevant: destroying a transaction without `Cancel()` is the commit in
this engine, and `ApplyTransacted` "commit-or-cancel[s] ... ended exactly once". A commit that does not carry the
created node into the graph would produce **exactly** this signature. **That is a hypothesis. It is not verified,
and the ledger says so.**

## Why no test caught it

The case that finds it is the one written this round: it compares the **reader's** node ids before and after
against the **node the report claims**. The earlier version of the same case checked `>=` on counts — a
comparison that cannot express its own claim — and its link pattern was taken from another suite's output. Both
were fixed this round, and the defect appeared. **That is the second time in this run that a check which could
not fail was hiding a real defect**, and the first time it was in the product's own guard.

## What this means for the release

**S04 cannot close, and neither can any claim that graph repoint is safe, until this is fixed.** C09's contract
fields are individually correct (`outcome=changed`, `compile=passed`, `exit 0`) and that makes the defect
*dangerous* rather than benign: the tool is confidently wrong. Every migrated and unmigrated mutation tool that
creates nodes should be examined for the same signature.

## Evidence paths

- this record; `.agent/tmp/fixture-state.py` (the sole script run — fixture rebuilt, one edit, no other calls)
- the output quoted above in full; editor: 0 running at the end, no lease

## Next action, in one step

Read `ApplyTransacted`'s node-creation path in `Source/TurboMCP/TurboMCPGraphRepoint.cpp` and establish whether
the created target is created **inside** `Tx` and **registered into the graph** before `Commit` — and whether the
source node's destruction is inside the transaction. Then reproduce with the transaction's own state printed
(`Tx.IsActive()`, the guard's state, and the graph's node count) at each step. Do not fix by weakening the report:
the report and the graph must agree.
