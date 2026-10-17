# S04 evidence — THE LOSS IS LOCATED: the plan is clean, the EDIT does it, and it consumes an existing node

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.
Severity: **P0, located.** No mechanism is proposed here. The readings decide where it happens.

## The three readings, plus two controls

```
READING 0 (after rebuild)      5 node(s), 3 call  ['f595ec', 'ff5a40', '2072e8']
PLAN said: preview: repoint ...:f595ec.then in AppendChainProbe
READING 1 (after PLAN, no edit) 5 node(s), 3 call  ['f595ec', 'ff5a40', '2072e8']
READING 2 (no action between)   5 node(s), 3 call  ['f595ec', 'ff5a40', '2072e8']
READING 3 (no action between)   5 node(s), 3 call  ['f595ec', 'ff5a40', '2072e8']
EDIT said: changed: repointed 1 reader(s) ...; target ...:ee49a8.then; old source ...
READING 4 (after EDIT)          4 node(s), 2 call  ['f595ec', '2072e8']
READING 5 (no action between)   4 node(s), 2 call  ['f595ec', '2072e8']

PLAN changed the graph       no
a plain re-read changed it   no
the EDIT changed it          YES
a later plain read changed it no
counts: r0=5 r1=5 r2=5 r3=5 r4=4 r5=4
```

## What is now SETTLED, by reading rather than by reasoning

1. **The plan is NOT destructive.** Readings 0, 1, 2 and 3 are identical — the preview, and two further plain
   re-reads at 2-second intervals, all report **5 nodes / 3 call functions with the same ids**. The read-only path,
   including two extra reads with nothing in between, changes nothing. Two of my earlier suspicions die here as
   well: the probe graph **and** any "the engine mutates it on its own" explanation. There is no background churn.

2. **The EDIT is the only thing that changes the graph**, and it changes it **exactly once**: readings 4 and 5 are
   identical, so the graph is **stable after the edit**. The count is not drifting; it is **reduced, once**.

3. **The reduction is net −1: one `K2Node_CallFunction` is consumed and the created target never appears.**
   `ff5a40` is gone; `f595ec` (the source) survives; the target `ee49a8` the report names is in **neither** reading.

4. **And the plan said the opposite of what happened.** The plan's own words: *"preview: repoint ...:f595ec.then"*
   — a repoint of one output. What the executor did: it **consumed an existing call node** and left the source's
   link in place.

## So the defect is in the EXECUTOR, and its shape is now precise

The plan is a pure function of the graph. The executor, given that plan, **removes a node the plan did not
mention and fails to materialise the node it reports**. Every measurement in this P0 is consistent with that one
sentence, and six proposed mechanisms were wasted trying to explain it from the code instead of reading it.

**The location is not in doubt. The line is.** The executor's region is `TurboMCPGraphRepoint.cpp:215-425`: the
transaction, the `SpawnNodeOfType` call at L236, the pin-move loop, and the removal block at L309.

## The one thing that would have shown this in round 25

The three readings cost one script and thirty seconds. They would have located the defect five rounds ago. The
record of that is already in the ledger; this note exists so the next reader sees **the readings first and the
code second**, which is the order that works.

## Evidence paths

- this record; `.agent/tmp/three-readings.py` — it proposes no mechanism, it only prints four readings and two
  controls
- the output quoted above; editor: 0 running at the end, no lease

## Next action, in one step

Instrument the executor at the three points the readings identify, and print the graph's call-function count at
each: **immediately after the transaction opens (L215)**, **immediately after `SpawnNodeOfType` (L246)**, and
**immediately after the pin-move loop ends**. The reading that differs from the last is the line, and the count
is by class so ids cannot mislead. Then fix that line, and re-run the three readings — they are now the acceptance
check for this P0.
