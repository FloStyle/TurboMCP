# S04 evidence — the fixture is DETERMINISTIC, so the leftover hypothesis is disproved and the P0 is real

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.
Severity: **P0, confirmed again — and this time the instrument is clean.**

## The measurement that should have come first

The graph's node ids, read **before** a fixture rebuild and **after** it, with **no edit anywhere in the script**:

```
AS-IS (no rebuild): 5 node(s)
  504adf/IfThenElse, f7df37/VariableGet, f3ad8a/CallFunction, 12b579/CallFunction, 1789b5/CallFunction

rebuild: ok=True  "the acceptance asset was rebuilt"

AFTER REBUILD: 5 node(s)
  f2fd88/IfThenElse, fe1d88/VariableGet, f89902/CallFunction, 2b4eb4/CallFunction, 143340/CallFunction

delta: 0 node(s)
```

**The rebuild changes nothing that matters: 5 nodes before, 5 after, 3 of them `K2Node_CallFunction` in both
reads.** The ids differ because a rebuilt asset has new node GUIDs — which is expected and, incidentally, is the
**one** part of the earlier "identity instability" hypothesis that was true. The **composition is identical.**

## What that disproves, and it is my own hypothesis from the last round

I had written: *"the vanished node is almost certainly the created target of a previous run, still in the
fixture and now consumed ... my discriminator may not be comparing a clean fixture with a dirty one."*

**That is wrong.** The fixture is **deterministic at 5 nodes (3 call functions)**, rebuilt or not. So:

- the instrument **is** clean;
- the P0's arithmetic **must** be explained inside a single edit;
- and the P0 is **real**, not a fixture artifact.

That is the **sixth** of my own mechanisms disproved in six rounds. The pattern is now the most reliable finding
in this P0, and it is a finding about my method: **I have proposed six mechanisms and measured one of them
correctly.** Every one was plausible from reading the code; none survived a measurement.

## So the P0 stands as measured, on a clean instrument

```
BEFORE (3 call): ... f3ad8a  12b579  1789b5 ...
report         : changed: repointed 1 reader(s) ...; target <new>.then; old source retained; compile: passed
AFTER  (2 call): ... f3ad8a          1789b5 ...
```

- the **source `f3ad8a` is retained** — accurate;
- one `K2Node_CallFunction` is **gone**, and it is not the source;
- the **created target is in neither list**;
- net **−1**, every run;
- and a node-count guard running **after the commit** saw the **expected** count.

## The next measurement, and the reasoning is now sharper

The guard saw a correct count **after the commit**, and the count is wrong by the time the *next call* reads. Two
things happen in that window: the tool's own **recompile**, and whatever the engine does afterwards.

But there is a **third** possibility this round's result makes worth testing, and it is cheap: **the PLAN might
itself be destructive.** `PreviewTurboMCPGraphRepoint` builds a plan, and the plan creates a transient probe node
— I disproved that the *probe* touches the real graph, but I never measured the **real** graph's count across a
**plan with no edit at all**. If a plan-only call changes the real graph's node count, that is the whole defect
and it is in the read-only path.

**One command:** read the count; call `PreviewTurboMCPGraphRepoint` with `targetTypeId` and **no edit**; read the
count again; then read it a second time later. If the count changes at the plan, the defect is in the plan. If it
changes only after an edit, it is in the executor. If it changes between two plain reads with nothing in
between, it is neither — and the tool's report is being produced across a state the engine mutates on its own.

## Evidence paths

- this record; `.agent/tmp/fixture-delta.py` (no edit anywhere in it)
- the output quoted above; editor: 0 running at the end, no lease

## Next action, in one step

Run the three-read measurement above. **Stop proposing mechanisms and take the three readings first** — that is
the discipline this P0 needs, and after six disproved hypotheses it is the only honest next step.
