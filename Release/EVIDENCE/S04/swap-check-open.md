# S04 evidence — the swap check: one attempt, one instrument defect, cause still not established

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**State: `implemented_unverified`, unchanged in substance. The graph repoint migration's regression evidence
still PASSES (round 19). The swap check is NOT diagnosed, and my own diagnostic was defective.**

## What the round attempted, and why it did not settle the question

The plan was one focused run: print the readback, the extracted replacement id, and the exact link notation the
case looks for. The diagnostic I wrote reproduced the failure but **did not reproduce the suite's conditions**,
and the output shows it:

```
node_id chosen: c96a8c
--- EDIT ---
error: node 'c96a8c' is not in graph 'AppendChainProbe'
replacement_id:
```

`EditTurboMCPGraphSwapCall` refused my call with `node 'c96a8c' is not in graph 'AppendChainProbe'` — while the
suite's own edit reports a **changed** swap and passes five of its six checks. So my probe chose a node by a
pattern that is **not** the suite's, and asked a tool that expects a different id form than the one I passed.

**That is the same defect as round 14**, in the same place: a diagnostic that is not the subject's pattern, read
as though it were. I am recording it as a second instance rather than repeating the mistake by drawing a
conclusion from the output above. The check's cause remains **NOT ESTABLISHED**, and nothing in this round's
per-run output supports a claim about it.

## What IS settled, and unchanged

| Item | State |
|---|---|
| The graph repoint migration's regression evidence | **PASSES** — `test-graph-repoint.py` exit 0, all four checks (round 19) |
| The ARC04 baseline mechanism and fix | verified (round 17) |
| The diagnostic scope conflict and Route A | verified (round 19) |
| The editor lifecycle (lease reclaim, reuse) | verified (rounds 12, 13) |
| `test-graph-swap-call.py`'s sixth check | **NOT diagnosed**; five of six pass |
| Whether it is finding G06 or a case defect | **not established** |

## The precise next step, narrowed by this round's failure

The suite's own run is the thing to instrument, not a re-creation of it: take `Tools/test-graph-swap-call.py`
and add **one print** of the three values that decide the check, immediately before the check itself —

```python
print("REPLACEMENT_ID:", repr(replacement_id))
print("LOOKING FOR   :", repr(":%s)" % replacement_id.rsplit(":", 1)[-1]) if replacement_id else None)
print("AFTER TAIL    :", repr(after[-600:]))
```

That prints the subject's own values rather than a re-creation of them, which is the whole reason this round's
attempt was inconclusive. The answer will be one of: the extraction regex no longer matches the edit's wording
(then the case is stale), or the link is genuinely absent from the readback (then it is finding G06, and the
swap tool is the defect).

## Evidence paths

- this record
- `Tools/test-graph-swap-call.py:64-69` — the extraction and the three checks it feeds
- editor: 0 running at the end of the round, no lease

## Next action, in one step

Instrument the SUITE, as above, and run it. Print the subject's own values rather than re-creating them.
