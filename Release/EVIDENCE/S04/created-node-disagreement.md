# S04 evidence — an unexplained report/reader disagreement on the created target

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.

**State: C09's contract assertions pass. A NEW, UNEXPLAINED disagreement is open — the tool reports creating a
node that the reader does not show — and I am recording it as unexplained rather than assigning a cause.**

## The disagreement, with both operands printed

The case now prints the numbers on failure, which is what made this visible:

```
FAIL the READER shows a link from the created node
     looked for '(-> AppendChainProbe:24928f)' in the readback; 0 line(s) matched
FAIL the created node is present in the graph after the edit
     before=['dddecc','27cd35','c97e7d','c86ad4','fe5dca']
     after =['dddecc','27cd35','c97e7d','fe5dca']
     created=24928f
```

**The edit's own report names the created node `24928f`** (that id is where the case extracts it from), and
**`24928f` appears in neither the before nor the after node list of the reader.** The node count also FELL by one
(5 → 4), and the missing id is `c86ad4`, not the created one.

So either:

1. the created node genuinely is not persisted into the graph the reader reads, and the report names a node that
   does not survive — a **product-level** disagreement between the writer's report and the reader; or
2. it is persisted under an id the reader renders differently, and the extraction and the node scan are looking
   at two different things.

**I have not established which**, and this record does not claim one. What is new and worth stating is that the
report/reader relationship on the **created-target** path is not yet understood, and it is the one place a
caller could be told something the graph does not show.

## What the round fixed, and what that exposed

**1. Two bad checks, replaced by ones that state what they measure.**

- The link check looked for `"(-> <id>"` and `"<id>)"` — patterns taken from the **swap** suite, not this reader.
  The reader's verbatim notation, printed from a real readback, is:
  `execute (-> TurboMCPAcceptance:AppendChainProbe:422ca0) ; in 0` — **graph-qualified**, not the full asset
  path. The check now uses `(-> %s:%s)` and prints the marker and the match count on failure.
- The node check compared counts with `>=` under the name "gained exactly one node" — a condition that cannot
  express its own claim, the same shape as `GetUndoCount() >= 0` and `$anyForeign.Count -eq 0`. Printing the
  counts showed its **premise** was also wrong: the count FELL. It is replaced with "the created node is present
  in the graph after the edit", which states what the feature promises.

**2. A lifecycle defect in my own case.** I called `tmcp_editor.ensure_editor()`, which **reused an editor that
was already running** — so the fixture the rebuild had just written was never loaded and the case ran against the
previous run's persisted graph. `test-graph-repoint.py` and `test-graph-swap-call.py` both call
`subprocess.call(["pwsh", ..., "start-editor.ps1"])` instead, and `rebuild_fixture` is documented to stop the
editor and not bring it back. Fixed to match them. **This is the same class as rounds 20 and 22: my harness was
not the subject's.**

## Verified / not verified

| Item | State |
|---|---|
| C09's contract assertions on a real success | **PASS on both runs** — outcome/changed/compile/rollback/save/exit |
| The case refuses against an unvouched-for fixture | **verified — exit 2, BLOCKED** |
| The case is deterministic | **runs 1 and 2 agree exactly** |
| The reader shows the created node | **NO — the report names `24928f`; the reader shows neither it nor a gain** |
| Whether that is a product defect | **NOT ESTABLISHED** |
| The two replaced checks | rewritten to state what they measure, with operands printed on failure |
| M01–M08 | not run |

## One caveat I must state about the fixture

`TurboMCPAcceptance` has been rebuilt **many** times during this sequence, and at least one earlier run of this
case left an edit **persisted** in it. The graph the case reads may therefore not be the graph the commandlet
builds. That does not excuse the disagreement above — the report and the reader are read in the same session —
but it does mean the fixture's state is not something I can vouch for, and the next run must establish it before
drawing a conclusion.

## Evidence paths

- this record; `Tools/test-c09-success.py`
- the failure output quoted above; editor: 0 running at the end, no lease

## Next action, in one step

Establish the fixture's true state first: rebuild it, start the editor, and print the graph's node count and ids
**before any edit**, then perform one created-target edit and print the count and ids after. If the count is
stable across the edit and the created id is absent both times, the report is naming a node that is not in the
graph — a product finding for ARC02's `assets`/report path. If the count moves, the extraction or the scan is
looking in the wrong place. Print both sides; do not infer.
