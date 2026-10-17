# S04 evidence — the fixture's errors are DELIBERATE, which makes my diagnostic rule too broad

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**State: `implemented_unverified`. The blocker is re-classified: it is NOT a broken fixture. It is a conflict
between my new-diagnostic rule and a fixture that is *designed* not to compile.**

## The re-classification, with the source

The previous two records called `TurboMCPAcceptance` "broken" and asked for it to be "repaired or regenerated".
That framing was **wrong**, and the commandlet says so itself:

```
L851  /**
L852   * `ValueCycleProbe` and `SelfLinkProbe`: pure-value cycles that used to recurse
L853   * through RenderPinValue -> RenderNode until the editor stack was exhausted.
L854   *
L855   * They are valid serialized pin links. The reader must identify the cycle before
L856   * the shared-value shortcut hides it, and must return a bounded diagnostic.
L857   */
L858  bool BuildValueCycleProbe(UBlueprint* InBlueprint, TArray<FString>& OutProblems)
```

and later, at the point where a probe must survive compilation:

```
L1573  // The parent class inherits this native Actor callback. Unlike the acceptance
L1574  // probe functions it is available before compilation, which matters because
L1575  // other probes deliberately contain invalid cycles for reader tests.
```

**The 7 compile errors are fixture DESIGN.** `ValueCycleProbe` and `SelfLinkProbe` exist so the graph reader
can be tested against a cyclic input — the exact case that once exhausted the editor stack. Regenerating the
asset will reproduce them, because the commandlet creates them on purpose and says why.

So "the fixture is broken" was a misdiagnosis, and it is corrected here rather than acted on. Repairing it would
have destroyed two reader test cases.

## The real conflict

The migrated repoint tool enforces ARC04's rule at **package scope**: it compares the whole asset's diagnostics
before and after and refuses on any new one. But:

- a repoint changes **one graph**;
- this asset, by design, has unrelated graphs that cannot compile;
- so on this asset the rule refuses every edit, whatever graph is being edited.

MEASURED: the six diagnostics it reported as "the edit would add" are the cycle errors from `ValueCycleProbe`
and `SelfLinkProbe` — graphs the repoint never touched. The rule is correct in principle and **too broad in
scope**.

## The design decision, and the two candidate routes

**Route A — scope the comparison to the affected graph.** Compare only the diagnostics that name the edited
function or its graph. This matches the rule to the operation: an edit to `AppendChainProbe` is judged against
`AppendChainProbe`. Cost: diagnostics that name no graph (package-level compile failures) must be handled
explicitly rather than dropped by accident.

**Route B — keep package scope, make the message name the scope.** Report the pre-existing count, state that
they are outside the edit's scope, and proceed. Cost: an edit *can* then succeed on an asset that does not
compile, which is a real weakening unless the scoping is explicit.

Route A is the one that keeps the rule meaningful, and it needs the diagnostic's **graph identity**, which my
`NormalizeDiagnostics` currently discards — it keeps severity and text only, and the record in
`first-migration.md` already noted that the producing node is not available through this API. Whether the graph
name is recoverable from the message text is **not established**, and it decides Route A's feasibility.

## Verified / not verified

| Item | State |
|---|---|
| The baseline mechanism and its fix | **verified** (round 17: 7 messages, 7 errors) |
| The refusal message no longer blames the caller | implemented |
| The fixture's errors are deliberate reader-test cycles | **verified from the commandlet's own comments and code** |
| "The fixture is broken" (rounds 15–17) | **CORRECTED — it is designed to be invalid** |
| A repoint of one graph is blocked by unrelated graphs' errors | **verified** |
| Whether the graph name is recoverable from a diagnostic | **not established** |
| The graph repoint migration preserved behaviour | **UNKNOWN** |
| `test-graph-swap-call.py` | not run |

## Evidence paths

- this record; `Source/TurboMCP/TurboMCPAcceptanceAssetCommandlet.cpp` L851-858 and L1573-1575
- round 17's measurement of the forced baseline and this round's source reading

## Next action, in one step

Establish whether a compile diagnostic carries its graph or function identity — read what
`FTokenizedMessage` offers beyond `GetSeverity()` and `ToText()` in one focused look. If it does, implement
**Route A**: scope the new-diagnostic comparison to the edited graph, so an edit is judged against its own
graph rather than the whole asset. Then run `test-graph-repoint.py` and `test-graph-swap-call.py` to a pass —
and if Route A is not feasible, bring the choice between A and B back as the one architectural question this run
has, rather than weakening the rule silently.
