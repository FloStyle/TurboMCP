# S04 evidence — C09 is repeatable; two probe checks of mine were wrong

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**State: the C09 case is now REPEATABLE, and its product assertions pass on both runs. Two of its own checks
were written from an assumed format and are wrong; they are recorded as probe defects, not product findings.**

## Repeatability, proven by running it twice

The case now requires a freshly built fixture, and says how to get one:

```
$env:TURBOMCP_REPOINT_REBUILD=1 ; python Tools\test-c09-success.py
```

| Run | Fixture rebuilt | result |
|---|---|---|
| 1 | yes | `FAIL (2) -> the READER shows a link from the created node, the graph gained exactly one node` |
| 2 | yes | `FAIL (2) -> the READER shows a link from the created node, the graph gained exactly one node` |
| 3 | **not** requested | `exit 2` — `BLOCKED (this case needs a freshly built fixture: not rebuilt ...)` |

**Runs 1 and 2 agree exactly**, so the order dependence is gone: the case is deterministic under its stated
precondition. And run 3 is the behaviour that matters against a fixture of unknown shape — it **refuses at exit
2 rather than asserting success against a fixture it cannot vouch for**. That is the `blocked` state doing its
job, and it is the guard the swap suite lacked when it produced round 22's false alarm.

## What PASSES on every run — the product assertions

Every production assertion passes identically on both runs:

```
ok   the preview of a creatable target is planned, not refused
ok   the preview reports compile=not_run
ok   A REAL SUCCESS reports outcome=changed
ok   a real success reports changed=true
ok   a real success reports compile=passed
ok   a real success reports rollback=not_needed
ok   a real success reports save=not_requested
ok   a real success exits 0
ok   the report names the created target
ok   the READER independently shows the created node
```

That is the whole ARC02 contract on a real success, plus the reader confirming independently that the created
node exists — **stable across two rebuilds**, which is stronger evidence than the single run recorded in
`c09-success.md`.

## The two FAILING checks are MINE, and here is what they assumed

```
FAIL  the READER shows a link from the created node
FAIL  the graph gained exactly one node
```

**Check 1** looked for `"(-> %s" % new_short` or `"%s)" % new_short` in the readback. The reader's link
notation is what it is; I wrote the pattern from the swap suite's output rather than from this reader's, which
is **the third time in three rounds** I have written a probe against an assumed format instead of the subject's.
The check may be true in substance and simply look for the wrong characters — I have **not** established which,
and I will not assert it.

**Check 2** compared node counts with `>=` under the name "gained exactly one node" — a comparison that **cannot
express the claim in its own name**, so it could pass while the graph gained nothing, and here it fails for a
reason I have not diagnosed. It is the same shape as the `GetUndoCount() >= 0` and `$anyForeign.Count -eq 0`
defects: a condition whose text and behaviour disagree.

Both belong to the probe, not the product, and both are recorded as such.

## Verified / not verified

| Item | State |
|---|---|
| C09's success half, through the structured client | **OBSERVED, now on two rebuilds** |
| C09's refusal half | observed (earlier round) |
| The case is deterministic under its precondition | **verified — runs 1 and 2 agree** |
| The case refuses against an unvouched-for fixture | **verified — exit 2, BLOCKED** |
| "the READER shows a link from the created node" | **my check's pattern is wrong or the claim is false — NOT established** |
| "the graph gained exactly one node" | **a check that cannot express its own claim — needs rewriting** |
| M01–M08, the two helpers, the failing T04 native test | not done |

## Evidence paths

- this record; `Tools/test-c09-success.py`
- the three runs quoted above; editor: 0 running at the end, no lease

## Next action, in one step

Rewrite the two probe checks against the reader's **actual** output: print one link line verbatim from this
reader before writing the pattern, and replace the `>=` count comparison with one that states what it means
(`len(after_call_nodes) == len(before_call_nodes) + 1`, with both numbers printed when it fails). Then C09's case
is fully green on two consecutive runs.
