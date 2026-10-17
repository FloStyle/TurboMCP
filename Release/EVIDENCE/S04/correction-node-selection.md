# S04 evidence — a correction: the round-13 hypothesis was WRONG

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**State: `implemented_unverified`. One correction, one narrowed failure, and the graph repoint migration's
regression status is STILL UNKNOWN.**

## The correction, measured

Round 13 recorded: "`test-graph-repoint.py` picks nodes **by position** without checking class", and carded a fix
to "select the call nodes BY CLASS". **That hypothesis was wrong**, and testing it took one command:

```python
# the suite's own pattern, run against the live read
r"/Game/Tests/TurboMCPAcceptance\.TurboMCPAcceptance:%s:([0-9a-f]{6}) ; class K2Node_CallFunction" % GRAPH
# -> ['c96a8c', '3ad9dd', 'c3c109']
```

The suite **already filters by class**. The earlier analysis that produced the wrong hypothesis was based on a
looser pattern (`r":%s:([0-9a-f]{6})"`) and took its results as if they were the suite's — the two lists differ,
and I did not notice. `K2Node_IfThenElse` and `K2Node_VariableGet` were never in the suite's candidate list.

**Recorded because this is the failure mode I keep having to correct:** a diagnostic pattern that is not the
subject's pattern, read as though it were. It is the same shape as the round-11 lease conclusion and the two
other self-corrections in this run.

## The narrowed failure, measured

With the editor reused (the round-13 fix working), the suite runs to its four checks:

```
  ok
  ok
  FAIL      <- checks[2]: edit.startswith("changed:") and "1 reader(s)" in edit
  ok
RESULT: FAIL
```

Three of four pass. `checks[1]` (the `NoSuchPin` refusal) passes, which means the **refusal path of the migrated
tool works through this suite**. `checks[2]` fails, and the reason is not yet established.

The plan's own refusal text from the live read gives the leading hypothesis, stated as a hypothesis and not a
conclusion:

```
preview: repoint ...:74e7c4.then in AppendChainProbe
0 reader(s) would move to ...:ca5c81.then
    refused source output has no linked readers; nothing would change
```

If `keys[0]` (`c96a8c`) has no linked readers, the plan **correctly** refuses and `checks[2]` cannot pass — which
would make this a third test-fixture assumption, not a migration defect. But that was **not** verified: it needs
`keys[0]`'s actual link count and the exact `edit` text, and asserting either without them would repeat exactly
the mistake this record is correcting.

## Verified / not verified

| Item | State |
|---|---|
| The reclaim rule (round 12) | **verified** |
| The reuse rule (round 13) | **verified — the suite passes the editor lifecycle and runs its checks** |
| The migrated tool's refusal path, through this suite | **verified — `checks[1]` passes** |
| The migrated tool's success path, through this suite | **not yet — `checks[2]` fails, cause not established** |
| The graph repoint migration preserved behaviour | **UNKNOWN** |
| "The suite selects nodes by position" (round 13) | **DISPROVED — it filters by class** |

## Evidence paths

- this record; the two regex results quoted above
- editor at the end of the round: 0 running, no lease

## Next action, in one step

Print, in one command: `keys[0]`'s output-pin link counts from the live read, and the exact `edit` text. Then the
cause of `checks[2]` is either a fixture assumption (fix the case) or a migration defect (fix the product) — and
it will be known rather than guessed. Then run `test-graph-repoint.py` and `test-graph-swap-call.py` to a pass.
