# S04 evidence — SEVENTH hypothesis disproved, and the instrument itself is now the suspect

Step: S04 — Implement the fixed shared contracts.
Date: 2026-10-16.
Severity: **P0, still open.** No cause is claimed here. Two readings are reported, and one of them points at my
own measurement.

## Reading 1 — the two resolvers return the SAME object

I instrumented both resolvers side by side, printing each pointer, name, **match count**, and whether they agree:

```
[TurboMCP/P0] resolve 'AppendChainProbe': local(3 arrays)=0000021756B048C0 'AppendChainProbe' [1 match(es)]
              | shared(4 arrays)=0000021756B048C0 'AppendChainProbe' [1 match(es)] | SAME OBJECT: YES
```

**Same pointer, one match in both, `SAME OBJECT: YES`.** The two-resolver divergence — the previous record's
localised cause — **does not occur on this fixture**. That is the **seventh** mechanism disproved, and it was
disproved by the exact run the previous record said not to skip. Doing that run first is why this cost one round
instead of several.

The divergence between the two functions **is** a real latent defect — three arrays against four, with nothing
comparing them — and it stays carded. It simply is not what caused these measurements.

## Reading 2 — the counts, and they do not reconcile

```
[TurboMCP/P0] A: transaction open: 6 node(s), 3 call function(s)
[TurboMCP/P0] B: after SpawnNodeOfType: 7 node(s), 4 call function(s)
[TurboMCP/P0] C: after the pin-move loop: 7 node(s), 4 call function(s)
reader counts: r0=5 r1=5 r2=5 r3=5 r4=4 r5=4
```

**The executor's resolver returns the same object the reader reads** — and the executor counts **6** nodes in it
while the reader's text shows **5**. Same object, same moment, two different counts.

**Therefore one of the two counts is wrong, and it is very likely MINE.** My count is a regex over the reader's
text output:

```python
NODE_RE = re.compile(r":%s:([0-9a-f]{6}) ; class (\S+)" % GRAPH)
```

That counts **lines the reader chose to print**, not nodes in the graph. And the reader has a `tokenBudget`
argument — which I pass as `""` and have never examined. A reader that **omits** a node (for budget, by detail
level, or deliberately) would produce exactly the whole P0: a "lost" node that was in fact **never shown**,
a created target absent from "both readings", and a count that falls by one.

**I have not established that**, and I am not claiming it. But it is now the leading explanation, it is testable
in one command, and it is the **first** explanation in this P0 that accounts for the executor and the reader
disagreeing while looking at the same object.

## The honest position

| Claim | State |
|---|---|
| The executor edits the same graph object the reader reads | **verified — same pointer, same resolver result** |
| The two resolvers differ (subset of arrays) | **real latent defect; NOT the cause here** |
| A node is genuinely lost from the graph | **NOT established — the only evidence is a count derived from printed text** |
| The reader omits nodes from its output | **NOT established; leading explanation** |
| Every mechanism so far | **seven proposed, seven disproved** |

## The one command, and it settles everything

Print the graph's node count from the **executor's** side (already instrumented: 6) and the reader's **raw**
output with any budget or detail limit made explicit — then compare the node ids the reader **printed** with the
`G->Nodes` list the executor **iterates**. If the reader omits nodes, the P0's "loss" is a **reporting** defect
(the reader is not showing the graph it describes, which is itself serious) and **not** data loss, and every
measurement in this P0 needs re-reading through that lens.

## Evidence paths

- this record; the instrumented log lines quoted above, from `TestProject\Saved\Logs\TurboMCPTest.log`
- `Source/TurboMCP/TurboMCPGraphRepoint.cpp` — `LogGraphResolution` and `LogNodeCount` (still in the tree; must
  be removed or cvar-gated before S17)
- editor: 0 running at the end, no lease

## Next action, in one step

Have the executor print the **ids** of every node in `G->Nodes`, and compare them with the ids the reader prints
in its answer for the same graph. Two lists, one run. If they differ, the reader is not showing the graph it
describes — and that, not data loss, is the defect this P0 has been chasing for ten rounds.
